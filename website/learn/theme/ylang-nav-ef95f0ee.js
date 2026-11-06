/* Add a reliable link back to the current YLang React homepage from every mdBook page. */
(() => {
  'use strict';
  const HOME_URL = 'https://yassine-jemi01.github.io/ylang/';

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

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', addHomeLink, { once: true });
  } else {
    addHomeLink();
  }
})();
