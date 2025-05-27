const express = require('express')
const router = express.Router()
const Funcionario = require("./Funcionarios")

router.post('/funcionario/salvar', (req, res) => {
    let { f_nome, f_sobrenome, f_nasc, f_cpf, f_telefone, f_email, f_senha} = req.body

    console.log("Dados recebidos do formulário:", req.body);

    Funcionario.create({
        nome: f_nome,
        sobrenome: f_sobrenome,
        nasc: f_nasc,
        cpf: f_cpf,
        telefone: f_telefone,
        email: f_email,
        senha: f_senha
    }).then(() => {
        res.redirect('/')
    }).catch(erro => {
        console.log(erro)
    })
})

module.exports = router