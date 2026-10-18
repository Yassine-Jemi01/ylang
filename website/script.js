/* Small enhancements: mobile navigation, copy buttons, and footer year. */
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
      setMenu(menuButton.getAttribute("aria-expanded") !== "true");
    });
    nav.addEventListener("click", (event) => {
      if (event.target instanceof Element && event.target.closest("a")) setMenu(false);
    });
    document.addEventListener("keydown", (event) => {
      if (event.key === "Escape") setMenu(false);
    });
    document.addEventListener("click", (event) => {
      if (menuButton.getAttribute("aria-expanded") !== "true") return;
      if (event.target instanceof Node && !menuButton.contains(event.target) && !nav.contains(event.target)) {
        setMenu(false);
      }
    });

    const media = window.matchMedia("(min-width: 701px)");
    const onMediaChange = (event) => { if (event.matches) setMenu(false); };
    if (typeof media.addEventListener === "function") media.addEventListener("change", onMediaChange);
    else if (typeof media.addListener === "function") media.addListener(onMediaChange);
  }

  async function copyText(text) {
    if (navigator.clipboard && window.isSecureContext) {
      await navigator.clipboard.writeText(text);
      return;
    }
    const field = document.createElement("textarea");
    field.value = text;
    field.setAttribute("readonly", "");
    field.style.position = "fixed";
    field.style.opacity = "0";
    field.style.pointerEvents = "none";
    document.body.appendChild(field);
    field.select();
    let copied = false;
    try { copied = document.execCommand("copy"); }
    finally { field.remove(); }
    if (!copied) throw new Error("Clipboard unavailable");
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
      try { await copyText(text); }
      catch { message = "Copy failed"; }
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
