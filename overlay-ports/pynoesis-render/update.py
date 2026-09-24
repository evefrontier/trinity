#!/usr/bin/env python3
# Copyright © 2026 CCP ehf.
"""Fill the pynoesis-render overlay port from a published pynoesis.

    py overlay-ports/pynoesis-render/update.py
    py overlay-ports/pynoesis-render/update.py <branch, tag or full commit sha>
    py overlay-ports/pynoesis-render/update.py --repo ../pynoesis main

Fetches one commit of evefrontier/pynoesis, copies include/pynr.h and include/pynr_python.h
into include/ byte for byte, sets the port version to the ABI version those headers
declare, and records the commit in vcpkg.json so a header diff says where it came from.

The fetch is plain git, so it authenticates the way any clone of the repository does.
--repo takes any git remote, including a local checkout, for trying out headers that are
not pushed yet.
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path

PORT_DIR = Path(__file__).resolve().parent

DEFAULT_REPO = "https://github.com/evefrontier/pynoesis.git"

HEADERS = ["pynr.h", "pynr_python.h"]


def git(repo: Path, *args: str) -> bytes:
    completed = subprocess.run(["git", *args], cwd=repo, stdout=subprocess.PIPE)
    if completed.returncode:
        # git has already said why on stderr.
        sys.exit(f"git {args[0]} failed")
    return completed.stdout


def fetch_headers(repo_url: str, ref: str) -> tuple[str, dict[str, bytes]]:
    """Return the commit `ref` names and the headers as that commit stores them."""
    # The fetch runs from a scratch repository, so a local checkout has to be absolute.
    if Path(repo_url).is_dir():
        repo_url = str(Path(repo_url).resolve())
    # On Windows a failed fetch can leave the scratch repository locked for a moment.
    with tempfile.TemporaryDirectory(ignore_cleanup_errors=True) as scratch:
        scratch_repo = Path(scratch)
        git(scratch_repo, "init", "--quiet")
        git(scratch_repo, "fetch", "--quiet", "--depth", "1", "--no-tags", repo_url, ref)
        commit = git(scratch_repo, "rev-parse", "FETCH_HEAD").decode().strip()
        headers = {name: git(scratch_repo, "show", f"FETCH_HEAD:include/{name}") for name in HEADERS}
    return commit, headers


def abi_version(header: bytes) -> str:
    """The ABI version pynr.h declares, read the same way portfile.cmake reads it."""
    parts = []
    for part in ("MAJOR", "MINOR"):
        match = re.search(rb"^#define PYNR_ABI_VERSION_%s ([0-9]+)$" % part.encode(), header, re.MULTILINE)
        if not match:
            sys.exit(f"pynr.h does not define PYNR_ABI_VERSION_{part}")
        parts.append(match.group(1).decode())
    return ".".join(parts)


def update_manifest(version: str, commit: str) -> str:
    """Set the port version and source commit, and return the version it replaced."""
    manifest_path = PORT_DIR / "vcpkg.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    previous = manifest["version"]
    # vcpkg ignores fields that start with "$", and format-manifest sorts them first.
    manifest = {"$pynoesis-commit": commit, **{k: v for k, v in manifest.items() if k != "$pynoesis-commit"}}
    manifest["version"] = version
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return previous


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("ref", nargs="?", default="main",
                        help="pynoesis branch, tag or full commit sha to take the headers from (default: main)")
    parser.add_argument("--repo", default=DEFAULT_REPO,
                        help=f"git remote or local checkout to fetch from (default: {DEFAULT_REPO})")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)

    commit, headers = fetch_headers(args.repo, args.ref)
    version = abi_version(headers["pynr.h"])

    include_dir = PORT_DIR / "include"
    include_dir.mkdir(exist_ok=True)
    for name, content in headers.items():
        (include_dir / name).write_bytes(content)
    previous = update_manifest(version, commit)

    print(f"pynoesis-render {version} from pynoesis {commit}")
    if previous.split(".")[0] != version.split(".")[0]:
        # SameMajorVersion refuses the new port until the consumers ask for the new major.
        print(f"The ABI major changed from {previous}: bump find_package(pynoesis-render) in "
              f"trinity/CMakeLists.txt and the noesis feature's version>= in vcpkg.json to match.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
