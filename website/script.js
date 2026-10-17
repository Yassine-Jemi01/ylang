const menuButton = document.querySelector(".menu-toggle");
const navigation = document.querySelector("#site-nav");

if (menuButton && navigation) {
  const closeMenu = () => {
    menuButton.setAttribute("aria-expanded", "false");
    menuButton.setAttribute("aria-label", "Open navigation");
    navigation.classList.remove("is-open");
  };

  menuButton.addEventListener("click", () => {
    const isOpen = menuButton.getAttribute("aria-expanded") === "true";
    menuButton.setAttribute("aria-expanded", String(!isOpen));
    menuButton.setAttribute("aria-label", isOpen ? "Open navigation" : "Close navigation");
    navigation.classList.toggle("is-open", !isOpen);
  });

  navigation.querySelectorAll("a").forEach((link) => {
    link.addEventListener("click", closeMenu);
  });

  document.addEventListener("keydown", (event) => {
    if (event.key === "Escape") closeMenu();
  });

  window.addEventListener("resize", () => {
    if (window.matchMedia("(min-width: 721px)").matches) closeMenu();
  });
}

document.querySelectorAll("[data-copy]").forEach((button) => {
  const defaultLabel = button.textContent.trim();

  button.addEventListener("click", async () => {
    const target = document.getElementById(button.dataset.copy);
    if (!target) return;

    try {
      await navigator.clipboard.writeText(target.innerText);
      button.textContent = "Copied!";
      button.setAttribute("aria-label", "Code copied to clipboard");
    } catch {
      button.textContent = "Select code to copy";
      button.setAttribute("aria-label", "Clipboard access unavailable; select the code manually");
    }

    window.setTimeout(() => {
      button.textContent = defaultLabel;
      button.removeAttribute("aria-label");
    }, 1800);
  });
});

const year = document.querySelector("#year");
if (year) year.textContent = String(new Date().getFullYear());
