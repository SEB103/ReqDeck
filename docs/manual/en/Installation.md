# Installation

ReqDeck runs on **Windows 10 and 11, 64-bit**. Every release has two packages on the
[releases page](https://github.com/SEB103/ReqDeck/releases/latest):

| Package | File | Use it when |
|---|---|---|
| Installer | `ReqDeck-<version>-Setup.exe` | you want Start menu entries and updates through the Maintenance Tool |
| Portable | `ReqDeck-<version>-win64.zip` | you want no installation, or want to carry ReqDeck on a USB stick |

Both contain the same program.

## Installer

1. Download `ReqDeck-<version>-Setup.exe` and run it.
2. The installer is **not code-signed yet**, so Microsoft Edge and Windows SmartScreen
   may warn. Choose *More info → Run anyway*.
3. Follow the wizard: folder, license, and an optional desktop shortcut. The default
   folder is `C:\Program Files\ReqDeck`.

The installer adds only these:

- the program folder;
- Start menu entries for ReqDeck and its Maintenance Tool;
- an optional desktop shortcut;
- the entry in Windows' list of installed apps.

It changes no settings of other programs.

### Silent install

```bat
ReqDeck-1.2.0-Setup.exe install --root "C:\Program Files\ReqDeck" --accept-licenses --default-answer --confirm-command
```

Installing into `Program Files` needs administrator rights; a folder inside the user
profile does not.

### Uninstall

Use *Settings → Apps*, the Maintenance Tool from the Start menu, or the command line:

```bat
"C:\Program Files\ReqDeck\ReqDeckMaintenanceTool.exe" purge --confirm-command --accept-licenses --default-answer
```

Uninstalling removes the program and keeps your workspaces and settings.

## Portable version

1. Unzip `ReqDeck-<version>-win64.zip` into any folder.
2. Run `ReqDeck.exe`.

A `portable.ini` file next to the executable switches ReqDeck to portable mode: all data
stays inside the folder, in `config\`, `data\`, and `logs\`. You can move or copy the
folder freely, and nothing is written to `%LOCALAPPDATA%`.

## Where data is stored

| | Installed | Portable |
|---|---|---|
| Settings (INI) | `%LOCALAPPDATA%\ReqDeck\ReqDeck` | `config\` |
| Data: history, secrets, cookies, tokens, backups, samples | `%LOCALAPPDATA%\ReqDeck\ReqDeck` | `data\` |
| Logs | `%LOCALAPPDATA%\ReqDeck\ReqDeck\log` | `logs\` |
| Default folder for new workspaces | `Documents\ReqDeck` | `data\workspaces` |

A workspace file itself lives wherever you save it. See [Workspaces and Data](Workspaces-and-Data).

## Updates

- *Help → Check for Updates* asks the GitHub Releases API for the latest release.
- In an **installed** copy, *Install update* starts the Maintenance Tool, which downloads
  the update from the update repository on GitHub Pages
  (`https://seb103.github.io/ReqDeck/updates/stable/`). ReqDeck then quits.
- In a **portable** copy, *Open download page* opens the releases page; unzip the new
  version over a copy of the folder, or next to it.
- The automatic check at start is off by default. It can be turned on in Settings.

## First start

The launcher shows your recent workspaces and **Open Samples**. The samples workspace sends
requests to two public test services, `httpbin.org` and `jsonplaceholder.typicode.com`.
Continue with [Getting Started](Getting-Started).
