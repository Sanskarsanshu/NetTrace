#!/usr/bin/env python3
"""
Validates all files in examples/*.json against schemas/topology.schema.json.
"""

import json
import os
import sys
from pathlib import Path

def main() -> int:
    is_ci = os.environ.get("CI") is not None
    try:
        import jsonschema
    except ImportError:
        if is_ci:
            print("ERROR: jsonschema package is required in CI but is not installed.", file=sys.stderr)
            return 1
        print("SKIPPED: jsonschema not installed")
        return 0

    repo_root = Path(__file__).resolve().parent.parent
    schema_path = repo_root / "schemas" / "topology.schema.json"
    examples_dir = repo_root / "examples"

    if not schema_path.exists():
        print(f"ERROR: Schema not found at {schema_path}", file=sys.stderr)
        return 1

    with open(schema_path, "r", encoding="utf-8") as f:
        schema = json.load(f)

    validator = jsonschema.Draft7Validator(schema)

    json_files = sorted(examples_dir.glob("*.json"))
    if not json_files:
        print(f"ERROR: No example JSON files found in {examples_dir}", file=sys.stderr)
        return 1

    print(f"Validating {len(json_files)} example files against {schema_path.name}...")
    errors_found = False

    for json_file in json_files:
        with open(json_file, "r", encoding="utf-8") as f:
            try:
                data = json.load(f)
            except Exception as e:
                print(f"  [X] {json_file.name}: Malformed JSON - {e}", file=sys.stderr)
                errors_found = True
                continue

        errors = sorted(validator.iter_errors(data), key=lambda e: e.path)
        if errors:
            errors_found = True
            print(f"  [X] {json_file.name}: {len(errors)} validation error(s):", file=sys.stderr)
            for err in errors:
                path = "/".join(str(p) for p in err.path) or "root"
                print(f"      - {path}: {err.message}", file=sys.stderr)
        else:
            print(f"  [OK] {json_file.name}")

    if errors_found:
        return 1

    print("SUCCESS: All example topologies are valid against the schema!")
    return 0

if __name__ == "__main__":
    sys.exit(main())
