const express = require('express');
const router = express.Router();
const { authenticateToken } = require('./auth/auth');
const Funcionario = require('./funcionarios/Funcionarios');
const esp32Routes = require('./esp32/esp32Controller');
const jwt = require('jsonwebtoken');
const cookieParser = require('cookie-parser');

router.use(cookieParser());

router.get('/', async (req, res) => {
    const token = req.cookies.token;
    let user = null;
    let funcionario = null;

    if (token) {
        try {
            const decoded = jwt.verify(token, process.env.JWT_SECRET);
            user = decoded;
            funcionario = await Funcionario.findByPk(decoded.id);
        } catch (err) {
            console.error(err);
        }
    }

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
        console.log('ID do usuário:', req.user.id);
        const funcionario = await Funcionario.findByPk(req.user.id);
        
        if (!funcionario) {
            console.log('Funcionário não encontrado');
            return res.redirect('/');
        }
        
        console.log('Funcionário encontrado:', funcionario.nome);
        res.render('dados', { 
            user: req.user, 
            funcionario 
        });
    } catch (err) {
        console.error('Erro ao carregar dados:', err);
        res.redirect('/');
    }
});

router.get('/preferencias', authenticateToken, async (req, res) => {
    try {
        const funcionario = await Funcionario.findByPk(req.user.id);
        res.render('preferencias', { user: req.user, funcionario });
    } catch (err) {
        console.error(err);
        res.redirect('/');
    }
});

router.post('/preferencias/atualizar', authenticateToken, async (req, res) => {
    try {
        const { temp_preferida, lumi_preferida, tag_nfc } = req.body;

        // Validação dos dados
        if (temp_preferida === undefined || lumi_preferida === undefined) {
            return res.status(400).json({ 
                success: false, 
                message: 'Temperatura e luminosidade são obrigatórios' 
            });
        }

        // Conversão e validação dos valores
        const temperatura = parseFloat(temp_preferida);
        const luminosidade = parseInt(lumi_preferida);

        if (isNaN(temperatura) || isNaN(luminosidade)) {
            return res.status(400).json({ 
                success: false, 
                message: 'Valores de temperatura ou luminosidade inválidos' 
            });
        }

        // Validação dos ranges
        if (temperatura < 16 || temperatura > 32) {
            return res.status(400).json({ 
                success: false, 
                message: 'Temperatura deve estar entre 16°C e 32°C' 
            });
        }

        if (luminosidade < 0 || luminosidade > 1000) {
            return res.status(400).json({ 
                success: false, 
                message: 'Luminosidade deve estar entre 0 e 1000 lux' 
            });
        }

        // Atualização no banco
        const funcionario = await Funcionario.findByPk(req.user.id);
        if (!funcionario) {
            return res.status(404).json({ 
                success: false, 
                message: 'Funcionário não encontrado' 
            });
        }

        await funcionario.update({
            temp_preferida: temperatura,
            lumi_preferida: luminosidade,
            tag_nfc: tag_nfc || funcionario.tag_nfc // Mantém o valor atual se não fornecido
        });

        res.json({ 
            success: true, 
            message: 'Preferências atualizadas com sucesso' 
        });
    } catch (err) {
        console.error('Erro ao atualizar preferências:', err);
        res.status(500).json({ 
            success: false, 
            message: 'Erro interno ao atualizar preferências' 
        });
    }
});

// Rotas do ESP32
router.use('/esp32', esp32Routes);

module.exports = router;