#!/usr/bin/env python3
"""Shared Kirin NNUE v1 format helpers."""

from __future__ import annotations

import json
import struct
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


MAGIC = b"KIRINNUE"
VERSION = 1
FEATURE_COUNT = 12 * 64
DEFAULT_HIDDEN_SIZE = 32
DEFAULT_ACTIVATION_LIMIT = 255
MAX_HIDDEN_SIZE = 1024

PIECE_TO_INDEX = {
    "P": 0,
    "N": 1,
    "B": 2,
    "R": 3,
    "Q": 4,
    "K": 5,
    "p": 6,
    "n": 7,
    "b": 8,
    "r": 9,
    "q": 10,
    "k": 11,
}


@dataclass
class Network:
    hidden_size: int
    activation_limit: int
    output_bias: int
    hidden_biases: list[int]
    output_weights: list[int]
    feature_weights: list[int]

    def validate(self) -> None:
        if self.hidden_size <= 0 or self.hidden_size > MAX_HIDDEN_SIZE:
            raise ValueError(f"hidden_size must be in [1, {MAX_HIDDEN_SIZE}]")
        if self.activation_limit <= 0:
            raise ValueError("activation_limit must be positive")
        if len(self.hidden_biases) != self.hidden_size:
            raise ValueError("hidden_biases length does not match hidden_size")
        if len(self.output_weights) != self.hidden_size:
            raise ValueError("output_weights length does not match hidden_size")
        expected = FEATURE_COUNT * self.hidden_size
        if len(self.feature_weights) != expected:
            raise ValueError(f"feature_weights length must be {expected}")


def clamp_i32(value: int) -> int:
    return max(-(2**31), min(2**31 - 1, int(value)))


def write_i32(file, value: int) -> None:
    file.write(struct.pack("<i", clamp_i32(value)))


def write_u32(file, value: int) -> None:
    file.write(struct.pack("<I", int(value)))


def read_i32(file) -> int:
    data = file.read(4)
    if len(data) != 4:
        raise ValueError("truncated i32")
    return struct.unpack("<i", data)[0]


def read_u32(file) -> int:
    data = file.read(4)
    if len(data) != 4:
        raise ValueError("truncated u32")
    return struct.unpack("<I", data)[0]


def write_knnue(path: Path, network: Network) -> None:
    network.validate()
    path.parent.mkdir(parents=True, exist_ok=True)

    with path.open("wb") as file:
        file.write(MAGIC)
        write_u32(file, VERSION)
        write_u32(file, FEATURE_COUNT)
        write_u32(file, network.hidden_size)
        write_i32(file, network.activation_limit)
        write_i32(file, network.output_bias)

        for value in network.hidden_biases:
            write_i32(file, value)
        for value in network.output_weights:
            write_i32(file, value)
        for value in network.feature_weights:
            write_i32(file, value)


def read_knnue(path: Path) -> Network:
    with path.open("rb") as file:
        magic = file.read(len(MAGIC))
        if magic != MAGIC:
            raise ValueError("invalid magic header")

        version = read_u32(file)
        feature_count = read_u32(file)
        hidden_size = read_u32(file)
        activation_limit = read_i32(file)
        output_bias = read_i32(file)

        if version != VERSION:
            raise ValueError(f"unsupported version {version}")
        if feature_count != FEATURE_COUNT:
            raise ValueError(f"unsupported feature count {feature_count}")

        hidden_biases = [read_i32(file) for _ in range(hidden_size)]
        output_weights = [read_i32(file) for _ in range(hidden_size)]
        feature_weights = [read_i32(file) for _ in range(feature_count * hidden_size)]

        if file.read(1):
            raise ValueError("trailing data")

    network = Network(
        hidden_size=hidden_size,
        activation_limit=activation_limit,
        output_bias=output_bias,
        hidden_biases=hidden_biases,
        output_weights=output_weights,
        feature_weights=feature_weights,
    )
    network.validate()
    return network


def network_to_json(path: Path, network: Network) -> None:
    network.validate()
    payload = {
        "format": "kirin-nnue-v1",
        "feature_count": FEATURE_COUNT,
        "hidden_size": network.hidden_size,
        "activation_limit": network.activation_limit,
        "output_bias": network.output_bias,
        "hidden_biases": network.hidden_biases,
        "output_weights": network.output_weights,
        "feature_weights": network.feature_weights,
    }
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")


def network_from_json(path: Path) -> Network:
    payload = json.loads(path.read_text(encoding="utf-8"))

    if payload.get("format") == "kirin-linear-nnue-v1":
        if int(payload.get("feature_count", FEATURE_COUNT)) != FEATURE_COUNT:
            raise ValueError("JSON feature_count does not match Kirin")
        return linear_feature_network(
            [int(value) for value in payload["feature_scores"]],
            int(payload.get("output_bias", 0)),
        )

    if payload.get("format") != "kirin-nnue-v1":
        raise ValueError("JSON network format must be kirin-nnue-v1 or kirin-linear-nnue-v1")
    if int(payload.get("feature_count", FEATURE_COUNT)) != FEATURE_COUNT:
        raise ValueError("JSON feature_count does not match Kirin")

    network = Network(
        hidden_size=int(payload["hidden_size"]),
        activation_limit=int(payload["activation_limit"]),
        output_bias=int(payload["output_bias"]),
        hidden_biases=[int(value) for value in payload["hidden_biases"]],
        output_weights=[int(value) for value in payload["output_weights"]],
        feature_weights=[int(value) for value in payload["feature_weights"]],
    )
    network.validate()
    return network


