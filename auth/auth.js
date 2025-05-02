require('dotenv').config();
const bcrypt = require('bcryptjs');
const jwt = require('jsonwebtoken');
const Accounts = require('../accounts/Accounts');
const express = require('express');
const router = express.Router();
const multer = require('multer');
const path = require('path');
const fs = require('fs');

function obterProximoNumeroSequencial() {
    const uploadDir = path.join(__dirname, '..', 'public', 'uploads');
    const arquivos = fs.readdirSync(uploadDir);
    
    let maiorNumero = 0;

    arquivos.forEach(arquivo => {
        const match = arquivo.match(/perfil-(\d+)\./);
        if (match) {
            const numero = parseInt(match, 10);
            if (numero > maiorNumero) {
                maiorNumero = numero;
            }
        }
    });

    return maiorNumero + 1;
}

const register = async (req, res) => {
    console.log('Início do processo de registro');
    let { f_nome, f_sobrenome, f_nasc, f_cpf, f_telefone, f_email, f_senha, confirmasenha } = req.body;
    console.log('Dados recebidos:', req.body);
    
    let file = req.file ? req.file.filename : null;
    const filePath = req.file ? path.join(__dirname, '..', 'public', 'uploads', file) : null;
    console.log('Arquivo de foto recebido:', file);

    // Validação das regras de senha
    const passwordValidation = {
        minLength: f_senha.length >= 8,
        hasUpperCase: /[A-Z]/.test(f_senha),
        hasSymbol: /[\W_]/.test(f_senha),
        passwordsMatch: f_senha === confirmasenha,
    };

    // Se alguma das regras não for cumprida, retorna um erro
    if (!passwordValidation.minLength || !passwordValidation.hasUpperCase || !passwordValidation.hasSymbol || !passwordValidation.passwordsMatch) {
        console.log('Erro: A senha não cumpre os requisitos');
        if (filePath) fs.unlinkSync(filePath);
        return res.status(400).json({
            message: 'Erro na senha. A senha deve cumprir os requisitos mínimos.',
        });
    }

    // Continua com o processo de registro se a senha for válida
    try {
        console.log('Verificando se o e-mail já está cadastrado');
        const userExists = await Accounts.findOne({ where: { email: f_email } });
        if (userExists) {
            console.log('Erro: E-mail já cadastrado');
            if (filePath) fs.unlinkSync(filePath);
            return res.status(400).json({ message: 'Erro no e-mail. \nEste e-mail já foi cadastrado.' });
        }

        console.log('Verificando se o CPF já está cadastrado');
        const cpfExists = await Accounts.findOne({ where: { cpf: f_cpf } });
        if (cpfExists) {
            console.log('Erro: CPF já cadastrado');
            if (filePath) fs.unlinkSync(filePath);
            return res.status(400).json({ message: 'Erro no CPF. \nEste CPF já foi cadastrado.' });
        }

        console.log('Gerando hash da senha');
        const hashedPassword = bcrypt.hashSync(f_senha, 10);

        console.log('Criando novo usuário no banco de dados');
        const newUser = await Accounts.create({
            nome: f_nome,
            sobrenome: f_sobrenome,
            nasc: f_nasc,
            cpf: f_cpf,
            telefone: f_telefone,
            email: f_email,
            senha: hashedPassword,
            foto: file,
        });

        console.log('Gerando token JWT para o novo usuário');
        const token = jwt.sign({ id: newUser.id }, process.env.JWT_SECRET);
        
        console.log('Definindo cookie do token');
        res.cookie('token', token, { httpOnly: true });

        console.log('Usuário registrado com sucesso, redirecionando para a página inicial');
        return res.redirect('/');
    } catch (err) {
        console.error('Erro durante o processo de registro:', err);
        if (filePath) fs.unlinkSync(filePath);
        return res.status(500).json({ message: 'Erro no servidor' });
    }
};

const login = async (req, res) => {
    const { email, senha } = req.body;

    try {
        const user = await Accounts.findOne({ where: { email: email } });

        if (!user || !bcrypt.compareSync(senha, user.senha)) {
            return res.status(400).json({ message: 'Email ou Senha incorreta. Tente novamente!' });
        }

        const token = jwt.sign({ id: user.id }, process.env.JWT_SECRET);

        res.cookie('token', token, { httpOnly: true });
        return res.json({ message: 'Login bem-sucedido!' });
    } catch (err) {
        console.error(err);
        return res.status(500).json({ message: 'Erro no servidor' });
    }
};

const authenticateToken = (req, res, next) => {
    const token = req.cookies.token;

    if (!token) {
        return res.redirect('/entrar');
    }

    jwt.verify(token, process.env.JWT_SECRET, (err, user) => {
        if (err) {
            return res.redirect('/');
        }

        req.user = user;
        next();
    });
};

const storage = multer.diskStorage({
    destination: function(req, file, cb){
        cb(null, './public/uploads/');
    },
    filename: function (req, file, cb) {
        const numeroSequencial = obterProximoNumeroSequencial();
        const extensao = path.extname(file.originalname);
        const nomeArquivo = `perfil-${numeroSequencial}${extensao}`;
        console.log(`Nome do arquivo gerado: ${nomeArquivo}`);
        cb(null, nomeArquivo);
    }
});

const upload = multer({ storage });

// Rotas de autenticação
router.post('/register', upload.single('file'), register);
router.post('/login', login);
router.post('/logout', (req, res) => {
    res.clearCookie('token');
    res.json({ message: 'Logout bem-sucedido!' });
});

module.exports = { router, authenticateToken, upload };