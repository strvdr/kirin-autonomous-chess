# Kirin NNUE Training Pipeline

Kirin loads `.knnue` files through UCI:

```text
setoption name EvalFile value /path/to/network.knnue
setoption name Use NNUE value true
```

The current trainer is intentionally dependency-free. It trains a linear
piece-square network inside the Kirin NNUE v1 container. That gives us a real
data-to-network pipeline before adding a heavier PyTorch trainer or an
incremental accumulator.

## Data Formats

CSV training data should include `fen` and `score_cp` columns:

```csv
fen,score_cp
"4k3/8/8/8/8/8/8/4KQ2 w - - 0 1",900
"4k1q1/8/8/8/8/8/8/4K3 w - - 0 1",-900
```

Scores are white-relative by default. For side-to-move-relative labels, pass
`--label-perspective side`.

EPD input is also supported when each line has a `ce <cp>` opcode:

```text
4k3/8/8/8/8/8/8/4KQ2 w - - ce 900;
```

## Train

If you only have PGN games, label them with Stockfish first:

```sh
python3 -m pip install python-chess

python3 kirin/tools/pgn_to_training_csv.py \
  --input data/lichess/kirin_games.pgn \
  --output data/lichess/kirin_labeled.csv \
  --stockfish /path/to/stockfish \
  --skip-plies 10 \
  --sample-rate 0.25 \
  --seed 1 \
  --exclude-bullet
```

For a larger external PGN corpus instead of Kirin-only bot games, disable the
Kirin name filter and add a quality floor:

```sh
python3 kirin/tools/pgn_to_training_csv.py \
  --input data/lichess/large_games.pgn \
  --output data/lichess/large_labeled.csv \
  --stockfish /path/to/stockfish \
  --no-kirin-filter \
  --rated-only \
  --min-elo 1800 \
  --exclude-bullet \
  --skip-plies 10 \
  --sample-rate 0.10 \
  --seed 1 \
  --max-positions 250000
```

For Lichess broadcast PGNs, use the embedded `[%eval ...]` annotations instead
of recalculating every position locally:

```sh
python3 kirin/tools/pgn_to_training_csv.py \
  --input data/lichess/broadcasts/lichess_db_broadcast_*.pgn \
  --output data/lichess/broadcast_labeled.csv \
  --label-source embedded \
  --no-kirin-filter \
  --skip-plies 10 \
  --sample-rate 1.0 \
  --seed 1
```

The PGN converter writes white-relative `score_cp` labels and metadata columns.
Stockfish mode labels pre-move positions. Embedded-eval mode labels the
post-move position attached to each PGN `[%eval ...]` comment. Both modes skip
early opening plies by default, de-duplicate positions by
board/side/castling/en-passant state, and cap mate scores at `--mate-score`.
Prefer `--sample-rate` over `--sample-every` when reducing a large PGN export:
every-N-ply sampling can accidentally keep mostly one side-to-move depending on
the starting ply, while seeded random sampling keeps both sides represented.

```sh
python3 kirin/tools/train_nnue.py \
  --input data/lichess/kirin_labeled.csv \
  --output build/networks/kirin-trained.knnue \
  --checkpoint build/networks/kirin-trained.json \
  --label-perspective white \
  --epochs 20 \
  --learning-rate 0.02
```

`--label-perspective white` is the correct setting for the PGN converter and
Stockfish-style centipawn labels where positive means White is better. Use
`--label-perspective side` only for data where positive means the side to move
is better.

The JSON checkpoint is compact and can be converted later:

```sh
python3 kirin/tools/convert_nnue.py \
  --input build/networks/kirin-trained.json \
  --output build/networks/kirin-trained.knnue
```

## CMake Helpers

```sh
cmake --build build --target bootstrap_nnue
cmake --build build --target nnue_tool_smoke
```

`bootstrap_nnue` writes a deterministic fallback net. `nnue_tool_smoke` trains
on a tiny built-in dataset and converts the resulting checkpoint, which is the
quickest verification that the tooling still matches the engine loader.
