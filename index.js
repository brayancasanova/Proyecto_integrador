const express = require('express');
const app = express();
const mongoose = require('mongoose');
const cors = require('cors');
app.use(cors());

// 👇 IMPORTANTE (te falta esto)
const Lectura = require('./models/Lectura');

mongoose.connect('mongodb://127.0.0.1:27017/sensores')
    .then(() => console.log("🟢 MongoDB conectado"))
    .catch(err => console.log("❌ Error MongoDB:", err));

app.use(express.json());

// 🔥 Ruta corregida
app.post('/data', async (req, res) => {
    try {
        const {
            temperatura_ambiente,
            temperatura_interna,
            ph,
            fecha,
            hora
        } = req.body;

        console.log("📡 Datos recibidos:");
        console.log("Temp ambiente:", temperatura_ambiente);
        console.log("Temp interna:", temperatura_interna);
        console.log("pH:", ph);
        console.log("Fecha:", fecha);
        console.log("Hora:", hora);

        // 👇 AQUÍ SE GUARDAN LOS DATOS
        const nuevaLectura = new Lectura({
            temperatura_ambiente,
            temperatura_interna,
            ph,
            fecha,
            hora
        });

        await nuevaLectura.save();

        console.log("💾 Datos guardados en MongoDB");

        res.json({
            mensaje: "Datos guardados correctamente"
        });

    } catch (error) {
        console.error("❌ Error:", error);
        res.status(500).json({ error: "Error al guardar datos" });
    }
});
app.get('/data', async (req, res) => {
    try {
        const datos = await Lectura.find().sort({ _id: -1 });

        res.json(datos);

    } catch (error) {
        console.error("❌ Error al obtener datos:", error);
        res.status(500).json({ error: "Error al obtener datos" });
    }
});

// Ruta de prueba
app.get('/', (req, res) => {
    res.send("Servidor funcionando 🚀");
});

app.listen(3000, '0.0.0.0' ,() => {
    console.log("Servidor corriendo en http://localhost:3000");
});