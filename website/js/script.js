// WeatherPaper Website Interactive Features
document.addEventListener('DOMContentLoaded', () => {
  // 1. App Showcase Mockup Tab Switcher
  const showcaseTabs = document.querySelectorAll('.showcase-tab');
  const showcaseImg = document.getElementById('showcase-image');

  const screenshots = {
    general: 'images/tab_fixed_general.png',
    themes: 'images/tab_fixed_themes.png',
    gallery: 'images/tab_fixed_gallery.png',
    performance: 'images/tab_fixed_performance.png',
    about: 'images/tab_fixed_about.png'
  };

  showcaseTabs.forEach(tab => {
    tab.addEventListener('click', () => {
      showcaseTabs.forEach(t => t.classList.remove('active'));
      tab.classList.add('active');

      const targetKey = tab.dataset.target;
      if (screenshots[targetKey] && showcaseImg) {
        showcaseImg.style.opacity = '0';
        setTimeout(() => {
          showcaseImg.src = screenshots[targetKey];
          showcaseImg.alt = `WeatherPaper ${tab.textContent.trim()} Screen`;
          showcaseImg.style.opacity = '1';
        }, 150);
      }
    });
  });

  // 2. OS Download Platform Switcher (Linux vs Windows)
  const osToggleBtns = document.querySelectorAll('.os-toggle-btn');
  const linuxPanel = document.getElementById('linux-install-panel');
  const windowsPanel = document.getElementById('windows-install-panel');

  osToggleBtns.forEach(btn => {
    btn.addEventListener('click', () => {
      osToggleBtns.forEach(b => b.classList.remove('active'));
      btn.classList.add('active');

      const os = btn.dataset.os;
      if (os === 'linux') {
        if (linuxPanel) linuxPanel.style.display = 'grid';
        if (windowsPanel) windowsPanel.style.display = 'none';
      } else {
        if (linuxPanel) linuxPanel.style.display = 'none';
        if (windowsPanel) windowsPanel.style.display = 'grid';
      }
    });
  });

  // 3. Auto-detect Visitor OS
  const userAgent = window.navigator.userAgent.toLowerCase();
  const heroDownloadBtn = document.getElementById('hero-download-btn');
  const heroDownloadMeta = document.getElementById('hero-download-meta');

  if (userAgent.includes('win')) {
    // Visitor is on Windows
    const winBtn = document.querySelector('.os-toggle-btn[data-os="windows"]');
    if (winBtn) winBtn.click();
    if (heroDownloadBtn) {
      heroDownloadBtn.href = 'https://github.com/Anas-Gazi/WeatherPaper/releases/tag/v0.1.0';
      heroDownloadBtn.innerHTML = `
        <svg width="18" height="18" viewBox="0 0 24 24" fill="currentColor">
          <path d="M0 3.449L9.75 2.1v9.451H0m10.949-9.602L24 0v11.4H10.949M0 12.6h9.75v9.451L0 20.699M10.949 12.6H24V24l-13.051-1.802"/>
        </svg>
        Download for Windows
      `;
    }
    if (heroDownloadMeta) {
      heroDownloadMeta.textContent = 'v0.1.0 • Windows 10 & 11 • Portable & Setup';
    }
  } else {
    // Visitor is on Linux / Default
    if (heroDownloadBtn) {
      heroDownloadBtn.href = 'https://github.com/Anas-Gazi/WeatherPaper/releases/download/v0.1.0/weatherpaper-installer.run';
    }
  }

  // 4. Copy-to-Clipboard Helpers
  const copyButtons = document.querySelectorAll('.copy-btn');
  const toast = document.getElementById('toast');

  function showToast(message = 'Copied to clipboard!') {
    if (!toast) return;
    toast.textContent = `✓ ${message}`;
    toast.classList.add('show');
    setTimeout(() => {
      toast.classList.remove('show');
    }, 2500);
  }

  copyButtons.forEach(btn => {
    btn.addEventListener('click', (e) => {
      const codeSnippet = btn.dataset.code || btn.previousElementSibling?.textContent;
      if (codeSnippet) {
        navigator.clipboard.writeText(codeSnippet.trim()).then(() => {
          showToast('Copied to clipboard!');
        }).catch(() => {
          showToast('Failed to copy');
        });
      }
    });
  });

  // 5. FAQ Accordion
  const faqItems = document.querySelectorAll('.faq-item');
  faqItems.forEach(item => {
    const questionBtn = item.querySelector('.faq-question');
    if (questionBtn) {
      questionBtn.addEventListener('click', () => {
        const isActive = item.classList.contains('active');
        // Close others
        faqItems.forEach(other => other.classList.remove('active'));
        // Toggle current
        if (!isActive) {
          item.classList.add('active');
        }
      });
    }
  });
});
