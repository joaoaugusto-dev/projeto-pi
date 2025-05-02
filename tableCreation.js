const Account = require('./accounts/Accounts');
const DadosESP32 = require('./esp32/DadosESP32');
const NFCTag = require('./esp32/NFCTag');

const createTables = async () => {
    try {
        await Account.sync();
        await DadosESP32.sync();
        await NFCTag.sync();
        console.log('Tabelas verificadas com sucesso!');
    } catch (error) {
        console.error('Erro ao verificar tabelas:', error);
    }
};

module.exports = createTables;