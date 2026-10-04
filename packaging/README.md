<!--
SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Release packaging

This folder turns a Release build of ReqDeck into ready-to-distribute Windows
artifacts. One build produces all three at once:

```
Build -> Canonical Deployment -> { Setup.exe (hybrid installer), *-win64.zip (portable), repository/ (updates) }
```

One script does everything: [`release.ps1`](release.ps1). All names and the
version come from a single file, [`product.json`](product.json).

---

## 1. Quick start

Open a terminal in the **project root** (the folder that contains `packaging/`)
and run the script with `-File`:

```powershell
powershell -ExecutionPolicy Bypass -File .\packaging\release.ps1
```

From Git Bash use forward slashes:

```bash
powershell -ExecutionPolicy Bypass -File ./packaging/release.ps1
```

When it finishes, the installer, the portable ZIP, and the update repository are
under `release/` (see section 3). The script finds its own location, so it also
works with an absolute path from anywhere. Always pass the script with `-File`.

---

## 2. Prerequisites

- **Visual Studio 2022 / MSVC** (x64 tools). No developer prompt is needed: if
  `cl.exe` is not on `PATH`, `release.ps1` loads the VS environment itself.
- **Qt 6.11.1**, kit `msvc2022_64`. Default path `C:/Qt/6.11.1/msvc2022_64`
  (override with `-QtPrefix`).
- **CMake and Ninja** from the Qt installation (`C:/Qt/Tools/CMake_64`,
  `C:/Qt/Tools/Ninja`; override with `-CMake` / `-Ninja`).
- **Qt Installer Framework** (`binarycreator.exe`, `repogen.exe`, `devtool.exe`).
  Default path `C:\Qt\Tools\QtInstallerFramework\4.10` (override with
  `-QtIfwRoot` or the `QTIFW_ROOT` environment variable).

---

## 3. What you get

Everything lands in `release/` (git-ignored):

```
release/
  ReqDeck-<version>/            # the app folder, ready to run
  ReqDeck-<version>-Setup.exe   # the hybrid installer
  ReqDeck-<version>-win64.zip   # portable package
  repository/                   # update repository (Updates.xml + data)
```

The `<version>` is taken from `product.json`. It is the only place the version is
defined and the same value CMake compiles into the executable, so the artifact
names, the installer metadata, and the running program can never disagree. There
is deliberately no command-line override.

---

## 4. Options and single stages

The pipeline runs six stages in order. To run just one, use `-Stage` (earlier
stages must already have produced their output):

| Stage | What it does |
|-------|--------------|
| `Build` | Configures (Ninja, `cl`, `BUILD_TESTING=OFF`) and builds Release into `build/release`. |
| `Deploy` | `cmake --install` -> `release/ReqDeck-<version>/` (the app folder with the Qt runtime deployed by the generated QML deploy script). |
| `Verify` | Runs [`verify-deployment.ps1`](verify-deployment.ps1): executable and version metadata, no debug Qt DLLs, platform/image/SVG/TLS plugins, Qt Quick Controls/Layouts/Dialogs QML modules, licensing files, no development artifacts, and **no OpenSSL DLLs** (the notices declare OpenSSL as not bundled). |
| `PackageInstaller` | Builds the **hybrid** `ReqDeck-<version>-Setup.exe` (`binarycreator --hybrid`: full offline payload plus the stable update repository URL), then dumps it with `devtool` and fails unless the binary really is hybrid. |
| `PackagePortable` | Builds `ReqDeck-<version>-win64.zip` (adds the `portable.ini` marker). |
| `GenerateRepository` | Builds `repository/` with `repogen` (`Updates.xml` + per-component archives). |

Examples (from the project root):

```powershell
# Only rebuild the installer (reuses the existing deployment)
powershell -ExecutionPolicy Bypass -File .\packaging\release.ps1 -Stage PackageInstaller

# Dark-themed installer wizard
powershell -ExecutionPolicy Bypass -File .\packaging\release.ps1 -Theme dark
```

---

## 5. Building just the application (development)

For day-to-day development you do not need the packaging script; build with the
CMake presets (see the top-level [`README.md`](../README.md)) or open
`CMakeLists.txt` in Qt Creator.

---

## 6. Installing the result

**Graphical install:** run `ReqDeck-<version>-Setup.exe` and follow the wizard
(folder, licence, optional desktop shortcut). The default location is
`C:\Program Files\ReqDeck`. The installer is **unsigned**, so Microsoft Edge and
Windows SmartScreen may warn after download (*More info → Run anyway*).

**Silent install (command line):**

