# Security Policy

## Supported versions

ReqDeck has not been released yet. Once releases are published on
<https://github.com/SEB103/ReqDeck/releases/latest>, only the latest release
receives security fixes. Older releases are not maintained; please update before
reporting.

| Version | Supported |
|---------|-----------|
| latest release | yes |
| older releases | no |

## Reporting a vulnerability

Please do **not** open a public issue for security problems.

Report vulnerabilities privately through GitHub:
**Security → Report a vulnerability** on
<https://github.com/SEB103/ReqDeck/security/advisories/new>.

Include, where possible:

- the affected version (**Help → About ReqDeck…**) and whether the installed or
  the portable package is used;
- the component (request execution, workspace files, installer, update check)
  and the steps to reproduce;
- the impact you expect (for example: credential disclosure, remote code
  execution, unintended network access).

You will receive an acknowledgement, and the fix is coordinated with you before
the advisory is published. There is no bug bounty.

## Scope notes

- ReqDeck sends HTTP requests that the user composes, to the hosts the user
  enters. Requests against untrusted servers are a usage choice, not a
  vulnerability.
- Vulnerabilities in third-party components (for example Qt) should be reported
  to those projects; a report here is still welcome if the bundled version is
  affected, so the dependency can be updated.
