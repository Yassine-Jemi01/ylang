# YLang website

A small, responsive landing page and documentation index built with plain HTML, CSS, and JavaScript. No framework or build step is required.

## Design

The design uses a clear blue-and-yellow palette inspired by the way Python presents its community language project, without copying its logo or page layout. Dark blue is reserved for the navigation, code examples, and footer; the reading areas stay light blue-gray for contrast.

## Files

- `index.html` — concise overview, feature links, compiler flow, and quick start.
- `docs.html` — stable language references separated from experimental 2.0 guides.
- `styles.css` and `docs.css` — shared responsive styles and documentation layouts.
- `script.js` — mobile navigation, copy-to-clipboard controls, and current footer year.
- `assets/` — YLang mark and favicon.
- `YLang-Book.pdf` — copied into the Pages artifact from `docs/book/YLang-Book.pdf` during deployment.

## Preview

Open `index.html` in a browser or serve the repository root with any static file server. No package installation is needed.

## Publishing

Changes to `website/**` and `docs/book/**` on `main` trigger the GitHub Pages workflow. The beta publishing workflow is separate and runs the test matrix before creating a prerelease.
