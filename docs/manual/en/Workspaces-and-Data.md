# Workspaces and Data

## The workspace file

All requests, folders, and environments of one project live in a single **workspace
file** with the extension `.reqdeck`. It is plain JSON, so it can be:

- kept next to the code;
- put under version control and reviewed in a diff;
- sent to a colleague.

ReqDeck saves it **atomically**: a new file is written first and then replaces the old
one, so a crash never leaves half a file. Saving without changes leaves the file
untouched.

### Format

The current format is **schema 2**. A shortened example:

```json
{
    "schema": 2,
    "id": "9a1f3c52-6d0e-4b7a-8c21-5e7f0a4d2b16",
    "displayName": "Samples",
    "activeEnvironmentId": "public-test-apis",
    "environments": [
        {
            "id": "public-test-apis",
            "name": "Public test APIs",
            "variables": [
                { "key": "baseUrl", "value": "https://httpbin.org" }
            ]
        }
    ],
    "collection": [
        {
            "kind": "folder",
            "id": "http-methods",
            "name": "HTTP methods",
            "children": [
                {
                    "kind": "request",
                    "id": "get",
                    "name": "GET with query parameters",
                    "request": {
                        "method": "GET",
                        "url": "{{baseUrl}}/get",
                        "queryParams": [ { "key": "page", "value": "1" } ],
                        "headers": [ { "key": "Accept", "value": "application/json" } ]
                    }
                }
            ]
        }
    ]
}
```

Main parts of a request:

| Key | Content |
|---|---|
| `method`, `url` | the request line; `url` may contain `{{variables}}` |
| `queryParams`, `headers` | lists of `{ "key", "value", "enabled" }` rows |
| `body` | `mode`: `none`, `raw` (with `rawType` `json`, `xml`, or `text`), `form`, `formData` (with `parts`), or `file` |
| `auth` | `type`: `none`, `inherit`, `basic`, `bearer`, `apiKey`, or `oauth2` with its fields |
| `timeoutMs` | transfer timeout; `0` means the default of 30 seconds |

Folders can carry `auth` and `variables` of their own; requests inside take them over
(see [Variables and Environments](Variables-and-Environments)).

ReqDeck checks a file when it opens it and refuses one it cannot read safely, with a
message that says why.

### Format changes and backups

Before a new version of ReqDeck writes a file in a newer format, it keeps a copy of the
old file and its secrets. *File → Restore Previous Copy…* puts that copy back. ReqDeck
1.0.0 moved files from schema 1 to schema 2 this way.

## What is kept outside the workspace file

Some data must not travel with the file. ReqDeck keeps it in its data folder (see
[Where data is stored](Installation#where-data-is-stored)), one set of files per workspace `id`:

| Data | File | Protection |
|---|---|---|
| Values of variables marked **Secret** | `secrets\<id>.json` | encrypted for the Windows user (DPAPI) |
| Persistent cookies | `secrets\<id>.cookies.json` | encrypted (DPAPI); session cookies are never written |
| OAuth 2.0 tokens | `secrets\<id>.tokens.json` | encrypted (DPAPI) |
| Copies before a format change | `backups\` | the secrets in them stay encrypted |
| History of sent requests | `history.json` | plain JSON; credential values removed |

> [!NOTE]
> Because the encryption is tied to your Windows account, secrets do not work on another
> computer. There, ReqDeck asks for the values again, and saving still works.

## History

- The history keeps the last **200** sent requests, without response bodies.
- Values of fields whose names mark a credential, such as `Authorization`, `token`,
  `password`, or `api_key`, are removed before saving. Other values are kept as sent.
- Two ReqDeck windows do not overwrite each other's history.
- *Clear history* in the History pane removes everything.

## Samples

The samples workspace is copied into the data folder on first use, so your changes to it
do not touch the program folder.
