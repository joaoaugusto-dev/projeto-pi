const express = require('express');
const router = express.Router();

// Rota de teste para garantir que o controller está funcionando
router.get('/logs/teste', (req, res) => {
  res.json({ success: true, message: 'logsController ativo!' });
});

module.exports = router;
