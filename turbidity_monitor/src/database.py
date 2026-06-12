from motor.motor_asyncio import AsyncIOMotorClient
import datetime

MONGO_DETAILS = "mongodb://localhost:27017"
client = AsyncIOMotorClient(MONGO_DETAILS)
database = client.turbidity_db
turbidity_collection = database.get_collection("logs")

async def log_turbidity_level(ntu_value: int, confidence: float):
    """Guarda el registro y avisa por consola."""
    try:
        log_data = {
            "ntu": ntu_value,
            "confidence": round(confidence, 3),
            "timestamp": datetime.datetime.now(datetime.timezone.utc)
        }
        await turbidity_collection.insert_one(log_data)
        
        # 🟢 Mensaje de éxito en la terminal
        print(f"✅ DATO GUARDADO EN MONGODB -> {ntu_value} NTU | Confianza: {confidence:.2f}")
        return log_data
        
    except Exception as e:
        # 🔴 Si MongoDB no está instalado o hay error, lo veremos aquí
        print(f"❌ ERROR CRÍTICO DE BASE DE DATOS: {e}")
        return None