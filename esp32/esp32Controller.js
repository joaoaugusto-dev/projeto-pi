const express = require('express');
const router = express.Router();
const Funcionario = require('../funcionarios/Funcionarios');
const Logs = require('../logs/Logs');

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
let comandoManual = "auto"; // Para iluminação
let modoManualClimatizador = false; // Flag global para o climatizador

// Variáveis para controle do climatizador
let comandoClimatizador = null; // Último comando enviado para o ESP32 (do app)
let estadoClimatizador = { // Estado virtual do climatizador mantido pelo servidor
  ligado: false,
  umidificando: false,
  velocidade: 0,
  ultima_velocidade: 1, // Padrão
  timer: 0,
  aleta_vertical: false,
  aleta_horizontal: false,
  ultima_atualizacao: null,
  // 'origem' pode ser mantido se o ESP32 enviar, mas no momento ele não envia para /ambiente
  origem: "sistema" 
};

// Função auxiliar para "snapping" de luminosidade para múltiplos de 25
function nivelValido(media) {
  if (media === 0) return 0;
  const niveis = [0, 25, 50, 75, 100];
  let nivelMaisProximo = niveis[0];
  let menorDiferenca = Math.abs(media - niveis[0]);

  for (let i = 1; i < niveis.length; i++) {
    const diferenca = Math.abs(media - niveis[i]);
    if (diferenca < menorDiferenca) {
      menorDiferenca = diferenca;
      nivelMaisProximo = niveis[i];
    }
  }
  return nivelMaisProximo;
}

// Rota para obter comando manual atual da iluminação
router.get('/comando', (req, res) => {
  res.send(comandoManual);
});

// Rota para definir comando manual ou voltar ao automático da iluminação
router.post('/manual', (req, res) => {
  const { luminosidade } = req.body;
  if (luminosidade === 'auto') {
    comandoManual = 'auto';
    return res.json({ success: true, mensagem: 'Modo automático de iluminação restaurado' });
  }
  const nivel = Number(luminosidade);
  // Garante que o nível é um múltiplo de 25
  if (!isNaN(nivel) && nivel >= 0 && nivel <= 100 && nivel % 25 === 0) {
    comandoManual = String(nivel);
    return res.json({ success: true, mensagem: `Comando manual de iluminação enviado: ${nivel}%` });
  }
  return res.status(400).json({ error: 'Nível inválido (use 0, 25, 50, 75 ou 100)' });
});

// Rota para obter o modo manual do climatizador (diretamente do servidor)
router.get('/climatizador/manual', (req, res) => {
  res.json({ modoManualClimatizador });
});

// Rota para enviar comandos para o climatizador (inclui ligar/desligar modo manual)
router.post('/climatizador/comando', (req, res) => {
  const { comando } = req.body;
  if (!comando) {
    return res.status(400).json({ error: 'Comando não especificado' });
  }
  
  // Lógica para ativar/desativar modo manual do climatizador via aplicativo
  if (comando === 'manual') {
    modoManualClimatizador = true;
    console.log(`Modo manual do climatizador ATIVADO via aplicativo.`);
  } else if (comando === 'auto') {
    modoManualClimatizador = false;
    console.log(`Modo manual do climatizador DESATIVADO via aplicativo.`);
  } else {
    // Para outros comandos (power, velocidade, etc.), o modo manual é ativado
    // se o comando não for "manual" ou "auto"
    if (!modoManualClimatizador) { // Só ativa se já não estiver ativado
      modoManualClimatizador = true;
      console.log(`Comando '${comando}' recebido. Modo manual do climatizador ATIVADO automaticamente.`);
    }
  }

  // Armazenar o comando para ser obtido pelo ESP32
  comandoClimatizador = comando;
  console.log(`Comando para climatizador recebido: ${comando}`);
  
  return res.json({ success: true, message: `Comando ${comando} registrado com sucesso` });
});

