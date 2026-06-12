import os
import yaml
from roboflow import Roboflow
from ultralytics import YOLO

class TurbidityModelTrainer:
    """
    Clase para gestionar la descarga del dataset y el entrenamiento del modelo YOLOv8.
    """
    # IMPORTANTE: Nota los dobles guiones bajos en __init__
    def __init__(self, api_key: str, workspace: str, project_name: str, version: int):
        self.api_key = api_key
        self.workspace = workspace
        self.project_name = project_name
        self.version = version
        self.dataset_path = None

    def download_dataset(self) -> str:
        print("[INFO] Conectando a Roboflow y descargando dataset...")
        rf = Roboflow(api_key=self.api_key)
        project = rf.workspace(self.workspace).project(self.project_name)
        dataset = project.version(self.version).download("yolov8")
        self.dataset_path = dataset.location
        print(f"[INFO] Dataset descargado exitosamente en: {self.dataset_path}")
        return self.dataset_path

    def train(self, epochs: int = 50, batch_size: int = 16, imgsz: int = 640):
        if not self.dataset_path:
            raise ValueError("El dataset no ha sido descargado.")

        yaml_path = os.path.join(self.dataset_path, "data.yaml")
        
        # --- BLOQUE DE CORRECCIÓN AUTOMATIZADA DE RUTAS ---
        print("[INFO] Corrigiendo rutas para compatibilidad absoluta...")
        with open(yaml_path, 'r') as file:
            yaml_data = yaml.safe_load(file)

        # Forzamos rutas absolutas exactas de este computador
        yaml_data['train'] = os.path.join(self.dataset_path, 'train', 'images').replace('\\', '/')
        
        # Lógica de contingencia si no hay imágenes de validación
        val_path = os.path.join(self.dataset_path, 'valid', 'images')
        if os.path.exists(val_path) and len(os.listdir(val_path)) > 0:
            yaml_data['val'] = val_path.replace('\\', '/')
        else:
            print("[ADVERTENCIA] Carpeta de validación vacía. Usando 'train' temporalmente.")
            yaml_data['val'] = yaml_data['train']

        if 'test' in yaml_data:
            yaml_data['test'] = os.path.join(self.dataset_path, 'test', 'images').replace('\\', '/')

        # Guardamos el archivo sobrescrito
        with open(yaml_path, 'w') as file:
            yaml.dump(yaml_data, file, default_flow_style=False)
        # ---------------------------------------------------

        print("[INFO] Iniciando el entrenamiento del modelo...")
        model = YOLO('yolov8n.pt') 

        # Ejecución
        results = model.train(
            data=yaml_path,
            epochs=epochs,
            imgsz=imgsz,
            batch=batch_size,
            project="Turbidity_Analysis",
            name="train_v1",
            device='cpu' # Se mantiene en CPU para este PC i5
        )
        print("[INFO] Entrenamiento finalizado.")
        return results