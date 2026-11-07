# YLang 2.3.0 — Post-Release Checklist

**Purpose:** Verify the published artifacts and public documentation after the stable publishing workflow completes. This checklist remains intentionally unchecked during development; do not mark it complete before the release exists. These checks must not be used as substitutes for the pre-publication gates in [RELEASE-CHECKLIST-2.3.0.md](RELEASE-CHECKLIST-2.3.0.md).

- [ ] Confirm GitHub Release `v2.3.0` exists and is marked as the latest stable release.
- [ ] Confirm the release tag points to the exact commit tested by the stable publishing workflow.
- [ ] Confirm the release notes contain the final `docs/CHANGELOG-2.3.0.md` content, without development-only language.
- [ ] Download the YSPM package and verify its checksum and archive contents.
- [ ] Install the published YSPM package in a clean isolated environment and run `ylang version`, `ylang help`, and a small native build.
- [ ] Remove the installed package and confirm its files are removed without touching unrelated user data.
- [ ] Confirm the online book, documentation landing page, and homepage identify 2.3.0 as stable and no longer label the current release as 2.2.0.
- [ ] Confirm the website's stable installation instructions and release links point to the new release.
- [ ] Verify the published source archive and package are available and downloadable.
- [ ] Record the release workflow run, tag SHA, package SHA-256, and any post-release corrections.

Do not rewrite a published tag or silently replace an artifact. If an asset or documentation problem is found, record it and publish a corrective patch release through the normal release process.
