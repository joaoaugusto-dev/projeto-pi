// Password validation
function validatePassword() {
    const senha = document.getElementById('f_senha');
    const confirmarSenha = document.getElementById('confirmasenha');
    const minLength = document.getElementById('minLength');
    const hasUpperCase = document.getElementById('hasUpperCase');
    const hasSymbol = document.getElementById('hasSymbol');
    const passwordMatch = document.getElementById('passwordMatch');

    if (!senha || !confirmarSenha) return;

    const val = senha.value;
    
    if (minLength) minLength.style.color = val.length >= 8 ? 'green' : 'red';
    if (hasUpperCase) hasUpperCase.style.color = /[A-Z]/.test(val) ? 'green' : 'red';
    if (hasSymbol) hasSymbol.style.color = /[\W_]/.test(val) ? 'green' : 'red';
    
    if (confirmarSenha.value && passwordMatch) {
        passwordMatch.textContent = val === confirmarSenha.value ? 
            'Senhas conferem' : 'Senhas não conferem';
        passwordMatch.style.color = val === confirmarSenha.value ? 
            'green' : 'red';
    }
}

// Form submissions
async function handleLoginSubmit(e) {
    e.preventDefault();
    const matricula = document.getElementById('matricula').value;
    const senha = document.getElementById('senha').value;

    try {
        const response = await fetch('/auth/login', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({ matricula, senha })
        });

        if (response.ok) {
            window.location.href = '/';
        } else {
            const data = await response.json();
            document.getElementById('errorMessage').textContent = data.message || 'Erro ao fazer login';
        }
    } catch (error) {
        console.error('Erro:', error);
        document.getElementById('errorMessage').textContent = 'Erro ao fazer login';
    }
}

async function handleRegisterSubmit(e) {
    e.preventDefault();
    const formData = new FormData(e.target);

    try {
        const response = await fetch('/auth/register', {
            method: 'POST',
            body: formData
        });

        const data = await response.json();

        if (response.ok && data.success) {
            window.location.href = '/';
        } else {
            const errorMessage = data.message || 'Erro ao processar o cadastro';
            document.getElementById('errorMessage').textContent = errorMessage;
        }
    } catch (error) {
        console.error('Erro:', error);
        document.getElementById('errorMessage').textContent = 'Erro ao processar o cadastro';
    }
}

async function handleLogout() {
    try {
        const response = await fetch('/auth/logout', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            }
        });

        if (response.ok) {
            window.location.href = '/';
        }
    } catch (error) {
        console.error('Erro:', error);
        document.getElementById('logoutErrorMessage').textContent = 'Erro ao fazer logout';
    }
}

// Setup event listeners
document.addEventListener('DOMContentLoaded', () => {
    const loginForm = document.getElementById('loginForm');
    const registerForm = document.getElementById('registerForm');
    const senha = document.getElementById('f_senha');
    const confirmarSenha = document.getElementById('confirmasenha');
    const confirmLogoutButton = document.getElementById('confirmLogoutButton');

    if (loginForm) {
        loginForm.addEventListener('submit', handleLoginSubmit);
    }

    if (registerForm) {
        registerForm.addEventListener('submit', handleRegisterSubmit);
    }

    if (senha) {
        senha.addEventListener('input', validatePassword);
    }

    if (confirmarSenha) {
        confirmarSenha.addEventListener('input', validatePassword);
    }

    if (confirmLogoutButton) {
        confirmLogoutButton.addEventListener('click', handleLogout);
    }
});