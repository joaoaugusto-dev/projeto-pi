require('dotenv').config();
const express = require('express');
const bodyParser = require('body-parser');
const sequelize = require('sequelize');

const app = express();

app.use(bodyParser.urlencoded({ extended: false }));
app.use(bodyParser.json());
app.use(express.static('public'));
app.set('view engine', 'ejs');

app.listen(3000, () => {
    console.log('Servidor rodando na porta 3000');
});