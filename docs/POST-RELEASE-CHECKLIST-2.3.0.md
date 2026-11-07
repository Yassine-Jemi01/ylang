# YLang 2.3.0 — Post-Release Checklist

**Audit status: 2026-11-07.** Completed items below are backed by the exact stable-release workflow, the release API record, or direct inspection of the published package. Unverified items remain unchecked; this checklist is not a substitute for the pre-publication gates in [RELEASE-CHECKLIST-2.3.0.md](RELEASE-CHECKLIST-2.3.0.md).

## Verified release and package

- [x] Confirm GitHub Release `v2.3.0` exists, is not a draft or prerelease, and is marked as the latest stable release.
- [x] Confirm tag `v2.3.0` points to the exact release-authorization commit tested by the stable publishing workflow: `31e30605a08b068cf4c3a81256464c3a72666112`.
- [ ] Resolve the release-documentation mismatch through a safe release-maintenance path. The published GitHub Release description and the documentation embedded in the original `v2.3.0` YSPM asset still contain pre-audit validation statements and older "unreleased 2.3" wording. The repository and online-book copies are corrected on `main`; do not rewrite the tag or silently replace the existing release asset.
- [x] Download the published YSPM package artifact, inspect its archive and metadata, and verify its SHA-256 against the GitHub Release asset digest.
- [x] Confirm package metadata: version `2.3.0`, architecture `x86_64`, ABI `yspm-abi-1`; published file size is 588,979 bytes.
- [x] Run the package's compiler directly: `--version`, `--help`, source check, native build, and execution of a smoke program all pass.
- [x] Confirm the stable publishing workflow installed the package into an isolated temporary root, checked the packaged compiler version, built and ran the sample program, and removed the package successfully.

## Public documentation and remaining distribution checks

- [x] Confirm the online book, English and Arabic documentation portals, and English and Arabic homepages identify 2.3.0 as stable.
- [x] Confirm public release-note links point to tag `v2.3.0`; add a direct download link for the Linux x86-64 YSPM asset to the YSPM installation guide and synchronize the online-book copy.
- [x] Record the stable publishing workflow [run #8](https://github.com/Yassine-Jemi01/ylang/actions/runs/42386464514), CI [run #5160](https://github.com/Yassine-Jemi01/ylang/actions/runs/42386464522), tag SHA, and published package digest below.
- [x] Correct the stale 2.3.0 validation section in the repository changelog and online book using the exact release evidence; separate the final release benchmark from earlier development-branch measurements.
- [ ] Verify the automatically generated GitHub source archive for tag `v2.3.0` is downloadable and usable.
- [ ] Confirm the published GitHub Release description is synchronized with the corrected repository and online-book notes.

## Recorded release evidence

- Release: [YLang 2.3.0](https://github.com/Yassine-Jemi01/ylang/releases/tag/v2.3.0), published 2026-11-07.
- Stable publishing workflow: [run #8](https://github.com/Yassine-Jemi01/ylang/actions/runs/42386464514), success.
- Exact-commit CI: [run #5160](https://github.com/Yassine-Jemi01/ylang/actions/runs/42386464522), success.
- Tag target / authorization SHA: `31e30605a08b068cf4c3a81256464c3a72666112`.
- Asset: `ylang-2.3.0-x86_64.yspkg`, 588,979 bytes.
- SHA-256: `5d088b62c32bb17e8fc691573244e7d6830dad27ff47541965f40b29bfc57fe4` (matches GitHub Release asset digest).
- Final paired benchmark artifact: [stable publishing workflow #8](https://github.com/Yassine-Jemi01/ylang/actions/runs/42386464514); analysis is in [PERFORMANCE.md](PERFORMANCE.md).

Do not rewrite a published tag or silently replace an artifact. If the source archive or release description needs correction, document it and use the normal release-maintenance process.
