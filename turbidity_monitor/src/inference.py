        # Inferencia de YOLOv8
import cv2
from ultralytics import YOLO

# Cargar el modelo YOLOv8
model = YOLO("../models/best.pt")

def get_ntu_from_class(class_name):
    """
    Extrae el valor numérico de la clase de forma modular.
    Soporta errores tipográficos como 'valo-X' además de 'valor-X'.
    """
    # Verificamos si el nombre empieza con cualquiera de los dos prefijos
    if class_name.startswith("valor-") or class_name.startswith("valo-"):
        # Dividimos el texto usando el guion como separador
        partes = class_name.split("-")
        
        if len(partes) == 2:
            try:
                # Tomamos la segunda parte (el número) y calculamos los NTU
                val_num = int(partes[1])
                return 1 + (val_num * 15)
            except ValueError:
                return None
    return None

def generate_frames():
    """Generador que lee el stream de la ESP-CAM y aplica YOLOv8"""
    stream_url = "http://192.168.52.156/stream"
    cap = cv2.VideoCapture(stream_url)

    while True:
        success, frame = cap.read()
        if not success:
            break
        
        # --- CAMBIO CRÍTICO: Bajamos el umbral a 0.45 ---
        # Esto permite detectar las clases con mAP más bajo (como valor-2 y valor-1)
        # Forzamos EXACTAMENTE la resolución de tu dataset de Roboflow
        results = model.predict(
            frame, 
            conf=0.1, 
            imgsz=512,  # Ajustado al preprocesamiento de tu imagen
            iou=0.5, 
            verbose=False
        )
        
        detected_ntu = None
        best_conf = 0.0

        for r in results:
            boxes = r.boxes
            for box in boxes:
                cls_id = int(box.cls[0])
                conf = float(box.conf[0])
                class_name = model.names[cls_id]
                
                # Descartamos el "Recipiente" para el cálculo y buscamos el nivel de turbidez
                # Guardamos solo la predicción con mayor nivel de confianza en ese frame
                if class_name != "Recipiente" and conf > best_conf:
                    ntu_calculado = get_ntu_from_class(class_name)
                    
                    if ntu_calculado is not None:
                        best_conf = conf
                        detected_ntu = ntu_calculado
                        
            # Dibujar las bounding boxes (cuadros) en el frame original
            frame = r.plot()

        # Codificar el frame a JPEG
        ret, buffer = cv2.imencode('.jpg', frame)
        frame_bytes = buffer.tobytes()

        # Emitir los datos
        yield (b'--frame\r\n'
               b'Content-Type: image/jpeg\r\n\r\n' + frame_bytes + b'\r\n', detected_ntu, best_conf)