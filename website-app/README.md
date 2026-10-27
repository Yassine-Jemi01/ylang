# YLang website refresh

React/Vite homepage refresh for YLang, with a restrained dark-teal and rust palette. This revision removes the unreliable ASCII background entirely, keeps the moving curved-text marquee, and points Book and Documentation links at the published GitHub Pages pages so they work in local preview as well. The artificial terminal/code-card remains removed from the hero. Curved text uses `requestAnimationFrame`.

## Stack

- React + Vite.
- `LogoLoop`, a tailored marquee component based on the supplied React Bits API.
- `TextLoop`, a compact SVG path-text animation driven by `requestAnimationFrame`.
- React Icons and Radix UI for platform icons, menu and install tabs.

## Run locally

Requires Node.js 22.12 or newer.

```sh
cd website-app
npm install
npm run dev
```

Open the local URL printed by Vite. Do not open `index.html` directly as a file.

Production build:

```sh
npm run build
npm run preview
```

The output is written to `dist/`. In the YLang repository, the deploy workflow preserves the existing online book and reference page, then overlays the built homepage.

## Integration

From the extracted package root, run:

```sh
./integrate.sh /path/to/ylang
```

The script copies `website-app/` into the target repository and backs up the existing Pages workflow before replacing it. Review the generated changes before committing.

## Claims and release status

The homepage describes YLang 2.2.0 as the current stable release and the C backend as the default workflow. The NASM backend, GUI/Game2D APIs, and native Windows path remain labelled experimental.
