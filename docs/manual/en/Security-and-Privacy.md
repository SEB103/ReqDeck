# Security and Privacy

An HTTP client handles passwords, tokens, and API keys all day. ReqDeck is built so that
they go only where you send them.

## Promises

- **No account.** Every function works without signing in anywhere.
- **No telemetry.** ReqDeck connects only to:
  - the servers your requests and logins name;
  - the GitHub API, when you check for updates;
  - the update repository on GitHub Pages, when the installer or the Maintenance Tool
    installs an update.

  The automatic update check is off until you turn it on.
- **Your data is one visible file** where you put it.
- **It installs only itself** and changes no settings of other programs.
- **No scripts, plugins, or cloud sync.** ReqDeck runs no code from workspaces or imports.

## Secrets

| What | Where it is kept |
|---|---|
| Variables marked Secret | apart from the workspace file, encrypted with Windows DPAPI for your user account |
| OAuth 2.0 tokens | the same |
| Persistent cookies | the same; session cookies are never written to disk |
| Values typed straight into auth fields | **in the workspace file** — ReqDeck offers *Move to a secret* |

Encrypted files cannot be read on another computer or by another Windows user. There,
ReqDeck asks for secret values again. A cookies or tokens file it cannot read is left
unchanged; new cookies and tokens are then kept only until the workspace closes.

## What leaves your computer, and how it is shown

- The **Sent** tab lists every hop of a request as it went out: method, URL, headers, and
  body size. Passwords, tokens, cookie values, and API keys are **masked**, for example
  `Bearer ••••••`.
- The **history** removes values of fields whose names mark a credential before saving.
  Other values stay, so clear the history when needed.

## Network behavior

- **Redirects** are followed by ReqDeck, not by the network library, so that:
  - credentials are dropped when a redirect leaves the original server;
  - a redirect from `https` to `http` is never followed;
  - files of a form-data body are not sent to another server by a `307`/`308` redirect.
- **TLS:** a server certificate that fails the check stops the request with a banner. You
  can accept it for **one** request to that server, for the errors it showed. The
  response then carries a warning, and the log records it.
- **Proxy:** requests use the proxy configured in Windows, except requests to this
  computer.
- **OAuth:** a warning appears when a token would travel over plain `http://` to another
  computer. Answers from the token server are limited in size.

## File uploads

A workspace or an imported collection can name a file on your disk for upload. To keep a
shared file from sending your data somewhere unexpected:

- a file you picked with **Browse…** is sent without a question;
- any other file path triggers an **Upload files?** question before the first upload of
  that file to that server, with the path and the server name;
- network paths (`\\server\share`) are not opened just to check them.

## Imports

Postman, Insomnia, and OpenAPI files are **read as data**. Scripts and tests in them are
not run; the import report lists them as not carried over.

## Installer

The installer is **not code-signed yet**, so Windows SmartScreen may warn. Each release
lists its files on the GitHub releases page, built by the public release workflow from the
tagged source by the public release workflow.

## Reporting a vulnerability

Please do **not** open a public issue. Report privately through
[GitHub security advisories](https://github.com/SEB103/ReqDeck/security/advisories/new),
as described in [`SECURITY.md`](https://github.com/SEB103/ReqDeck/blob/master/SECURITY.md).
Only the latest release receives security fixes.
