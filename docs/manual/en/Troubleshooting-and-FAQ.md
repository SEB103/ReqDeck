# Troubleshooting and FAQ

## When something does not work

| What you see | What it means |
|---|---|
| Connection refused | Nothing answers at this address. Is the server running? Is the port right? |
| Host not found | The server name is wrong or the network is down. |
| Timeout | The server took too long. Raise the timeout or try later. |
| Unresolved variable | A `{{name}}` has no value. Pick the right environment, or enter the value when ReqDeck asks. |
| Certificate error | The server's HTTPS certificate is not trusted, common on test servers. If you know the server, press **Continue**. |
| Proxy or company network | ReqDeck uses the proxy set in Windows, like your browser. Requests to your own computer go direct. |
| Secret values are empty | The workspace was saved on another computer or by another Windows user. Enter the values again and save. |

*View → Show Log Panel* shows what ReqDeck did and what went wrong.

## Frequently asked questions

**Do I need an account?**
No. Every function works without signing in anywhere.

**Does ReqDeck send anything about me anywhere?**
No. There is no telemetry. ReqDeck connects only to the servers your requests and logins
name, and to GitHub when you check for updates. See [Security and Privacy](Security-and-Privacy).

**Can I keep my workspace in Git?**
Yes. A `.reqdeck` file is plain JSON. Values of secret variables, cookies, and tokens are
not in it, so it can be committed. Values typed straight into auth fields *are* in it:
move them to secret variables first.

**Can I share a workspace with a colleague?**
Yes, send the file. Secret values stay on your computer; your colleague's ReqDeck asks for
them on the first send.

**Why does Windows warn when I run the installer?**
The installer is not code-signed yet, so SmartScreen does not know it. Choose
*More info → Run anyway*, or use the portable ZIP.

**Can ReqDeck run scripts, like Postman's pre-request scripts?**
No, on purpose. ReqDeck runs no code from workspaces or imports. For passing a value from
one answer to the next request, use `{{response("Request", "$.path")}}`; see
[Variables and Environments](Variables-and-Environments#chaining-requests-with-response).

**Is there a macOS or Linux version?**
No. ReqDeck is built and tested only for Windows 10 and 11, 64-bit.

**Which languages does ReqDeck speak?**
English and German. **EN** or **DE** at the top right switches the interface; the F1 help
and this manual follow it. A German Windows starts in German.

**Where are my settings and history?**
See [Where data is stored](Installation#where-data-is-stored).

## Not included

- scripts, tests, and plugins;
- cloud sync and team accounts;
- mock servers;
- WebSocket, gRPC, and other protocols besides HTTP;
- a command-line runner.

## Known limitations

- The **Sent** tab does not list a few headers that the network layer adds by itself,
  such as `Host` and `Accept-Encoding`.
- Values for `response()` live only until ReqDeck closes.
- If several requests have the same name, `response("Name", …)` takes the one whose answer
  came last.
- Folder auth fields can use secret variables, but there is no *Move to a secret* hint for
  them.
- An error message shown before you switch the interface language keeps its language
  until the request is sent again.

## Reporting a problem

Open an issue on <https://github.com/SEB103/ReqDeck/issues> with the version from
*Help → About*, whether you use the installed or the portable copy, and the steps to
reproduce. Security problems go privately through
[security advisories](https://github.com/SEB103/ReqDeck/security/advisories/new).
