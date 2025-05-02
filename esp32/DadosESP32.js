const sequelize = require('sequelize');
const { connESP32 } = require('../database/conexao');

const DadosESP32 = connESP32.define('dados_sensor', {
    id: {
        type: sequelize.INTEGER,
        autoIncrement: true,
        primaryKey: true
    },
    temperatura: {
        type: sequelize.FLOAT,
        allowNull: false
    },
    umidade: {
        type: sequelize.FLOAT,
        allowNull: false
    },
    luminosidade: {
        type: sequelize.FLOAT,
        allowNull: false
    },
    timestamp: {
        type: sequelize.DATE,
        defaultValue: sequelize.NOW
    }
});

module.exports = DadosESP32;