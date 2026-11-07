/* Add the homepage link and set the correct reading direction for Arabic chapters. */
(() => {
  'use strict';
  const HOME_URL = 'https://yassine-jemi01.github.io/ylang/';
  const BOOK_URL = HOME_URL + 'learn/';

  function setBookLanguage() {
    const isArabic = /\/ar(?:\/|$)/.test(window.location.pathname);
    if (isArabic) {
      document.documentElement.lang = 'ar';
      document.documentElement.dir = 'rtl';
      document.body.classList.add('ylang-ar');
    }
  }

  function addLanguageSwitcher() {
    if (document.querySelector('.ylang-language-switcher')) return;
    const toolbar = document.querySelector('#menu-bar .left-buttons') || document.querySelector('#menu-bar');
    if (!toolbar) return;
    const isArabic = document.documentElement.lang === 'ar';
    const details = document.createElement('details');
    details.className = 'ylang-language-switcher';
    const summary = document.createElement('summary');
    summary.textContent = isArabic ? 'AR' : 'EN';
    summary.setAttribute('aria-label', 'Select book language');
    const menu = document.createElement('div');
    menu.className = 'ylang-language-menu';
    const choices = [
      { label: 'English (EN)', href: BOOK_URL, lang: 'en', current: !isArabic },
      { label: 'العربية (AR)', href: BOOK_URL + 'ar/start-here.html', lang: 'ar', current: isArabic }
    ];
    for (const choice of choices) {
      const link = document.createElement('a');
      link.href = choice.href;
      link.lang = choice.lang;
      link.hreflang = choice.lang;
      link.textContent = choice.label;
      if (choice.current) link.setAttribute('aria-current', 'page');
      menu.append(link);
    }
    details.append(summary, menu);
    toolbar.append(details);
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
    addLanguageSwitcher();
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', initialize, { once: true });
  } else {
    initialize();
  }
})();
