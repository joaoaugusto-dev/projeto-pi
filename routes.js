const express = require('express');
const router = express.Router();
const { authenticateToken } = require('./auth/auth');
const Account = require('./accounts/Accounts');
const jwt = require('jsonwebtoken');

router.use((req, res, next) => {
    const token = req.cookies ? req.cookies.token : null;
    if (token) {
        try {
            const decoded = jwt.verify(token, process.env.JWT_SECRET);
            req.user = decoded;
        } catch (err) {
            res.clearCookie('token');
        }
    }
    next();
});

router.get('/', async (req, res) => {
    const token = req.cookies.token;
    let user = null;
    let account = null;

    if (token) {
        try {
            const decoded = jwt.verify(token, process.env.JWT_SECRET);
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

module.exports = router;