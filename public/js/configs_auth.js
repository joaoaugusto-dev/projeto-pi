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

async function handleLogout() {
  try {
    const response = await fetch('/logout', {
      method: 'POST',
      credentials: 'include',
      headers: { 'Content-Type': 'application/json' }
    });

    if (response.ok) {
      console.log('Logout realizado com sucesso');
      window.location.href = '/inicio'; 
    } else {
      const err = await response.json();
      console.error('Falha no logout:', err.message);
      document.getElementById('logoutErrorMessage').textContent = err.message;
    }
  } catch (error) {
    console.error('Erro:', error);
    document.getElementById('logoutErrorMessage').textContent = 'Erro ao fazer logout';
  }
}

// Setup event listeners
document.addEventListener('DOMContentLoaded', () => {
    const loginForm = document.getElementById('loginForm');
    const senha = document.getElementById('f_senha');
    const confirmarSenha = document.getElementById('confirmasenha');
    const confirmLogoutButton = document.getElementById('confirmLogoutButton');

    if (loginForm) {
        loginForm.addEventListener('submit', handleLoginSubmit);
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