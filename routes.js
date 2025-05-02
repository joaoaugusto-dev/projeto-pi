const express = require('express');
const router = express.Router();
const { authenticateToken } = require('./auth/auth');
const Account = require('./accounts/Accounts');
const esp32Routes = require('./esp32/esp32Controller');
const jwt = require('jsonwebtoken');

router.get('/', async (req, res) => {
    const token = req.cookies.token;
    let user = null;
    let account = null;

    if (token) {
        try {
            const decoded = jwt.verify(token, process.env.TokenJWT);
            user = decoded;
            account = await Account.findByPk(decoded.id);
        } catch (err) {
            console.error(err);
        }
    }

    res.render('index', { user, account });
});

router.get('/cadastro', (req, res) => {
    res.render('signup', { user: null, account: null });
});

router.get('/entrar', (req, res) => {
    res.render('login', { user: null, account: null });
});

router.get('/dados', authenticateToken, async (req, res) => {
    try {
        const account = await Account.findByPk(req.user.id);
        res.render('dados', { user: req.user, account });
    } catch (err) {
        console.error(err);
        res.redirect('/');
    }
});

// Rotas do ESP32
router.use('/esp32', esp32Routes);

module.exports = router;