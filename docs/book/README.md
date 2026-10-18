# Standalone YLang book

`YLang-Book.pdf` is the downloadable book bundled with the repository. Its content is generated from the repository's Markdown learning chapters and technical references, including the 2.0.0 stable-core specification and release notes. This PDF is separate from the website build, although both draw from the repository's documented language behavior.

Regenerate locally from the repository root with Python 3 and ReportLab:

```sh
python -m pip install reportlab
python docs/book/build_pdf.py
```

When a source chapter or reference changes on `main`, the GitHub Actions workflow rebuilds and commits the refreshed PDF. Experimental features remain labeled as experimental in the references.
