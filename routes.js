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

module.exports = router;