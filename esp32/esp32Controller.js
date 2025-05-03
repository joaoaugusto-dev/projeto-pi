const express = require('express');
const router = express.Router();
const Funcionario = require('../funcionarios/Funcionarios');

// Variável para controlar o status do cadastro de tag
let cadastroTagPendente = null;

// Variável para controlar tags recém registradas
let tagsRegistradas = new Map();

// Adicionar variável para controlar tags que foram realmente registradas
let tagsConfirmadas = new Map();

// Rota para consultar usuário por tag NFC
router.get('/tag/:tagNfc', async (req, res) => {
    const { tagNfc } = req.params;
    
    if (!tagNfc) {
        return res.status(400).json({ error: 'Tag NFC é necessária' });
    }

    try {
        const usuario = await Funcionario.findOne({ where: { tag_nfc: tagNfc } });
        if (!usuario) {
            return res.status(404).json({ error: 'Tag não encontrada' });
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

// Rota para consultar usuário por matrícula
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

// Rota para atualizar preferências
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

// Rota para buscar preferências do funcionário por tag NFC
router.get('/preferencias/:tagNFC', async (req, res) => {
    try {
        const funcionario = await Funcionario.findOne({
            where: { tag_nfc: req.params.tagNFC }
        });

        if (!funcionario) {
            return res.status(404).json({ message: 'Tag NFC não encontrada' });
        }

        return res.json({
            temperatura: funcionario.temp_preferida,
            luminosidade: funcionario.lumi_preferida
        });
    } catch (err) {
        console.error(err);
        return res.status(500).json({ message: 'Erro ao buscar preferências' });
    }
});

// Rota para validar tag NFC
router.post('/validar-tag', async (req, res) => {
    const { tag_nfc } = req.body;

    try {
        const funcionario = await Funcionario.findOne({
            where: { tag_nfc }
        });

        if (!funcionario) {
            return res.status(404).json({ message: 'Tag NFC não encontrada' });
        }

        return res.json({
            valido: true,
            funcionario: {
                nome: funcionario.nome,
                sobrenome: funcionario.sobrenome,
                temperatura: funcionario.temp_preferida,
                luminosidade: funcionario.lumi_preferida
            }
        });
    } catch (err) {
        console.error(err);
        return res.status(500).json({ message: 'Erro ao validar tag' });
    }
});

// Rota para verificar status do cadastro de tag
router.get('/tag-status/pendente', async (req, res) => {
    if (cadastroTagPendente) {
        res.json({
            status: 'aguardando',
            matricula: cadastroTagPendente
        });
    } else {
        res.json({ status: 'nenhum' });
    }
});

// Rota para verificar status específico de uma matrícula
router.get('/tag-status/:matricula', async (req, res) => {
    const { matricula } = req.params;
    try {
        const usuario = await Funcionario.findOne({ where: { matricula } });
        if (!usuario) {
            return res.status(404).json({ status: 'erro', message: 'Usuário não encontrado' });
        }

        // Verifica se a tag foi confirmada (registrada com sucesso)
        if (tagsConfirmadas.has(matricula)) {
            const tagInfo = tagsConfirmadas.get(matricula);
            tagsConfirmadas.delete(matricula);
            return res.json({ 
                status: 'concluido',
                tag_nfc: tagInfo.tag_nfc
            });
        }

        // Verifica se está em modo de cadastro
        if (cadastroTagPendente === matricula) {
            return res.json({ status: 'aguardando' });
        }
        
        return res.json({ status: 'nenhum' });
    } catch (err) {
        console.error(err);
        return res.status(500).json({ status: 'erro', message: 'Erro ao verificar status' });
    }
});

// Rota para iniciar processo de cadastro de nova tag
router.post('/iniciar-cadastro-tag/:matricula', async (req, res) => {
    const { matricula } = req.params;

    try {
        const usuario = await Funcionario.findOne({ where: { matricula } });
        if (!usuario) {
            return res.status(404).json({ message: 'Usuário não encontrado' });
        }

        cadastroTagPendente = matricula;
        res.json({
            message: 'Aguardando leitura de nova tag',
            matricula: matricula
        });
    } catch (err) {
        console.error(err);
        return res.status(500).json({ message: 'Erro ao iniciar cadastro de tag' });
    }
});

// Rota para registrar nova tag
router.post('/registrar-tag/:matricula', async (req, res) => {
    const { matricula } = req.params;
    const { tag_nfc } = req.body;

    try {
        // Verificar se a tag já está em uso
        const tagExistente = await Funcionario.findOne({ where: { tag_nfc } });
        if (tagExistente) {
            return res.status(400).json({ message: 'Tag já está em uso por outro usuário' });
        }

        // Atualizar o usuário com a nova tag
        const usuario = await Funcionario.findOne({ where: { matricula } });
        if (!usuario) {
            return res.status(404).json({ message: 'Usuário não encontrado' });
        }

        await usuario.update({ tag_nfc });
        cadastroTagPendente = null; // Limpa o status pendente
        
        // Registra a tag como confirmada
        tagsConfirmadas.set(matricula, { 
            tag_nfc,
            timestamp: Date.now() 
        });
        
        res.json({
            message: 'Tag registrada com sucesso',
            tag: tag_nfc,
            usuario: {
                matricula: usuario.matricula,
                nome: usuario.nome
            }
        });
    } catch (err) {
        console.error(err);
        return res.status(500).json({ message: 'Erro ao registrar tag' });
    }
});

// Rota para cancelar o registro de tag
router.post('/cancelar-registro-tag/:matricula', async (req, res) => {
    const { matricula } = req.params;
    
    try {
        if (cadastroTagPendente === matricula) {
            cadastroTagPendente = null;
            res.json({ message: 'Registro de tag cancelado com sucesso' });
        } else {
            res.status(400).json({ message: 'Nenhum registro de tag pendente para esta matrícula' });
        }
    } catch (err) {
        console.error(err);
        res.status(500).json({ message: 'Erro ao cancelar registro de tag' });
    }
});

module.exports = router;