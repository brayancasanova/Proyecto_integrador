acomoda el código para que sea óptimo:
#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "time.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHT.h>
#include <RTClib.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ------------------ CONFIG OLED ------------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1 
// Se asume dirección I2C 0x3C (la más común para OLED 0.96")
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ------------------ CONFIG SENSORES ------------------
#define DHT_PIN 1
#define DHT_TYPE DHT22

DHT dht(DHT_PIN, DHT_TYPE);

#define DS18B20_PIN 5
OneWire oneWire(DS18B20_PIN);
DallasTemperature ds18b20(&oneWire);

#define PH_PIN 6
#define NUM_SAMPLES 10
float voltagePH4 = 3.00;
float voltagePH7 = 2.50;

RTC_DS1307 rtc;

// Variables para el filtro promedio del LM35
#define NUM_MUESTRAS_DHT 10

float lecturasDHT[NUM_MUESTRAS_DHT] = {0};

bool bufferInicializado = false;

// ------------------ CONFIG RED ------------------
const char* ssid = "Casa";
const char* password = "12345678";
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -18000;
const int daylightOffset_sec = 0;

// ------------------ FREERTOS ------------------
// Variables globales protegidas
float g_tempAmb = 0.0;
float g_tempInt = 0.0;
float g_ph = 0.0;
DateTime g_now;

// Handles para los Mutex
SemaphoreHandle_t xDataMutex;
SemaphoreHandle_t xI2CMutex;

// ------------------ FUNCIONES BASE ------------------

int readAverageADC(int pin, int samples) {
  long sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += analogRead(pin);
    vTaskDelay(pdMS_TO_TICKS(20));
  }
  return sum / samples;
}

float readPHSensor() {
  int raw = readAverageADC(PH_PIN, NUM_SAMPLES);
  float voltage = (raw / 4095.0) * 3.3;
  return 7.0 + ((voltagePH7 - voltage) * 5.70);
}

float readDHT22() {

  float nuevaTemp = dht.readTemperature();

  if (isnan(nuevaTemp)) {

    Serial.println("Error leyendo DHT22");

    float suma = 0;

    for (int i = 0; i < NUM_MUESTRAS_DHT; i++) {
      suma += lecturasDHT[i];
    }

    return suma / NUM_MUESTRAS_DHT;
  }

  if (!bufferInicializado) {

    for (int i = 0; i < NUM_MUESTRAS_DHT; i++) {
      lecturasDHT[i] = nuevaTemp;
    }

    bufferInicializado = true;

    return nuevaTemp;
  }

  for (int i = NUM_MUESTRAS_DHT - 1; i > 0; i--) {
    lecturasDHT[i] = lecturasDHT[i - 1];
  }

  lecturasDHT[0] = nuevaTemp;

  float suma = 0;

  for (int i = 0; i < NUM_MUESTRAS_DHT; i++) {
    suma += lecturasDHT[i];
  }

  return suma / NUM_MUESTRAS_DHT;
}

void syncTimeToRTCIfWifi() {
  Serial.println("Conectando WiFi...");
  WiFi.begin(ssid, password);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(200);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi conectado");
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      DateTime now(
        timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
        timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec
      );
      
      // Ajuste de RTC (Aún no hay tareas corriendo, es seguro sin mutex)
      rtc.adjust(now);
      Serial.println("RTC sincronizado con NTP");
    } else {
      Serial.println("Error NTP");
    }
  } else {
    Serial.println("No WiFi en Setup");
  }
}

// ------------------ TAREAS FREERTOS ------------------

void vTaskSensores(void *pvParameters) {
  for (;;) {
    // 1. Lectura de sensores análogos y 1-Wire (No bloquean el I2C)
    float tAmb = readDHT22();
    
    ds18b20.requestTemperatures();
    float tInt = ds18b20.getTempCByIndex(0);
    
    float phVal = readPHSensor();

    // 2. Lectura del RTC (Requiere Mutex de I2C)
    DateTime tNow;
    if (xSemaphoreTake(xI2CMutex, portMAX_DELAY) == pdTRUE) {
      tNow = rtc.now();
      xSemaphoreGive(xI2CMutex);
    }

    // 3. Guardar datos en variables globales (Requiere Mutex de Datos)
    if (xSemaphoreTake(xDataMutex, portMAX_DELAY) == pdTRUE) {
      g_tempAmb = tAmb;
      g_tempInt = tInt;
      g_ph = phVal;
      g_now = tNow;
      xSemaphoreGive(xDataMutex);
    }

    vTaskDelay(pdMS_TO_TICKS(2000)); // Esperar 2 segundos
  }
}

