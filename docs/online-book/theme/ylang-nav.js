/* Shared YLang book navigation with page-aware language switching. */
(() => {
  'use strict';
  const HOME_URL = 'https://yassine-jemi01.github.io/ylang/';
  const BOOK_URL = HOME_URL + 'learn/';
  const AR_TO_EN = {
    'index.html': 'index.html',
    'start-here.html': 'getting-started.html',
    'introduction.html': 'introduction.html',
    'getting-started.html': 'getting-started.html',
    'hello-world.html': 'hello-world.html',
    'compiler-workflow.html': 'compiler-workflow.html',
    'language-basics.html': 'language-basics.html',
    'types.html': 'types.html',
    'modules.html': 'modules.html',
    'standard-library.html': 'standard-library.html',
    'platform-support.html': 'platform-support.html',
    'binary-io.html': 'binary-io.html',
    'voice-audio.html': 'voice-audio.html',
    'tui.html': 'tui.html',
    'gui-game2d.html': 'gui-game2d.html',
    'cli.html': 'reference/CLI.html',
    'json.html': 'reference/JSON.html',
    'http.html': 'reference/HTTP.html',
    'diagnostics.html': 'reference/error-codes.html',
    'security.html': 'reference/SECURITY.html',
    'windows.html': 'reference/WINDOWS.html'
  };
  const EN_TO_AR = {
    'index.html': 'start-here.html',
    'introduction.html': 'introduction.html',
    'getting-started.html': 'getting-started.html',
    'hello-world.html': 'hello-world.html',
    'compiler-workflow.html': 'compiler-workflow.html',
    'language-basics.html': 'language-basics.html',
    'types.html': 'types.html',
    'modules.html': 'modules.html',
    'standard-library.html': 'standard-library.html',
    'platform-support.html': 'platform-support.html',
    'binary-io.html': 'binary-io.html',
    'voice-audio.html': 'voice-audio.html',
    'tui.html': 'tui.html',
    'gui-game2d.html': 'gui-game2d.html',
    'reference/CLI.html': 'cli.html',
    'reference/JSON.html': 'json.html',
    'reference/HTTP.html': 'http.html',
    'reference/error-codes.html': 'diagnostics.html',
    'reference/SECURITY.html': 'security.html',
    'reference/WINDOWS.html': 'windows.html',
    'reference/GAME2D.html': 'gui-game2d.html',
    'reference/GUI-API-SPEC.html': 'gui-game2d.html',
    'reference/MODULE-SYSTEM-SPEC.html': 'modules.html',
    'reference/BACKEND-ROADMAP.html': 'compiler-workflow.html'
  };

  function relativeBookPath() {
    const marker = '/learn/';
    const index = window.location.pathname.indexOf(marker);
    if (index < 0) return 'index.html';
    let path = decodeURIComponent(window.location.pathname.slice(index + marker.length));
    if (!path || path.endsWith('/')) path += 'index.html';
    return path;
  }

  function languageTarget(isArabic) {
    const path = relativeBookPath();
    if (isArabic) {
      const slug = path.startsWith('ar/') ? path.slice(3) : path;
      return BOOK_URL + (AR_TO_EN[slug] || 'index.html');
    }
    if (path.startsWith('ar/')) return BOOK_URL + path;
    return BOOK_URL + 'ar/' + (EN_TO_AR[path] || 'start-here.html');
  }

  function addSharedTheme() {
    if (document.querySelector('link[data-ylang-shared-theme]')) return;
    const link = document.createElement('link');
    link.rel = 'stylesheet';
    link.href = HOME_URL + 'assets/site-theme.css';
    link.setAttribute('data-ylang-shared-theme', 'true');
    document.head.append(link);
  }

  function setBookLanguage() {
    const isArabic = relativeBookPath().startsWith('ar/');
    document.documentElement.lang = isArabic ? 'ar' : 'en';
    document.documentElement.dir = isArabic ? 'rtl' : 'ltr';
    if (isArabic) document.body.classList.add('ylang-ar');
  }

  function addLanguageSwitcher() {
    if (document.querySelector('.language-switcher')) return;
    const toolbar = document.querySelector('#menu-bar .left-buttons') || document.querySelector('#menu-bar');
    if (!toolbar) return;
    const isArabic = relativeBookPath().startsWith('ar/');
    const details = document.createElement('details');
    details.className = 'language-switcher';
    const summary = document.createElement('summary');
    summary.textContent = isArabic ? 'AR' : 'EN';
    summary.setAttribute('aria-label', 'Select book language');
    const menu = document.createElement('div');
    menu.className = 'language-menu';
    const choices = [
      { label: 'English (EN)', href: languageTarget(true), lang: 'en', current: !isArabic },
      { label: 'العربية (AR)', href: languageTarget(false), lang: 'ar', current: isArabic }
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
    const mark = document.createElement('img');
    mark.src = HOME_URL + 'ylang-mark.svg';
    mark.alt = '';
    mark.width = 24;
    mark.height = 24;
    const label = document.createElement('span');
    label.className = 'ylang-home-label';
    label.textContent = 'YLang';
    link.append(mark, label);
    toolbar.prepend(link);
  }

  function initialize() {
    addSharedTheme();
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
