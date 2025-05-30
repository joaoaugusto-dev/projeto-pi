# DOCUMENTAÇÃO - FUNÇÃO verificarComandos()
# Sistema de Automação Residencial ESP32 v3.0
# Estrutura Organizacional para Apresentação

## 📋 VISÃO GERAL DA FUNÇÃO

A função `verificarComandos()` é responsável por:
- Verificar comandos vindos do aplicativo móvel
- Processar comandos de iluminação e climatização
- Sincronizar modos manuais/automáticos
- Fornecer feedback visual ao usuário

---

## 🏗️ ESTRUTURA EM BLOCOS

### BLOCO 1: VALIDAÇÃO INICIAL
**Objetivo:** Verificar condições básicas antes de processar comandos
- ✅ Verifica se o intervalo mínimo foi respeitado
- ✅ Verifica se o WiFi está conectado
- ✅ Define timestamp para próxima execução

### BLOCO 2: COMANDOS DE ILUMINAÇÃO
**Objetivo:** Processar comandos relacionados à iluminação
- 📡 Faz requisição HTTP para endpoint `/comando`
- 🔄 Processa comandos manuais e automáticos
- 💡 Controla níveis de luz (0%, 25%, 50%, 75%, 100%)

### BLOCO 3: COMANDOS DO CLIMATIZADOR
**Objetivo:** Processar comandos do sistema de climatização
- 📡 Faz requisição HTTP para endpoint `/climatizador/comando`
- 🌡️ Executa comandos IR específicos
- ❄️ Controla power, velocidade, timer, umidificação, aletas

### BLOCO 4: SINCRONIZAÇÃO DE MODO MANUAL
**Objetivo:** Sincronizar estado manual/automático
- 📡 Consulta endpoint `/climatizador/manual`
- 🔄 Sincroniza flags locais com servidor
- ⚙️ Mantém consistência entre app e hardware

---

## ⚡ AÇÕES DETALHADAS

### AÇÃO 1: COMANDO MANUAL DE NÍVEL DE LUZ
**Entrada:** Valor numérico (0, 25, 50, 75, 100)
**Processamento:**
1. Valida se o valor está na faixa permitida
2. Ativa modo manual de iluminação
3. Exibe feedback visual no LCD
4. Configura relés conforme o nível solicitado

**Saída:** Luzes ajustadas + modo manual ativo

### AÇÃO 2: COMANDO AUTOMÁTICO DE LUZ
**Entrada:** Comando "auto"
**Processamento:**
1. Desativa modo manual
2. Reativa monitoramento do sensor LDR
3. Permite controle automático baseado em presença

**Saída:** Sistema volta ao modo automático

### AÇÃO 3: COMANDO ESPECÍFICO DO CLIMATIZADOR
**Entrada:** Comandos (power, velocidade, timer, etc.)
**Processamento:**
1. Ativa modo manual do climatizador
2. Mostra feedback do comando no LCD
3. Executa comando IR correspondente
4. Atualiza estado interno do sistema

**Saída:** Climatizador controlado + feedback visual

### AÇÃO 4: SINCRONIZAÇÃO DE MODO MANUAL
**Entrada:** Estado boolean do servidor
**Processamento:**
1. Compara estado local vs servidor
2. Atualiza flag local se necessário
3. Exibe feedback da mudança

**Saída:** Estados sincronizados

---

## 🎯 FUNÇÕES DE APOIO

### Feedback Visual
- `mostrarFeedbackModoManualLuz()` - Tela de entrada no modo manual
- `ativarModoManualClimatizador()` - Tela de ativação manual clima
- `mostrarFeedbackComandoClimatizador()` - Tela de comando específico

### Execução de Comandos
- `executarComandoIR()` - Mapeia comando string para código IR
- `sincronizarModoClima()` - Atualiza modo baseado no servidor

---

## 🔄 FLUXO DE EXECUÇÃO

```
verificarComandos()
    ├── BLOCO 1: Validação
    │   └── Verifica tempo + WiFi
    │
    ├── BLOCO 2: Iluminação
    │   ├── GET /comando
    │   ├── AÇÃO 1: Nível manual (0-100%)
    │   └── AÇÃO 2: Modo automático
    │
    ├── BLOCO 3: Climatizador
    │   ├── GET /climatizador/comando
    │   └── AÇÃO 3: Comando específico
    │
    └── BLOCO 4: Sincronização
        ├── GET /climatizador/manual
        └── AÇÃO 4: Sync modo manual
```

---

## 📊 VANTAGENS DA ORGANIZAÇÃO

✅ **Clareza:** Cada bloco tem responsabilidade específica
✅ **Manutenibilidade:** Fácil localizar e modificar funcionalidades
✅ **Debugging:** Logs específicos para cada ação
✅ **Escalabilidade:** Fácil adicionar novos comandos
✅ **Apresentação:** Estrutura clara para demonstração

---

## 🎓 PONTOS PARA APRESENTAÇÃO

1. **Modularidade:** Cada bloco é independente
2. **Robustez:** Tratamento de erros em cada etapa
3. **Feedback:** Usuario sempre informado das ações
4. **Sincronização:** Consistência entre app e hardware
5. **Performance:** Otimizado para não bloquear o sistema