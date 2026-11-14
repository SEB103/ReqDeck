# Import and Export

## cURL

### Import

*File → Import cURL…* takes a `curl` command, for example one copied from the browser's
developer tools (*Copy as cURL*). The dialog checks the command while you type and shows
what it will create.

Supported, among others:

- method and URL;
- headers;
- data and form fields;
- form data with files (`-F`);
- a file as the body (`--data-binary @file`, `-T`);
- user and password.

### Export

*Request → Copy as cURL* copies the current request as a command for a terminal. It adds
`-L` and `-m` so that the command follows redirects and stops after the same timeout as
ReqDeck does.

## Collections from other tools

*File → Import Collection…* reads these formats, as JSON or YAML where the format allows
it:

| Source | Formats |
|---|---|
| Postman | collections v2.1, and environments |
| Insomnia | exports v4 |
| OpenAPI | OpenAPI 3 documents |

What happens:

1. ReqDeck reads the file and shows a **report** before anything changes: what will be
   created, and what does not carry over.
2. After you confirm, the content goes into a **new folder** of the current workspace;
   environments are added as environments.
3. Folders, requests, headers, bodies, auth settings (OAuth 2.0 included), and variables
   are brought along where ReqDeck has an equivalent.

**Nothing in an imported file is run.** Pre-request scripts, tests, and other code from
Postman or Insomnia are not executed. They are listed in the report as not carried over,
because ReqDeck has no scripts at all.

## Coming from Postman

You do not have to start again:

1. In Postman, export the collection as *Collection v2.1*, and each environment you need.
   In Insomnia, use *Export Data* with the *Insomnia v4 (JSON)* format. For an API
   description, take its `openapi.json` or `openapi.yaml`.
2. In ReqDeck, choose *File → Import Collection…* and pick the file.
3. Read the list of what does not carry over, then press **Import**.
4. Save the workspace with **Ctrl+S** to keep the new folder.

| File | What you get |
|---|---|
| Postman collection | Folders, requests, headers, bodies, auth (OAuth 2.0 included), and collection variables; the variables become settings of the new folder |
| Postman environment | A new environment; secret values come over empty, enter them again |
| Insomnia export | The first workspace with its folders and requests; the base environment becomes folder variables, the other environments become ReqDeck environments |
| OpenAPI 3 document | One request per operation, in folders by tag, with example values; the server address becomes the variable `{{baseUrl}}` |

Auth set on a folder is used by every request in it whose auth is *Inherit from folder*.
Right-click a folder and choose *Folder Settings…* to see or change it.

When you send, ReqDeck asks for the values the import could not fill in, such as
`{{token}}`, and can keep them in the active environment. For passing a value from one
answer to the next request, ReqDeck has `{{response(…)}}` instead of scripts (see
[Variables and Environments](Variables-and-Environments#chaining-requests-with-response)).

## Files in imported requests

A request that uploads a file names the file by its path. When such a path came from a
file or an import rather than from **Browse…**, ReqDeck asks before the first upload of that
file to that server. It shows the path and the server. A relative path means a file next to
the workspace. See [Security and Privacy](Security-and-Privacy).
