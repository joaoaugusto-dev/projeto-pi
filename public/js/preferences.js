function atualizarDadosAmbiente() {
    fetch('/esp32/ambiente')
        .then(response => response.json())
        .then(data => {
            const tempSpan = document.getElementById('temp_atual');
            const humidSpan = document.getElementById('humid_atual');
            const lumiSpan = document.getElementById('lumi_atual');
            const lumiMediaSpan = document.getElementById('lumi_media');
            
            if (tempSpan) {
                tempSpan.textContent = data.atualizado ? data.temperatura : '--';
                tempSpan.style.color = data.atualizado ? 'inherit' : '#999';
            }
            if (humidSpan) {
                humidSpan.textContent = data.atualizado ? data.humidade : '--';
                humidSpan.style.color = data.atualizado ? 'inherit' : '#999';
            }
            if (lumiSpan) {
                lumiSpan.textContent = data.luminosidade || '--';
            }
            if (lumiMediaSpan) {
                lumiMediaSpan.textContent = data.luminosidade_media || '--';
            }
        })
        .catch(error => console.error('Erro ao atualizar dados:', error));
}

document.addEventListener('DOMContentLoaded', function() {
    atualizarDadosAmbiente();
    setInterval(atualizarDadosAmbiente, 5000);
    setupPreferenceFormListeners();
});

function setupPreferenceFormListeners() {
    const tempSlider = document.getElementById('tempRange');
    const lumiSlider = document.getElementById('lumiRange');
    const preferencesForm = document.getElementById('preferencesForm');

    if (tempSlider) {
        tempSlider.addEventListener('input', function() {
            document.getElementById('tempValue').textContent = this.value + '°C';
        });
    }

    if (lumiSlider) {
        lumiSlider.addEventListener('input', function() {
            document.getElementById('lumiValue').textContent = this.value + ' %';
        });
    }

    if (preferencesForm) {
        preferencesForm.addEventListener('submit', handlePreferenceSubmit);
    }
}

async function handlePreferenceSubmit(e) {
    e.preventDefault();
    
    const temperatura = document.getElementById('tempRange').value;
    const luminosidade = document.getElementById('lumiRange').value;

    try {
        const response = await fetch('/preferencias/atualizar', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                temp_preferida: parseFloat(temperatura),
                lumi_preferida: parseInt(luminosidade)
            })
        });

        const data = await response.json();
        
        if (!response.ok) {
            throw new Error(data.message answering: 'Erro ao atualizar preferências');
        }

        if (data.success) {
            setTimeout(() => window.location.href = '/dados', 1500);
        } else {
            throw new Error(data.message || 'Erro ao atualizar preferências');
        }
    } catch (error) {
        console.error('Erro:', error);
        alert('Erro ao atualizar preferências');
    }
}