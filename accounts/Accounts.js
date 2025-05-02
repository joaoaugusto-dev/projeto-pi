const sequelize = require('sequelize');
const conn = require('../database/conexao');

const Accounts = conn.define('accounts', {
    id: {
        type: sequelize.INTEGER,
        autoIncrement: true,
        primaryKey: true
    },
    nome: {
        type: sequelize.STRING,
        allowNull: false
    },
    sobrenome: {
        type: sequelize.STRING,
        allowNull: false
    },
    nasc: {
        type: sequelize.STRING,
        allowNull: false
    },
    cpf: {
        type: sequelize.STRING,
        allowNull: false
    },
    telefone: {
        type: sequelize.STRING,
        allowNull: false
    },
    email: {
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
    }
});

module.exports = Accounts;