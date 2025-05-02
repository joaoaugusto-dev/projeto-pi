const sequelize = require('sequelize');
const { connESP32 } = require('../database/conexao');

const NFCTag = connESP32.define('nfc_tags', {
    id: {
        type: sequelize.INTEGER,
        autoIncrement: true,
        primaryKey: true
    },
    uid: {
        type: sequelize.STRING(20),
        allowNull: false,
        unique: true
    },
    nome: {
        type: sequelize.STRING(100),
        allowNull: false
    },
    temp_preferida: {
        type: sequelize.FLOAT,
        allowNull: false
    },
    lumi_preferida: {
        type: sequelize.INTEGER,
        allowNull: false
    }
}, {
    timestamps: false,
    freezeTableName: true
});

module.exports = NFCTag;