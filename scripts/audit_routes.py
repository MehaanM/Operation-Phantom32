#!/usr/bin/env python3
"""audit_routes.py — Cross-check docs/route-inventory.json against source.

For every route marked auth_required: true in the inventory, verify that
the corresponding source file contains both:
  1. A server.on("<path>", ...) registration.
  2. An auth_check() call inside that handler block.

Exit 0 on success, 1 on any discrepancy.

Usage:
  python3 scripts/audit_routes.py              # from repo root
  python3 scripts/audit_routes.py --verbose    # print each route result
"""

import argparse
import json
import re
import sys
from pathlib import Path


def load_inventory(path: Path) -> list[dict]:
    with path.open() as f:
        data = json.load(f)
    return data["routes"]


def load_source(path: Path) -> str:
    with path.open() as f:
        return f.read()


def find_handler_block(content: str, route_path: str) -> str | None:
    """Return the text of the server.on() handler that registers route_path,
    or None if the registration is not found."""
    pat = re.compile(
        r'server\.on\s*\(\s*"' + re.escape(route_path) + r'".*?(?=server\.on|\Z)',
        re.DOTALL,
    )
    m = pat.search(content)
    return m.group(0) if m else None


def check_route(route: dict, sources: dict[str, str]) -> tuple[bool, str]:
    """Check one route entry. Returns (ok, message)."""
    path       = route["path"]
    method     = route["method"]
    source_key = route["source_file"]
    auth_req   = route["auth_required"]

    if source_key not in sources:
        return False, f"[MISSING SOURCE] {path}: source file '{source_key}' not found on disk"

    content = sources[source_key]
    handler = find_handler_block(content, path)

    if handler is None:
        return False, f"[NOT REGISTERED] {path}: no server.on() found in {source_key}"

    if auth_req and "auth_check" not in handler:
        return False, f"[UNGUARDED] {path}: auth_required=true but no auth_check() in handler ({source_key})"

    if not auth_req and "auth_check" in handler:
        # Non-sensitive route has an auth guard — not an error, just informational.
        return True, f"[OK/INFO] {path}: auth_required=false but auth_check() present (over-protected, fine)"

    return True, f"[OK] {method} {path}"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--verbose", "-v", action="store_true",
                        help="Print result for every route, not just failures")
    parser.add_argument("--inventory", default="docs/route-inventory.json",
                        help="Path to route-inventory.json")
    args = parser.parse_args()

    repo_root = Path(__file__).parent.parent

    inventory_path = repo_root / args.inventory
    if not inventory_path.exists():
        print(f"ERROR: Inventory not found: {inventory_path}", file=sys.stderr)
        return 1

    routes = load_inventory(inventory_path)

    # Pre-load every unique source file referenced in the inventory.
    source_keys = {r["source_file"] for r in routes}
    sources: dict[str, str] = {}
    for key in source_keys:
        full = repo_root / key
        if full.exists():
            sources[key] = load_source(full)
        else:
            sources[key] = ""  # will trigger NOT REGISTERED below

    failures = []
    for route in routes:
        ok, msg = check_route(route, sources)
        if not ok:
            failures.append(msg)
            print(msg)
        elif args.verbose:
            print(msg)

    total    = len(routes)
    guarded  = sum(1 for r in routes if r["auth_required"])
    public   = total - guarded

    print()
    print(f"Route inventory summary:")
    print(f"  Total routes    : {total}")
    print(f"  Auth-required   : {guarded}")
    print(f"  Public          : {public}")
    print(f"  Failures        : {len(failures)}")

    if failures:
        print("\nAudit FAILED.", file=sys.stderr)
        return 1

    print("\nAudit PASSED.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