```bat
ReqDeck-0.0.1-Setup.exe install --root "C:\Program Files\ReqDeck" --accept-licenses --default-answer --confirm-command
```

Installing into `Program Files` needs administrator rights; a folder inside the
user profile does not.

**Uninstall:** use the Maintenance Tool in the install folder
(`ReqDeckMaintenanceTool.exe`), from its Start-Menu shortcut or:

```bat
"C:\Program Files\ReqDeck\ReqDeckMaintenanceTool.exe" purge --confirm-command --accept-licenses --default-answer
```

**Portable (no install):** unzip `ReqDeck-<version>-win64.zip` anywhere and run
`ReqDeck.exe`. All data stays next to it (`config/`, `data/`, `logs/`), so the
folder can be moved freely.

**Updates:** the in-app update check is disabled until the first public release
(`update.enabled` is `false` in `product.json`). Once enabled, an installed copy
offers **Install update** (starts the Maintenance Tool with `--start-updater` and
quits) and a portable copy offers **Open download page**. The update flow has not
been exercised end to end for ReqDeck yet.

---

## 7. Where the app stores data (installed vs portable)

The same executable picks its mode at runtime (`src/framework/apppaths.*`):

- **Installed** (no `portable.ini`): program files stay in the install folder;
  settings and logs go to `%LOCALAPPDATA%\ReqDeck\ReqDeck`, workspaces default to
  `Documents\ReqDeck`.
- **Portable** (the ZIP ships a `portable.ini` marker next to the exe): all user
  data lives inside the app folder (`config/`, `data/`, `logs/`).

Uninstalling removes the program but keeps user data.

---

## 8. Themes

`-Theme light` (default) or `-Theme dark` selects the wizard style
(`installer/styles/*.qss`, magenta accent). The banner is generated by
[`make-installer-images.ps1`](make-installer-images.ps1) from the application
logo. The Modern wizard layout itself is fixed by Qt IFW, so this is a
branding pass, not a full re-skin.

---

## 9. Troubleshooting

| Symptom | Cause / fix |
|---------|-------------|
| *is not recognized as the name of a script file* | Wrong path or already inside `packaging/`. Run from the project root and always use `-File`. |
| *running scripts is disabled* | Add `-ExecutionPolicy Bypass`. |
| `PermissionDenied … Setup.exe … Remove-Item` | A previously built `Setup.exe` is still running and locks its file; close it first. |
| `binarycreator not found` / `repogen not found` | Qt IFW is not at the default path. Pass `-QtIfwRoot "…"` or set `QTIFW_ROOT`. |
| `CMake configure/build failed` | Qt kit not found. Pass `-QtPrefix "C:/Qt/6.11.1/msvc2022_64"`. |
| `Verify` reports OpenSSL DLLs | A TLS setup now ships OpenSSL. Add the OpenSSL license and notices to `LICENSES/` and `THIRD_PARTY_NOTICES.md`, then adjust the check. |

---

## 10. Notes for maintainers

**Single source of truth.** `product.json` holds the product name, version,
publisher, URLs, installer identity, and the update endpoints. `identifier`,
`orgDomain`, `exeName`, and `componentId` (`com.reqdeck.app`) must stay stable
once a version has been released, so data locations and the update path survive
a later rename; only `displayName` and `installDirName` may change.

**Hybrid installer.** `Setup.exe` carries the complete payload and keeps the
`<RemoteRepositories>` from `installer/config/config.xml.in`, whose URL is
`update.channels.stable` in `product.json`. `release.ps1` verifies the result with
`devtool dump` (the binary's `config-internal.ini` must record
`hybridInstaller=true`).

**Update repository.** `repogen` turns the same deployment into
`release/repository/`. `Updates.xml` is generated; never edit it by hand.

**Deployment content.** The QML deploy script scans the whole source tree for QML
imports (Qt 6.11 always uses the target's source directory as the scanner root),
so the `QtTest` import of `tests/qml` also deploys `Qt6Test.dll`,
`Qt6QuickTest.dll`, and `qml/QtTest` (about 0.7 MB). This is harmless and accepted.

**Licensing.** The app ships Qt as LGPL-3.0 shared libraries (see
`THIRD_PARTY_NOTICES.md`). The installer and Maintenance Tool are Qt Installer
Framework programs by The Qt Company; add the IFW/Qt attribution before a public
release. Local pipeline runs produce unsigned artifacts.

**Branding.** The application icon and logo in `resources/images/app/` are
generated placeholders until final artwork exists; replace `ReqDeck.ico`,
`MaintenanceTool.ico`, `ReqDeckLogo.png`, and the sources in
`resources/images/app/source/` together.

