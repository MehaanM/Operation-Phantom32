#!/usr/bin/env python3
"""
phantom_doctor.py — Phantom32 development environment preflight checker.

Checks that all required tools, Python packages, and configuration
are present and compatible before attempting a build.

Usage:
    python3 scripts/phantom_doctor.py
    python3 scripts/phantom_doctor.py --board doom_s3
    python3 scripts/phantom_doctor.py --json
"""

import sys
import os
import shutil
import subprocess
import json
import argparse

MIN_PYTHON = (3, 8)
MIN_PIO    = (6, 2, 0)

SUPPORTED_BOARDS = [
    "doom", "doom_nodemcu", "doom_m5stack", "doom_m5core2",
    "doom_ttgo", "doom_tcall", "doom_heltec",
    "doom_s3_devkit", "doom_c3",
    "doom_wrover", "doom_s3", "doom_tdisplay_s3", "doom_m5cores3",
]

PSRAM_BOARDS = {"doom_wrover", "doom_s3", "doom_tdisplay_s3", "doom_m5cores3"}

RESET  = "\033[0m"
RED    = "\033[91m"
GREEN  = "\033[92m"
YELLOW = "\033[93m"
CYAN   = "\033[96m"
BOLD   = "\033[1m"


def _c(color, text):
    if sys.stdout.isatty():
        return f"{color}{text}{RESET}"
    return text


class Check:
    def __init__(self, name, ok, detail="", fix=""):
        self.name   = name
        self.ok     = ok
        self.detail = detail
        self.fix    = fix

    def __repr__(self):
        status = _c(GREEN, "✓") if self.ok else _c(RED, "✗")
        line = f"  {status}  {self.name}"
        if self.detail:
            line += f"  {_c(CYAN, self.detail)}"
        if not self.ok and self.fix:
            line += f"\n       {_c(YELLOW, '→')} {self.fix}"
        return line


def check_python():
    v = sys.version_info
    ok = v >= MIN_PYTHON
    return Check(
        "Python version",
        ok,
        f"{v.major}.{v.minor}.{v.micro}",
        f"Install Python {MIN_PYTHON[0]}.{MIN_PYTHON[1]}+  https://python.org" if not ok else "",
    )


def check_platformio():
    pio = shutil.which("pio") or shutil.which("platformio")
    if not pio:
        return Check(
            "PlatformIO CLI",
            False,
            "not found",
            "pip install platformio  or  https://platformio.org/install",
        )
    try:
        result = subprocess.run(
            ["pio", "--version"], capture_output=True, text=True, timeout=15
        )
        ver_str = result.stdout.strip().replace("PlatformIO Core, version ", "").replace("PlatformIO, version ", "")
        parts = tuple(int(x) for x in ver_str.split(".")[:3])
        ok = parts >= MIN_PIO
        return Check(
            "PlatformIO version",
            ok,
            ver_str,
            f"pip install -U platformio  (need ≥ {'.'.join(str(x) for x in MIN_PIO)})" if not ok else "",
        )
    except Exception as e:
        return Check("PlatformIO version", False, str(e), "pip install platformio")


def check_esptool():
    found = shutil.which("esptool.py") or shutil.which("esptool")
    if found:
        try:
            r = subprocess.run(
                ["esptool.py", "version"], capture_output=True, text=True, timeout=10
            )
            ver = r.stdout.strip().splitlines()[0] if r.stdout else "found"
            return Check("esptool.py", True, ver)
        except Exception:
            return Check("esptool.py", True, "found (version unknown)")
    return Check(
        "esptool.py",
        False,
        "not found",
        "pip install esptool  (required for manual flashing; web flasher does not need it)",
    )


def check_git():
    if not shutil.which("git"):
        return Check("git", False, "not found", "Install git  https://git-scm.com")
    try:
        r = subprocess.run(["git", "--version"], capture_output=True, text=True, timeout=5)
        return Check("git", True, r.stdout.strip())
    except Exception as e:
        return Check("git", False, str(e))


