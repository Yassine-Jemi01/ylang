# YLang 2.1.0 Post-release Verification

Use this checklist only **after** the stable publishing workflow has completed successfully. It is intentionally separate from the pre-publication acceptance checklist so post-release tasks cannot block the workflow that creates the release.

- [ ] Confirm the published release is [v2.1.0](https://github.com/Yassine-Jemi01/ylang/releases/tag/v2.1.0) and points to the exact commit tested by the publishing workflow.
- [ ] Confirm the release notes are taken from `docs/CHANGELOG-2.1.0.md` and identify ownership as experimental.
- [ ] Confirm the expected `ylang-2.1.0-x86_64.yspkg` asset is attached and its download completes.
- [ ] Verify the release's installation instructions and checksum/asset metadata, where provided.
- [ ] Confirm the online book and website expose the correct 2.1.0 release notes without changing the documented 2.0.0 stable-core compatibility contract.
- [ ] Record the workflow run URL and final published commit in the release announcement or project changelog.

Do not mark any item complete until it has been checked against the actual published release.
