const express = require('express');
const router = express.Router();
const Funcionario = require('../funcionarios/Funcionarios');

let ultimaLeitura = {
  temperatura: null,
  humidade: null,
  luminosidade: null,
  pessoas: 0,
  tags: [],
  timestamp: null
};

let historicoLeituras = [];
const MAX_HISTORICO = 10;
let comandoManual = "auto";

// Função auxiliar para "snapping" de luminosidade
function nivelValido(media) {
  const niveis = [0, 25, 50, 75, 100];
  for (let i = 0; i < niveis.length - 1; i++) {
    if (media > niveis[i] && media <= niveis[i + 1]) {
      return niveis[i + 1];
    }
  }
  return 100; // Se for maior que 100 ou exatamente 100
}

// Rota para obter comando manual atual
router.get('/comando', (req, res) => {
  res.send(comandoManual);
});

// Rota para definir comando manual ou voltar ao automático
router.post('/manual', (req, res) => {
  const { luminosidade } = req.body;
  if (luminosidade === 'auto') {
    comandoManual = 'auto';
    return res.json({ success: true, mensagem: 'Modo automático restaurado' });
  }
  const nivel = Number(luminosidade);
  if ([0,25,50,75,100].includes(nivel)) {
    comandoManual = String(nivel);
    return res.json({ success: true, mensagem: `Comando manual enviado: ${nivel}%` });
  }
  return res.status(400).json({ error: 'Nível inválido (use 0,25,50,75 ou 100)' });
});

// Cálculo de preferências médias baseado em tags
router.post('/preferencias', async (req, res) => {
  const { tags } = req.body;
  if (!Array.isArray(tags)) {
    return res.status(400).json({ error: 'Tags inválidas', luminosidade: 0, temperatura: 25.0 });
  }
  try {
    const funcionarios = await Funcionario.findAll({ where: { tag_nfc: tags } });
    let tempMedia = 25.0;
    let lumiMedia = 0;
    if (funcionarios.length) {
      const somaTemp = funcionarios.reduce((s, f) => s + (f.temp_preferida || 25.0), 0);
      const somaLumi = funcionarios.reduce((s, f) => s + (f.lumi_preferida || 0), 0);
      tempMedia = parseFloat((somaTemp / funcionarios.length).toFixed(1));
      lumiMedia = nivelValido(somaLumi / funcionarios.length);
    }
    return res.json({ temperatura: tempMedia, luminosidade: lumiMedia });
  } catch (err) {
    console.error('Erro em /preferencias:', err);
    return res.status(500).json({ error: err.message, luminosidade: 0, temperatura: 25.0 });
  }
});

// Consulta por matrícula
router.get('/matricula/:matricula', async (req, res) => {
  const { matricula } = req.params;
  if (!matricula) return res.status(400).json({ error: 'Matrícula é necessária' });
  try {
    const user = await Funcionario.findOne({ where: { matricula } });
    if (!user) return res.status(404).json({ error: 'Usuário não encontrado' });
    return res.json({
      matricula: user.matricula,
      nome: user.nome,
      temperatura: user.temp_preferida,
      luminosidade: user.lumi_preferida
    });
  } catch (err) {
    console.error('Erro em GET /matricula:', err);
    return res.status(500).json({ error: err.message });
  }
});

// Atualiza preferências do usuário
router.post('/atualizar/:matricula', async (req, res) => {
  const { matricula } = req.params;
  const { temp_preferida, lumi_preferida } = req.body;
  try {
    const user = await Funcionario.findOne({ where: { matricula } });
    if (!user) return res.status(404).json({ error: 'Usuário não encontrado' });
    await user.update({ temp_preferida, lumi_preferida });
    return res.json({
      matricula: user.matricula,
      nome: user.nome,
      temperatura: user.temp_preferida,
      luminosidade: user.lumi_preferida
    });
  } catch (err) {
    console.error('Erro em POST /atualizar:', err);
    return res.status(500).json({ error: err.message });
  }
});

