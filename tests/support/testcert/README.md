<!--
SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Test certificate

`test-only.crt` and `test-only.key` are a self-signed certificate and its key for
`localhost` and `127.0.0.1`, valid until 2126. The network tests use them to serve
HTTPS that no system trusts. They protect nothing and are not part of any build of
the application. Made with:

```bash
openssl req -x509 -newkey rsa:2048 -nodes -keyout test-only.key -out test-only.crt \
    -days 36500 -subj "/CN=ReqDeck test server" \
    -addext "subjectAltName=DNS:localhost,IP:127.0.0.1"
```
