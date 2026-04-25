#!/usr/bin/env python3
"""Convert PGN games into Stockfish-labeled Kirin NNUE training CSV."""

from __future__ import annotations

import argparse
import csv
import shutil
import sys
from dataclasses import dataclass
from pathlib import Path


@dataclass
class ExportStats:
    games_seen: int = 0
    games_used: int = 0
    positions_seen: int = 0
    positions_written: int = 0
    duplicates_skipped: int = 0
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


def should_use_game(headers, args) -> bool:
    if headers.get("Variant", "Standard") != "Standard":
        return False
    if args.rated_only and not headers.get("Event", "").lower().startswith("rated "):
        return False
    if args.kirin_name and not kirin_color(headers, args.kirin_name):
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


def export_positions(args) -> ExportStats:
    chess, _, chess_pgn = require_python_chess()

    stockfish = args.stockfish or shutil.which("stockfish")
    if not stockfish:
        raise SystemExit(
            "Stockfish was not found. Pass --stockfish /path/to/stockfish "
            "or install stockfish so it is on PATH."
        )

    stats = ExportStats()
    seen: set[str] = set()

    with Path(args.input).open("r", encoding="utf-8", errors="replace") as pgn_file:
        with Path(args.output).open("w", encoding="utf-8", newline="") as csv_file:
            fieldnames = [
                "fen",
                "score_cp",
                "game_id",
                "ply",
                "kirin_color",
                "result",
                "time_control",
                "eco",
                "white",
                "black",
            ]
            writer = csv.DictWriter(csv_file, fieldnames=fieldnames)
            writer.writeheader()

            with chess.engine.SimpleEngine.popen_uci(stockfish) as engine:
                if args.threads:
                    engine.configure({"Threads": args.threads})
                if args.hash_mb:
                    engine.configure({"Hash": args.hash_mb})

                while True:
                    game = chess_pgn.read_game(pgn_file)
                    if game is None:
                        break

                    stats.games_seen += 1
                    if args.limit_games and stats.games_seen > args.limit_games:
                        break

                    headers = game.headers
                    if not should_use_game(headers, args):
                        stats.games_skipped += 1
                        continue

                    stats.games_used += 1
                    board = game.board()
                    kirin = kirin_color(headers, args.kirin_name)

                    for ply, move in enumerate(game.mainline_moves(), start=1):
                        if should_export_position(board, ply, kirin, args):
                            stats.positions_seen += 1
                            dedupe_key = normalized_fen(board)
                            if args.dedupe and dedupe_key in seen:
                                stats.duplicates_skipped += 1
                            else:
                                seen.add(dedupe_key)
                                score_cp = analyse_score_cp(chess, engine, board, args)
                                writer.writerow(
                                    {
                                        "fen": board.fen(),
                                        "score_cp": score_cp,
                                        "game_id": headers.get("GameId", ""),
                                        "ply": ply,
                                        "kirin_color": kirin,
                                        "result": game_result(headers),
                                        "time_control": headers.get("TimeControl", ""),
                                        "eco": headers.get("ECO", ""),
                                        "white": headers.get("White", ""),
                                        "black": headers.get("Black", ""),
                                    }
                                )
                                stats.positions_written += 1

                                if args.max_positions and stats.positions_written >= args.max_positions:
                                    return stats

                        board.push(move)

    return stats


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, help="Input Lichess PGN export")
    parser.add_argument("--output", required=True, help="Output labeled CSV")
    parser.add_argument("--stockfish", help="Path to Stockfish binary; defaults to PATH lookup")
    parser.add_argument("--kirin-name", default="KirinChessBot")
    parser.add_argument(
        "--positions",
        choices=("all", "kirin-to-move", "opponent-to-move"),
        default="all",
        help="Which pre-move positions to label",
    )
    parser.add_argument("--skip-plies", type=int, default=10)
    parser.add_argument("--max-ply", type=int, default=0)
    parser.add_argument("--sample-every", type=int, default=2)
    parser.add_argument("--max-positions", type=int, default=0)
    parser.add_argument("--limit-games", type=int, default=0)
    parser.add_argument("--depth", type=int, default=10)
    parser.add_argument("--nodes", type=int, default=0)
    parser.add_argument("--target-clip", type=int, default=2000)
    parser.add_argument("--mate-score", type=int, default=2000)
    parser.add_argument("--threads", type=int, default=0)
    parser.add_argument("--hash-mb", type=int, default=0)
    parser.add_argument("--rated-only", action="store_true")
    parser.add_argument("--exclude-bullet", action="store_true")
    parser.add_argument("--no-dedupe", dest="dedupe", action="store_false")
    parser.set_defaults(dedupe=True)
    args = parser.parse_args()

    if args.sample_every <= 0:
        parser.error("--sample-every must be positive")
    if args.skip_plies < 0:
        parser.error("--skip-plies must be non-negative")
    if args.depth <= 0 and args.nodes <= 0:
        parser.error("use either a positive --depth or --nodes")

    stats = export_positions(args)
    print(
        "PGN labeling complete: "
        f"games_seen={stats.games_seen} "
        f"games_used={stats.games_used} "
        f"games_skipped={stats.games_skipped} "
        f"positions_seen={stats.positions_seen} "
        f"positions_written={stats.positions_written} "
        f"duplicates_skipped={stats.duplicates_skipped}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
