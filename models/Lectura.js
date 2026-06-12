const mongoose = require('mongoose');

const LecturaSchema = new mongoose.Schema({
    temperatura_ambiente: Number,
    temperatura_interna: Number,
    ph: Number,
    fecha: String,
    hora: String
});

module.exports = mongoose.model('Lectura', LecturaSchema);

