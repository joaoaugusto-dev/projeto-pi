const { Sequelize } = require('sequelize');
const fs = require('fs');
const path = require('path');
require('dotenv').config();

const caCertPath = path.join(__dirname, 'ca.pem');
const caCert = fs.readFileSync(caCertPath);

const defaultConfig = {
    host: process.env.DB_HOST,
    port: process.env.DB_PORT,
    dialect: 'mysql',
    dialectOptions: {
        ssl: {
            rejectUnauthorized: true,
            ca: caCert.toString()
        }
    }
};

const connUI = new Sequelize('dados_ui', process.env.DB_USER, process.env.DB_PASSWORD, defaultConfig);
const connESP32 = new Sequelize('dados_esp32', process.env.DB_USER, process.env.DB_PASSWORD, defaultConfig);

module.exports = {
    connUI,
    connESP32
};