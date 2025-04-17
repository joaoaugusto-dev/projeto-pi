const { Sequelize } = require('sequelize');
const fs = require('fs');
const path = require('path');

const caCertPath = path.join(__dirname, 'ca.pem');

const caCert = fs.readFileSync(caCertPath);

const conn = new Sequelize('defaultdb', 'avnadmin', 'AVNS_BVnYIbomO63o4vMExlb', {
    host: 'pi-iot-pi-iot.g.aivencloud.com',
    port: 16277,
    dialect: 'mysql',
    dialectOptions: {
        ssl: {
            rejectUnauthorized: true,
            ca: caCert.toString()
        }
    }
});

module.exports = conn;