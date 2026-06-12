from fastapi import FastAPI, Request
from fastapi.responses import StreamingResponse, HTMLResponse
from fastapi.staticfiles import StaticFiles
from pathlib import Path
from src.inference import generate_frames
from src.database import log_turbidity_level
import asyncio

app = FastAPI()
# --- CONSTRUCCIÓN DINÁMICA DE RUTAS (Clean Code) ---
# Path(__file__).resolve() obtiene la ruta absoluta de main.py
# .parent.parent sube dos niveles (de main.py a src, y de src a la raíz del proyecto)
BASE_DIR = Path(__file__).resolve().parent.parent
STATIC_DIR = BASE_DIR / "static"

# Servir archivos estáticos (nuestra página web)
app.mount("/static", StaticFiles(directory=str(STATIC_DIR)), name="static")
# Variable global para guardar el estado actual para la API
current_turbidity = {"ntu": None, "confidence": 0.0}

async def video_streamer():
    """Consume el generador de video y registra en BD de forma asíncrona"""
    global current_turbidity
    last_log_time = asyncio.get_event_loop().time()
    
    for frame_bytes, ntu, conf in generate_frames():
        if ntu is not None:
            current_turbidity["ntu"] = ntu
            current_turbidity["confidence"] = conf
            
            # Cambiamos a 2 segundos para no perder las detecciones intermitentes
            current_time = asyncio.get_event_loop().time()
            if current_time - last_log_time > 2:
                # Disparamos el guardado de forma asíncrona sin esperar
                asyncio.create_task(log_turbidity_level(ntu, conf))
                last_log_time = current_time

        yield frame_bytes

@app.get("/", response_class=HTMLResponse)
async def index():
    # Usar también la ruta calculada para abrir el HTML
    html_path = STATIC_DIR / "index.html"
    with open(html_path, "r", encoding="utf-8") as f:
        return f.read()

@app.get("/video_feed")
async def video_feed():
    """Endpoint que emite el video procesado (Motion JPEG)"""
    return StreamingResponse(video_streamer(), media_type="multipart/x-mixed-replace; boundary=frame")

@app.get("/api/turbidity")
async def get_turbidity():
    """Endpoint para que el frontend consulte el nivel actual"""
    return current_turbidity