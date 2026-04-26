#!/usr/bin/env python3
"""Train Kirin NNUE networks from labeled FEN data."""

from __future__ import annotations

import argparse
import csv
import math
import random
import re
from dataclasses import dataclass
from pathlib import Path

from nnue_format import (
    DEFAULT_ACTIVATION_LIMIT,
    FEATURE_COUNT,
    Network,
    fen_features,
    linear_feature_network,
    network_to_json,
    write_knnue,
    write_linear_checkpoint,
)


@dataclass
class Example:
    features: list[int]
    target: float
    side: str


def target_from_score(score_cp: float, side: str, perspective: str) -> float:
    if perspective == "white":
        return score_cp
    if perspective == "side":
        return score_cp if side == "w" else -score_cp
    raise ValueError(f"unknown label perspective: {perspective}")


MATERIAL_SCORE = [100, 300, 350, 500, 1000, 10000, -100, -300, -350, -500, -1000, -10000]
PAWN_SCORE = [
    90, 90, 90, 90, 90, 90, 90, 90,
    30, 30, 30, 40, 40, 30, 30, 30,
    20, 20, 20, 30, 30, 30, 20, 20,
    10, 10, 10, 20, 20, 10, 10, 10,
    5, 5, 10, 20, 20, 5, 5, 5,
    0, 0, 0, 5, 5, 0, 0, 0,
    0, 0, 0, -10, -10, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
]
KNIGHT_SCORE = [
    -5, 0, 0, 0, 0, 0, 0, -5,
    -5, 0, 0, 10, 10, 0, 0, -5,
    -5, 5, 20, 20, 20, 20, 5, -5,
    -5, 10, 20, 30, 30, 20, 10, -5,
    -5, 10, 20, 30, 30, 20, 10, -5,
    -5, 5, 20, 10, 10, 20, 5, -5,
    -5, 0, 0, 0, 0, 0, 0, -5,
    -5, -10, 0, 0, 0, 0, -10, -5,
]
BISHOP_SCORE = [
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 10, 10, 0, 0, 0,
    0, 0, 10, 20, 20, 10, 0, 0,
    0, 0, 10, 20, 20, 10, 0, 0,
    0, 10, 0, 0, 0, 0, 10, 0,
    0, 30, 0, 0, 0, 0, 30, 0,
    0, 0, -10, 0, 0, -10, 0, 0,
]
ROOK_SCORE = [
    50, 50, 50, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 50, 50, 50, 50,
    0, 0, 10, 20, 20, 10, 0, 0,
    0, 0, 10, 20, 20, 10, 0, 0,
    0, 0, 10, 20, 20, 10, 0, 0,
    0, 0, 10, 20, 20, 10, 0, 0,
    0, 0, 10, 20, 20, 10, 0, 0,
    0, 0, 0, 20, 20, 0, 0, 0,
]
KING_SCORE = [
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 5, 5, 5, 5, 0, 0,
    0, 5, 5, 10, 10, 5, 5, 0,
    0, 5, 10, 20, 20, 10, 5, 0,
    0, 5, 10, 20, 20, 10, 5, 0,
    0, 0, 5, 10, 10, 5, 0, 0,
    0, 5, 5, -5, -5, 0, 5, 0,
    0, 0, 5, 0, -15, 0, 10, 0,
]
PASSED_PAWN_BONUS = [0, 10, 30, 50, 75, 100, 150, 200]


def bit_count(value: int) -> int:
    return value.bit_count()


def file_mask(file_index: int) -> int:
    if file_index < 0 or file_index > 7:
        return 0
    mask = 0
    for rank_index in range(8):
        mask |= 1 << (rank_index * 8 + file_index)
    return mask


FILE_MASKS = [file_mask(square % 8) for square in range(64)]
ISOLATED_MASKS = [file_mask((square % 8) - 1) | file_mask((square % 8) + 1) for square in range(64)]