// Recebe nova leitura de ambiente
router.post('/ambiente', (req, res) => {
  const { temperatura, humidade, luminosidade, pessoas, tags } = req.body;
  if ([temperatura, humidade, luminosidade, pessoas, tags].some(v => v === undefined)) {
    return res.status(400).json({ error: 'Dados incompletos' });
  }
  ultimaLeitura = { temperatura, humidade, luminosidade, pessoas, tags, timestamp: Date.now() };
  historicoLeituras.push(ultimaLeitura);
  if (historicoLeituras.length > MAX_HISTORICO) historicoLeituras.shift();
  return res.json({ success: true });
});

// Fornece dados atuais, médias históricas e preferências presentes
router.get('/ambiente', async (req, res) => {
  const agora = Date.now();
  const atualizado = ultimaLeitura.timestamp && (agora - ultimaLeitura.timestamp) < 60000;

  // Médias históricas
  let tempHist = null, lumiHist = null;
  if (historicoLeituras.length) {
    const somaT = historicoLeituras.reduce((s, l) => s + l.temperatura, 0);
    const somaL = historicoLeituras.reduce((s, l) => s + l.luminosidade, 0);
    tempHist = parseFloat((somaT / historicoLeituras.length).toFixed(1));
    lumiHist = nivelValido(somaL / historicoLeituras.length);
  }

  // Preferências de presentes
  let presentes = [], tempPrefMed = null, lumiPrefMed = null;
  if (atualizado && ultimaLeitura.tags.length) {
    try {
      const funcs = await Funcionario.findAll({ where: { tag_nfc: ultimaLeitura.tags } });
      presentes = funcs.map(f => ({ nome: `${f.nome} ${f.sobrenome}`, temp_preferida: f.temp_preferida, lumi_preferida: f.lumi_preferida }));
      if (funcs.length) {
  const sumTp = funcs.reduce((s, f) => s + (f.temp_preferida || 0), 0);
  const sumLp = funcs.reduce((s, f) => s + (f.lumi_preferida || 0), 0);
  tempPrefMed = parseFloat((sumTp / funcs.length).toFixed(1));
  const lumiRealMedia = sumLp / funcs.length;
  lumiPrefMed = parseFloat(lumiRealMedia.toFixed(1)); // mostra a média real com 1 casa decimal
  lumiUtilizada = nivelValido(lumiRealMedia); // snapping só aqui, para uso real
}
    } catch (err) {
      console.error('Erro ao calcular presentes:', err);
    }
  }

  return res.json({
    temperatura: atualizado ? ultimaLeitura.temperatura : null,
    humidade: atualizado ? ultimaLeitura.humidade : null,
    luminosidade: atualizado ? ultimaLeitura.luminosidade : null,
    pessoas: atualizado ? ultimaLeitura.pessoas : 0,
    tempMediaHistorica: tempHist,
    lumiMediaHistorica: lumiHist,
    presentes,
    tempPreferidaMedia: tempPrefMed,
    lumiPreferidaMedia: lumiPrefMed,
    atualizado
  });
});

// Ajusta luminosidade de um usuário específico
router.post('/ajustar-luminosidade', async (req, res) => {
  const { matricula, luminosidade } = req.body;
  if (!matricula || luminosidade === undefined) return res.status(400).json({ error: 'Dados inválidos' });
  if (![0,25,50,75,100].includes(luminosidade)) return res.status(400).json({ error: 'Nível inválido' });
  try {
    const user = await Funcionario.findOne({ where: { matricula } });
    if (!user) return res.status(404).json({ error: 'Usuário não encontrado' });
    await user.update({ lumi_preferida: luminosidade });
    ultimaLeitura.luminosidade = luminosidade;
    ultimaLeitura.timestamp = Date.now();
    return res.json({ success: true, luminosidade });
  } catch (err) {
    console.error('Erro em /ajustar-luminosidade:', err);
    return res.status(500).json({ error: err.message });
  }
});

// Consulta por tag NFC única
router.get('/tag/:tag_nfc', async (req, res) => {
  try {
    const user = await Funcionario.findOne({ where: { tag_nfc: req.params.tag_nfc } });
    if (!user) return res.status(404).json({ error: 'Tag não registrada' });
    return res.json({ matricula: user.matricula, nome: user.nome, temperatura: user.temp_preferida, luminosidade: user.lumi_preferida });
  } catch (err) {
    console.error('Erro em /tag:', err);
    return res.status(500).json({ error: err.message });
  }
});

module.exports = router;