# Kirin Manim Video

Manim workspace for a 1.5-2 minute overview video covering the Kirin autonomous chess board hardware and software implementation.

## Setup

From the repo root:

```bash
cd video
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -U pip
python -m pip install -r requirements.txt
```

This machine already has `manim` available, but the local virtual environment keeps the video dependencies reproducible.

## Render

Fast preview:

```bash
cd video
manim -pql scenes/kirin_board_overview.py KirinBoardOverview
```

Medium-quality render using the repo config:

```bash
cd video
manim scenes/kirin_board_overview.py KirinBoardOverview
```

The output video is written under `video/media/videos/`.

## Current Structure

- `scenes/kirin_board_overview.py` contains the first storyboarded Manim scene.
- `manim.cfg` keeps generated media inside `video/media/`.
- `scripts/render_preview.sh` runs the low-quality preview command.

## Visual Direction

The starter scene uses a Tokyo Night-inspired palette and pins text rendering to `JetBrains Mono`. Keeping a specific font avoids Manim/Pango default-font fallback, which can make letter spacing look uneven between or even within words. Low-quality preview renders can still exaggerate antialiasing artifacts, so use medium quality when checking typography.

## Storyboard Draft

1. Title: Kirin as an autonomous physical chess board.
2. Hardware: 12 inch board, side storage, 2-axis gantry, electromagnet.
3. Sensing: 96 hall effect sensors through six 16-channel multiplexers.
4. Software: engine, board scanner, interpreter, path planner, GRBL output.
5. Closing: software decisions become reliable physical moves.
