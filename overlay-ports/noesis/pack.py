#!/usr/bin/env python3
# Copyright © 2026 CCP ehf.
"""Build a trimmed NoesisGUI zip for the overlay (and later registry) port.

The zip is the compile-time SDK Trinity consumes: headers, import libs, shared
libraries, and the two D3D12 render-device HLSL files. It is not a full SDK.

Example:
    python overlay-ports/noesis/pack.py --sdk C:\\code\\noesis-sdk-win-4.0.0 --platform windows
    python overlay-ports/noesis/pack.py --sdk C:\\code\\noesis-sdk-macos-4.0.0 --platform macos --hlsl-sdk C:\\code\\noesis-sdk-win-4.0.0
"""
from __future__ import annotations

import argparse
import hashlib
import os
import shutil
import sys
import tempfile
import zipfile
from pathlib import Path


PORT_DIR = Path(__file__).resolve().parent
SHADER_SRC = Path("Src/Packages/Render/D3D12RenderDevice/Src")
WINDOWS_ARCH = "windows_x86_64"
MACOS_BIN_CANDIDATES = (
    "macos_arm64",
    "osx_arm64",
    "macos",
    "osx",
    "macos_x86_64",
    "osx_x86_64",
)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--sdk",
        type=Path,
        default=os.environ.get("NOESIS_GUI_SDK_PATH"),
        help="Path to a full NoesisGUI SDK (default: NOESIS_GUI_SDK_PATH)",
    )
    parser.add_argument(
        "--platform",
        choices=("windows", "macos"),
        default="windows" if os.name == "nt" else "macos",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=PORT_DIR / "dist",
        help="Directory to write the zip into (default: overlay-ports/noesis/dist)",
    )
    parser.add_argument(
        "--hlsl-sdk",
        type=Path,
        help="SDK tree to take ShaderVS/ShaderPS.hlsl from (default: --sdk). "
        "The macOS SDK often omits Src/; pass the Windows SDK there.",
    )
    parser.add_argument(
        "--version",
        default="4.0.0",
        help="Version token used in the zip filename (must match the port's version)",
    )
    args = parser.parse_args()

    if not args.sdk:
        parser.error("pass --sdk or set NOESIS_GUI_SDK_PATH")
    sdk = args.sdk.expanduser().resolve()
    if not sdk.is_dir():
        parser.error("SDK directory not found: %s" % sdk)

    hlsl_sdk = (args.hlsl_sdk.expanduser().resolve() if args.hlsl_sdk else sdk)
    if args.hlsl_sdk and not hlsl_sdk.is_dir():
        parser.error("HLSL SDK directory not found: %s" % hlsl_sdk)

    staging = Path(tempfile.mkdtemp(prefix="noesis-pack-"))
    try:
        if args.platform == "windows":
            stage_windows(sdk, staging)
        else:
            stage_macos(sdk, staging)
        stage_common(sdk, hlsl_sdk, staging)

        args.output.mkdir(parents=True, exist_ok=True)
        zip_path = args.output / ("noesis-v%s-%s.zip" % (args.version, args.platform))
        write_zip(staging, zip_path)
    finally:
        shutil.rmtree(staging, ignore_errors=True)

    digest = sha512_file(zip_path)
    print("Wrote %s" % zip_path)
    print("SHA512 %s" % digest)
    return 0


def stage_common(sdk: Path, hlsl_sdk: Path, staging: Path) -> None:
    include = sdk / "Include"
    require_dir(include, "Include")
    copy_tree(include, staging / "include")

    vs = hlsl_sdk / SHADER_SRC / "ShaderVS.hlsl"
    ps = hlsl_sdk / SHADER_SRC / "ShaderPS.hlsl"
    require_file(vs, "ShaderVS.hlsl")
    require_file(ps, "ShaderPS.hlsl")
    shaders = staging / "shaders"
    shaders.mkdir(parents=True, exist_ok=True)
    shutil.copy2(vs, shaders / "ShaderVS.hlsl")
    shutil.copy2(ps, shaders / "ShaderPS.hlsl")

    for name in ("version.txt", "THIRD_PARTY.txt"):
        src = sdk / name
        if src.is_file():
            shutil.copy2(src, staging / name)


def stage_windows(sdk: Path, staging: Path) -> None:
    lib = sdk / "Lib" / WINDOWS_ARCH
    bin_dir = sdk / "Bin" / WINDOWS_ARCH
    require_dir(lib, "Lib/%s" % WINDOWS_ARCH)
    require_dir(bin_dir, "Bin/%s" % WINDOWS_ARCH)
    (staging / "lib").mkdir(parents=True, exist_ok=True)
    (staging / "bin").mkdir(parents=True, exist_ok=True)
    for name in ("Noesis", "NoesisEditor"):
        shutil.copy2(require_file(lib / ("%s.lib" % name), "%s.lib" % name), staging / "lib" / ("%s.lib" % name))
        shutil.copy2(require_file(bin_dir / ("%s.dll" % name), "%s.dll" % name), staging / "bin" / ("%s.dll" % name))


def stage_macos(sdk: Path, staging: Path) -> None:
    arch, bin_dir = find_macos_bin(sdk)
    print("Using macOS Bin/%s" % arch)
    (staging / "bin").mkdir(parents=True, exist_ok=True)
    for src_name, dest_name in (
        ("libNoesis.dylib", "libNoesis.dylib"),
        ("Noesis.dylib", "libNoesis.dylib"),
        ("libNoesisEditor.dylib", "libNoesisEditor.dylib"),
        ("NoesisEditor.dylib", "libNoesisEditor.dylib"),
    ):
        src = bin_dir / src_name
        dest = staging / "bin" / dest_name
        if src.is_file() and not dest.exists():
            shutil.copy2(src, dest)
    require_file(staging / "bin" / "libNoesis.dylib", "libNoesis.dylib")
    require_file(staging / "bin" / "libNoesisEditor.dylib", "libNoesisEditor.dylib")


def find_macos_bin(sdk: Path) -> tuple[str, Path]:
    for arch in MACOS_BIN_CANDIDATES:
        bin_dir = sdk / "Bin" / arch
        if (bin_dir / "libNoesis.dylib").is_file() or (bin_dir / "Noesis.dylib").is_file():
            return arch, bin_dir
    die("no libNoesis.dylib / Noesis.dylib under %s/Bin (tried %s)" % (sdk, ", ".join(MACOS_BIN_CANDIDATES)))
    raise AssertionError


def write_zip(staging: Path, zip_path: Path) -> None:
    if zip_path.exists():
        zip_path.unlink()
    with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        for path in sorted(staging.rglob("*")):
            if path.is_file():
                zf.write(path, path.relative_to(staging).as_posix())


def sha512_file(path: Path) -> str:
    h = hashlib.sha512()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def copy_tree(src: Path, dest: Path) -> None:
    shutil.copytree(src, dest)


def require_dir(path: Path, what: str) -> Path:
    if not path.is_dir():
        die("%s not found: %s" % (what, path))
    return path


def require_file(path: Path, what: str) -> Path:
    if not path.is_file():
        die("%s not found: %s" % (what, path))
    return path


def die(message: str) -> None:
    print("error: %s" % message, file=sys.stderr)
    raise SystemExit(1)


if __name__ == "__main__":
    raise SystemExit(main())
