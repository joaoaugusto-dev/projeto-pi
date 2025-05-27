require('dotenv').config();
const bcrypt = require('bcryptjs');
const jwt = require('jsonwebtoken');
const Funcionarios = require('../funcionarios/Funcionarios');
const express = require('express');
const router = express.Router();
const multer = require('multer');
const path = require('path');
const fs = require('fs');

const register = async (req, res) => {
    let { matricula, f_nome, f_sobrenome, f_senha, confirmasenha } = req.body;
    let originalFilename = req.file ? req.file.filename : null;
    let filePath = req.file ? path.join(__dirname, '..', 'public', 'uploads', originalFilename) : null;

    // Validação da senha
    const passwordValidation = {
        minLength: f_senha.length >= 8,
        hasUpperCase: /[A-Z]/.test(f_senha),
        hasSymbol: /[\W_]/.test(f_senha),
        passwordsMatch: f_senha === confirmasenha,
    };

    if (!passwordValidation.minLength || !passwordValidation.hasUpperCase ||
        !passwordValidation.hasSymbol || !passwordValidation.passwordsMatch) {
        if (filePath) fs.unlinkSync(filePath);
        return res.status(400).json({
            message: 'Erro na senha. A senha deve cumprir os requisitos mínimos.',
        });
    }

    try {
        const matriculaExists = await Funcionarios.findOne({ where: { matricula: matricula } });
        if (matriculaExists) {
            if (filePath) fs.unlinkSync(filePath);
            return res.status(400).json({ message: 'Esta matrícula já está cadastrada.' });
        }

        const hashedPassword = bcrypt.hashSync(f_senha, 10);

        const newUser = await Funcionarios.create({
            matricula: matricula,
            nome: f_nome,
            sobrenome: f_sobrenome,
            senha: hashedPassword,
            foto: null // será atualizado depois
        });

        // Renomear o arquivo com base no ID do usuário
        let newFilename = null;
        if (filePath) {
            newFilename = `perfil-${newUser.id}${path.extname(originalFilename)}`;
            const newFilePath = path.join(__dirname, '..', 'public', 'uploads', newFilename);
            fs.renameSync(filePath, newFilePath);
            await newUser.update({ foto: newFilename });
        }

        const token = jwt.sign({ id: newUser.id }, process.env.JWT_SECRET);
        res.cookie('token', token, { httpOnly: true });

        return res.status(200).json({ success: true, message: 'Cadastro realizado com sucesso!' });
    } catch (err) {
        console.error('Erro durante o processo de registro:', err);
        if (filePath) fs.unlinkSync(filePath);
        return res.status(500).json({ success: false, message: 'Erro no servidor' });
    }
};

const login = async (req, res) => {
    const { matricula, senha } = req.body;

    try {
        const user = await Funcionarios.findOne({ where: { matricula: matricula } });
    
        if (!user || !bcrypt.compareSync(senha, user.senha)) {
            return res.status(400).json({ message: 'Matrícula ou senha incorreta. Tente novamente!' });
        }
    
        const token = jwt.sign({ id: user.id }, process.env.JWT_SECRET);
    
        res.cookie('token', token, { httpOnly: true });
    
        // Log de login bem-sucedido
        console.log(`Login bem-sucedido para o usuário com ID: ${user.id}, matricula: ${user.matricula}`);
    
        return res.redirect('/');
    } catch (err) {
        console.error(err);
        return res.status(500).json({ message: 'Erro no servidor' });
    }    
};

const authenticateToken = (req, res, next) => {
    const token = req.cookies.token;

    if (!token) {
        return res.redirect('/inicio');
    }

    jwt.verify(token, process.env.JWT_SECRET, (err, user) => {
        if (err) {
            return res.redirect('/');
        }

        req.user = user;
        next();
    });
};

// Novo storage sem uuid
const storage = multer.diskStorage({
    destination: function(req, file, cb){
        cb(null, './public/uploads/');
    },
    filename: function (req, file, cb) {
        const tempName = `temp-${Date.now()}${path.extname(file.originalname)}`;
        cb(null, tempName);
    }
});

const upload = multer({ storage });

// Rotas de autenticação
router.post('/register', upload.single('file'), register);
router.post('/login', login);
router.post('/logout', (req, res) => {
  console.log('Cookie antes do logout:', req.cookies.token);
  res.clearCookie('token', { path: '/' });
  console.log('Enviado Set-Cookie para limpar o token');
  return res.status(200).json({ success: true, message: 'Logout realizado' });
});
module.exports = { router, authenticateToken, upload };