// Rota para obter o último comando para o climatizador (para o ESP32)
router.get('/climatizador/comando', (req, res) => {
  const cmd = comandoClimatizador;
  comandoClimatizador = null; // Resetar após leitura para evitar reenvios
  res.send(cmd || 'none');
});

// !!! Rota router.post('/climatizador/estado') foi removida/comentada,
// pois o ESP32 envia o estado do climatizador via a rota '/ambiente' !!!

// Rota para obter o estado atual do climatizador (para o frontend)
router.get('/climatizador/estado', (req, res) => {
  res.json({
    ...estadoClimatizador,
    modoManualClimatizador: modoManualClimatizador, // Adiciona o estado global
    atualizado: estadoClimatizador.ultima_atualizacao !== null && 
                (Date.now() - estadoClimatizador.ultima_atualizacao) < 60000 // Menos de 1 minuto
  });
});

// Cálculo de preferências médias baseado em tags
router.post('/preferencias', async (req, res) => {
  const { tags } = req.body;
  if (!Array.isArray(tags)) {
    console.log('Erro: Tags inválidas recebidas em /preferencias:', tags);
    return res.status(400).json({ error: 'Tags inválidas', luminosidade: 50, temperatura: 25.0 });
  }
  try {
    // Buscar apenas funcionários que têm tags conhecidas
    const funcionarios = await Funcionario.findAll({ where: { tag_nfc: tags } });
    
    // Separar tags conhecidas e desconhecidas
    const tagsConhecidas = funcionarios.map(f => f.tag_nfc);
    const tagsDesconhecidas = tags.filter(tag => !tagsConhecidas.includes(tag));
    
    let tempMedia = 25.0; // Padrão se não houver funcionários cadastrados
    let lumiMedia = 50; // Padrão se não houver funcionários cadastrados
    
    // IMPORTANTE: Só calcular médias se houver pelo menos 1 funcionário cadastrado
    if (funcionarios.length > 0) {
      // Filtrar funcionários com preferências válidas para temperatura (entre 16°C e 32°C)
      const funcionariosTemp = funcionarios.filter(f => 
        f.temp_preferida && f.temp_preferida >= 16 && f.temp_preferida <= 32
      );
      
      // Filtrar funcionários com preferências válidas para luminosidade (0% a 100%)
      const funcionariosLumi = funcionarios.filter(f => 
        f.lumi_preferida !== null && f.lumi_preferida !== undefined && f.lumi_preferida >= 0 && f.lumi_preferida <= 100
      );
      
      // Calcular média de temperatura apenas com valores válidos
      if (funcionariosTemp.length > 0) {
        const somaTemp = funcionariosTemp.reduce((s, f) => s + f.temp_preferida, 0);
        tempMedia = parseFloat((somaTemp / funcionariosTemp.length).toFixed(1));
      }
      
      // Calcular média de luminosidade apenas com valores válidos
      if (funcionariosLumi.length > 0) {
        const somaLumi = funcionariosLumi.reduce((s, f) => s + f.lumi_preferida, 0);
        lumiMedia = nivelValido(somaLumi / funcionariosLumi.length);
      }
      
      console.log(`✓ Preferências calculadas para ${funcionarios.length} funcionários CADASTRADOS (de ${tags.length} tags totais)`);
      console.log(`  -> Temperatura: ${funcionariosTemp.length} valores válidos, média: ${tempMedia}°C`);
      console.log(`  -> Luminosidade: ${funcionariosLumi.length} valores válidos, média: ${lumiMedia}%`);
      console.log(`  -> Tags cadastradas: ${tagsConhecidas.join(', ')}`);
    } else {
      console.log(`⚠ NENHUM funcionário cadastrado encontrado. Usando valores padrão.`);
      console.log(`  -> Temperatura padrão: ${tempMedia}°C`);
      console.log(`  -> Luminosidade padrão: ${lumiMedia}%`);
    }
    
    if (tagsDesconhecidas.length > 0) {
      console.log(`❌ Tags DESCONHECIDAS IGNORADAS no cálculo: ${tagsDesconhecidas.join(', ')}`);
    }
    
    return res.json({ temperatura: tempMedia, luminosidade: lumiMedia });
  } catch (err) {
    console.error('Erro em /preferencias:', err);
    return res.status(500).json({ error: err.message, luminosidade: 50, temperatura: 25.0 });
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
router.post('/ambiente', async (req, res) => {
  const { t, h, l, p, tags, c } = req.body;
  
  // Verificar dados essenciais
  if (t === undefined || h === undefined || l === undefined || p === undefined) {
    console.log('Erro: Dados incompletos em /ambiente', req.body);
    return res.status(400).json({ error: 'Dados incompletos' });
  }
  
  // Mapear os campos compactos para os nomes completos
  const temperatura = t;
  const humidade = h;
  const luminosidade = l;
  const pessoas = p;
  
  // Registrar logs de entrada/saída baseado em mudanças nas tags
  if (tags && Array.isArray(tags)) {
    const tagsAntigas = ultimaLeitura.tags || [];
    const novasTags = tags;
    
    // Detectar entradas (tags que estão agora mas não estavam antes)
    const entradas = novasTags.filter(tag => !tagsAntigas.includes(tag));
    
    // Detectar saídas (tags que estavam antes mas não estão agora)
    const saidas = tagsAntigas.filter(tag => !novasTags.includes(tag));
    
    // Registrar entradas
    for (const tag of entradas) {
      try {
        const funcionario = await Funcionario.findOne({ where: { tag_nfc: tag } });
        if (funcionario) {
          await Logs.create({
            funcionario_id: funcionario.id,
            matricula: funcionario.matricula,
            nome_completo: `${funcionario.nome} ${funcionario.sobrenome}`,
            tipo: 'entrada',
            tag_nfc: tag
          });
          console.log(`Entrada registrada: ${funcionario.nome} ${funcionario.sobrenome} (${funcionario.matricula})`);
        } else {
          // Registrar entrada de tag desconhecida
          await Logs.create({
            funcionario_id: null,
            matricula: null,
            nome_completo: 'TAG DESCONHECIDA',
            tipo: 'entrada',
            tag_nfc: tag
          });
          console.log(`Entrada registrada: TAG DESCONHECIDA (${tag})`);
        }
      } catch (err) {
        console.error('Erro ao registrar entrada:', err);
      }
    }
    
    // Registrar saídas
    for (const tag of saidas) {
      try {
        const funcionario = await Funcionario.findOne({ where: { tag_nfc: tag } });
        if (funcionario) {
          await Logs.create({
            funcionario_id: funcionario.id,
            matricula: funcionario.matricula,
            nome_completo: `${funcionario.nome} ${funcionario.sobrenome}`,
            tipo: 'saida',
            tag_nfc: tag
          });
          console.log(`Saída registrada: ${funcionario.nome} ${funcionario.sobrenome} (${funcionario.matricula})`);
        } else {
          // Registrar saída de tag desconhecida
          await Logs.create({
            funcionario_id: null,
            matricula: null,
            nome_completo: 'TAG DESCONHECIDA',
            tipo: 'saida',
            tag_nfc: tag
          });
          console.log(`Saída registrada: TAG DESCONHECIDA (${tag})`);
        }
      } catch (err) {
        console.error('Erro ao registrar saída:', err);
      }
    }
  }
  
  // ATENÇÃO: Corrigido o mapeamento do objeto 'c' (climatizador)
  if (c) {
    estadoClimatizador = {
      ligado: !!c.l,
      umidificando: !!c.u,
      velocidade: c.v !== undefined ? c.v : 0,
      ultima_velocidade: c.uv !== undefined ? c.uv : 1, // Mapeado de c.uv
      timer: c.t !== undefined ? c.t : 0,
      aleta_vertical: !!c.av,      // Mapeado de c.av
      aleta_horizontal: !!c.ah,    // Mapeado de c.ah
      ultima_atualizacao: Date.now(),
      origem: estadoClimatizador.origem // O ESP32 não envia 'origem' neste POST, mantém o valor anterior
    };
    // Sincroniza a flag global de modo manual do climatizador com a do ESP32
    // Agora o ESP32 envia c.mmc no /ambiente POST
    if (c.mmc !== undefined) {
      modoManualClimatizador = !!c.mmc;
    }
    console.log(`Estado do climatizador atualizado via /ambiente: ${JSON.stringify(estadoClimatizador)}`);
  }
  
  // Armazenar dados de leitura do ambiente
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
  let lumiUtilizada = 0; // Esta é a luminosidade que o sistema de automação está tentando usar
  if (atualizado && ultimaLeitura.tags && ultimaLeitura.tags.length) {
    try {
      const funcs = await Funcionario.findAll({ where: { tag_nfc: ultimaLeitura.tags } });
      
      // Separar tags conhecidas e desconhecidas
      const tagsConhecidas = funcs.map(f => f.tag_nfc);
      const tagsDesconhecidas = ultimaLeitura.tags.filter(tag => !tagsConhecidas.includes(tag));
      
      // Adicionar funcionários conhecidos
      presentes = funcs.map(f => ({ 
        nome: `${f.nome} ${f.sobrenome}`, 
        temp_preferida: f.temp_preferida, 
        lumi_preferida: f.lumi_preferida 
      }));
      
      // Adicionar tags desconhecidas (SEM valores de preferência)
      tagsDesconhecidas.forEach(tag => {
        presentes.push({
          nome: 'TAG DESCONHECIDA',
          temp_preferida: null,
          lumi_preferida: null,
          tag_desconhecida: tag
        });
      });
      
      // *** MUDANÇA CRÍTICA: Calcular médias APENAS com funcionários conhecidos e com valores válidos ***
      if (funcs.length > 0) {
        // Filtrar funcionários com preferências válidas para temperatura (entre 16°C e 32°C)
        const funcsValidosTemp = funcs.filter(f => 
          f.temp_preferida && f.temp_preferida >= 16 && f.temp_preferida <= 32
        );
        
        // Filtrar funcionários com preferências válidas para luminosidade (0% a 100%)
        const funcsValidosLumi = funcs.filter(f => 
          f.lumi_preferida !== null && f.lumi_preferida !== undefined && f.lumi_preferida >= 0 && f.lumi_preferida <= 100
        );
        
        // Calcular médias apenas com valores válidos
        if (funcsValidosTemp.length > 0) {
          const sumTp = funcsValidosTemp.reduce((s, f) => s + f.temp_preferida, 0);
          tempPrefMed = parseFloat((sumTp / funcsValidosTemp.length).toFixed(1));
        } else {
          tempPrefMed = 25.0; // Padrão se não houver valores válidos
        }
        
        if (funcsValidosLumi.length > 0) {
          const sumLp = funcsValidosLumi.reduce((s, f) => s + f.lumi_preferida, 0);
          const lumiRealMedia = sumLp / funcsValidosLumi.length;
          lumiPrefMed = parseFloat(lumiRealMedia.toFixed(1)); // mostra a média real com 1 casa decimal
          lumiUtilizada = nivelValido(lumiRealMedia); // snapping só aqui, para uso real
        } else {
          lumiPrefMed = 50.0; // Padrão se não houver valores válidos
          lumiUtilizada = 50;
        }
        
        console.log(`✓ Médias calculadas com ${funcs.length} funcionários cadastrados:`);
        console.log(`  -> Temp: ${funcsValidosTemp.length} valores válidos, média: ${tempPrefMed}°C`);
        console.log(`  -> Lumi: ${funcsValidosLumi.length} valores válidos, média: ${lumiPrefMed}% (utilizada: ${lumiUtilizada}%)`);
      } else {
        // Se não há funcionários cadastrados, usar valores padrão
        tempPrefMed = 25.0;
        lumiPrefMed = 50.0;
        lumiUtilizada = 50;
        console.log(`⚠ Nenhum funcionário cadastrado. Usando valores padrão: Temp: ${tempPrefMed}°C, Lumi: ${lumiPrefMed}%`);
      }
      
      if (tagsDesconhecidas.length > 0) {
        console.log(`❌ Tags desconhecidas IGNORADAS no cálculo: ${tagsDesconhecidas.join(', ')}`);
      }
    } catch (err) {
      console.error('Erro ao calcular presentes:', err);
    }
  }

  // Mapear estado do climatizador para os campos esperados pelo frontend
  const dadosClimatizador = estadoClimatizador ? {
    climatizador_ligado: estadoClimatizador.ligado,
    climatizador_umidificando: estadoClimatizador.umidificando,
    climatizador_velocidade: estadoClimatizador.velocidade,
    climatizador_ultima_velocidade: estadoClimatizador.ultima_velocidade,
    climatizador_timer: estadoClimatizador.timer,
    climatizador_aleta_vertical: estadoClimatizador.aleta_vertical,
    climatizador_aleta_horizontal: estadoClimatizador.aleta_horizontal
  } : {};

  return res.json({
    temperatura: atualizado && ultimaLeitura.temperatura !== null ? ultimaLeitura.temperatura : null,
    humidade: atualizado && ultimaLeitura.humidade !== null ? ultimaLeitura.humidade : null,
    luminosidade: atualizado && ultimaLeitura.luminosidade !== null ? ultimaLeitura.luminosidade : null,
    pessoas: atualizado ? ultimaLeitura.pessoas : 0,
    tempMediaHistorica: tempHist,
    lumiMediaHistorica: lumiHist,
    presentes,
    tempPreferidaMedia: tempPrefMed,
    lumiPreferidaMedia: lumiPrefMed,
    lumiNivelUtilizado: lumiUtilizada, // Adicionado para exibir no frontend
    modoManualIluminacao: comandoManual !== 'auto', // Indica se iluminação está em modo manual
    modoManualClimatizador: modoManualClimatizador, // Inclui o estado do modo manual do climatizador
    atualizado,
    ...dadosClimatizador
  });
});

// Ajusta luminosidade de um usuário específico (prefeência)
router.post('/ajustar-luminosidade', async (req, res) => {
  const { matricula, luminosidade } = req.body;
  if (!matricula || luminosidade === undefined) return res.status(400).json({ error: 'Dados inválidos' });
  if (![0,25,50,75,100].includes(luminosidade)) return res.status(400).json({ error: 'Nível inválido' });
  try {
    const user = await Funcionario.findOne({ where: { matricula } });
    if (!user) return res.status(404).json({ error: 'Usuário não encontrado' });
    await user.update({ lumi_preferida: luminosidade });
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

// Rota para fornecer logs de histórico em JSON (para atualização automática)
router.get('/historico-logs', async (req, res) => {
  try {
    const logs = await Logs.findAll({
      order: [['createdAt', 'DESC']],
      limit: 100,
    });
    // Buscar a foto de cada funcionário pela matrícula
    const logsComFoto = await Promise.all(logs.map(async log => {
      let foto = null;
      if (log.matricula) {
        const funcionario = await Funcionario.findOne({ where: { matricula: log.matricula } });
        if (funcionario && funcionario.foto) {
          foto = funcionario.foto;
        }
      }
      return { ...log.toJSON(), foto };
    }));
    res.json(logsComFoto);
  } catch (err) {
    console.error('Erro ao buscar logs:', err);
    res.status(500).json({ error: 'Erro ao carregar histórico' });
  }
});

module.exports = router;