<!--
SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Changelog

All notable changes to **ReqDeck** are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html)
(`MAJOR.MINOR.PATCH`).

The product version is defined in exactly one place — `packaging/product.json`
(`version`). From there it flows to the Help&nbsp;>&nbsp;About dialog, the Windows
executable metadata, and the installer and update repository. To cut a release,
bump that single field, then rebuild and run `packaging/release.ps1`.

## [Unreleased]

### Added
- Repository base: license texts, notices, REUSE metadata, and security policy.
- CMake build skeleton driven by `packaging/product.json` (product metadata header,
  Windows version resource, MSVC 2022 presets); the executable is `ReqDeck.exe`.
- `ReqDeckFramework` library: portable/installed data-location policy (`AppPaths`)
  and the in-application log models, with unit tests.
- Workspace files (`.reqdeck`, JSON schema 1) with validated loading and atomic
  saving, and the workspace manager for create/open/save/save as/close and the
  recent-workspaces list, with unit tests.
- Application layer: startup, settings store, application log (panel model and
  rotating release log file), About metadata and license list, UI language
  plumbing (English), and the update check (disabled until the first release).
- User interface: launcher with recent workspaces, main window with File,
  Request, View, and Help menus (request actions are placeholders for now), a
  three-pane workspace layout, log panel, status bar, settings, About, and
  update dialogs; magenta accent theme for light and dark mode; placeholder
  application icon.
- Windows release packaging: `packaging/release.ps1` builds a hybrid Qt IFW
  installer, a portable ZIP, and an update repository from one verified
  deployment; installer wizard styles in the magenta accent.
- GitHub Actions: CI (build, unit and QML tests, qmllint) on every push, and a
  tag-driven release workflow that publishes the installer, the portable ZIP,
  and the update repository (GitHub Release and Pages).
- HTTP core in `ReqDeckCore` (not yet wired to the UI): request model with
  query parameters, headers, raw/form bodies, and Basic/Bearer/API-key auth;
  `{{variable}}` resolution; request builder; asynchronous executor with
  timeout, abort, redirect, and TLS-error handling; cURL import and export.
  Unit tests run against a local mock HTTP server only.
- Workspace persistence (not yet wired to the UI): `.reqdeck` files hold a
  validated collection of folders and requests and typed environments; values of
  secret variables stay out of the file, in a per-user secrets file (not
  encrypted); request history of 200 entries with literal credentials removed; a
  samples workspace against public test APIs, seeded into the data directory.
- Collection sidebar and request editor: build a tree of folders and requests,
  rename, duplicate, and delete them, and edit method, URL, parameters, headers,
  body, and authentication; changes are saved with the workspace.
- Sending requests and a response viewer: status, time, size, headers, and a
  Pretty or Raw body with JSON and XML highlighting; requests can be cancelled,
  and very long or binary bodies are limited in the view.
- Environments: a selector in the top bar, an editor for names and variables with
  masked secret values, and `{{variable}}` highlighting in the URL field, where
  unresolved references are colored and underlined.
- History of sent requests in the sidebar, kept across runs without response bodies
  or literal credentials; an entry can be opened as a new request.
- cURL import with a live check of the command, and Copy as cURL.
- Keyboard shortcuts, shown in the menus, and Open Samples in the launcher.
- New look: colors for methods and status, compact headers and tabs, an inline
  request title, line numbers in the response, and tables with column titles.
- Help with F1: a short explanation, in English or Russian after the system
  language, of the control under the pointer or the field being typed in, also
  inside dialogs.
- Beginner guide as a PDF in English and Russian, opened from the Help menu and
  the help panel, with links to a public API for practice and to a reference of
  HTTP status codes.
- Ctrl+F in the response, with a count of matches and F3 to move between them.
- Save the response to a file as received, unpacked when it came compressed;
  responses too large for memory are kept on disk, shown in part, and saved
  whole.
- An untrusted server certificate can be accepted for one request, its server,
  and the errors it showed, with a warning in the response and an entry in the
  log.
- Requests use the proxy configured in Windows, except to this computer.

### Fixed
- Responses over HTTP/2 show the name of their status, for example 200 OK.
