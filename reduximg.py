#!/usr/bin/env python3
"""
Redux Image Tool

A small wrapper around FATtools that can list, copy, read, and remove files
inside FAT/exFAT disk images without mounting them or requiring admin rights.

Install dependency:
    python -m pip install --user FATtools

Examples:
    python reduximg.py ls disk.img
    python reduximg.py put disk.img hello.elf /
    python reduximg.py put disk.img notes.txt /docs/notes.txt
    python reduximg.py get disk.img /HELLO.TXT extracted.txt
    python reduximg.py cat disk.img /HELLO.TXT
    python reduximg.py rm disk.img /HELLO.TXT

Close QEMU before modifying the image.
"""

from __future__ import annotations

import argparse
import importlib.metadata
import os
import sys
from pathlib import Path


def fail(message: str, code: int = 1) -> "NoReturn":
    print(f"Error: {message}", file=sys.stderr)
    raise SystemExit(code)


def get_fattools_main():
    try:
        distribution = importlib.metadata.distribution("FATtools")
    except importlib.metadata.PackageNotFoundError:
        fail(
            "FATtools is not installed.\n"
            "Install it with:\n"
            "  python -m pip install --user FATtools"
        )

    for entry_point in distribution.entry_points:
        if entry_point.group == "console_scripts" and entry_point.name.lower() == "fattools":
            return entry_point.load()

    fail("The installed FATtools package does not provide its 'fattools' command.")


def image_path(image: str, internal_path: str = "/") -> str:
    image_file = Path(image).expanduser().resolve()

    if not image_file.is_file():
        fail(f"Image not found: {image_file}")

    internal = internal_path.replace("\\", "/").strip()

    if internal in ("", "/"):
        return f"{image_file.as_posix()}/"

    internal = internal.lstrip("/")
    return f"{image_file.as_posix()}/{internal}"


def run_fattools(arguments: list[str]) -> int:
    main_function = get_fattools_main()
    old_argv = sys.argv[:]

    try:
        sys.argv = ["fattools", *arguments]
        result = main_function()
        return int(result) if isinstance(result, int) else 0
    except SystemExit as exc:
        return int(exc.code) if isinstance(exc.code, int) else 0
    finally:
        sys.argv = old_argv


def command_ls(args: argparse.Namespace) -> int:
    return run_fattools(["ls", image_path(args.image, args.path)])


def command_put(args: argparse.Namespace) -> int:
    source = Path(args.source).expanduser().resolve()

    if not source.is_file():
        fail(f"Source file not found: {source}")

    destination = args.destination

    if destination in ("", "/"):
        destination = f"/{source.name}"

    return run_fattools([
        "cp",
        str(source),
        image_path(args.image, destination),
    ])


def command_get(args: argparse.Namespace) -> int:
    destination = Path(args.destination).expanduser().resolve()
    destination.parent.mkdir(parents=True, exist_ok=True)

    return run_fattools([
        "cp",
        image_path(args.image, args.source),
        str(destination),
    ])


def command_cat(args: argparse.Namespace) -> int:
    return run_fattools(["cat", image_path(args.image, args.path)])


def command_rm(args: argparse.Namespace) -> int:
    if not args.yes:
        answer = input(f"Remove {args.path!r} from {args.image!r}? [y/N] ")
        if answer.strip().lower() not in ("y", "yes"):
            print("Cancelled.")
            return 0

    return run_fattools(["rm", image_path(args.image, args.path)])


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Read and write FAT/exFAT disk images using FATtools."
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    ls_parser = subparsers.add_parser("ls", help="List files inside an image")
    ls_parser.add_argument("image", help="Path to disk.img")
    ls_parser.add_argument("path", nargs="?", default="/", help="Directory inside the image")
    ls_parser.set_defaults(handler=command_ls)

    put_parser = subparsers.add_parser("put", help="Copy a host file into an image")
    put_parser.add_argument("image", help="Path to disk.img")
    put_parser.add_argument("source", help="Host file to copy")
    put_parser.add_argument(
        "destination",
        nargs="?",
        default="/",
        help="Destination path inside the image",
    )
    put_parser.set_defaults(handler=command_put)

    get_parser = subparsers.add_parser("get", help="Extract a file from an image")
    get_parser.add_argument("image", help="Path to disk.img")
    get_parser.add_argument("source", help="File path inside the image")
    get_parser.add_argument("destination", help="Host output path")
    get_parser.set_defaults(handler=command_get)

    cat_parser = subparsers.add_parser("cat", help="Print a text file from an image")
    cat_parser.add_argument("image", help="Path to disk.img")
    cat_parser.add_argument("path", help="File path inside the image")
    cat_parser.set_defaults(handler=command_cat)

    rm_parser = subparsers.add_parser("rm", help="Remove a file from an image")
    rm_parser.add_argument("image", help="Path to disk.img")
    rm_parser.add_argument("path", help="File path inside the image")
    rm_parser.add_argument("-y", "--yes", action="store_true", help="Do not ask for confirmation")
    rm_parser.set_defaults(handler=command_rm)

    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    return args.handler(args)


if __name__ == "__main__":
    raise SystemExit(main())
