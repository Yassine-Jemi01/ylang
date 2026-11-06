/* Add the homepage link and set the correct reading direction for Arabic chapters. */
(() => {
  'use strict';
  const HOME_URL = 'https://yassine-jemi01.github.io/ylang/';

  function setBookLanguage() {
    const isArabic = /\/ar(?:\/|$)/.test(window.location.pathname);
    if (isArabic) {
      document.documentElement.lang = 'ar';
      document.documentElement.dir = 'rtl';
      document.body.classList.add('ylang-ar');
    }
  }

  function addHomeLink() {
    if (document.querySelector('.ylang-home-link')) return;
    const toolbar = document.querySelector('#menu-bar .left-buttons') || document.querySelector('#menu-bar');
    if (!toolbar) return;

    const link = document.createElement('a');
    link.className = 'ylang-home-link';
    link.href = HOME_URL;
    link.setAttribute('aria-label', 'YLang homepage');
    link.title = 'YLang homepage';

    const mark = document.createElement('span');
    mark.className = 'ylang-home-mark';
    mark.setAttribute('aria-hidden', 'true');
    mark.textContent = 'Y';
    const label = document.createElement('span');
    label.className = 'ylang-home-label';
    label.textContent = 'YLang home';
    link.append(mark, label);
    toolbar.prepend(link);
  }

  function initialize() {
    setBookLanguage();
    addHomeLink();
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', initialize, { once: true });
  } else {
    initialize();
  }
})();
