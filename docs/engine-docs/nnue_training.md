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
  --sample-every 2 \
  --exclude-bullet
```

The PGN converter writes white-relative `score_cp` labels and metadata columns.
It labels pre-move positions, skips early opening plies by default, de-duplicates
positions by board/side/castling/en-passant state, and caps mate scores at
`--mate-score`.

```sh
python3 kirin/tools/train_nnue.py \
  --input data/lichess/kirin_labeled.csv \
  --output build/networks/kirin-trained.knnue \
  --checkpoint build/networks/kirin-trained.json \
  --epochs 20 \
  --learning-rate 0.02
```

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
