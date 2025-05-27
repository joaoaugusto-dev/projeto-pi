// Theme management
let currentTheme = localStorage.getItem('theme') || 'light';

function setTheme(theme) {
    currentTheme = theme;
    document.body.setAttribute('data-theme', theme);
    localStorage.setItem('theme', theme);
    
    // Update active buttons
    document.getElementById('lightThemeBtn')?.classList.toggle('active', theme === 'light');
    document.getElementById('darkThemeBtn')?.classList.toggle('active', theme === 'dark');
    document.getElementById('contrastThemeBtn')?.classList.toggle('active', theme === 'high-contrast');
}

// Font size management
let currentFontSize = parseInt(localStorage.getItem('fontSize')) || 16;
const minFontSize = 14;
const maxFontSize = 20;

function updateFontSize() {
    document.body.style.fontSize = currentFontSize + 'px';
    localStorage.setItem('fontSize', currentFontSize);
    
    const decreaseBtn = document.getElementById('decreaseFontBtn');
    const increaseBtn = document.getElementById('increaseFontBtn');
    
    if (decreaseBtn) decreaseBtn.disabled = currentFontSize <= minFontSize;
    if (increaseBtn) increaseBtn.disabled = currentFontSize >= maxFontSize;
}

function increaseFont() {
    if (currentFontSize < maxFontSize) {
        currentFontSize += 2;
        updateFontSize();
    }
}

function decreaseFont() {
    if (currentFontSize > minFontSize) {
        currentFontSize -= 2;
        updateFontSize();
    }
}

// Initialize on page load
document.addEventListener('DOMContentLoaded', () => {
    updateFontSize();
    setTheme(currentTheme);
});