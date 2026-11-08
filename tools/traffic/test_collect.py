#!/usr/bin/env python3
"""Unit tests for the YLang GitHub traffic archive."""

import unittest

from collect import date_key, merge_snapshots


class TrafficArchiveTests(unittest.TestCase):
    def test_date_key_normalises_to_utc(self):
        self.assertEqual(
            date_key("2026-11-08T00:30:00+01:00"),
            "2026-11-07",
        )

    def test_merge_keeps_history_and_updates_overlapping_days(self):
        existing = {
            "schema_version": 1,
            "repository": "Yassine-Jemi01/ylang",
            "first_collected_at": "2026-10-26T03:00:00Z",
            "days": {
                "2026-10-26": {
                    "clones": 10,
                    "unique_cloners": 4,
                    "views": 20,
                    "unique_visitors": 8,
                },
                "2026-11-07": {"clones": 5, "unique_cloners": 3},
            },
        }
        clones = {
            "clones": [
                {"timestamp": "2026-11-07T00:00:00Z", "count": 9, "uniques": 6},
                {"timestamp": "2026-11-08T00:00:00Z", "count": 12, "uniques": 7},
            ]
        }
        views = {
            "views": [
                {"timestamp": "2026-11-07T00:00:00Z", "count": 30, "uniques": 11},
            ]
        }

        result = merge_snapshots(
            existing, clones, views, "2026-11-08T03:00:00Z"
        )

        self.assertEqual(result["first_collected_at"], "2026-10-26T03:00:00Z")
        self.assertEqual(result["last_updated_at"], "2026-11-08T03:00:00Z")
        self.assertEqual(result["days"]["2026-10-26"]["clones"], 10)
        self.assertEqual(result["days"]["2026-11-07"]["clones"], 9)
        self.assertEqual(result["days"]["2026-11-07"]["unique_cloners"], 6)
        self.assertEqual(result["days"]["2026-11-07"]["views"], 30)
        self.assertEqual(result["days"]["2026-11-07"]["unique_visitors"], 11)
        self.assertEqual(result["days"]["2026-11-08"]["clones"], 12)
        self.assertEqual(list(result["days"]), sorted(result["days"]))

    def test_empty_api_windows_do_not_erase_saved_history(self):
        existing = {
            "schema_version": 1,
            "first_collected_at": "2026-10-26T03:00:00Z",
            "days": {"2026-10-26": {"clones": 10, "unique_cloners": 4}},
        }
        result = merge_snapshots(
            existing, {"clones": []}, {"views": []}, "2026-11-08T03:00:00Z"
        )
        self.assertEqual(result["days"], existing["days"])


if __name__ == "__main__":
    unittest.main()
