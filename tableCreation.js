const Funcionario = require('./funcionarios/Funcionarios');

const createTables = async () => {
    try {
        await Funcionario.sync();
        console.log('Tabelas verificadas com sucesso!');
    } catch (error) {
        console.error('Erro ao verificar tabelas:', error);
    }
};

module.exports = createTables;