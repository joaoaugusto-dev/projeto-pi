require('dotenv').config();
const express = require('express');
const bodyParser = require('body-parser');
const routes = require('./routes');
const { connUI, connESP32 } = require('./database/conexao');
const createTables = require('./tableCreation');

const app = express();

app.use(bodyParser.urlencoded({ extended: false }));
app.use(bodyParser.json());
app.use(express.static('public'));
app.set('view engine', 'ejs');

app.use('/', routes);

Promise.all([connUI.authenticate(), connESP32.authenticate()])
    .then(async () => {
        console.log('Bancos conectados!');
        await createTables();
    })
    .catch((erro) => {
        console.log(erro);
    });

app.listen(3000, () => {
    console.log('Servidor rodando na porta 3000');
});