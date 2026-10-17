# YLang website

A lightweight, responsive landing page built with HTML, CSS, and vanilla JavaScript.

## Brand assets and book

The site uses the official YLang logo files from `assets/branding/`. The online guide is `docs.html`; during deployment, the workflow copies `docs/book/YLang-Book.pdf` to the Pages artifact as `YLang-Book.pdf`, so the book can be opened directly from the website. Changes to `docs/book/**` also trigger a redeployment.

## Preview locally

Open `index.html` in a browser, or serve this directory with any static file server. No npm install or build step is required.

## Publishing

The site lives in `website/` and is deployed with the `.github/workflows/deploy-website.yml` workflow when website files or the book change on `main`. GitHub Pages should use **GitHub Actions** as its deployment source.

The downloads section intentionally links to the GitHub Releases page and explains that prebuilt installers are not published yet. Replace that notice only after release artifacts and checksums exist.
