#!/usr/bin/env python3
"""Train a dependency-free Kirin linear NNUE from labeled FEN data."""

from __future__ import annotations

import argparse
import csv
import math
import random
import re
from dataclasses import dataclass
from pathlib import Path

from nnue_format import (
    FEATURE_COUNT,
    fen_features,
    linear_feature_network,
    write_knnue,
    write_linear_checkpoint,
)


@dataclass
class Example:
    features: list[int]
    target: float


def target_from_score(score_cp: float, side: str, perspective: str) -> float:
    if perspective == "side":
        return score_cp
    if perspective == "white":
        return score_cp if side == "w" else -score_cp
    raise ValueError(f"unknown label perspective: {perspective}")


def parse_csv_examples(path: Path, perspective: str, target_clip: int) -> list[Example]:
    examples: list[Example] = []
    with path.open("r", encoding="utf-8", newline="") as file:
        sample = file.read(4096)
        file.seek(0)
        try:
            has_header = csv.Sniffer().has_header(sample)
        except csv.Error:
            has_header = False

        if has_header:
            reader = csv.DictReader(file)
            for row in reader:
                fen = row.get("fen") or row.get("FEN")
                score = row.get("score_cp") or row.get("score") or row.get("cp")
                if fen is None or score is None:
                    raise ValueError("CSV header must include fen and score_cp columns")
                features, side = fen_features(fen)
                target = target_from_score(float(score), side, perspective)
                examples.append(Example(features, clamp(target, -target_clip, target_clip)))
        else:
            reader = csv.reader(file)
            for row in reader:
                if not row or row[0].strip().startswith("#"):
                    continue
                if len(row) < 2:
                    raise ValueError("CSV rows without a header must be: fen,score_cp")
                features, side = fen_features(row[0])
                target = target_from_score(float(row[1]), side, perspective)
                examples.append(Example(features, clamp(target, -target_clip, target_clip)))

    return examples


def parse_epd_examples(path: Path, perspective: str, target_clip: int) -> list[Example]:
    examples: list[Example] = []
    ce_pattern = re.compile(r"(?:^|[;\s])ce\s+(-?\d+(?:\.\d+)?)")

    with path.open("r", encoding="utf-8") as file:
        for line_number, line in enumerate(file, 1):
            stripped = line.strip()
            if not stripped or stripped.startswith("#"):
                continue

            fields = stripped.split()
            if len(fields) < 4:
                raise ValueError(f"EPD line {line_number} does not contain 4 FEN fields")
            match = ce_pattern.search(stripped)
            if not match:
                raise ValueError(f"EPD line {line_number} is missing a 'ce <cp>' opcode")

            fen = " ".join(fields[:4]) + " 0 1"
            features, side = fen_features(fen)
            target = target_from_score(float(match.group(1)), side, perspective)
            examples.append(Example(features, clamp(target, -target_clip, target_clip)))

    return examples


def load_examples(path: Path, data_format: str, perspective: str, target_clip: int) -> list[Example]:
    if data_format == "auto":
        data_format = "epd" if path.suffix.lower() == ".epd" else "csv"
    if data_format == "csv":
        return parse_csv_examples(path, perspective, target_clip)
    if data_format == "epd":
        return parse_epd_examples(path, perspective, target_clip)
    raise ValueError(f"unknown data format: {data_format}")


def clamp(value: float, low: float, high: float) -> float:
    return max(low, min(high, value))


def evaluate(weights: list[float], bias: float, example: Example) -> float:
    return bias + sum(weights[feature] for feature in example.features)


def rmse(weights: list[float], bias: float, examples: list[Example]) -> float:
    if not examples:
        return 0.0
    total = 0.0
    for example in examples:
        error = evaluate(weights, bias, example) - example.target
        total += error * error
    return math.sqrt(total / len(examples))


