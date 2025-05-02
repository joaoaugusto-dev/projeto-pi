const Account = require('./accounts/Accounts');

//Account.sync({ force: true });

const createTables = async () => {
    try {
        await Account.sync();
        console.log('Tabelas verificadas com sucesso!');
    } catch (error) {
        console.error('Erro ao verificar tabelas:', error);
    }
};

module.exports = createTables;