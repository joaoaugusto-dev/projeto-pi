const express = require('express');
const router = express.Router();
const Funcionario = require('../funcionarios/Funcionarios');

let ultimaLeitura = {
    temperatura: null,
    humidade: null,
    timestamp: null
};

router.get('/matricula/:matricula', async (req, res) => {
    const { matricula } = req.params;
    
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
            temperatura: usuario.temp_preferida,
            luminosidade: usuario.lumi_preferida
        });
    } catch (error) {
        res.status(500).json({ error: error.message });
    }
});

router.post('/atualizar/:matricula', async (req, res) => {
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
            temperatura: usuario.temp_preferida,
            luminosidade: usuario.lumi_preferida
        });
    } catch (error) {
        res.status(500).json({ error: error.message });
    }
});

router.post('/ambiente', (req, res) => {
    const { temperatura, humidade, luminosidade } = req.body;
    
    if (temperatura !== undefined && humidade !== undefined && luminosidade !== undefined) {
        ultimaLeitura = {
            temperatura,
            humidade,
            luminosidade,
            timestamp: Date.now()
        };
        res.json({ success: true });
    } else {
        res.status(400).json({ success: false, message: 'Dados incompletos' });
    }
});

router.get('/ambiente', (req, res) => {
    const agora = Date.now();
    const dadosAtualizados = ultimaLeitura.timestamp && (agora - ultimaLeitura.timestamp) < 60000;
    
    res.json({
        temperatura: dadosAtualizados ? ultimaLeitura.temperatura : null,
        humidade: dadosAtualizados ? ultimaLeitura.humidade : null,
        luminosidade: dadosAtualizados ? ultimaLeitura.luminosidade : null,
        atualizado: dadosAtualizados
    });
});

module.exports = router;