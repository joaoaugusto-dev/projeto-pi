const express = require('express');
const router = express.Router();
const { authenticateToken } = require('./auth/auth');
const Funcionario = require('./funcionarios/Funcionarios');
const esp32Routes = require('./esp32/esp32Controller');
const jwt = require('jsonwebtoken');

router.get('/', async (req, res) => {
    //const token = req.cookies.token;
    let user = null;
    let funcionario = null;

    /*if (token) {
        try {
            const decoded = jwt.verify(token, process.env.TokenJWT);
            user = decoded;
            funcionario = await Funcionario.findByPk(decoded.id);
        } catch (err) {
            console.error(err);
        }
    }*/

    res.render('index', { user, funcionario });
});

router.get('/cadastro', (req, res) => {
    res.render('signup', { user: null, funcionario: null });
});

router.get('/entrar', (req, res) => {
    res.render('login', { user: null, funcionario: null });
});

router.get('/dados', authenticateToken, async (req, res) => {
    try {
        const funcionario = await Funcionario.findByPk(req.user.id);
        res.render('dados', { user: req.user, funcionario });
    } catch (err) {
        console.error(err);
        res.redirect('/');
    }
});

// Rotas do ESP32
router.use('/esp32', esp32Routes);

module.exports = router;