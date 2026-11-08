# SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
# SPDX-License-Identifier: GPL-3.0-or-later
"""Builds the beginner guide PDF files from the help topics of ReqDeck.

The chapters live in resources/help/help.<language>.json next to the F1 topics,
so the guide and the help panel cannot drift apart. This script turns each file
into one HTML page styled by guide.css and prints it to PDF with Microsoft Edge
in headless mode. Screenshots come from packaging/help/shots; the screens test
test_guideShots makes them when REQDECK_GUIDE_SHOTS names that folder.

Usage:
    python packaging/help/build_guide.py [--edge PATH] [--html-only] [en ru]

The PDF files are written to resources/help/ReqDeck-Guide.<language>.pdf and are
committed, so a build does not need Edge.
"""
import argparse
import html
import json
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parent.parent
HELP_DIR = PROJECT / "resources" / "help"
SHOTS_DIR = HERE / "shots"
LOGO = PROJECT / "resources" / "images" / "app" / "ReqDeckLogo.png"
EDGE_LOCATIONS = [Path(r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"),
                  Path(r"C:\Program Files\Microsoft\Edge\Application\msedge.exe")]
DEFAULT_EDGE = next((path for path in EDGE_LOCATIONS if path.is_file()), EDGE_LOCATIONS[0])

# Light theme colors of BsTheme.methodColor() and BsTheme.statusColor().
METHOD_CLASSES = {"GET": "get", "POST": "post", "PUT": "put", "PATCH": "patch", "DELETE": "delete"}

INLINE = re.compile(r"(\{\{\w+\}\})|\b(Ctrl\+(?:Shift\+)?(?:Enter|[A-Z])|F1|Esc)\b")

ICONS = {
    "tip": '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M9 21h6v-1.5H9V21zm3-19a7 7 0 0 0-4 12.7V17h8v-2.3A7 7 0 0 0 12 2z"/></svg>',
    "warn": '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M1 21h22L12 2 1 21zm12-3h-2v-2h2v2zm0-4h-2v-4h2v4z"/></svg>',
}

COVER_ART = """
<svg class="cover-art" viewBox="0 0 520 190" aria-hidden="true">
  <g class="card">
    <rect x="6" y="20" width="200" height="150" rx="16"/>
    <text x="28" y="62" class="method">GET</text>
    <text x="28" y="92" class="mono">/posts/1</text>
    <rect x="28" y="112" width="130" height="10" rx="5" class="line"/>
    <rect x="28" y="134" width="96" height="10" rx="5" class="line"/>
  </g>
  <g class="arrow">
    <path d="M222 78 h70" />
    <path d="M280 66 l14 12 l-14 12" />
    <path d="M292 120 h-70" />
    <path d="M234 108 l-14 12 l14 12" />
  </g>
  <g class="card">
    <rect x="310" y="20" width="200" height="150" rx="16"/>
    <text x="332" y="62" class="ok">200 OK</text>
    <text x="332" y="92" class="mono">{ "id": 1,</text>
    <text x="332" y="116" class="mono">  "title": "…" }</text>
    <rect x="332" y="134" width="110" height="10" rx="5" class="line"/>
  </g>
</svg>
"""


def inline(text):
    """Escapes text and marks {{variables}} and key combinations."""
    out = []
    last = 0
    for match in INLINE.finditer(text):
        out.append(html.escape(text[last:match.start()]))
        if match.group(1):
            out.append('<code class="var">%s</code>' % html.escape(match.group(1)))
        else:
            out.append("<kbd>%s</kbd>" % html.escape(match.group(2)))
        last = match.end()
    out.append(html.escape(text[last:]))
    return "".join(out)


def first_cell(chapter_id, text):
    """Colors methods and status codes in the first column like the application does."""
    if chapter_id == "requests":
        words = [w.strip() for w in text.split("/")]
        if all(w in METHOD_CLASSES for w in words):
            return " / ".join('<span class="m-%s">%s</span>' % (METHOD_CLASSES[w], w) for w in words)
    if chapter_id == "response":
        code = text.split(" ", 1)[0]
        if code.isdigit():
            return '<span class="s-%sxx">%s</span>' % (code[0], html.escape(text))
    return inline(text)


def block_html(chapter_id, block, guide):
    kind = block["type"]
    if kind == "p":
        return "<p>%s</p>" % inline(block["text"])
    if kind == "steps":
        items = "".join("<li>%s</li>" % inline(item) for item in block["items"])
        return '<ol class="steps">%s</ol>' % items
    if kind in ("tip", "warn"):
        return '<aside class="%s">%s<div><b>%s</b>%s</div></aside>' % (
            kind, ICONS[kind], html.escape(guide[kind]), inline(block["text"]))
    if kind == "table":
        rows = "".join("<tr><th>%s</th><td>%s</td></tr>" % (first_cell(chapter_id, row[0]), inline(row[1]))
                       for row in block["rows"])
        return "<table>%s</table>" % rows
    if kind == "shot":
        image = SHOTS_DIR / block["file"]
        if not image.is_file():
            raise SystemExit("Missing screenshot: %s" % image)
        size = ' class="small"' if block.get("size") == "small" else ""
        return '<figure%s><img src="%s" alt=""><figcaption>%s</figcaption></figure>' % (
            size, image.as_uri(), inline(block["caption"]))
    raise SystemExit("Unknown block type: %s" % kind)


def page_html(help_data):
    guide = help_data["guide"]
    chapters = help_data["chapters"]
    language = help_data["language"]
    parts = ['<!doctype html><html lang="%s"><head><meta charset="utf-8">' % language,
             "<title>%s</title>" % html.escape(guide["title"]),
             "<style>%s</style></head><body>" % (HERE / "guide.css").read_text(encoding="utf-8")]

    parts.append('<section class="cover"><img class="logo" src="%s" alt="">' % LOGO.as_uri())
    parts.append('<p class="brand">ReqDeck</p><h1>%s</h1><p class="subtitle">%s</p>%s</section>'
                 % (html.escape(guide["title"]), html.escape(guide["subtitle"]), COVER_ART))

    parts.append('<section class="contents"><h2>%s</h2><ol>' % html.escape(guide["contents"]))
    for number, chapter in enumerate(chapters, 1):
        parts.append('<li><a href="#%s"><span class="num">%d</span>%s</a></li>'
                     % (html.escape(chapter["id"], quote=True), number, html.escape(chapter["title"])))
    parts.append('<li><a href="#keys"><span class="num">★</span>%s</a></li>' % html.escape(guide["keys"]))
    parts.append('<li><a href="#links"><span class="num">↗</span>%s</a></li>' % html.escape(guide["links"]))
    parts.append("</ol></section>")

    for number, chapter in enumerate(chapters, 1):
        parts.append('<section class="chapter" id="%s"><header><span class="badge">%s %d</span><h2>%s</h2></header>'
                     % (html.escape(chapter["id"], quote=True), html.escape(guide["chapter"]), number,
                        html.escape(chapter["title"])))
        parts.extend(block_html(chapter["id"], block, guide) for block in chapter["blocks"])
        parts.append("</section>")

    parts.append('<section class="chapter" id="keys"><header><h2>%s</h2></header>'
                 '<dl class="keys">' % html.escape(guide["keys"]))
    for key, meaning in guide["shortcuts"]:
        caps = "+".join("<kbd>%s</kbd>" % html.escape(k) for k in key.split("+"))
        parts.append("<dt>%s</dt><dd>%s</dd>" % (caps, html.escape(meaning)))
    parts.append("</dl>")

    links = help_data["links"]
    parts.append('<h2 id="links">%s</h2><ul class="links">' % html.escape(guide["links"]))
    for key in ("practice", "statusCodes"):
        url = links[key]
        parts.append('<li><a href="%s">%s</a><span>%s</span></li>'
                     % (html.escape(url, quote=True), html.escape(url), html.escape(guide[key])))
    parts.append("</ul></section></body></html>")
    return "".join(parts)


def print_pdf(edge, page, target):
    """Prints page to target; the old file goes first, so it cannot pass for a new one."""
    target.unlink(missing_ok=True)
    with tempfile.TemporaryDirectory() as profile:
        command = [str(edge), "--headless", "--disable-gpu", "--no-first-run", "--no-default-browser-check",
                   "--user-data-dir=" + profile, "--no-pdf-header-footer", "--generate-pdf-document-outline",
                   "--print-to-pdf=" + str(target), page.as_uri()]
        try:
            result = subprocess.run(command, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, timeout=180,
                                    text=True, errors="replace")
        except subprocess.TimeoutExpired:
            raise SystemExit("Edge did not finish %s within 180 s." % target) from None
    if result.returncode != 0 or not target.is_file() or target.read_bytes()[:5] != b"%PDF-":
        raise SystemExit("Edge did not write %s (exit code %d):\n%s"
                         % (target, result.returncode, result.stderr.strip()[-2000:]))


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("languages", nargs="*", default=["en", "ru"])
    parser.add_argument("--edge", type=Path, default=DEFAULT_EDGE, help="path of msedge.exe")
    parser.add_argument("--html-only", action="store_true", help="write the HTML pages to build/ instead of printing them")
    args = parser.parse_args()
    if not args.html_only and not args.edge.is_file():
        raise SystemExit("Microsoft Edge was not found at %s; pass --edge." % args.edge)

    for language in args.languages:
        help_data = json.loads((HELP_DIR / ("help.%s.json" % language)).read_text(encoding="utf-8"))
        with tempfile.TemporaryDirectory() as work:
            page = Path(work) / ("guide.%s.html" % language)
            page.write_text(page_html(help_data), encoding="utf-8")
            if args.html_only:
                (PROJECT / "build").mkdir(exist_ok=True)
                shutil.copy(page, PROJECT / "build" / page.name)
                print("wrote", Path("build") / page.name)
                continue
            target = HELP_DIR / ("ReqDeck-Guide.%s.pdf" % language)
            print_pdf(args.edge, page, target)
            print("wrote", target.relative_to(PROJECT))
    return 0


if __name__ == "__main__":
    sys.exit(main())