def write_linear_checkpoint(path: Path, feature_scores: list[int], output_bias: int) -> None:
    if len(feature_scores) != FEATURE_COUNT:
        raise ValueError(f"feature_scores length must be {FEATURE_COUNT}")

    payload = {
        "format": "kirin-linear-nnue-v1",
        "feature_count": FEATURE_COUNT,
        "output_bias": int(output_bias),
        "feature_scores": [int(value) for value in feature_scores],
    }
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")


def linear_feature_network(feature_scores: list[int], output_bias: int) -> Network:
    if len(feature_scores) != FEATURE_COUNT:
        raise ValueError(f"feature_scores length must be {FEATURE_COUNT}")

    hidden_size = FEATURE_COUNT
    feature_weights = [0] * (FEATURE_COUNT * hidden_size)
    for feature in range(FEATURE_COUNT):
        feature_weights[feature * hidden_size + feature] = 1

    network = Network(
        hidden_size=hidden_size,
        activation_limit=1,
        output_bias=int(output_bias),
        hidden_biases=[0] * hidden_size,
        output_weights=[int(value) for value in feature_scores],
        feature_weights=feature_weights,
    )
    network.validate()
    return network


def square_index(file_index: int, rank: int) -> int:
    return (8 - rank) * 8 + file_index


def feature_index(piece: str, square: int) -> int:
    return PIECE_TO_INDEX[piece] * 64 + square


def fen_features(fen: str) -> tuple[list[int], str]:
    fields = fen.strip().split()
    if len(fields) < 2:
        raise ValueError(f"FEN must include board and side to move: {fen!r}")

    board, side = fields[0], fields[1]
    features: list[int] = []
    ranks = board.split("/")
    if len(ranks) != 8:
        raise ValueError(f"FEN board must contain 8 ranks: {fen!r}")

    for rank_index, rank_text in enumerate(ranks):
        file_index = 0
        rank_number = 8 - rank_index
        for char in rank_text:
            if char.isdigit():
                file_index += int(char)
                continue
            if char not in PIECE_TO_INDEX:
                raise ValueError(f"invalid FEN piece {char!r}")
            if file_index >= 8:
                raise ValueError(f"too many files in FEN rank: {rank_text!r}")
            features.append(feature_index(char, square_index(file_index, rank_number)))
            file_index += 1
        if file_index != 8:
            raise ValueError(f"FEN rank does not contain 8 files: {rank_text!r}")

    if side not in ("w", "b"):
        raise ValueError(f"invalid FEN side to move: {side!r}")
    return features, side


def material_bootstrap_network() -> Network:
    hidden_size = DEFAULT_HIDDEN_SIZE
    feature_weights = [0] * (FEATURE_COUNT * hidden_size)
    output_weights = [0] * hidden_size
    hidden_biases = [0] * hidden_size

    for piece in range(12):
        for square in range(64):
            value = bootstrap_feature_value(piece, square) // 16
            index = piece * 64 + square
            feature_weights[index * hidden_size] = value
            feature_weights[index * hidden_size + 1] = -value

    output_weights[0] = 16
    output_weights[1] = -16
    return Network(
        hidden_size=hidden_size,
        activation_limit=DEFAULT_ACTIVATION_LIMIT,
        output_bias=0,
        hidden_biases=hidden_biases,
        output_weights=output_weights,
        feature_weights=feature_weights,
    )


def bootstrap_feature_value(piece: int, square: int) -> int:
    value = base_piece_value(piece)
    if piece in (1, 2, 7, 8):
        value += centrality_bonus(square)
    if piece in (0, 6):
        value += pawn_advance_bonus(piece, square)
    return value if piece <= 5 else -value


def base_piece_value(piece: int) -> int:
    if piece in (0, 6):
        return 100
    if piece in (1, 7):
        return 320
    if piece in (2, 8):
        return 330
    if piece in (3, 9):
        return 500
    if piece in (4, 10):
        return 900
    return 0


def centrality_bonus(square: int) -> int:
    rank = square // 8
    file = square % 8
    rank_distance = 3 - rank if rank < 4 else rank - 4
    file_distance = 3 - file if file < 4 else file - 4
    return 14 - 4 * (rank_distance + file_distance)


def pawn_advance_bonus(piece: int, square: int) -> int:
    rank = square // 8
    if piece == 0:
        return (6 - rank) * 6
    if piece == 6:
        return (rank - 1) * 6
    return 0


def iter_chunks(values: list[int], size: int) -> Iterable[list[int]]:
    for index in range(0, len(values), size):
        yield values[index:index + size]
