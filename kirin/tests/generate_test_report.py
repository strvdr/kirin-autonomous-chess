#!/usr/bin/env python3
"""
generate_test_report.py
Run all Kirin test binaries and write a GitHub-flavoured Markdown report to
tests/TEST_RESULTS.md (relative to the project root).

Usage (from the build directory):
    python3 ../tests/generate_test_report.py [--build-dir BUILD_DIR] [--output OUTPUT]

CMake invokes it as a custom target:
    cmake --build build --target test_report
"""

import argparse
import os
import re
import subprocess
import sys
from datetime import datetime, timezone

# ── Configuration ─────────────────────────────────────────────────────────────

TESTS = [
    {
        "binary":      "captured_piece_test",
        "name":        "Captured Piece Detection",
        "description": "Move encoding — captured piece bit-field round-trips for all 12 piece types.",
    },
    {
        "binary":      "board_interpreter_test",
        "name":        "Board Interpreter",
        "description": "Physical board path planning, relocation/restoration logic, knight routing, edge cases.",
    },
    {
        "binary":      "game_controller_test",
        "name":        "Game Controller",
        "description": "Integration layer: coordinate conversion, piece conversion, PhysicalBoard sync, "
                       "parseBoardMove, isGameOver, hardware-gated functions.",
    },
    {
        "binary":      "engine_test",
        "name":        "Engine Validity & Skill Level",
        "description": "Perft node counts (4 positions × up to depth 4), evaluation sanity, "
                       "tactical position structure, skill-level globals, repetition detection.",
    },
]

ANSI_ESCAPE = re.compile(r"\x1b\[[0-9;]*m")
PASS_RE     = re.compile(r"[✔✓]|^\[PASS\]|\bPASS\b", re.IGNORECASE)
FAIL_RE     = re.compile(r"[✗✘]|^\[FAIL\]|\bFAIL\b", re.IGNORECASE)
SUITE_RE    = re.compile(r"──\s+(.+?)\s+──")
SUMMARY_RE  = re.compile(r"(\d+)/(\d+)\s+(?:tests\s+)?passed", re.IGNORECASE)


# ── Helpers ───────────────────────────────────────────────────────────────────

def strip_ansi(text: str) -> str:
    return ANSI_ESCAPE.sub("", text)


def run_binary(path: str):
    """Run a test binary and return (returncode, clean_output_lines)."""
    try:
        result = subprocess.run(
            [path],
            capture_output=True,
            text=True,
            timeout=120,
        )
        raw = result.stdout + result.stderr
    except FileNotFoundError:
        return -1, [f"ERROR: binary not found: {path}"]
    except subprocess.TimeoutExpired:
        return -1, ["ERROR: test timed out after 120 s"]

    lines = [strip_ansi(ln) for ln in raw.splitlines()]
    return result.returncode, lines


def parse_output(lines):
    """
    Return a list of suite dicts:
        { "name": str, "cases": [{"label": str, "passed": bool}] }
    and an overall (passed, total) tuple.
    """
    suites   = []
    current  = {"name": "General", "cases": []}
    total_p  = 0
    total_t  = 0

    for line in lines:
        # Suite header  ── Name ──
        m = SUITE_RE.search(line)
        if m:
            if current["cases"]:
                suites.append(current)
            current = {"name": m.group(1).strip(), "cases": []}
            continue

        # Summary line  e.g. "Results: 10/10 passed"
        m = SUMMARY_RE.search(line)
        if m:
            total_p += int(m.group(1))
            total_t += int(m.group(2))
            continue

        stripped = line.strip()
        if not stripped:
            continue

        # Individual test result
        if PASS_RE.search(stripped):
            label = PASS_RE.sub("", stripped).strip().lstrip("–-:").strip()
            current["cases"].append({"label": label, "passed": True})
        elif FAIL_RE.search(stripped):
            label = FAIL_RE.sub("", stripped).strip().lstrip("–-:").strip()
            # Capture optional [line N] suffix
            current["cases"].append({"label": label, "passed": False})

    if current["cases"]:
        suites.append(current)

    return suites, (total_p, total_t)


def badge(passed: int, total: int) -> str:
    """Return a shields.io badge URL for the pass rate."""
    if total == 0:
        color, label = "lightgrey", "no tests"
    elif passed == total:
        color, label = "brightgreen", f"{passed}%2F{total}+passing"
    elif passed / total >= 0.8:
        color, label = "yellow", f"{passed}%2F{total}+passing"
    else:
        color, label = "red", f"{passed}%2F{total}+passing"
    return f"https://img.shields.io/badge/tests-{label}-{color}"


# ── Markdown generation ───────────────────────────────────────────────────────

