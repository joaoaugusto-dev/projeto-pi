require('dotenv').config();
const express = require('express');
const bodyParser = require('body-parser');
const routes = require('./routes');
const conn = require('./database/conexao');

const app = express();

app.use(bodyParser.urlencoded({ extended: false }));
app.use(bodyParser.json());
app.use(express.static('public'));
app.set('view engine', 'ejs');

app.use('/', routes);

conn.authenticate().then(() => {
    console.log('Banco conectado!');
}).catch((erro) => {
    console.log(erro);
});

app.listen(3000, () => {
    console.log('Servidor rodando na porta 3000');
});