def check_platformio_ini(repo_root):
    ini = os.path.join(repo_root, "platformio.ini")
    if not os.path.exists(ini):
        return Check(
            "platformio.ini",
            False,
            "not found",
            f"Run from the repository root (expected at {ini})",
        )
    return Check("platformio.ini", True, ini)


def check_board(board, repo_root):
    if board not in SUPPORTED_BOARDS:
        return Check(
            f"Board '{board}'",
            False,
            "unknown",
            f"Supported boards: {', '.join(SUPPORTED_BOARDS)}",
        )
    tier = "PSRAM" if board in PSRAM_BOARDS else "LITE"
    chip = "ESP32-S3" if "s3" in board else ("ESP32-C3" if "c3" in board else "ESP32")
    return Check(f"Board '{board}'", True, f"{chip} · {tier}")


def check_partitions(repo_root):
    csv = os.path.join(repo_root, "partitions.csv")
    if not os.path.exists(csv):
        return Check(
            "partitions.csv",
            False,
            "not found",
            "Restore partitions.csv from the repository",
        )
    return Check("partitions.csv", True)


def check_src_dir(repo_root):
    src = os.path.join(repo_root, "src", "main.cpp")
    if not os.path.exists(src):
        return Check(
            "src/main.cpp",
            False,
            "not found",
            "Make sure the full repository is checked out",
        )
    return Check("src/main.cpp", True)


def check_python_packages():
    missing = []
    for pkg in ["platformio"]:
        try:
            __import__(pkg)
        except ImportError:
            missing.append(pkg)
    if missing:
        return Check(
            "Python packages",
            False,
            f"missing: {', '.join(missing)}",
            f"pip install {' '.join(missing)}",
        )
    return Check("Python packages", True, "all present")


def repo_root():
    here = os.path.dirname(os.path.abspath(__file__))
    return os.path.dirname(here)


def run(board=None, as_json=False):
    root = repo_root()

    checks = [
        check_python(),
        check_platformio(),
        check_esptool(),
        check_git(),
        check_platformio_ini(root),
        check_partitions(root),
        check_src_dir(root),
    ]
    if board:
        checks.append(check_board(board, root))

    passed  = sum(1 for c in checks if c.ok)
    failed  = len(checks) - passed
    all_ok  = failed == 0

    if as_json:
        out = {
            "passed": passed,
            "failed": failed,
            "ok": all_ok,
            "checks": [
                {
                    "name":   c.name,
                    "ok":     c.ok,
                    "detail": c.detail,
                    "fix":    c.fix,
                }
                for c in checks
            ],
        }
        print(json.dumps(out, indent=2))
        return 0 if all_ok else 1

    print()
    print(_c(BOLD, "  Phantom32 — preflight check"))
    print(_c(CYAN, f"  Repository: {root}"))
    if board:
        print(_c(CYAN, f"  Target board: {board}"))
    print()

    for c in checks:
        print(repr(c))

    print()
    if all_ok:
        print(_c(GREEN, f"  ✓ All {passed} checks passed. Ready to build."))
        if board:
            print(_c(CYAN, f"\n  Build command:  pio run -e {board}"))
            print(_c(CYAN, f"  Test command:   pio test -e native"))
    else:
        print(_c(RED,    f"  ✗ {failed} check(s) failed. Resolve the issues above before building."))
        print(_c(YELLOW, "    See docs/installation.md for a full setup guide."))
    print()

    return 0 if all_ok else 1


def main():
    parser = argparse.ArgumentParser(
        description="Phantom32 development environment preflight checker.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="Supported boards:\n  " + "\n  ".join(SUPPORTED_BOARDS),
    )
    parser.add_argument(
        "--board", metavar="ENV",
        help="Validate a specific board environment (e.g. doom_s3)",
    )
    parser.add_argument(
        "--json", action="store_true",
        help="Output results as JSON (for CI/tooling integration)",
    )
    args = parser.parse_args()
    sys.exit(run(board=args.board, as_json=args.json))


if __name__ == "__main__":
    main()