def build_markdown(results, timestamp: str) -> str:
    lines = []

    # ── Title & timestamp ────────────────────────────────────────────────────
    lines += [
        "# Kirin Chess Engine — Test Results",
        "",
        f"> Generated: {timestamp}  ",
        f"> Run `cmake --build build --target test_report` to refresh.",
        "",
    ]

    # ── Summary table ────────────────────────────────────────────────────────
    grand_pass = sum(r["passed"] for r in results)
    grand_total = sum(r["total"]  for r in results)

    badge_url = badge(grand_pass, grand_total)
    lines += [
        f"![tests]({badge_url})",
        "",
        "## Summary",
        "",
        "| Test Suite | Result | Passed | Failed | Time |",
        "|---|---|---|---|---|",
    ]

    for r in results:
        icon   = "✅" if r["failed"] == 0 and r["returncode"] == 0 else "❌"
        p, f   = r["passed"], r["failed"]
        t      = f"{r['elapsed']:.2f} s"
        lines.append(f"| {r['name']} | {icon} | {p} | {f} | {t} |")

    lines += [
        f"| **Total** | {'✅' if grand_pass == grand_total else '❌'} "
        f"| **{grand_pass}** | **{grand_total - grand_pass}** | — |",
        "",
    ]

    # ── Per-suite detail ─────────────────────────────────────────────────────
    lines += ["## Detail", ""]

    for r in results:
        # Suite header
        suite_icon = "✅" if r["failed"] == 0 and r["returncode"] == 0 else "❌"
        lines += [
            f"### {suite_icon} {r['name']}",
            "",
            f"_{r['description']}_",
            "",
        ]

        if r["returncode"] == -1:
            lines += [
                "> ⚠️ **Binary not found or failed to launch.**",
                "> Build the project first: `cmake --build build`",
                "",
            ]
            continue

        if r["returncode"] != 0 and not r["suites"]:
            lines += [
                f"> ⚠️ **Process exited with code {r['returncode']} and produced no parseable output.**",
                "",
            ]

        for suite in r["suites"]:
            # Collapsible block — open if any failures
            any_fail   = any(not c["passed"] for c in suite["cases"])
            open_attr  = " open" if any_fail else ""
            s_pass     = sum(1 for c in suite["cases"] if c["passed"])
            s_total    = len(suite["cases"])
            summary_txt = (
                f"{suite['name']} &nbsp;—&nbsp; "
                f"{s_pass}/{s_total} passed"
            )

            lines += [
                f"<details{open_attr}>",
                f"<summary><b>{summary_txt}</b></summary>",
                "",
            ]

            for case in suite["cases"]:
                icon  = "✅" if case["passed"] else "❌"
                label = case["label"] if case["label"] else "_(unnamed)_"
                lines.append(f"- {icon} {label}")

            lines += ["", "</details>", ""]

        lines.append("")

    # ── Footer ───────────────────────────────────────────────────────────────
    lines += [
        "---",
        "",
        "_Kirin Chess Engine · Colorado School of Mines ProtoFund 2026_",
        "",
    ]

    return "\n".join(lines)


# ── Main ──────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description="Generate Kirin test report.")
    parser.add_argument("--build-dir", default=".",
                        help="Directory containing compiled test binaries (default: .)")
    parser.add_argument("--output", default=None,
                        help="Output markdown path (default: <project_root>/tests/TEST_RESULTS.md)")
    args = parser.parse_args()

    build_dir   = os.path.abspath(args.build_dir)
    script_dir  = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(script_dir)  # tests/ is one level below root
    output_path = args.output or os.path.join(project_root, "tests", "TEST_RESULTS.md")

    timestamp = datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M UTC")

    print(f"\nKirin Test Report Generator")
    print(f"Build dir : {build_dir}")
    print(f"Output    : {output_path}")
    print(f"Timestamp : {timestamp}\n")

    results = []

    for test in TESTS:
        binary_path = os.path.join(build_dir, test["binary"])
        print(f"  Running {test['binary']} ...", end="", flush=True)

        t_start = datetime.now()
        rc, output_lines = run_binary(binary_path)
        elapsed = (datetime.now() - t_start).total_seconds()

        suites, (tot_p, tot_t) = parse_output(output_lines)

        # Count from parsed cases if summary line wasn't found
        if tot_t == 0:
            for s in suites:
                tot_p += sum(1 for c in s["cases"] if c["passed"])
                tot_t += len(s["cases"])

        failed = tot_t - tot_p
        status = "PASS" if rc == 0 else "FAIL"
        print(f" {status}  ({tot_p}/{tot_t} in {elapsed:.2f}s)")

        results.append({
            "name":        test["name"],
            "description": test["description"],
            "returncode":  rc,
            "elapsed":     elapsed,
            "passed":      tot_p,
            "failed":      failed,
            "total":       tot_t,
            "suites":      suites,
        })

    md = build_markdown(results, timestamp)

    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, "w", encoding="utf-8") as f:
        f.write(md)

    grand_pass  = sum(r["passed"] for r in results)
    grand_total = sum(r["total"]  for r in results)
    grand_fail  = grand_total - grand_pass

    print(f"\nResults: {grand_pass}/{grand_total} passed, {grand_fail} failed")
    print(f"Report written to: {output_path}\n")

    sys.exit(0 if grand_fail == 0 else 1)


if __name__ == "__main__":
    main()
