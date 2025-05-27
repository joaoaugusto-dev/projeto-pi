const Funcionario = require('./funcionarios/Funcionarios');
const Logs = require('./logs/Logs');

const createTables = async () => {
    try {
        await Funcionario.sync();
        await Logs.sync();
        console.log('Tabelas verificadas com sucesso!');
    } catch (error) {
        console.error('Erro ao verificar tabelas:', error);
    }
};

module.exports = createTables;