#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
# SPDX-License-Identifier: GPL-3.0-or-later
"""Turn the user manual and the developer pages into GitHub wiki pages.

The manual (docs/manual/<lang>/*.md) is the same Markdown the help window of
ReqDeck shows; the developer pages (docs/dev/*.md) appear only in the wiki.
Wiki page names are global, so English pages keep their names and German pages
get the prefix "de-", with their links and images rewritten to match.

Usage:
    python packaging/docs/manual2wiki.py --out <wiki clone> [--clean] [--version X.Y.Z]
    python packaging/docs/manual2wiki.py --check

--clean removes the files a previous run wrote (listed in .manual2wiki-manifest)
and the hand-written pages this generator replaced; other pages are kept.
--check writes into a temporary folder and fails when a link or an image of the
result does not resolve.
"""

from __future__ import annotations

import argparse
import json
import re
import shutil
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MANUAL = ROOT / "docs" / "manual"
DEV = ROOT / "docs" / "dev"
PRODUCT = ROOT / "packaging" / "product.json"
MANIFEST = ".manual2wiki-manifest"
# Hand-written pages of the first wiki that the manual replaced.
REPLACED = ["Features.md", "FAQ-and-Known-Limitations.md"]
LINK = re.compile(r"\]\(([^)\s]+)\)")
SECTIONS = {"en": "English", "de": "Deutsch"}


def wiki_name(language: str, page: str) -> str:
    """Returns the wiki page name of the manual page page in language."""
    return page if language == "en" else f"{language}-{page}"


def rewrite(text: str, language: str, pages: set[str]) -> str:
    """Rewrites the links and images of a German page to the prefixed wiki names."""
    if language == "en":
        return text

    def one(match: re.Match) -> str:
        target = match.group(1)
        if target.startswith("images/"):
            return f"](images/{language}/{target[len('images/'):]})"
        page, _, anchor = target.partition("#")
        if page in pages:
            return "](" + wiki_name(language, page) + (f"#{anchor}" if anchor else "") + ")"
        return match.group(0)

    return LINK.sub(one, text)


def generate(out: Path, version: str, clean: bool) -> list[str]:
    """Writes the wiki into out and returns the files written."""
    out.mkdir(parents=True, exist_ok=True)
    manifest = out / MANIFEST
    if clean:
        old = manifest.read_text(encoding="utf-8").splitlines() if manifest.is_file() else []
        for rel in old + REPLACED:
            (out / rel).unlink(missing_ok=True)
    written: list[str] = []

    def write(rel: str, text: str) -> None:
        (out / rel).parent.mkdir(parents=True, exist_ok=True)
        (out / rel).write_text(text, encoding="utf-8", newline="\n")
        written.append(rel)

    sidebar = [f"**ReqDeck {version}**", ""]
    for language, label in SECTIONS.items():
        folder = MANUAL / language
        meta = json.loads((folder / "manual.json").read_text(encoding="utf-8"))
        pages = {p["id"] for p in meta["pages"]}
        sidebar.append(f"**{label}**")
        for page in meta["pages"]:
            name = wiki_name(language, page["id"])
            write(f"{name}.md", rewrite((folder / f"{page['id']}.md").read_text(encoding="utf-8"), language, pages))
            sidebar.append(f"- [{page['title']}]({name})")
        sidebar.append("")
        images = folder / "images"
        for image in sorted(images.glob("*.png")):
            rel = f"images/{image.name}" if language == "en" else f"images/{language}/{image.name}"
            (out / rel).parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(image, out / rel)
            written.append(rel)

    sidebar.append("**Developers**")
    for page in sorted(DEV.glob("*.md")):
        write(page.name, page.read_text(encoding="utf-8"))
        sidebar.append(f"- [{page.stem.replace('-', ' ')}]({page.stem})")
    sidebar += ["", "**Links**",
                "- [Download](https://github.com/SEB103/ReqDeck/releases/latest)",
                "- [Changelog](https://github.com/SEB103/ReqDeck/blob/master/CHANGELOG.md)",
                "- [Issues](https://github.com/SEB103/ReqDeck/issues)", ""]
    write("_Sidebar.md", "\n".join(sidebar))
    write("_Footer.md", f"ReqDeck is free software under the GNU GPL v3 or later. This wiki describes version {version}.\n")
    manifest.write_text("\n".join(written) + "\n", encoding="utf-8", newline="\n")
    return written


def check(out: Path) -> list[str]:
    """Returns the links and images of the wiki in out that do not resolve."""
    pages = {p.stem for p in out.glob("*.md")}
    broken = []
    for page in out.glob("*.md"):
        for target in LINK.findall(page.read_text(encoding="utf-8")):
            if target.startswith(("http://", "https://", "mailto:", "#")):
                continue
            if target.startswith("images/"):
                if not (out / target).is_file():
                    broken.append(f"{page.name}: {target}")
                continue
            if target.partition("#")[0] not in pages:
                broken.append(f"{page.name}: {target}")
    return broken


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--out", type=Path, help="wiki clone to write into")
    parser.add_argument("--clean", action="store_true", help="remove what a previous run wrote")
    parser.add_argument("--check", action="store_true", help="write into a temporary folder and check links")
    parser.add_argument("--version", help="version named in the sidebar and footer (default: product.json)")
    args = parser.parse_args()
    version = args.version or json.loads(PRODUCT.read_text(encoding="utf-8"))["version"]
    if args.check:
        with tempfile.TemporaryDirectory() as temp:
            generate(Path(temp), version, clean=False)
            broken = check(Path(temp))
        for line in broken:
            print(f"broken: {line}", file=sys.stderr)
        print("wiki check:", "failed" if broken else "ok")
        return 1 if broken else 0
    if not args.out:
        parser.error("--out or --check is required")
    written = generate(args.out, version, args.clean)
    print(f"wrote {len(written)} files to {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
