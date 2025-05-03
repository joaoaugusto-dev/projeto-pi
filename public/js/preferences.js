// Update ambiente data
function atualizarDadosAmbiente() {
    fetch('/esp32/ambiente')
        .then(response => response.json())
        .then(data => {
            const tempSpan = document.getElementById('temp_atual');
            const humidSpan = document.getElementById('humid_atual');
            
            if (tempSpan) {
                tempSpan.textContent = data.atualizado ? `${data.temperatura}°C` : '--°C';
                tempSpan.style.color = data.atualizado ? 'inherit' : '#999';
            }
            if (humidSpan) {
                humidSpan.textContent = data.atualizado ? `${data.humidade}%` : '--%';
                humidSpan.style.color = data.atualizado ? 'inherit' : '#999';
            }
        })
        .catch(error => console.error('Erro ao atualizar dados:', error));
}

// NFC Tag management
let tagModal = null;
let aguardandoTag = false;
let modoRegistro = false;

document.addEventListener('DOMContentLoaded', function() {
    tagModal = new bootstrap.Modal(document.getElementById('tagModal'));
    
    // Initialize ambiente data update
    atualizarDadosAmbiente();
    setInterval(atualizarDadosAmbiente, 30000);

    // Setup preference form listeners
    setupPreferenceFormListeners();
});

function setupPreferenceFormListeners() {
    const tempSlider = document.getElementById('temp_preferida');
    const lumiSlider = document.getElementById('lumi_preferida');
    const preferencesForm = document.getElementById('preferencesForm');

    if (tempSlider) {
        tempSlider.addEventListener('input', function() {
            document.getElementById('temp_value').textContent = this.value + '°C';
        });
    }

    if (lumiSlider) {
        lumiSlider.addEventListener('input', function() {
            document.getElementById('lumi_value').textContent = this.value + ' lux';
        });
    }

    if (preferencesForm) {
        preferencesForm.addEventListener('submit', handlePreferenceSubmit);
    }
}

async function handlePreferenceSubmit(e) {
    e.preventDefault();
    
    const temperatura = document.getElementById('temp_preferida').value;
    const luminosidade = document.getElementById('lumi_preferida').value;
    const tagNFC = document.getElementById('tag_nfc').value;

    try {
        const response = await fetch('/preferencias/atualizar', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                temp_preferida: temperatura,
                lumi_preferida: luminosidade,
                tag_nfc: tagNFC
            })
        });

        if (response.ok) {
            window.location.href = '/dados';
        } else {
            const data = await response.json();
            document.getElementById('errorMessage').textContent = data.message || 'Erro ao atualizar preferências';
        }
    } catch (error) {
        console.error('Erro:', error);
        document.getElementById('errorMessage').textContent = 'Erro ao atualizar preferências';
    }
}

// NFC Tag registration functions
async function iniciarCadastroTag(matricula) {
    try {
        tagModal.show();
        document.getElementById('tagMessage').textContent = 'Conectando ao ESP32...';
        document.getElementById('tagDetails').textContent = '';
        document.getElementById('statusIcon').innerHTML = '<div class="spinner-border text-primary" role="status"><span class="visually-hidden">Carregando...</span></div>';

        const response = await fetch(`/esp32/iniciar-cadastro-tag/${matricula}`, {
            method: 'POST'
        });

        if (response.ok) {
            aguardandoTag = true;
            modoRegistro = false;
            document.getElementById('tagMessage').textContent = 'ESP32 conectado! Iniciando modo de registro...';
            document.getElementById('tagDetails').textContent = 'Matrícula: ' + matricula;
            document.getElementById('statusIcon').innerHTML = '<i class="bi bi-broadcast text-success" style="font-size: 2rem;"></i>';
            iniciarVerificacaoTag();
        } else {
            const data = await response.json();
            document.getElementById('tagMessage').textContent = data.message || 'Erro ao conectar com ESP32';
            document.getElementById('statusIcon').innerHTML = '<i class="bi bi-x-circle text-danger" style="font-size: 2rem;"></i>';
            aguardandoTag = false;
        }
    } catch (error) {
        console.error('Erro:', error);
        document.getElementById('tagMessage').textContent = 'Erro ao conectar com ESP32';
        document.getElementById('statusIcon').innerHTML = '<i class="bi bi-x-circle text-danger" style="font-size: 2rem;"></i>';
        aguardandoTag = false;
    }
}

async function verificarNovaTag(matricula) {
    if (!aguardandoTag) return;

    try {
        const responseEsp = await fetch('/esp32/tag-status/pendente');
        const dataEsp = await responseEsp.json();

        if (dataEsp.status === 'aguardando' && !modoRegistro) {
            modoRegistro = true;
            document.getElementById('tagMessage').textContent = 'Aproxime sua tag do leitor...';
            document.getElementById('statusIcon').innerHTML = '<i class="bi bi-broadcast text-primary" style="font-size: 2rem;"></i>';
            return;
        }

        if (modoRegistro) {
            const response = await fetch(`/esp32/tag-status/${matricula}`);
            const data = await response.json();

            if (data.status === 'concluido' && data.tag_nfc) {
                document.getElementById('statusIcon').innerHTML = '<i class="bi bi-check-circle text-success" style="font-size: 2rem;"></i>';
                document.getElementById('tagMessage').textContent = 'Tag cadastrada com sucesso!';
                document.getElementById('tag_nfc').value = data.tag_nfc;
                aguardandoTag = false;
                modoRegistro = false;
                setTimeout(() => {
                    tagModal.hide();
                    window.location.reload();
                }, 1500);
            }
        }
    } catch (error) {
        console.error('Erro:', error);
        aguardandoTag = false;
        modoRegistro = false;
        document.getElementById('statusIcon').innerHTML = '<i class="bi bi-x-circle text-danger" style="font-size: 2rem;"></i>';
        document.getElementById('tagMessage').textContent = 'Erro ao verificar status da tag';
        setTimeout(() => tagModal.hide(), 1500);
    }
}

function iniciarVerificacaoTag() {
    verificarNovaTag();
    const intervalo = setInterval(() => {
        if (!aguardandoTag) {
            clearInterval(intervalo);
            return;
        }
        verificarNovaTag();
    }, 2000);

    // Mantendo exatamente o mesmo tempo limite que o ESP32 (30 segundos)
    setTimeout(() => {
        if (aguardandoTag) {
            aguardandoTag = false;
            modoRegistro = false;
            document.getElementById('tagMessage').textContent = 'Tempo limite excedido';
            document.getElementById('statusIcon').innerHTML = '<i class="bi bi-x-circle text-warning" style="font-size: 2rem;"></i>';
            setTimeout(() => tagModal.hide(), 1500);
        }
    }, 30000);
}