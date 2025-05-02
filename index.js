require('dotenv').config();
const express = require('express');
const app = express();
const cookieParser = require('cookie-parser');
const session = require('express-session');
const path = require('path');
const routes = require('./routes');
const { router: authRouter } = require('./auth/auth');
const connection = require('./database/conexao');
const Funcionarios = require('./funcionarios/Funcionarios');

// Configurações
app.set('view engine', 'ejs');
app.use(express.static('public'));
app.use('/uploads', express.static('uploads'));
app.use(express.json());
app.use(express.urlencoded({ extended: false }));
app.use(cookieParser());

// Configuração da sessão
app.use(session({
    secret: process.env.SESSION_SECRET || 'default_secret',
    resave: false,
    saveUninitialized: false,
    cookie: { secure: false }
}));

// Middleware global para verificar autenticação
app.use((req, res, next) => {
    const token = req.cookies.token;
    res.locals.user = token ? { authenticated: true } : null;
    next();
});

// Rotas
app.use('/', routes);
app.use('/auth', authRouter);

// Sincronização com o banco de dados
connection
    .sync()
    .then(() => {
        console.log('Banco de dados conectado com sucesso!');
    })
    .catch((error) => {
        console.log(error);
    });

// Inicialização do servidor
const PORT = process.env.PORT || 3000;
app.listen(PORT, () => {
    console.log(`Servidor rodando na porta ${PORT}`);
});