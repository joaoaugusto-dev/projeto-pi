const express = require('express');
const router = express.Router();
const Funcionario = require('../funcionarios/Funcionarios');

// Rota para consultar usuário por matrícula
router.get('/consulta', async (req, res) => {
    const { matricula } = req.query;
    
    if (!matricula) {
        return res.status(400).json({ error: 'Matrícula é necessária' });
    }

    try {
        const usuario = await Funcionario.findOne({ where: { matricula } });
        if (!usuario) {
            return res.status(404).json({ error: 'Usuário não encontrado' });
        }
        res.json({
            matricula: usuario.matricula,
            nome: usuario.nome,
            temp_preferida: usuario.temp_preferida,
            lumi_preferida: usuario.lumi_preferida
        });
    } catch (error) {
        res.status(500).json({ error: error.message });
    }
});

// Rota para atualizar preferências
router.put('/preferencias/:matricula', async (req, res) => {
    const { matricula } = req.params;
    const { temp_preferida, lumi_preferida } = req.body;

    try {
        const usuario = await Funcionario.findOne({ where: { matricula } });
        if (!usuario) {
            return res.status(404).json({ error: 'Usuário não encontrado' });
        }

        await usuario.update({
            temp_preferida,
            lumi_preferida
        });
        res.json({
            matricula: usuario.matricula,
            nome: usuario.nome,
            temp_preferida: usuario.temp_preferida,
            lumi_preferida: usuario.lumi_preferida
        });
    } catch (error) {
        res.status(500).json({ error: error.message });
    }
});

module.exports = router;