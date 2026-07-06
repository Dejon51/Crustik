#!/usr/bin/env python3

import re
import subprocess
import sys

# Compile Engine
subprocess.run(["./run.sh"], check=True)

# Run the benchmark
result = subprocess.run(
    ["./crustik", "bench"],
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

# Git add
subprocess.run(["git", "add", "."], check=True)

# Git commit
commit_message = f"bench:{nodes}"
subprocess.run(["git", "commit", "-m", commit_message], check=True)

print(f"Committed with message: {commit_message}")
