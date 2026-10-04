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
