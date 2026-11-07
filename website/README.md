# YLang website

The public website combines a React/Vite homepage, bilingual documentation portals, and two separately generated mdBook editions.

## Sources

- `website-app/` — the English React homepage, its logos, and the production Vite build.
- `website/docs.html` — the English documentation portal.
- `website/docs-ar.html` — the Arabic documentation portal, with RTL layout and links into the Arabic learning path.
- `docs/online-book/src/` — the English book chapters and technical references.
- `docs/online-book/src/ar/` — the canonical Arabic learning-path chapters.
- `docs/online-book/theme/` — shared book appearance, navigation, and favicon.
- `tools/build_docs_book.py` — synchronizes canonical technical references and prepares a separate normalized Arabic build source without changing the original Arabic chapters.
- `tools/build_website.py` — builds both book editions, assembles the complete Pages output, generates the sitemap, and runs integrity checks.
- `tools/check_website_build.py` — validates the assembled production output, including local HTML/CSS references and chapter-language routes.

Generated book output is not committed. The online book is the canonical learning edition; the former standalone PDF and the retired YLang 3.0 progress page are not published.

## Build a local production copy

Build the homepage first:

```sh
npm ci --prefix website-app
npm run build --prefix website-app
```

Install mdBook 0.5.4, then assemble and verify the complete site:

```sh
python3 tools/build_website.py site-dist
```

The resulting `site-dist/` directory is the same directory structure uploaded to GitHub Pages.

## Publishing

CI and the GitHub Pages workflows use the same `tools/build_website.py` pipeline. Changes to the homepage, documentation, book sources, shared theme, or website build/check scripts are validated before deployment.
