const express = require('express');
const app = express();
require('dotenv').config();
const cookieParser = require('cookie-parser');
const bodyParser = require('body-parser');
const createTables = require('./tableCreation');

const conn = require('./database/conexao');
const routes = require('./routes');
const { router: authRouter, authenticateToken } = require('./auth/auth');
const Funcionario = require('./funcionarios/Funcionarios');
const funcionariosController = require('./funcionarios/funcionariosController');
const esp32Controller = require('./esp32/esp32Controller');
const logsController = require('./logs/logsController'); // Importando logsController

app.use(bodyParser.urlencoded({ extended: false }));
app.use(bodyParser.json({ limit: '5mb' })); // Limite otimizado
app.use(cookieParser());
app.use(express.static('public'));
app.set('view engine', 'ejs');

// Middleware otimizado para cache
app.use((req, res, next) => {
  res.set({
    'Cache-Control': 'no-store',
    'X-Powered-By': 'ESP32-IoT'
  });
  next();
});

// Rotas públicas (antes da autenticação)
app.use('/', authRouter);
app.use('/esp32', esp32Controller);

app.get('/cadastro', (req, res) => res.render('signup', { user: req.user, funcionario: null }));
app.get('/entrar', (req, res) => res.render('login', { user: req.user, funcionario: null }));
app.get('/inicio', (req, res) => res.render('inicio', { user: req.user, funcionario: null }));

// Middleware global para proteger tudo depois daqui
app.use(authenticateToken);

async function checkUserOwnership(req, res, next) {
  try {
    if (!req.user || typeof req.user.id !== 'number') {
      return res.redirect('/inicio');
    }

    const funcionario = await Funcionario.findByPk(req.user.id);

    if (!funcionario) {
      return res.redirect('/inicio');
    }

    req.funcionario = funcionario;
    res.locals.funcionario = funcionario;

    next();
  } catch (error) {
    console.error('Erro na verificação de propriedade do usuário:', error);
    return res.redirect('/inicio');
  }
}

// Conexão com o banco
conn.authenticate().then(async () => {
  console.log('Banco conectado!');
  await createTables();
}).catch((erro) => {
  console.log(erro);
});

// Rotas protegidas
app.get('/', checkUserOwnership, async (req, res) => {
  const funcionario = await Funcionario.findByPk(req.user.id);
  res.render('index', { user: req.user, funcionario });
});

app.get('/dados', checkUserOwnership, async (req, res) => {
  const funcionario = await Funcionario.findByPk(req.user.id);
  res.render('dados', { user: req.user, funcionario });
});

app.get('/preferencias', checkUserOwnership, async (req, res) => {
  const funcionario = await Funcionario.findByPk(req.user.id);
  res.render('preferencias', { user: req.user, funcionario });
});

// Controladores adicionais
app.use('/', funcionariosController);
app.use('/', routes);
app.use('/', logsController); // Adicionando o controlador de logs

// Start do servidor
app.listen(process.env.PORT, "0.0.0.0", () => {
  console.log(`Servidor rodando na porta ${process.env.PORT}`);
});