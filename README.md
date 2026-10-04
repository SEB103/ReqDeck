# ReqDeck

ReqDeck is a Qt Quick desktop client for composing, sending, and saving HTTP/REST
requests.

**Status:** early development. There is no release yet, and the application does
not send requests yet. The first release (0.1.0) is planned to provide request
collections in a JSON workspace file (`.reqdeck`), environments with
`{{variables}}`, Basic/Bearer/API-key authentication, a send history, and cURL
import/export.

## Project baseline

- Qt 6.11 (Qt Quick / QML, Qt Network)
- C++20
- CMake
- Microsoft Visual Studio 2022 / MSVC 2022, Windows x86_64

## Configure and build with CMake presets

Clone the repository:

```powershell
git clone https://github.com/SEB103/ReqDeck.git
```

Set `QTDIR` to the root of the selected Qt 6.11 MSVC 2022 kit, for example:

```powershell
$env:QTDIR = "C:\Qt\6.11.1\msvc2022_64"
```

Configure and build Debug:

```powershell
cmake --preset windows-msvc2022-debug
cmake --build --preset windows-msvc2022-debug
```

Configure and build Release:

```powershell
cmake --preset windows-msvc2022-release
cmake --build --preset windows-msvc2022-release
```

The presets use the Visual Studio 2022 x64 generator and create build trees below
`build/`. `CMakeUserPresets.json` is ignored and may be used for machine-local
overrides.

## Run tests

```powershell
ctest --preset windows-msvc2022-debug
```

## License

ReqDeck is free software licensed under the GNU General Public License, version 3
or (at your option) any later version (`GPL-3.0-or-later`). See `LICENSE` and
`NOTICE`. Third-party components and their licenses are listed in
`THIRD_PARTY_NOTICES.md`; the full license texts are in `LICENSES/`.
