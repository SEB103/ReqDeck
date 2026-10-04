# Third-party notices

ReqDeck is licensed under **GPL-3.0-or-later** (see `LICENSE` and
`LICENSES/GPL-3.0-or-later.txt`). This file lists the third-party components that
the application uses, bundles, or may distribute. Each component remains governed
by its own license; the licenses below do **not** apply to the ReqDeck source code
itself.

The exact set of deployed runtime files depends on the selected Qt kit and build
configuration. Full license texts are stored in the `LICENSES/` directory using
their SPDX identifiers as file names.

## Qt 6.11

The application is built against Qt 6.11 and links the following Qt modules
dynamically (shared libraries): Qt Core, Qt GUI, Qt Network, Qt QML, Qt Quick,
Qt Quick Controls, Qt Quick Dialogs, and Qt SVG. Qt Linguist Tools is used at build
time only and is not distributed.

- **License (as used here):** GNU Lesser General Public License, version 3
  (`LICENSES/LGPL-3.0-only.txt`), which incorporates the GNU General Public
  License, version 3 (`LICENSES/GPL-3.0-or-later.txt`) by reference.
- Qt is also available under the GNU General Public License, version 2, and under
  commercial licenses from The Qt Company. ReqDeck relies on the open-source
  LGPLv3 option.
- **LGPLv3 obligation:** Qt is linked dynamically. Recipients may replace the Qt
  shared libraries with their own compatible builds. To do so, obtain the
  corresponding Qt source for the version in use, build it with a compatible
  configuration, and replace the Qt library files next to the executable.
- **Source:** <https://www.qt.io> — archived sources at
  <https://download.qt.io/archive/qt/>.

### TLS

HTTPS is provided by Qt Network through the TLS backend plugins deployed with Qt.
ReqDeck does not bundle OpenSSL. If an OpenSSL-based TLS backend is ever shipped
together with OpenSSL runtime libraries, add the OpenSSL license
(Apache License 2.0) and notices for the distributed binaries.

## Google Material Symbols (icons)

The SVG icons under `resources/images/svg/` are based on Google Material Symbols
Outlined and are embedded in the application resources.

- **License:** Apache License 2.0 (`LICENSES/Apache-2.0.txt`; a component-specific
  copy is also kept at `resources/images/LICENSE-Material-Symbols.txt`).
- **Source:** <https://github.com/google/material-design-icons>.

The SVG path data was not modified; only the file names were normalized. See
`resources/images/README.md`.

## Country flag (UI-language selector)

The SVG flag `resources/images/flags/gb.svg` is used by the UI-language selector.
It carries a Creative Commons Public Domain dedication in its source markup.

- **License:** Public Domain (national flags are not subject to copyright; the SVG
  artwork is released into the public domain).

## Before distributing binaries

Verify that the license texts and notices for every component you actually ship
(Qt and the icon set) are included with the distribution and that the LGPLv3
relinking obligation for Qt is satisfied.
