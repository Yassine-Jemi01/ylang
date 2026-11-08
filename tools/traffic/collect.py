#!/usr/bin/env python3
"""Archive GitHub repository traffic before GitHub's 14-day window expires."""

from __future__ import annotations

import json
import os
import sys
import urllib.error
import urllib.request
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[2]
DATA_PATH = ROOT / "website" / "traffic" / "data.json"
API_VERSION = "2022-11-28"


def utc_now() -> datetime:
    return datetime.now(timezone.utc)


def date_key(timestamp: str) -> str:
    """Convert GitHub's ISO-8601 timestamp into a UTC YYYY-MM-DD key."""
    parsed = datetime.fromisoformat(timestamp.replace("Z", "+00:00"))
    if parsed.tzinfo is None:
        parsed = parsed.replace(tzinfo=timezone.utc)
    return parsed.astimezone(timezone.utc).date().isoformat()


def merge_snapshots(
    existing: dict[str, Any],
    clones_payload: dict[str, Any],
    views_payload: dict[str, Any],
    collected_at: str,
) -> dict[str, Any]:
    """Merge the latest rolling-window response without discarding older dates."""
    result = {
        "schema_version": 1,
        "repository": existing.get("repository", ""),
        "first_collected_at": existing.get("first_collected_at") or collected_at,
        "last_updated_at": collected_at,
        "days": dict(existing.get("days", {})),
    }

    for item in clones_payload.get("clones", []):
        key = date_key(item["timestamp"])
        day = dict(result["days"].get(key, {}))
        day["clones"] = int(item.get("count", 0))
        day["unique_cloners"] = int(item.get("uniques", 0))
        result["days"][key] = day

    for item in views_payload.get("views", []):
        key = date_key(item["timestamp"])
        day = dict(result["days"].get(key, {}))
        day["views"] = int(item.get("count", 0))
        day["unique_visitors"] = int(item.get("uniques", 0))
        result["days"][key] = day

    result["days"] = dict(sorted(result["days"].items()))
    return result


def fetch_traffic(repo: str, metric: str, token: str) -> dict[str, Any]:
    url = f"https://api.github.com/repos/{repo}/traffic/{metric}?per=day"
    request = urllib.request.Request(
        url,
        headers={
            "Accept": "application/vnd.github+json",
            "Authorization": f"Bearer {token}",
            "X-GitHub-Api-Version": API_VERSION,
            "User-Agent": "ylang-traffic-archive",
        },
    )
    try:
        with urllib.request.urlopen(request, timeout=30) as response:
            return json.loads(response.read().decode("utf-8"))
    except urllib.error.HTTPError as error:
        details = error.read().decode("utf-8", errors="replace")
        if error.code in (401, 403):
            raise RuntimeError(
                "GitHub rejected the traffic API request. Add a fine-grained "
                "TRAFFIC_TOKEN repository secret with Administration: read "
                "permission for this repository. API response: "
                f"{details}"
            ) from error
        raise RuntimeError(
            f"GitHub traffic API returned HTTP {error.code}: {details}"
        ) from error
    except urllib.error.URLError as error:
        raise RuntimeError(f"Could not reach GitHub traffic API: {error}") from error


def load_existing() -> dict[str, Any]:
    if not DATA_PATH.exists():
        return {"schema_version": 1, "repository": "", "days": {}}
    with DATA_PATH.open(encoding="utf-8") as file:
        data = json.load(file)
    if data.get("schema_version") != 1 or not isinstance(data.get("days"), dict):
        raise RuntimeError(f"Unsupported or invalid traffic archive: {DATA_PATH}")
    return data


def main() -> int:
    repo = os.environ.get("GITHUB_REPOSITORY", "Yassine-Jemi01/ylang")
    token = os.environ.get("TRAFFIC_TOKEN")
    if not token:
        print(
            "TRAFFIC_TOKEN is missing. Add a fine-grained token with "
            "Administration: read permission as a repository Actions secret.",
            file=sys.stderr,
        )
        return 2

    try:
        clones = fetch_traffic(repo, "clones", token)
        views = fetch_traffic(repo, "views", token)
        collected_at = utc_now().isoformat(timespec="seconds").replace("+00:00", "Z")
        archive = merge_snapshots(load_existing(), clones, views, collected_at)
        archive["repository"] = repo

        DATA_PATH.parent.mkdir(parents=True, exist_ok=True)
        temporary_path = DATA_PATH.with_suffix(".json.tmp")
        with temporary_path.open("w", encoding="utf-8", newline="\n") as file:
            json.dump(archive, file, indent=2, sort_keys=True)
            file.write("\n")
        temporary_path.replace(DATA_PATH)

        print(f"Archived {len(archive['days'])} daily traffic records for {repo}.")
        print(f"Updated: {archive['last_updated_at']}")
        return 0
    except (RuntimeError, KeyError, TypeError, ValueError, json.JSONDecodeError) as error:
        print(f"Traffic archive failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
