#!/usr/bin/env python3
"""Convert PGN games into Stockfish-labeled Kirin NNUE training CSV."""

from __future__ import annotations

import argparse
import contextlib
import csv
import random
import re
import shutil
from dataclasses import dataclass
from pathlib import Path


@dataclass
class ExportStats:
    games_seen: int = 0
    games_used: int = 0
    positions_seen: int = 0
    positions_written: int = 0
    white_to_move_written: int = 0
    black_to_move_written: int = 0
    duplicates_skipped: int = 0
    random_samples_skipped: int = 0
    missing_eval_skipped: int = 0
    games_skipped: int = 0


def require_python_chess():
    try:
        import chess
        import chess.engine
        import chess.pgn
    except ModuleNotFoundError as exc:
        raise SystemExit(
            "python-chess is required for PGN conversion. Install it with:\n"
            "  python3 -m pip install python-chess"
        ) from exc
    return chess, chess.engine, chess.pgn


def normalized_fen(board) -> str:
    # Drop halfmove and fullmove counters for dedupe while preserving side,
    # castling rights, and en-passant state.
    parts = board.fen().split()
    return " ".join(parts[:4])


def game_result(headers) -> str:
    return headers.get("Result", "*")


def kirin_color(headers, kirin_name: str) -> str:
    if headers.get("White", "").lower() == kirin_name.lower():
        return "white"
    if headers.get("Black", "").lower() == kirin_name.lower():
        return "black"
    return ""


def header_int(headers, name: str) -> int | None:
    try:
        return int(headers.get(name, ""))
    except ValueError:
        return None


def should_use_game(headers, args) -> bool:
    if headers.get("Variant", "Standard") != "Standard":
        return False
    if args.rated_only and not headers.get("Event", "").lower().startswith("rated "):
        return False
    if args.kirin_name and not kirin_color(headers, args.kirin_name):
        return False
    if args.min_elo:
        white_elo = header_int(headers, "WhiteElo")
        black_elo = header_int(headers, "BlackElo")
        if white_elo is None or black_elo is None:
            return False
        if white_elo < args.min_elo or black_elo < args.min_elo:
            return False
    if args.exclude_bullet and "bullet" in headers.get("Event", "").lower():
        return False
    return True


def should_export_position(board, ply: int, kirin: str, args) -> bool:
    if ply < args.skip_plies:
        return False
    if args.max_ply and ply > args.max_ply:
        return False
    if args.sample_every > 1 and (ply - args.skip_plies) % args.sample_every != 0:
        return False
    if board.is_game_over(claim_draw=True):
        return False
    if args.positions == "all":
        return True
    if not kirin:
        return False

    kirin_turn_is_white = kirin == "white"
    kirin_to_move = board.turn == kirin_turn_is_white
    return kirin_to_move if args.positions == "kirin-to-move" else not kirin_to_move


def analyse_score_cp(chess, engine, board, args) -> int:
    if args.nodes:
        limit = chess.engine.Limit(nodes=args.nodes)
    else:
        limit = chess.engine.Limit(depth=args.depth)

    info = engine.analyse(board, limit)
    score = info["score"].pov(chess.WHITE)
    cp = score.score(mate_score=args.mate_score)
    if cp is None:
        mate = score.mate()
        if mate is None:
            raise RuntimeError("Stockfish returned a score without cp or mate value")
        cp = args.mate_score if mate > 0 else -args.mate_score
    return max(-args.target_clip, min(args.target_clip, int(cp)))


def parse_embedded_eval_cp(comment: str, args) -> int | None:
    match = re.search(r"\[%eval\s+([^\]\s]+)\s*\]", comment)
    if not match:
        return None

    value = match.group(1)
    if value.startswith("#"):
        mate = value[1:]
        cp = -args.mate_score if mate.startswith("-") else args.mate_score
    else:
        try:
            cp = round(float(value) * 100)
        except ValueError:
            return None

    return max(-args.target_clip, min(args.target_clip, int(cp)))