void vTaskDisplay(void *pvParameters) {
  for (;;) {
    // 1. Obtener copias locales de los datos
    float tA, tI, p;
    DateTime n;

    if (xSemaphoreTake(xDataMutex, portMAX_DELAY) == pdTRUE) {
      tA = g_tempAmb;
      tI = g_tempInt;
      p = g_ph;
      n = g_now;
      xSemaphoreGive(xDataMutex);
    }

    // 2. Dibujar en la pantalla OLED (Requiere Mutex de I2C)
    if (xSemaphoreTake(xI2CMutex, portMAX_DELAY) == pdTRUE) {
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);

      display.setCursor(0, 0);
      display.printf("Amb: %.1f C", tA);

      display.setCursor(0, 12);
      display.printf("Int: %.1f C", tI);

      display.setCursor(70, 0);
      display.printf("pH: %.1f", p);

      display.setCursor(0, 30);
      display.printf("Fec: %02d/%02d/%04d", n.day(), n.month(), n.year());

      display.setCursor(0, 42);
      display.printf("Hor: %02d:%02d:%02d", n.hour(), n.minute(), n.second());

      display.display();
      xSemaphoreGive(xI2CMutex);
    }

    // Imprimir por serial
    Serial.printf("[DISPLAY] Amb: %.2f | Int: %.2f | pH: %.2f | %02d:%02d:%02d\n", tA, tI, p, n.hour(), n.minute(), n.second());

    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

void vTaskHTTP(void *pvParameters) {
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(5000)); // Ejecutar cada 5 segundos

    if (WiFi.status() == WL_CONNECTED) {
      // 1. Obtener los datos más recientes
      float tA, tI, p;
      DateTime n;

      if (xSemaphoreTake(xDataMutex, portMAX_DELAY) == pdTRUE) {
        tA = g_tempAmb;
        tI = g_tempInt;
        p = g_ph;
        n = g_now;
        xSemaphoreGive(xDataMutex);
      }

      char fecha[11];
      snprintf(fecha, sizeof(fecha), "%02d-%02d-%04d", n.day(), n.month(), n.year());

      char hora[9];
      snprintf(hora, sizeof(hora), "%02d:%02d:%02d", n.hour(), n.minute(), n.second());

      // 2. Enviar JSON
      HTTPClient http;
      http.begin("http://192.168.194.31:3000/data"); //------------------------------------------------------------------------------
      http.addHeader("Content-Type", "application/json");

      String json = "{";
      json += "\"temperatura_ambiente\":" + String(tA, 2) + ",";
      json += "\"temperatura_interna\":" + String(tI, 2) + ",";
      json += "\"ph\":" + String(p, 2) + ",";
      json += "\"fecha\":\"" + String(fecha) + "\",";
      json += "\"hora\":\"" + String(hora) + "\"";
      json += "}";

      int httpResponseCode = http.POST(json);

      Serial.print("[HTTP] POST Código: ");
      Serial.println(httpResponseCode);

      if (httpResponseCode > 0) {
        Serial.println(http.getString());
      }

      http.end();
    } else {
      Serial.println("[HTTP] WiFi desconectado.");
    }
  }
}

// ------------------ SETUP ------------------

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n--- SISTEMA INICIADO ---");

  // Inicializar pines y buses
  Wire.begin(8, 9);
  ds18b20.begin();
  dht.begin();
  pinMode(PH_PIN, INPUT);

  // Inicializar OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3c)) {
    Serial.println(F("Fallo al iniciar SSD1306 OLED"));
    for (;;); // Bucle infinito si falla la pantalla
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Iniciando...");
  display.display();

  // Inicializar RTC
  if (!rtc.begin()) {
    Serial.println("RTC no detectado");
  } else if (!rtc.isrunning()) {
    Serial.println("RTC detenido, ajustando tiempo de compilacion...");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // Conexión WiFi y NTP
  display.println("Conectando WiFi...");
  display.display();
  syncTimeToRTCIfWifi();

  // Inicializar Semáforos (Mutex)
  xDataMutex = xSemaphoreCreateMutex();
  xI2CMutex = xSemaphoreCreateMutex();

  if (xDataMutex == NULL || xI2CMutex == NULL) {
    Serial.println("Error creando Mutex");
    for (;;);
  }

  // Crear Tareas FreeRTOS
  // xTaskCreate(Función, "Nombre", Stack, Parámetros, Prioridad, Handle);
  xTaskCreate(vTaskSensores, "TaskSensors", 4096, NULL, 2, NULL);
  xTaskCreate(vTaskDisplay,  "TaskDisplay", 4096, NULL, 1, NULL);
  
  // A la tarea de red le damos más memoria por el manejo del String y HTTPClient
  xTaskCreate(vTaskHTTP,     "TaskHTTP",    8192, NULL, 1, NULL); 

  Serial.println("FreeRTOS Scheduler Iniciado.");
}

// ------------------ LOOP ------------------
void loop() {
  vTaskDelete(NULL); 
}
