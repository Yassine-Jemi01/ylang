# YLang traffic archive

GitHub's repository traffic API exposes only a rolling 14-day window. This collector snapshots the daily clone and view buckets into `website/traffic/data.json`, preserving older dates as the rolling window advances.

## One-time setup

1. Create a fine-grained personal access token for **Yassine-Jemi01/ylang**.
2. Grant the token **Repository permissions → Administration: Read-only**. No write permission is needed for this token.
3. Add it to the repository at **Settings → Secrets and variables → Actions** as `TRAFFIC_TOKEN`.
4. Open **Actions → Archive repository traffic → Run workflow** once to seed the archive with the currently available 14-day history.

The workflow uses the token only for read-only traffic API requests. The separate built-in `GITHUB_TOKEN` is used by Actions to commit the JSON archive and requires the workflow's `contents: write` permission.

## Collection and dashboard

- The workflow runs daily at 03:17 UTC and can also be started manually.
- Each run imports all daily buckets still available from GitHub and merges them by UTC date.
- Existing dates are updated when GitHub refreshes a bucket; older dates are never discarded.
- The public dashboard is deployed at [https://yassine-jemi01.github.io/ylang/traffic/](https://yassine-jemi01.github.io/ylang/traffic/). It reads the latest JSON directly from the repository's `main` branch, so daily archive commits update the figures without requiring a website redeployment.
- The website deployment workflow already watches `website/**`, so a new archive commit triggers a Pages deployment.

## Metric definitions

- **Total clones / views:** sum of the archived daily event counts.
- **Daily unique cloners / visitors:** sum of GitHub's daily unique counts. These are not deduplicated across the full archive, so they must not be described as lifetime unique people.
- GitHub traffic metrics can include automated activity and do not prove that each event came from a human.

## Local tests

```sh
python3 tools/traffic/test_collect.py
```
