#!/usr/bin/env python3
"""Write a deterministic Kirin NNUE v1 bootstrap network."""

from __future__ import annotations

import argparse
from pathlib import Path

from nnue_format import material_bootstrap_network, network_to_json, write_knnue


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, help="Path to write .knnue file")
    parser.add_argument(
        "--json",
        help="Optional path to also write a JSON checkpoint for inspection",
    )
    args = parser.parse_args()

    network = material_bootstrap_network()
    output = Path(args.output)
    write_knnue(output, network)
    print(f"Wrote {output}")

    if args.json:
        json_output = Path(args.json)
        network_to_json(json_output, network)
        print(f"Wrote {json_output}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