def should_keep_random_sample(rng: random.Random, args) -> bool:
    if args.sample_rate >= 1.0:
        return True
    return rng.random() < args.sample_rate


def export_positions(args) -> ExportStats:
    chess, _, chess_pgn = require_python_chess()

    needs_stockfish = args.label_source in ("stockfish", "auto")
    stockfish = args.stockfish or shutil.which("stockfish")
    if needs_stockfish and not stockfish:
        raise SystemExit(
            "Stockfish was not found. Pass --stockfish /path/to/stockfish "
            "or install stockfish so it is on PATH."
        )

    stats = ExportStats()
    seen: set[str] = set()
    rng = random.Random(args.seed)

    engine_context = chess.engine.SimpleEngine.popen_uci(stockfish) if needs_stockfish else None

    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    with output_path.open("w", encoding="utf-8", newline="") as csv_file:
        fieldnames = [
            "fen",
            "score_cp",
            "game_id",
            "source_file",
            "ply",
            "side_to_move",
            "kirin_color",
            "result",
            "time_control",
            "eco",
            "white",
            "black",
        ]
        writer = csv.DictWriter(csv_file, fieldnames=fieldnames)
        writer.writeheader()

        with (engine_context if engine_context is not None else contextlib.nullcontext()) as engine:
            if engine is not None:
                if args.threads:
                    engine.configure({"Threads": args.threads})
                if args.hash_mb:
                    engine.configure({"Hash": args.hash_mb})

            for input_path in args.input:
                input_path = Path(input_path)
                with input_path.open("r", encoding="utf-8", errors="replace") as pgn_file:
                    while True:
                        game = chess_pgn.read_game(pgn_file)
                        if game is None:
                            break

                        stats.games_seen += 1
                        if args.limit_games and stats.games_seen > args.limit_games:
                            return stats

                        headers = game.headers
                        if not should_use_game(headers, args):
                            stats.games_skipped += 1
                            continue

                        stats.games_used += 1
                        board = game.board()
                        kirin = kirin_color(headers, args.kirin_name)
                        node = game

                        for ply, move in enumerate(game.mainline_moves(), start=1):
                            if args.label_source == "stockfish":
                                candidate_board = board
                                comment = ""
                            else:
                                board.push(move)
                                node = node.variation(0)
                                candidate_board = board
                                comment = node.comment

                            if should_export_position(candidate_board, ply, kirin, args):
                                stats.positions_seen += 1
                                if not should_keep_random_sample(rng, args):
                                    stats.random_samples_skipped += 1
                                else:
                                    score_cp = None
                                    if args.label_source == "stockfish":
                                        score_cp = analyse_score_cp(chess, engine, candidate_board, args)
                                    if args.label_source in ("embedded", "auto"):
                                        score_cp = parse_embedded_eval_cp(comment, args)
                                    if score_cp is None and args.label_source == "auto":
                                        score_cp = analyse_score_cp(chess, engine, candidate_board, args)

                                    if score_cp is None:
                                        stats.missing_eval_skipped += 1
                                    else:
                                        dedupe_key = normalized_fen(candidate_board)
                                        if args.dedupe and dedupe_key in seen:
                                            stats.duplicates_skipped += 1
                                        else:
                                            seen.add(dedupe_key)
                                            writer.writerow(
                                                {
                                                    "fen": candidate_board.fen(),
                                                    "score_cp": score_cp,
                                                    "game_id": headers.get("GameId", ""),
                                                    "source_file": input_path.name,
                                                    "ply": ply,
                                                    "side_to_move": "white" if candidate_board.turn == chess.WHITE else "black",
                                                    "kirin_color": kirin,
                                                    "result": game_result(headers),
                                                    "time_control": headers.get("TimeControl", ""),
                                                    "eco": headers.get("ECO", ""),
                                                    "white": headers.get("White", ""),
                                                    "black": headers.get("Black", ""),
                                                }
                                            )
                                            stats.positions_written += 1
                                            if candidate_board.turn == chess.WHITE:
                                                stats.white_to_move_written += 1
                                            else:
                                                stats.black_to_move_written += 1

                                            if args.max_positions and stats.positions_written >= args.max_positions:
                                                return stats

                            if args.label_source == "stockfish":
                                board.push(move)
                                node = node.variation(0)

    return stats


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, nargs="+", help="Input Lichess PGN export(s)")
    parser.add_argument("--output", required=True, help="Output labeled CSV")
    parser.add_argument("--stockfish", help="Path to Stockfish binary; defaults to PATH lookup")
    parser.add_argument(
        "--label-source",
        choices=("stockfish", "embedded", "auto"),
        default="stockfish",
        help=(
            "Use local Stockfish labels, embedded PGN [%%eval] labels, or embedded "
            "labels with Stockfish fallback for missing evals"
        ),
    )
    parser.add_argument("--kirin-name", default="KirinChessBot")
    parser.add_argument(
        "--no-kirin-filter",
        action="store_true",
        help="Use all standard games instead of requiring --kirin-name to appear as White or Black",
    )
    parser.add_argument(
        "--positions",
        choices=("all", "kirin-to-move", "opponent-to-move"),
        default="all",
        help="Which pre-move positions to label",
    )
    parser.add_argument("--skip-plies", type=int, default=10)
    parser.add_argument("--max-ply", type=int, default=0)
    parser.add_argument(
        "--sample-every",
        type=int,
        default=1,
        help="Deterministically keep every Nth candidate position after --skip-plies",
    )
    parser.add_argument(
        "--sample-rate",
        type=float,
        default=1.0,
        help="Seeded random keep probability after deterministic filters; use this instead of --sample-every for unbiased downsampling",
    )
    parser.add_argument("--seed", type=int, default=1, help="Random seed for --sample-rate")
    parser.add_argument("--max-positions", type=int, default=0)
    parser.add_argument("--limit-games", type=int, default=0)
    parser.add_argument("--depth", type=int, default=10)
    parser.add_argument("--nodes", type=int, default=0)
    parser.add_argument("--target-clip", type=int, default=2000)
    parser.add_argument("--mate-score", type=int, default=2000)
    parser.add_argument("--threads", type=int, default=0)
    parser.add_argument("--hash-mb", type=int, default=0)
    parser.add_argument("--rated-only", action="store_true")
    parser.add_argument("--min-elo", type=int, default=0, help="Require both PGN players to have at least this Elo")
    parser.add_argument("--exclude-bullet", action="store_true")
    parser.add_argument("--no-dedupe", dest="dedupe", action="store_false")
    parser.set_defaults(dedupe=True)
    args = parser.parse_args()

    if args.sample_every <= 0:
        parser.error("--sample-every must be positive")
    if args.sample_rate <= 0.0 or args.sample_rate > 1.0:
        parser.error("--sample-rate must be in the range (0, 1]")
    if args.skip_plies < 0:
        parser.error("--skip-plies must be non-negative")
    if args.depth <= 0 and args.nodes <= 0:
        parser.error("use either a positive --depth or --nodes")
    if args.min_elo < 0:
        parser.error("--min-elo must be non-negative")
    if args.no_kirin_filter:
        args.kirin_name = ""

    stats = export_positions(args)
    print(
        "PGN labeling complete: "
        f"games_seen={stats.games_seen} "
        f"games_used={stats.games_used} "
        f"games_skipped={stats.games_skipped} "
        f"positions_seen={stats.positions_seen} "
        f"positions_written={stats.positions_written} "
        f"white_to_move_written={stats.white_to_move_written} "
        f"black_to_move_written={stats.black_to_move_written} "
        f"duplicates_skipped={stats.duplicates_skipped}"
        f" random_samples_skipped={stats.random_samples_skipped}"
        f" missing_eval_skipped={stats.missing_eval_skipped}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
