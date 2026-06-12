import os
from dotenv import load_dotenv
from src.trainer import TurbidityModelTrainer

def main():
    # 1. Cargar variables de entorno desde el archivo .env
    load_dotenv()
    
    API_KEY = os.getenv("ROBOFLOW_API_KEY")
    WORKSPACE = os.getenv("ROBOFLOW_WORKSPACE")
    PROJECT = os.getenv("ROBOFLOW_PROJECT")
    VERSION = int(os.getenv("ROBOFLOW_VERSION", 1))

    if not all([API_KEY, WORKSPACE, PROJECT]):
        print("[ERROR] Faltan variables de entorno. Revisa tu archivo .env.")
        return

    # 2. Instanciar la clase de entrenamiento
    trainer = TurbidityModelTrainer(
        api_key=API_KEY,
        workspace=WORKSPACE,
        project_name=PROJECT,
        version=VERSION
    )

    # 3. Flujo de ejecución secuencial
    try:
        trainer.download_dataset()
        
        # Parámetros ajustables según la capacidad de tu hardware
        # Si te quedas sin VRAM, reduce el batch_size a 8 o 4.
        trainer.train(epochs=100, batch_size=16, imgsz=640)
        
    except Exception as e:
        print(f"[ERROR CRÍTICO] Ocurrió un fallo durante la ejecución: {e}")

if __name__ == "__main__":
    main()