def passed_mask(square: int, white: bool) -> int:
    file_index = square % 8
    rank_index = square // 8
    mask = 0
    files = [file_index - 1, file_index, file_index + 1]
    ranks = range(0, rank_index) if white else range(rank_index + 1, 8)
    for rank in ranks:
        for file in files:
            if 0 <= file <= 7:
                mask |= 1 << (rank * 8 + file)
    return mask


PASSED_WHITE_MASKS = [passed_mask(square, True) for square in range(64)]
PASSED_BLACK_MASKS = [passed_mask(square, False) for square in range(64)]


def classical_white_score(features: list[int]) -> int:
    bitboards = [0] * 12
    for feature in features:
        piece = feature // 64
        square = feature % 64
        bitboards[piece] |= 1 << square

    score = 0
    for feature in features:
        piece = feature // 64
        square = feature % 64
        mirror = square ^ 56
        rank = 7 - (square // 8)
        mirror_rank = 7 - (mirror // 8)
        score += MATERIAL_SCORE[piece]

        if piece == 0:
            score += PAWN_SCORE[square]
            double_pawns = bit_count(bitboards[0] & FILE_MASKS[square])
            if double_pawns > 1:
                score += double_pawns * -10
            if (bitboards[0] & ISOLATED_MASKS[square]) == 0:
                score += -10
            if (PASSED_WHITE_MASKS[square] & bitboards[6]) == 0:
                score += PASSED_PAWN_BONUS[rank]
        elif piece == 1:
            score += KNIGHT_SCORE[square]
        elif piece == 2:
            score += BISHOP_SCORE[square]
        elif piece == 3:
            score += ROOK_SCORE[square]
        elif piece == 5:
            score += KING_SCORE[square]
        elif piece == 6:
            score -= PAWN_SCORE[mirror]
            double_pawns = bit_count(bitboards[6] & FILE_MASKS[square])
            if double_pawns > 1:
                score -= double_pawns * -10
            if (bitboards[6] & ISOLATED_MASKS[square]) == 0:
                score -= -10
            if (PASSED_BLACK_MASKS[square] & bitboards[0]) == 0:
                score -= PASSED_PAWN_BONUS[mirror_rank]
        elif piece == 7:
            score -= KNIGHT_SCORE[mirror]
        elif piece == 8:
            score -= BISHOP_SCORE[mirror]
        elif piece == 9:
            score -= ROOK_SCORE[mirror]
        elif piece == 11:
            score -= KING_SCORE[mirror]

    return score


def make_example(fen: str, score_cp: float, perspective: str, target_clip: int, target_mode: str) -> Example:
    features, side = fen_features(fen)
    target = target_from_score(score_cp, side, perspective)
    if target_mode == "residual-classical":
        target -= classical_white_score(features)
    elif target_mode != "absolute":
        raise ValueError(f"unknown target mode: {target_mode}")
    return Example(features, clamp(target, -target_clip, target_clip), side)


def parse_csv_examples(path: Path, perspective: str, target_clip: int, target_mode: str) -> list[Example]:
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
                examples.append(make_example(fen, float(score), perspective, target_clip, target_mode))
        else:
            reader = csv.reader(file)
            for row in reader:
                if not row or row[0].strip().startswith("#"):
                    continue
                if len(row) < 2:
                    raise ValueError("CSV rows without a header must be: fen,score_cp")
                examples.append(make_example(row[0], float(row[1]), perspective, target_clip, target_mode))

    return examples


def parse_epd_examples(path: Path, perspective: str, target_clip: int, target_mode: str) -> list[Example]:
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
            examples.append(make_example(fen, float(match.group(1)), perspective, target_clip, target_mode))

    return examples


def load_examples(path: Path, data_format: str, perspective: str, target_clip: int, target_mode: str) -> list[Example]:
    if data_format == "auto":
        data_format = "epd" if path.suffix.lower() == ".epd" else "csv"
    if data_format == "csv":
        return parse_csv_examples(path, perspective, target_clip, target_mode)
    if data_format == "epd":
        return parse_epd_examples(path, perspective, target_clip, target_mode)
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


def require_numpy():
    try:
        import numpy as np
    except ModuleNotFoundError as exc:
        raise SystemExit(
            "NumPy is required for --architecture hidden. Install it with:\n"
            "  python3 -m pip install numpy"
        ) from exc
    return np


def examples_to_arrays(examples: list[Example], np):
    max_features = max(len(example.features) for example in examples)
    feature_indices = np.zeros((len(examples), max_features), dtype=np.int16)
    feature_mask = np.zeros((len(examples), max_features), dtype=np.float32)
    targets = np.empty(len(examples), dtype=np.float32)

    for row, example in enumerate(examples):
        count = len(example.features)
        feature_indices[row, :count] = example.features
        feature_mask[row, :count] = 1.0
        targets[row] = example.target

    return feature_indices, feature_mask, targets


def hidden_predict(np, feature_weights, hidden_biases, output_weights, output_bias, feature_indices, feature_mask, activation_limit):
    accumulator = hidden_biases + (feature_weights[feature_indices] * feature_mask[..., None]).sum(axis=1)
    activations = np.clip(accumulator, 0.0, activation_limit)
    predictions = activations @ output_weights + output_bias
    return predictions, accumulator, activations


def hidden_rmse(np, feature_weights, hidden_biases, output_weights, output_bias, feature_indices, feature_mask, targets, activation_limit, batch_size):
    total = 0.0
    count = 0
    for start in range(0, len(targets), batch_size):
        end = min(start + batch_size, len(targets))
        predictions, _, _ = hidden_predict(
            np,
            feature_weights,
            hidden_biases,
            output_weights,
            output_bias,
            feature_indices[start:end],
            feature_mask[start:end],
            activation_limit,
        )
        errors = predictions - targets[start:end]
        total += float(errors @ errors)
        count += end - start
    return math.sqrt(total / count) if count else 0.0


def adam_update(np, param, grad, m, v, step, learning_rate, beta1=0.9, beta2=0.999, eps=1e-8):
    m *= beta1
    m += (1.0 - beta1) * grad
    v *= beta2
    v += (1.0 - beta2) * (grad * grad)
    m_hat = m / (1.0 - beta1 ** step)
    v_hat = v / (1.0 - beta2 ** step)
    param -= learning_rate * m_hat / (np.sqrt(v_hat) + eps)


def train_hidden(
    examples: list[Example],
    hidden_size: int,
    activation_limit: int,
    epochs: int,
    learning_rate: float,
    l2: float,
    validation_split: float,
    seed: int,
    batch_size: int,
    quantize_clip: int,
) -> tuple[Network, float, float]:
    if not examples:
        raise ValueError("training data is empty")
    if hidden_size <= 0:
        raise ValueError("hidden_size must be positive")
    if activation_limit <= 0:
        raise ValueError("activation_limit must be positive")

    np = require_numpy()
    rng = np.random.default_rng(seed)

    shuffled = list(examples)
    random.Random(seed).shuffle(shuffled)
    split_index = int(len(shuffled) * (1.0 - validation_split))
    split_index = max(1, min(len(shuffled), split_index))
    train_examples = shuffled[:split_index]
    validation_examples = shuffled[split_index:]

    train_features, train_mask, train_targets = examples_to_arrays(train_examples, np)
    validation_features, validation_mask, validation_targets = examples_to_arrays(validation_examples, np)

    # The runtime format stores integer weights with clipped-ReLU activations.
    # Initialize around that scale so quantization after training does not erase
    # the learned model.
    feature_weights = rng.normal(0.0, 2.0, size=(FEATURE_COUNT, hidden_size)).astype(np.float32)
    hidden_biases = rng.normal(-12.0, 4.0, size=hidden_size).astype(np.float32)
    output_weights = rng.normal(0.0, 2.0, size=hidden_size).astype(np.float32)
    output_bias = np.float32(float(train_targets.mean()))

    m_fw = np.zeros_like(feature_weights)
    v_fw = np.zeros_like(feature_weights)
    m_hb = np.zeros_like(hidden_biases)
    v_hb = np.zeros_like(hidden_biases)
    m_ow = np.zeros_like(output_weights)
    v_ow = np.zeros_like(output_weights)
    m_ob = np.float32(0.0)
    v_ob = np.float32(0.0)
    step = 0

    for epoch in range(1, epochs + 1):
        order = rng.permutation(len(train_targets))

        for batch_start in range(0, len(order), batch_size):
            batch_indices = order[batch_start:batch_start + batch_size]
            batch_features = train_features[batch_indices]
            batch_mask = train_mask[batch_indices]
            batch_targets = train_targets[batch_indices]
            batch_count = len(batch_targets)

            predictions, accumulator, activations = hidden_predict(
                np,
                feature_weights,
                hidden_biases,
                output_weights,
                output_bias,
                batch_features,
                batch_mask,
                activation_limit,
            )

            error = predictions - batch_targets
            grad_prediction = (2.0 / batch_count) * error

            grad_output_weights = activations.T @ grad_prediction
            grad_output_bias = np.float32(grad_prediction.sum())
            grad_accumulator = grad_prediction[:, None] * output_weights[None, :]
            grad_accumulator *= ((accumulator > 0.0) & (accumulator < activation_limit)).astype(np.float32)

            grad_hidden_biases = grad_accumulator.sum(axis=0)
            grad_feature_weights = np.zeros_like(feature_weights)
            flat_features = batch_features.reshape(-1)
            flat_mask = batch_mask.reshape(-1)
            flat_grad = np.repeat(grad_accumulator, batch_features.shape[1], axis=0) * flat_mask[:, None]
            active = flat_mask > 0.0
            np.add.at(grad_feature_weights, flat_features[active], flat_grad[active])

            if l2:
                grad_feature_weights += l2 * feature_weights
                grad_hidden_biases += l2 * hidden_biases
                grad_output_weights += l2 * output_weights

            step += 1
            adam_update(np, feature_weights, grad_feature_weights, m_fw, v_fw, step, learning_rate)
            adam_update(np, hidden_biases, grad_hidden_biases, m_hb, v_hb, step, learning_rate)
            adam_update(np, output_weights, grad_output_weights, m_ow, v_ow, step, learning_rate)

            m_ob = np.float32(0.9 * m_ob + 0.1 * grad_output_bias)
            v_ob = np.float32(0.999 * v_ob + 0.001 * grad_output_bias * grad_output_bias)
            m_ob_hat = m_ob / (1.0 - 0.9 ** step)
            v_ob_hat = v_ob / (1.0 - 0.999 ** step)
            output_bias -= np.float32(learning_rate * m_ob_hat / (math.sqrt(float(v_ob_hat)) + 1e-8))

        train_error = hidden_rmse(
            np,
            feature_weights,
            hidden_biases,
            output_weights,
            output_bias,
            train_features,
            train_mask,
            train_targets,
            activation_limit,
            batch_size,
        )
        validation_error = hidden_rmse(
            np,
            feature_weights,
            hidden_biases,
            output_weights,
            output_bias,
            validation_features,
            validation_mask,
            validation_targets,
            activation_limit,
            batch_size,
        )
        print(
            f"epoch {epoch:03d}: train_rmse={train_error:.2f} "
            f"validation_rmse={validation_error:.2f}"
        )

    quantized = Network(
        hidden_size=hidden_size,
        activation_limit=activation_limit,
        output_bias=int(round(float(output_bias))),
        hidden_biases=[int(value) for value in np.clip(np.rint(hidden_biases), -quantize_clip, quantize_clip)],
        output_weights=[int(value) for value in np.clip(np.rint(output_weights), -quantize_clip, quantize_clip)],
        feature_weights=[
            int(value)
            for value in np.clip(np.rint(feature_weights), -quantize_clip, quantize_clip).reshape(-1)
        ],
    )

    train_error = hidden_rmse(
        np,
        feature_weights,
        hidden_biases,
        output_weights,
        output_bias,
        train_features,
        train_mask,
        train_targets,
        activation_limit,
        batch_size,
    )
    validation_error = hidden_rmse(
        np,
        feature_weights,
        hidden_biases,
        output_weights,
        output_bias,
        validation_features,
        validation_mask,
        validation_targets,
        activation_limit,
        batch_size,
    )
    return quantized, train_error, validation_error


def self_test_examples() -> list[Example]:
    rows = [
        ("4k3/8/8/8/8/8/8/4K3 w - - 0 1", 0),
        ("4k3/8/8/8/8/8/8/4KQ2 w - - 0 1", 900),
        ("4k3/8/8/8/8/8/8/4KR2 w - - 0 1", 500),
        ("4k3/8/8/8/8/8/8/4KP2 w - - 0 1", 100),
        ("4k1q1/8/8/8/8/8/8/4K3 w - - 0 1", -900),
        ("4k3/8/8/8/8/8/8/4KQ2 b - - 0 1", 900),
    ]
    examples: list[Example] = []
    for fen, score in rows:
        features, side = fen_features(fen)
        examples.append(Example(features, target_from_score(score, side, "white"), side))
    return examples


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", help="CSV or EPD training data")
    parser.add_argument("--output", required=True, help="Output .knnue file")
    parser.add_argument("--checkpoint", help="Optional compact JSON checkpoint")
    parser.add_argument("--architecture", choices=("linear", "hidden"), default="linear")
    parser.add_argument("--hidden-size", type=int, default=64)
    parser.add_argument("--activation-limit", type=int, default=DEFAULT_ACTIVATION_LIMIT)
    parser.add_argument("--batch-size", type=int, default=4096)
    parser.add_argument("--quantize-clip", type=int, default=4096)
    parser.add_argument("--format", choices=("auto", "csv", "epd"), default="auto")
    parser.add_argument(
        "--target-mode",
        choices=("absolute", "residual-classical"),
        default="absolute",
        help="Train direct Stockfish scores or residual corrections over Kirin's classical evaluator",
    )
    parser.add_argument(
        "--label-perspective",
        choices=("white", "side"),
        default="white",
        help=(
            "Perspective of input scores. Use 'white' for Stockfish/PGN labels "
            "where positive means White is better, or 'side' when positive means "
            "the side to move is better."
        ),
    )
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
        examples = load_examples(
            Path(args.input),
            args.format,
            args.label_perspective,
            args.target_clip,
            args.target_mode,
        )

    white_to_move = sum(1 for example in examples if example.side == "w")
    black_to_move = len(examples) - white_to_move
    print(
        f"loaded examples={len(examples)} "
        f"white_to_move={white_to_move} black_to_move={black_to_move} "
        f"label_perspective={args.label_perspective} "
        f"target_mode={args.target_mode}"
    )

    if args.architecture == "linear":
        weights, bias, train_error, validation_error = train_linear(
            examples=examples,
            epochs=args.epochs,
            learning_rate=args.learning_rate,
            l2=args.l2,
            validation_split=args.validation_split,
            seed=args.seed,
        )
        network = linear_feature_network(weights, bias)
    else:
        network, train_error, validation_error = train_hidden(
            examples=examples,
            hidden_size=args.hidden_size,
            activation_limit=args.activation_limit,
            epochs=args.epochs,
            learning_rate=args.learning_rate,
            l2=args.l2,
            validation_split=args.validation_split,
            seed=args.seed,
            batch_size=args.batch_size,
            quantize_clip=args.quantize_clip,
        )

    output = Path(args.output)
    write_knnue(output, network)
    print(f"Wrote {output}")
    print(f"final train_rmse={train_error:.2f} validation_rmse={validation_error:.2f}")

    if args.checkpoint:
        checkpoint = Path(args.checkpoint)
        if args.architecture == "linear":
            write_linear_checkpoint(checkpoint, weights, bias)
        else:
            network_to_json(checkpoint, network)
        print(f"Wrote {checkpoint}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
