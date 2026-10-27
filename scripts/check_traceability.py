#!/usr/bin/env python3
"""
Traceability and Test Registration Verification Script (Standard Library Only).

Ensures bidirectional consistency across:
1. engine/CMakeLists.txt (registered ctest test targets)
2. docs/traceability.md (test execution tables and traceability matrix)
3. TASKS.md (ticked/completed test checkboxes)
"""

import os
import re
import sys
from pathlib import Path

# Known CI-only test targets (not registered in standard ctest binary)
CI_ONLY_TESTS = {"T-1-18", "T-2-34"}

def find_repo_root() -> Path:
    current = Path(__file__).resolve().parent
    while current != current.parent:
        if (current / "engine" / "CMakeLists.txt").exists():
            return current
        current = current.parent
    return Path.cwd()

def get_cmake_tests(cmake_path: Path) -> set:
    content = cmake_path.read_text(encoding="utf-8")
    # Matches add_test(NAME T-X-YY ...)
    pattern = re.compile(r"add_test\s*\(\s*NAME\s+([A-Za-z0-9_-]+)", re.IGNORECASE)
    return set(pattern.findall(content))

def get_traceability_tests(traceability_path: Path) -> tuple[set, set]:
    """
    Returns (all_traceability_tests, execution_table_tests)
    where execution_table_tests are tests from Test Execution Mapping tables
    or rows explicitly marked 'passed' or 'implemented'.
    """
    content = traceability_path.read_text(encoding="utf-8")
    all_tests = set(re.findall(r"\b(T-\d+-\d+)\b", content))
    
    execution_tests = set()
    in_execution_table = False

    for line in content.splitlines():
        line_strip = line.strip()
        if line_strip.startswith("## ") and "Test Execution Mapping" in line_strip:
            in_execution_table = True
            continue
        elif line_strip.startswith("## "):
            in_execution_table = False

        if in_execution_table:
            # Matches | T-X-YY | ... | status |
            match = re.match(r"^\|\s*(T-\d+-\d+)\s*\|", line_strip)
            if match:
                execution_tests.add(match.group(1))
        else:
            # In other tables, if the row is marked 'implemented' or 'passed'
            if re.search(r"\|\s*(?:implemented|passed)\s*\|?$", line_strip, re.IGNORECASE):
                for tid in re.findall(r"\b(T-\d+-\d+)\b", line_strip):
                    execution_tests.add(tid)

    return all_tests, execution_tests

def get_tasks_completed_tests(tasks_path: Path) -> set:
    content = tasks_path.read_text(encoding="utf-8")
    # Matches - [x] T-X-YY
    pattern = re.compile(r"-\s*\[x\]\s*(T-\d+-\d+)\b", re.IGNORECASE)
    return set(pattern.findall(content))

def main() -> int:
    repo_root = find_repo_root()
    cmake_path = repo_root / "engine" / "CMakeLists.txt"
    traceability_path = repo_root / "docs" / "traceability.md"
    tasks_path = repo_root / "TASKS.md"

    if not cmake_path.exists():
        print(f"ERROR: CMakeLists.txt not found at {cmake_path}", file=sys.stderr)
        return 1
    if not traceability_path.exists():
        print(f"ERROR: traceability.md not found at {traceability_path}", file=sys.stderr)
        return 1
    if not tasks_path.exists():
        print(f"ERROR: TASKS.md not found at {tasks_path}", file=sys.stderr)
        return 1

    cmake_tests = get_cmake_tests(cmake_path)
    all_traceability_tests, execution_tests = get_traceability_tests(traceability_path)
    tasks_completed_tests = get_tasks_completed_tests(tasks_path)

    print("==================================================")
    print("   NetTrace Traceability & CTest Verification     ")
    print("==================================================")
    print(f"Registered in CMake ({len(cmake_tests)}): {sorted(cmake_tests)}")
    print(f"Completed in TASKS.md ({len(tasks_completed_tests)}): {sorted(tasks_completed_tests)}")
    print(f"In Traceability Execution Tables ({len(execution_tests)}): {sorted(execution_tests)}")
    print("--------------------------------------------------")

    errors = []

    # Check (a1): Ticked tests in TASKS.md must be registered in CMake (unless CI-only)
    for test_id in sorted(tasks_completed_tests):
        if test_id in CI_ONLY_TESTS:
            continue
        if test_id not in cmake_tests:
            errors.append(f"Test '{test_id}' is marked completed in TASKS.md but NOT registered in engine/CMakeLists.txt")

    # Check (a2): Tests in traceability execution table must be registered in CMake (unless CI-only)
    for test_id in sorted(execution_tests):
        if test_id in CI_ONLY_TESTS:
            continue
        if test_id not in cmake_tests:
            errors.append(f"Test '{test_id}' is in docs/traceability.md execution mapping but NOT registered in engine/CMakeLists.txt")

    # Check (b): Every test registered in CMake must be in docs/traceability.md
    for test_id in sorted(cmake_tests):
        if test_id not in all_traceability_tests:
            errors.append(f"Test '{test_id}' is registered in engine/CMakeLists.txt but missing from docs/traceability.md")

    if errors:
        print(f"FAILED with {len(errors)} error(s):", file=sys.stderr)
        for err in errors:
            print(f"  [X] {err}", file=sys.stderr)
        return 1

    print(f"SUCCESS: All {len(cmake_tests)} registered tests are verified and fully traceable!")
    if CI_ONLY_TESTS:
        print(f"NOTE: CI-only tests verified separately via CI logs: {sorted(CI_ONLY_TESTS)}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
