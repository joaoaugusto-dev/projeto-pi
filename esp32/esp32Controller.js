const express = require('express');
const router = express.Router();
const DadosESP32 = require('./DadosESP32');
const NFCTag = require('./NFCTag');

// Rota para receber dados dos sensores
router.post('/dados', async (req, res) => {
    const { temperatura, umidade, luminosidade } = req.body;
    
    try {
        const dados = await DadosESP32.create({
            temperatura,
            umidade,
            luminosidade
        });
        res.json(dados);
    } catch (error) {
        res.status(500).json({ error: error.message });
    }
});

// Rota para obter últimas leituras
router.get('/ultimas-leituras', async (req, res) => {
    try {
        const dados = await DadosESP32.findAll({
            limit: 10,
            order: [['timestamp', 'DESC']]
        });
        res.json(dados);
    } catch (error) {
        res.status(500).json({ error: error.message });
    }
});

// Rota para consultar tag NFC
router.get('/nfc/consulta', async (req, res) => {
    const { uid } = req.query;
    
    if (!uid) {
        return res.status(400).json({ error: 'UID é necessário' });
    }

    try {
        const tag = await NFCTag.findOne({ where: { uid } });
        if (!tag) {
            return res.status(404).json({ error: 'Tag não encontrada' });
        }
        res.json(tag);
    } catch (error) {
        res.status(500).json({ error: error.message });
    }
});

// Rota para cadastrar nova tag
router.post('/nfc/cadastro', async (req, res) => {
    const { uid, nome, temp_preferida, lumi_preferida } = req.body;

    if (!uid || !nome || !temp_preferida || !lumi_preferida) {
        return res.status(400).json({ error: 'Todos os campos são obrigatórios' });
    }

    try {
        const tag = await NFCTag.create({
            uid,
            nome,
            temp_preferida,
            lumi_preferida
        });
        res.status(201).json(tag);
    } catch (error) {
        if (error.name === 'SequelizeUniqueConstraintError') {
            return res.status(400).json({ error: 'UID já cadastrado' });
        }
        res.status(500).json({ error: error.message });
    }
});

// Rota para atualizar tag
router.put('/nfc/atualizar/:uid', async (req, res) => {
    const { uid } = req.params;
    const { nome, temp_preferida, lumi_preferida } = req.body;

    try {
        const tag = await NFCTag.findOne({ where: { uid } });
        if (!tag) {
            return res.status(404).json({ error: 'Tag não encontrada' });
        }

        await tag.update({
            nome,
            temp_preferida,
            lumi_preferida
        });
        res.json(tag);
    } catch (error) {
        res.status(500).json({ error: error.message });
    }
});

// Rota para deletar tag
router.delete('/nfc/deletar/:uid', async (req, res) => {
    const { uid } = req.params;

    try {
        const tag = await NFCTag.findOne({ where: { uid } });
        if (!tag) {
            return res.status(404).json({ error: 'Tag não encontrada' });
        }

        await tag.destroy();
        res.json({ message: 'Tag deletada com sucesso' });
    } catch (error) {
        res.status(500).json({ error: error.message });
    }
});

module.exports = router;