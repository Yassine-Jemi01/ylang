#!/usr/bin/env python3
"""Backward-compatible entry point for the assembled website integrity checker."""

from check_website_build import main


if __name__ == "__main__":
    raise SystemExit(main())
