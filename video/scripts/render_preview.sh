#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."
manim -pql scenes/kirin_board_overview.py KirinBoardOverview
