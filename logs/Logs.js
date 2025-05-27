const { DataTypes } = require('sequelize');
const sequelize = require('../database/conexao');

const Logs = sequelize.define('Logs', {
  funcionario_id: {
    type: DataTypes.INTEGER,
    allowNull: true
  },
  matricula: {
    type: DataTypes.STRING,
    allowNull: true
  },
  nome_completo: {
    type: DataTypes.STRING,
    allowNull: true
  },
  tipo: {
    type: DataTypes.ENUM('entrada', 'saida'),
    allowNull: false
  },
  tag_nfc: {
    type: DataTypes.STRING,
    allowNull: true
  }
}, {
  timestamps: true
});

module.exports = Logs;