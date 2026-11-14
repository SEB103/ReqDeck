# Authentication

The **Auth** tab of a request, and the settings of a folder, offer these types:

| Type | What ReqDeck sends |
|---|---|
| No authentication | nothing |
| Basic | `Authorization: Basic …` from a user name and a password |
| Bearer token | `Authorization: Bearer <token>` |
| API key | a name and a value, *sent as header* or *sent as query parameter* |
| Inherit from folder | the auth of the nearest folder above that sets one |
| OAuth 2.0 | `Authorization: Bearer <access token>`, with the token obtained as described below |

All fields accept `{{variables}}`. Keep passwords, tokens, and keys in **secret variables**
so that they stay out of the workspace file. When you type one straight into a field,
ReqDeck offers **Move to a secret**. See [Variables and Environments](Variables-and-Environments).

## Inheritance from folders

Set the auth once on a folder, and choose **Inherit from folder** in the requests inside
it. ReqDeck looks from the request's own folder outwards:

- the first folder that sets an auth type is used;
- a folder set to **No authentication** stops the search;
- if no folder sets one, the request is sent without auth.

## OAuth 2.0

Choose how the token is obtained:

### Authorization code (browser)

For logins where a person signs in. ReqDeck uses **PKCE** (S256).

1. Fill in *Authorization URL*, *Token URL*, *Client ID*, and, if the provider needs
   them, *Client secret*, *Scope*, and *Audience*.
2. Press **Get Token**. ReqDeck opens the sign-in page in your default browser and waits
   for the answer on a one-shot local address (`127.0.0.1`).
3. Sign in. The browser page says that you are logged in, and the token appears in
   ReqDeck.

**Cancel Login** stops the wait. Without an answer, the wait ends by itself after five
minutes.

### Client credentials

For machine-to-machine access without a person. **Get Token** asks the token URL
directly, and ReqDeck fetches a new token by itself when it is needed.

### Tokens

- Tokens are kept **encrypted for your Windows user**, apart from the workspace file,
  per workspace.
- A token is renewed with its refresh token, or fetched again for Client Credentials,
  shortly before it expires.
- **Reset** forgets the token.
- Get Token refuses to start while a field names a variable without a value, and says
  which one.
- An orange warning appears when the token URL or the request URL uses plain `http://`
  to another computer, because the token would travel unencrypted.

## Credentials and redirects

ReqDeck follows redirects itself. When a redirect leads to **another server**, it drops the
`Authorization` header and other credentials, and it never follows a redirect from
`https` to `http`. The **Sent** tab of the response shows each hop with credentials
masked.
