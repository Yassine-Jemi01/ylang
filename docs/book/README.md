# The standalone YLang book

`YLang-Book.pdf` is the detailed, beginner-oriented book bundled with the repository. It is deliberately a separate publication from the online reference site: the standalone book teaches the language step by step with explanations, worked examples, troubleshooting, limitations, and exercises.

## Source of truth

The book's own manuscript lives in `docs/book/source/`:

- `part-1-start-here.md`: introduction, installation, first program, and commands.
- `part-2-language-basics.md`: declarations, types, strings, operators, conditions, and loops.
- `part-3-functions-library.md`: functions, arrays, conversions, math, strings, and file I/O.
- `part-4-errors-extensions.md`: diagnostics, runtime errors, a complete example, imports, structs, and enums.
- `part-5-ecosystem-reference.md`: GUI/Game2D, NASM, platforms, YSPM, limitations, quick reference, and exercises.

The online book and technical specifications are useful references, but they are not substituted wholesale for this detailed manuscript. Stable-core behavior and experimental extensions are labelled separately.

## Regenerate the PDF locally

Install Python 3 and ReportLab, then run from the repository root:

```sh
python -m pip install reportlab
python docs/book/build_pdf.py
```

GitHub Actions regenerates `docs/book/YLang-Book.pdf` when the standalone manuscript or its builder changes, checks the PDF output, and commits the refreshed file. The book should be reviewed for teaching clarity as well as factual compatibility whenever the language changes.
