# ReqDeck

ReqDeck is a free desktop client for Windows to compose, send, and save HTTP and
REST requests. It keeps your work in one file on your computer, needs no account,
and explains every part of itself: point at anything and press F1.

## What it does

- **Requests:** any method, query parameters, headers, and bodies as raw JSON,
  XML, or text, URL-encoded forms, multipart form data with files, or a whole
  file. Several requests at once in tabs.
- **Collections and folders** in a workspace file (`.reqdeck`, JSON). Folders
  pass their authorization and variables on to the requests inside.
- **Environments** with `{{variables}}`, completion after `{{`, a warning for a
  name without a value, and a question for its value before Send.
- **Authentication:** Basic, Bearer, API key, and OAuth 2.0 (Authorization Code
  with PKCE in your browser, and Client Credentials), with tokens renewed by
  themselves.
- **Responses:** status, time, size, formatted JSON and XML, search with Ctrl+F,
  a JSONPath filter, saving to a file, large bodies kept on disk instead of in
  memory, and a **Sent** tab that shows each request as it went out, redirects
  included.
- **Chaining:** `{{response("Login", "$.token")}}` puts a value from the last
  answer of another request into this one, by plain text substitution.
- **Cookies** kept per workspace, with a manager to view, edit, and delete them.
- **History** with search, **cURL** import and export, and import of Postman
  collections and environments (v2.1), Insomnia exports (v4), and OpenAPI 3
  documents, with a report of what did not carry over.
- **Help:** F1 on any control in English and Russian, a beginner guide as PDF,
  Ctrl+K to find a request or a command, a light and a dark theme.

## Promises

- **No account.** Every function works without signing in anywhere.
- **Your data is one visible file.** A workspace is a `.reqdeck` file where you
  put it. Before a new version changes the file format, ReqDeck keeps a copy of
  the old file, and File > Restore Previous Copy brings it back.
- **No telemetry.** ReqDeck connects only to the servers your requests and
  logins name, and to GitHub when you check for updates. The automatic update
  check is off until you turn it on.
- **It installs only itself:** its folder, Start menu entries, and an optional
  desktop shortcut. It changes no settings of other programs.
- **No ads,** no banners, no panels you cannot close.
- **It shows what it really sent:** the Sent tab lists method, URL, headers, and
  body size of every hop. Passwords, tokens, cookie values, and API keys are
  masked there.
- **Secrets stay out of the workspace file.** Variables marked Secret, stored
  cookies, and OAuth tokens are kept apart from it, encrypted for your Windows
  user account (DPAPI). ReqDeck offers to move a password or token typed into an
  authorization field into a secret variable, because typed values are saved in
  the file.
- **Numbers only with their method.** Speed and memory figures are measured with
  `packaging/measure.ps1`, and published with the machine and the date.

## Good to know

- **History is plain text.** It is stored as JSON in your data folder. Values of
  fields whose names mark a credential (for example `Authorization`, `token`,
  `password`, `api_key`) are removed before saving; other values are kept as
  sent. Clear the history from its pane when needed.
- **The Sent tab** shows the headers ReqDeck sets. The network layer adds a few
  of its own, such as `Host` and `Accept-Encoding`, which are not listed.
- **response() values** come from answers received since ReqDeck started; they
  are never written to disk. After a restart, send the request again.
- **Files:** before a request uploads a file it has not uploaded before, ReqDeck
  shows the path and asks. A relative path means a file next to the workspace.
- **Windows 10 and 11, 64-bit only.** The user interface is in English; F1 help
  and the guide are in English and Russian.
- **No scripts, plugins, cloud sync, or mock servers.** ReqDeck runs no code from
  workspaces or imports.
- The installer is not code-signed yet, so Windows SmartScreen may warn before
  the first start.

## Install

Download the installer (`ReqDeck-<version>-Setup.exe`) or the portable ZIP from
<https://github.com/SEB103/ReqDeck/releases/latest>. The portable version keeps
all its data in its own folder. Updates come through the Maintenance Tool of the
installed version, or as a new ZIP.

## Build from source

The project uses Qt 6.11 (Qt Quick / QML, Qt Network), C++20, CMake, and
Microsoft Visual Studio 2022 / MSVC 2022 on Windows x86_64.

Set `QTDIR` to the root of a Qt 6.11 MSVC 2022 kit, for example:

```powershell
$env:QTDIR = "C:\Qt\6.11.1\msvc2022_64"
```

Configure and build Debug or Release:

```powershell
cmake --preset windows-msvc2022-debug
cmake --build --preset windows-msvc2022-debug
```

```powershell
cmake --preset windows-msvc2022-release
cmake --build --preset windows-msvc2022-release
```

The presets use the Visual Studio 2022 x64 generator and create build trees below
`build/`. `CMakeUserPresets.json` is ignored and may hold machine-local overrides.

Run the tests:

```powershell
ctest --preset windows-msvc2022-debug
```

The installer, the portable ZIP, and the update repository are made by
`packaging/release.ps1`; see `packaging/README.md`.

## Security

Report vulnerabilities privately as described in `SECURITY.md`.

## License

ReqDeck is free software licensed under the GNU General Public License, version 3
or (at your option) any later version (`GPL-3.0-or-later`). See `LICENSE` and
`NOTICE`. Third-party components and their licenses are listed in
`THIRD_PARTY_NOTICES.md`; the full license texts are in `LICENSES/`.
