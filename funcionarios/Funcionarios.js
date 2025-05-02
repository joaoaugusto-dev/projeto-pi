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
        allowNull: true
    },
    lumi_preferida: {
        type: sequelize.INTEGER,
        allowNull: true
    },
    tag_nfc: {
        type: sequelize.STRING,
        allowNull: true
    },
});

module.exports = Funcionarios;