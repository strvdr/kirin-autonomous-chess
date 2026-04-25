#!/usr/bin/env python3
"""Convert Kirin NNUE JSON checkpoints to loadable .knnue files."""

from __future__ import annotations

import argparse
from pathlib import Path

from nnue_format import network_from_json, network_to_json, read_knnue, write_knnue


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, help="Input JSON checkpoint or .knnue file")
    parser.add_argument("--output", required=True, help="Output .knnue or JSON path")
    parser.add_argument(
        "--to-json",
        action="store_true",
        help="Convert a .knnue file back to JSON for inspection",
    )
    args = parser.parse_args()

    input_path = Path(args.input)
    output_path = Path(args.output)

    if args.to_json:
        network = read_knnue(input_path)
        network_to_json(output_path, network)
    else:
        network = network_from_json(input_path)
        write_knnue(output_path, network)

    print(f"Wrote {output_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
