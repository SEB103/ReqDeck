# Building from Source

## Prerequisites

| Tool | Version |
|---|---|
| Windows | 10 or 11, x64 |
| Visual Studio | 2022 with the *Desktop development with C++* workload (MSVC x64) |
| Qt | 6.11 (CI and releases use 6.11.1), kit `msvc2022_64` |
| CMake | 3.21 or newer (the one from the Qt installation works) |
| Ninja | optional; the CI and the release script use it |
| Qt Installer Framework | 4.10, only for building the installer |

No other libraries are needed: yaml-cpp is vendored in `third_party/` and built as part of
the project.

## Get the code

```powershell
git clone https://github.com/SEB103/ReqDeck.git
cd ReqDeck
```

## Build with the presets

Point `QTDIR` at the Qt kit:

```powershell
$env:QTDIR = "C:\Qt\6.11.1\msvc2022_64"
```

Debug:

```powershell
cmake --preset windows-msvc2022-debug
cmake --build --preset windows-msvc2022-debug
```

Release:

```powershell
cmake --preset windows-msvc2022-release
cmake --build --preset windows-msvc2022-release
```

The presets use the Visual Studio 2022 x64 generator and put the build trees below
`build/`. `CMakeUserPresets.json` is ignored by Git and may hold machine-local overrides.

## Build with Qt Creator

Open `CMakeLists.txt` in Qt Creator and choose a Qt 6.11 MSVC 2022 64-bit kit.

## Run

The executable is `ReqDeck.exe` in the build tree. Optionally pass a workspace file:

```powershell
.\ReqDeck.exe path\to\project.reqdeck
```

A development build uses the installed-mode data locations unless a `portable.ini` file
lies next to the executable. See [Where data is stored](Installation#where-data-is-stored).

## Tests

```powershell
ctest --preset windows-msvc2022-debug
```

See [Testing](Testing) for what the tests cover and how to run a single one.

## Lint the QML

```powershell
cmake --build --preset windows-msvc2022-debug --target all_qmllint
```

The CI runs this target on every push; it must stay free of warnings.

## Installer and portable package

`packaging/release.ps1` builds the Release configuration and produces the installer, the
portable ZIP, and the update repository:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File packaging/release.ps1
```

See [Release Process](Release-Process).

## Project conventions

- **C++20** and modern, target-based CMake; Qt 6.11 APIs only.
- QML imports without version numbers.
- QML declaration order: `id`, properties, signals, functions, object properties,
  children, states, transitions.
- Header comments are Doxygen-style (`/** … */`). Source and QML comments use QDoc
  (`/*! … */`).
- New `.cpp`, `.qml`, or resource files need no installer change. A new non-Qt runtime
  DLL, data folder, or executable does; see `packaging/README.md`.
