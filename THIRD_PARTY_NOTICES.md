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

The screenshots in `packaging/help/shots/` and the beginner guide PDF files in
`resources/help/`, installed to `help/`, show these icons. The PDF files also
embed subsets of the Segoe UI and Consolas fonts of Windows, as written by the
Microsoft Edge PDF printer.

## yaml-cpp

The import reads YAML files with yaml-cpp 0.9.0, built from the sources in
`third_party/yaml-cpp/` and linked statically into the application.

- **License:** MIT (`LICENSES/MIT.txt`; the original text is kept at
  `third_party/yaml-cpp/LICENSE`).
- **Copyright:** Copyright (c) 2008-2015 Jesse Beder.
- **Bundled code:** `src/contrib/dragonbox.h`, Copyright (c) 2020-2024 Junekey
  Jeon, offered under Apache-2.0 WITH LLVM-exception, BSL-1.0, or MIT; used here
  under MIT.
- **Source:** <https://github.com/jbeder/yaml-cpp>. See
  `third_party/yaml-cpp/VENDORED.md` for the exact archive and what was kept.

MIT License

Copyright (c) 2008-2015 Jesse Beder.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

## Qt Installer Framework

The installer (`ReqDeck-<version>-Setup.exe`) and the Maintenance Tool it
installs (`ReqDeckMaintenanceTool.exe`) are programs of the Qt Installer
Framework 4.10 by The Qt Company, built by its `binarycreator` with ReqDeck's
configuration, wizard images, and install script. The portable ZIP does not
contain them.

- **License:** GNU General Public License, version 3 (`LICENSES/GPL-3.0-or-later.txt`
  holds the same text), with The Qt Company GPL Exception 1.0
  (`LICENSES/Qt-GPL-exception-1.0.txt`), which lets an installer created with the
  framework carry an application under other terms.
- **Source:** <https://code.qt.io/cgit/installer-framework/installer-framework.git/>;
  documentation at <https://doc.qt.io/qtinstallerframework/>.

## Country flag (UI-language selector)

The SVG flag `resources/images/flags/gb.svg` is used by the UI-language selector.
It carries a Creative Commons Public Domain dedication in its source markup.

- **License:** Public Domain (national flags are not subject to copyright; the SVG
  artwork is released into the public domain).

## Before distributing binaries

Verify that the license texts and notices for every component you actually ship
(Qt, the icon set, yaml-cpp, and the Qt Installer Framework in the installer) are
included with the distribution and that the LGPLv3 relinking obligation for Qt is
satisfied.
