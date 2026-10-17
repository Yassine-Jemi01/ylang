# YLang website

A responsive, English-only landing page and documentation index built with plain HTML, CSS, and JavaScript. There is no frontend build step or JavaScript framework.

## Design

The website uses a compact, responsive blue-and-gold palette with a slightly tinted blue-gray background, readable sans-serif headings, simple rounded cards, and high-contrast code samples. It takes general usability cues from developer-focused sites such as Python.org without reusing their logo or page design. The homepage separates the stable 1.1.0 release from the experimental 2.0.0-beta.1 preview.

## Files

- `index.html` — project landing page, compilation pipeline, feature overview, release links, and quick start.
- `docs.html` — documentation index separating stable references from beta-only feature guides.
- `styles.css` and `docs.css` — shared visual system and documentation-page layouts.
- `script.js` — mobile navigation, copy-to-clipboard controls, and the footer year.
- `assets/` — project logo and favicon.
- `YLang-Book.pdf` — copied into the Pages artifact during deployment.

## Preview locally

Open `index.html` in a browser, or serve the repository root with any static file server. No package installation is required. The website deployment workflow copies `docs/book/YLang-Book.pdf` to the artifact as `YLang-Book.pdf`.

## Publishing

Changes to `website/**` and `docs/book/**` on `main` trigger the GitHub Pages deployment workflow. GitHub Pages should use **GitHub Actions** as its deployment source.

The beta release workflow is separate from website deployment: it runs the full test matrix before creating `v2.0.0-beta.1`. The source-only prerelease does not include compiled binaries or installers.
