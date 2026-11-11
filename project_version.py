#!/usr/bin/env python3
import json
import subprocess
import sys

if __name__ == "__main__":
    cargo = sys.argv[1]
    metadata = subprocess.check_output(
        [cargo, "metadata", "--frozen", "--format-version=1", "--no-deps"]
    )
    print(json.loads(metadata)["packages"][0]["version"])
