const sequelize = require('sequelize');

const conn = new sequelize('projetoiot', 'root', '', {
    host: 'localhost',
    dialect: 'mysql',
    //port: '3307'
});

module.exports = conn;

/*
const { Sequelize } = require('sequelize');
const fs = require('fs');
const path = require('path');
require('dotenv').config();

const caCertPath = path.join(__dirname, 'ca.pem');
const caCert = fs.readFileSync(caCertPath);

const conn = new Sequelize(process.env.DB_NAME, process.env.DB_USER, process.env.DB_PASSWORD, {
    host: process.env.DB_HOST,
    port: process.env.DB_PORT,
    dialect: 'mysql',
    dialectOptions: {
        ssl: {
            rejectUnauthorized: true,
            ca: caCert.toString()
        }
    }
});

module.exports = conn;*/