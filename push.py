#!/usr/bin/env python3

import re
import subprocess
import sys
import os

# Detect the operating system
is_windows = sys.platform == "win32"

# Choose the right commands for the platform
if is_windows:
    # Windows: run .bat via cmd, and the compiled .exe
    compile_cmd = ["cmd", "/c", "run.bat"]
    bench_cmd = ["crustik.exe", "bench"]   # assuming your compiler outputs crustik.exe
else:
    # Unix-like (Linux/macOS)
    compile_cmd = ["./run.sh"]
    bench_cmd = ["./crustik", "bench"]

# Compile engine
subprocess.run(compile_cmd, check=True)

# Run the benchmark
result = subprocess.run(
    bench_cmd,
    capture_output=True,
    text=True
)

print(result.stdout)

if result.returncode != 0:
    print("Benchmark failed.")
    print(result.stderr)
    sys.exit(result.returncode)

# Extract "Nodes searched"
match = re.search(r"Nodes searched:\s*([0-9]+)", result.stdout)
if not match:
    print("Could not find 'Nodes searched' in output.")
    sys.exit(1)

nodes = match.group(1)

# Stage all changes
subprocess.run(["git", "add", "."], check=True)

# Commit (skip if nothing changed)
commit_message = f"bench:{nodes}"
commit = subprocess.run(
    ["git", "commit", "-m", commit_message],
    text=True,
    capture_output=True,
)

if commit.returncode == 0:
    print(f"Committed with message: {commit_message}")
elif "nothing to commit" in commit.stdout.lower() or "nothing to commit" in commit.stderr.lower():
    print("Nothing to commit.")
else:
    print(commit.stdout)
    print(commit.stderr)
    sys.exit(commit.returncode)

# Push every local branch and set upstream if needed
branches = subprocess.check_output(
    ["git", "for-each-ref", "--format=%(refname:short)", "refs/heads"],
    text=True,
).splitlines()

for branch in branches:
    print(f"Pushing {branch}...")
    subprocess.run(
        ["git", "push", "-u", "origin", branch],
        check=True,
    )

# Push tags (optional)
subprocess.run(["git", "push", "--tags"], check=True)

print("Done! All branches and tags have been pushed.")