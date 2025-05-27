const sequelize = require('sequelize');
const conn = require('../database/conexao');

const Funcionarios = conn.define('funcionarios', {
    id: {
        type: sequelize.INTEGER,
        autoIncrement: true,
        primaryKey: true
    },
    matricula: {
        type: sequelize.INTEGER,
        allowNull: false,
        unique: true
    },
    nome: {
        type: sequelize.STRING,
        allowNull: false
    },
    sobrenome: {
        type: sequelize.STRING,
        allowNull: false
    },
    senha: {
        type: sequelize.STRING,
        allowNull: false
    },
    foto: {
        type: sequelize.STRING,
        allowNull: true
    },
    temp_preferida: {
        type: sequelize.FLOAT,
        allowNull: true,
        defaultValue: 24.0
    },
    lumi_preferida: {
        type: sequelize.INTEGER,
        allowNull: true,
        defaultValue: 75
    },
    tag_nfc: {
        type: sequelize.STRING,
        allowNull: true,
        unique: true
    }
});

module.exports = Funcionarios;