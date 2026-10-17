/* Small progressive enhancements: mobile navigation, copy buttons, and footer year. */
(() => {
  "use strict";

  const $ = (selector, scope = document) => scope.querySelector(selector);
  const menuButton = $(".menu-toggle");
  const nav = $("#site-nav");

  function setMenu(open) {
    if (!menuButton || !nav) return;
    menuButton.setAttribute("aria-expanded", String(open));
    menuButton.setAttribute("aria-label", open ? "Close navigation" : "Open navigation");
    nav.classList.toggle("is-open", open);
  }

  if (menuButton && nav) {
    menuButton.addEventListener("click", () => {
      const isOpen = menuButton.getAttribute("aria-expanded") === "true";
      setMenu(!isOpen);
    });
    nav.addEventListener("click", (event) => {
      if (event.target.closest("a")) setMenu(false);
    });
    document.addEventListener("keydown", (event) => {
      if (event.key === "Escape") setMenu(false);
    });
    window.matchMedia("(min-width: 781px)").addEventListener("change", (event) => {
      if (event.matches) setMenu(false);
    });
  }

  document.querySelectorAll(".copy-button").forEach((button) => {
    let resetTimer;
    button.addEventListener("click", async () => {
      const sourceId = button.dataset.copy;
      const source = sourceId ? document.getElementById(sourceId) : null;
      const text = source ? source.innerText : "";
      if (!text) return;

      const originalLabel = button.dataset.originalLabel || button.textContent.trim();
      button.dataset.originalLabel = originalLabel;
      let message = "Copied";
      try {
        await navigator.clipboard.writeText(text);
      } catch {
        message = "Select text to copy";
      }
      button.textContent = message;
      button.classList.add("copy-feedback");
      window.clearTimeout(resetTimer);
      resetTimer = window.setTimeout(() => {
        button.textContent = originalLabel;
        button.classList.remove("copy-feedback");
      }, 1800);
    });
  });

  const year = $("#year");
  if (year) year.textContent = String(new Date().getFullYear());
})();
