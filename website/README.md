# YLang website

A lightweight, responsive landing page built with HTML, CSS, and vanilla JavaScript.

## Brand assets and book

The site uses the official YLang logo files from `assets/branding/` (the full wordmark and the logo icon). The learning section and navigation link to the existing language book at `docs/book/YLang-Book.pdf`.

## Preview locally

Open `index.html` in a browser, or serve this directory with any static file server. No npm install or build step is required.

## Publishing

The site lives in `website/` and is deployed with the `.github/workflows/deploy-website.yml` workflow when website files change on `main`. GitHub Pages should use **GitHub Actions** as its deployment source.

The downloads section intentionally links to the GitHub Releases page and explains that prebuilt installers are not published yet. Replace that notice only after release artifacts and checksums exist.
