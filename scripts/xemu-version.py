#!/usr/bin/env python3
import datetime
import subprocess
import sys

root = sys.argv[1] if len(sys.argv) > 1 else "."

version = "0.0.0"
commit = ""
date = ""

def git(*args):
    try:
        return subprocess.check_output(
            ["git", "-C", root, *args],
            stderr=subprocess.DEVNULL,
            text=True,
        ).strip()
    except (OSError, subprocess.CalledProcessError):
        return ""

if root:
    date = datetime.datetime.now(datetime.timezone.utc).strftime("%a %b %d %H:%M:%S UTC %Y")
    commit = git("rev-parse", "HEAD")
    described = git("describe", "--tags", "--match", "v*")
    if described:
        version = described.removeprefix("v")

parts = version.split("-", 1)
base = parts[0].split(".")
while len(base) < 3:
    base.append("0")
version_major, version_minor, version_patch = base[:3]
version_commit = parts[1] if len(parts) > 1 and parts[1].isdigit() else "0"

print(f'#define XEMU_VERSION       "{version}"')
print(f"#define XEMU_VERSION_MAJOR {version_major}")
print(f"#define XEMU_VERSION_MINOR {version_minor}")
print(f"#define XEMU_VERSION_PATCH {version_patch}")
print(f"#define XEMU_VERSION_COMMIT {version_commit}")
print(f'#define XEMU_COMMIT        "{commit}"')
print(f'#define XEMU_DATE          "{date}"')