def train_linear(
    examples: list[Example],
    epochs: int,
    learning_rate: float,
    l2: float,
    validation_split: float,
    seed: int,
) -> tuple[list[int], int, float, float]:
    if not examples:
        raise ValueError("training data is empty")

    rng = random.Random(seed)
    shuffled = list(examples)
    rng.shuffle(shuffled)

    split_index = int(len(shuffled) * (1.0 - validation_split))
    split_index = max(1, min(len(shuffled), split_index))
    train_examples = shuffled[:split_index]
    validation_examples = shuffled[split_index:]

    weights = [0.0] * FEATURE_COUNT
    bias = 0.0

    for epoch in range(1, epochs + 1):
        rng.shuffle(train_examples)
        for example in train_examples:
            prediction = evaluate(weights, bias, example)
            error = prediction - example.target
            scale = learning_rate / max(1, len(example.features))
            bias -= learning_rate * error

            for feature in example.features:
                if l2:
                    weights[feature] *= 1.0 - learning_rate * l2
                weights[feature] -= scale * error

        train_rmse = rmse(weights, bias, train_examples)
        validation_rmse = rmse(weights, bias, validation_examples)
        print(
            f"epoch {epoch:03d}: train_rmse={train_rmse:.2f} "
            f"validation_rmse={validation_rmse:.2f}"
        )

    quantized_weights = [round(value) for value in weights]
    quantized_bias = round(bias)
    return quantized_weights, quantized_bias, rmse(weights, bias, train_examples), rmse(weights, bias, validation_examples)


def self_test_examples() -> list[Example]:
    rows = [
        ("4k3/8/8/8/8/8/8/4K3 w - - 0 1", 0),
        ("4k3/8/8/8/8/8/8/4KQ2 w - - 0 1", 900),
        ("4k3/8/8/8/8/8/8/4KR2 w - - 0 1", 500),
        ("4k3/8/8/8/8/8/8/4KP2 w - - 0 1", 100),
        ("4k1q1/8/8/8/8/8/8/4K3 w - - 0 1", -900),
        ("4k3/8/8/8/8/8/8/4KQ2 b - - 0 1", -900),
    ]
    examples: list[Example] = []
    for fen, score in rows:
        features, side = fen_features(fen)
        examples.append(Example(features, target_from_score(score, side, "white")))
    return examples


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", help="CSV or EPD training data")
    parser.add_argument("--output", required=True, help="Output .knnue file")
    parser.add_argument("--checkpoint", help="Optional compact JSON checkpoint")
    parser.add_argument("--format", choices=("auto", "csv", "epd"), default="auto")
    parser.add_argument("--label-perspective", choices=("white", "side"), default="white")
    parser.add_argument("--epochs", type=int, default=20)
    parser.add_argument("--learning-rate", type=float, default=0.02)
    parser.add_argument("--l2", type=float, default=0.0)
    parser.add_argument("--validation-split", type=float, default=0.1)
    parser.add_argument("--target-clip", type=int, default=2000)
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--self-test", action="store_true", help="Train on a tiny built-in data set")
    args = parser.parse_args()

    if args.self_test:
        examples = self_test_examples()
    else:
        if not args.input:
            parser.error("--input is required unless --self-test is used")
        examples = load_examples(Path(args.input), args.format, args.label_perspective, args.target_clip)

    weights, bias, train_error, validation_error = train_linear(
        examples=examples,
        epochs=args.epochs,
        learning_rate=args.learning_rate,
        l2=args.l2,
        validation_split=args.validation_split,
        seed=args.seed,
    )

    network = linear_feature_network(weights, bias)
    output = Path(args.output)
    write_knnue(output, network)
    print(f"Wrote {output}")
    print(f"final train_rmse={train_error:.2f} validation_rmse={validation_error:.2f}")

    if args.checkpoint:
        checkpoint = Path(args.checkpoint)
        write_linear_checkpoint(checkpoint, weights, bias)
        print(f"Wrote {checkpoint}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
