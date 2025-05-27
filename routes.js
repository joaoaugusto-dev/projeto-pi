const express = require('express');
const router = express.Router();
const Funcionario = require('./funcionarios/Funcionarios');
const multer = require('multer');
const path = require('path');
const fs = require('fs');

const storage = multer.diskStorage({
    destination: function(req, file, cb){
        cb(null, './public/uploads/');
    },
    filename: function (req, file, cb) {
        const nomeArquivo = `perfil-${req.user.id}${path.extname(file.originalname)}`;
        cb(null, nomeArquivo);
    }
});

const upload = multer({ storage });

router.post('/preferencias/atualizar', async (req, res) => {
    try {
        const { temp_preferida, lumi_preferida, tag_nfc } = req.body;

        if (temp_preferida === undefined || lumi_preferida === undefined) {
            return res.status(400).json({ 
                success: false, 
                message: 'Temperatura e luminosidade são obrigatórios' 
            });
        }

        const temperatura = parseFloat(temp_preferida);
        const luminosidade = parseInt(lumi_preferida);

        if (isNaN(temperatura) || isNaN(luminosidade)) {
            return res.status(400).json({ 
                success: false, 
                message: 'Valores de temperatura ou luminosidade inválidos' 
            });
        }

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
            tag_nfc: tag_nfc || funcionario.tag_nfc
        });

        // Forçar atualização no ESP32 após salvar preferências
        try {
            fetch('http://localhost:3000/esp32/ambiente', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ forceUpdate: true })
            });
        } catch (e) {
            console.error('Erro ao forçar atualização no ESP32:', e);
        }

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

router.post('/funcionario/alterar-foto', upload.single('novaFoto'), async (req, res) => {
    const userId = req.user.id;
    const file = req.file ? req.file.filename : null;

    try {
        const user = await Funcionario.findByPk(userId);
        const oldPhoto = user.foto;

        if (file && file !== oldPhoto) {
            const filePath = path.join(__dirname, 'public/uploads', file);
            const oldPhotoPath = path.join(__dirname, 'public/uploads', oldPhoto || '');

            if (oldPhoto && fs.existsSync(oldPhotoPath)) {
                fs.unlinkSync(oldPhotoPath);
            }

            await user.update({ foto: file });
        }

        res.redirect('/dados');
    } catch (error) {
        console.error('Erro ao atualizar foto:', error);
        res.status(500).json({ message: 'Erro ao atualizar foto' });
    }
});

// Rota para página de histórico
router.get('/historico', async (req, res) => {
    try {
        const logs = await require('./logs/Logs').findAll({
            order: [['createdAt', 'DESC']],
            limit: 100,
        });
        const Funcionario = require('./funcionarios/Funcionarios');
        let funcionario = null;
        if (req.user && req.user.id) {
            funcionario = await Funcionario.findByPk(req.user.id);
        }
        res.render('historico', { logs, user: req.user, funcionario });
    } catch (err) {
        console.error('Erro ao buscar logs:', err);
        res.status(500).send('Erro ao carregar histórico');
    }
});

module.exports = router;