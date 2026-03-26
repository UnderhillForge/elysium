#!/usr/bin/env python3
"""Lightweight asset packer for glTF-first Elysium runtime builds."""

from __future__ import annotations

import argparse
import pathlib
import shutil

PROFILE_EXTENSIONS = {
    "client": {
        ".glb",
        ".gltf",
        ".lua",
        ".wav",
        ".ogg",
        ".mp3",
        ".flac",
        ".png",
        ".jpg",
        ".jpeg",
        ".ktx",
        ".ktx2",
        ".json",
    },
    "server": {
        ".glb",
        ".gltf",
        ".lua",
        ".json",
    },
    "editor": {
        ".glb",
        ".gltf",
        ".fbx",
        ".obj",
        ".lua",
        ".wav",
        ".ogg",
        ".mp3",
        ".flac",
        ".png",
        ".jpg",
        ".jpeg",
        ".ktx",
        ".ktx2",
        ".json",
        ".mat",
        ".material",
        ".vert",
        ".frag",
        ".glsl",
        ".comp",
    },
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Package Elysium runtime resources")
    parser.add_argument("--input", required=True, type=pathlib.Path)
    parser.add_argument("--output", required=True, type=pathlib.Path)
    parser.add_argument("--manifest", required=True, type=pathlib.Path)
    parser.add_argument(
        "--profile",
        default="client",
        choices=sorted(PROFILE_EXTENSIONS.keys()),
        help="Packaging profile to apply",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    src_root = args.input.resolve()
    out_root = args.output.resolve()
    manifest_path = args.manifest.resolve()
    include_extensions = PROFILE_EXTENSIONS[args.profile]

    out_root.mkdir(parents=True, exist_ok=True)
    manifest_path.parent.mkdir(parents=True, exist_ok=True)

    copied = []
    for src in src_root.rglob("*"):
        if not src.is_file() or src.suffix.lower() not in include_extensions:
            continue
        rel = src.relative_to(src_root)
        dst = out_root / rel
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)
        copied.append((src, rel, src.stat().st_size))

    copied.sort(key=lambda item: str(item[1]).lower())

    with manifest_path.open("w", encoding="utf-8") as f:
        f.write("# Elysium packaged resources\n")
        f.write(f"# profile={args.profile}\n")
        f.write("# source|package_path|bytes\n")
        for src, rel, size in copied:
            f.write(f"{src}|{rel}|{size}\n")

    print(f"Packed {len(copied)} resources into {out_root}")
    print(f"Manifest: {manifest_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
