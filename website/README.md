# YLang website

A small landing page plus a multi-page online programming book. The marketing pages use plain HTML/CSS/JavaScript; the reader-facing book is generated as static HTML by mdBook.

## Design

The design uses a clear blue-and-yellow palette inspired by the way Python presents its community language project, without copying its logo or page layout. Dark blue is reserved for the navigation, code examples, and footer; the reading areas stay light blue-gray for contrast.

## Files

- `index.html` — concise overview, feature links, compiler flow, and quick start.
- `docs.html` — entry point for the online book, PDF edition, and stable/beta reference pages.
- `learn/` — generated mdBook HTML output, including the chapter sidebar, search, theme selector, and navigation.
- `../docs/online-book/` — mdBook configuration and tutorial source.
- `../tools/build_docs_book.py` — syncs canonical `docs/*.md` reference files into the book before a build.
- `styles.css` and `docs.css` — shared responsive styles and documentation layouts.
- `script.js` — mobile navigation, copy-to-clipboard controls, and current footer year.
- `assets/` — YLang mark and favicon.
- `YLang-Book.pdf` — copied into the Pages artifact from `docs/book/YLang-Book.pdf` during deployment.

## Preview

Open `index.html` in a browser or serve the repository root with any static file server. No package installation is needed.

## Publishing

Changes to `website/**`, `docs/**`, or the book builder on `main` trigger GitHub Pages to rebuild the HTML book and deploy the full website. The beta publishing workflow is separate and runs the test matrix before creating a prerelease.
