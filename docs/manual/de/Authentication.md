# Authentifizierung

Der Reiter **Auth** einer Anfrage und die Einstellungen eines Ordners bieten diese Arten:

| Art | Was ReqDeck sendet |
|---|---|
| Keine Authentifizierung | nichts |
| Basic | `Authorization: Basic …` aus Benutzername und Passwort |
| Bearer-Token | `Authorization: Bearer <Token>` |
| API-Schlüssel | einen Namen und einen Wert, *als Header* oder *als Query-Parameter* gesendet |
| Vom Ordner erben | die Authentifizierung des nächsten Ordners darüber, der eine festlegt |
| OAuth 2.0 | `Authorization: Bearer <Access-Token>`, mit einem Token, das wie unten beschrieben beschafft wird |

Alle Felder akzeptieren `{{Variablen}}`.

> [!TIP]
> Halten Sie Passwörter, Token und Schlüssel in **geheimen Variablen**, damit sie nicht in
> der Arbeitsbereichsdatei landen. Wenn Sie einen solchen Wert direkt in ein Feld
> eingeben, bietet ReqDeck **In einen geheimen Wert verschieben** an. Siehe
> [Variablen und Umgebungen](Variables-and-Environments).

## Vererbung von Ordnern

Legen Sie die Authentifizierung einmal an einem Ordner fest und wählen Sie in den Anfragen
darin **Vom Ordner erben**. ReqDeck sucht vom eigenen Ordner der Anfrage nach außen:

- der erste Ordner, der eine Art festlegt, wird verwendet;
- ein Ordner mit **Keine Authentifizierung** beendet die Suche;
- legt kein Ordner eine fest, wird die Anfrage ohne Authentifizierung gesendet.

## OAuth 2.0

Wählen Sie, wie das Token beschafft wird:

### Autorisierungscode (Browser)

Für Anmeldungen, bei denen sich ein Mensch anmeldet. ReqDeck verwendet **PKCE** (S256).

1. Füllen Sie *Autorisierungs-URL*, *Token-URL*, *Client-ID* und, wenn der Anbieter sie
   braucht, *Client-Secret*, *Scope* und *Audience* aus.
2. Drücken Sie **Token abrufen**. ReqDeck öffnet die Anmeldeseite in Ihrem
   Standardbrowser und wartet auf die Antwort an einer einmaligen lokalen Adresse
   (`127.0.0.1`).
3. Melden Sie sich an. Die Browserseite meldet, dass Sie angemeldet sind, und das Token
   erscheint in ReqDeck.

**Anmeldung abbrechen** beendet das Warten. Ohne Antwort endet es nach fünf Minuten von
selbst.

### Client Credentials

Für Zugriffe von Maschine zu Maschine ohne Menschen. **Token abrufen** fragt die Token-URL
direkt, und ReqDeck holt sich selbst ein neues Token, wenn es nötig ist.

### Token

- Token werden **für Ihren Windows-Benutzer verschlüsselt** aufbewahrt, getrennt von der
  Arbeitsbereichsdatei, je Arbeitsbereich.
- Ein Token wird kurz vor seinem Ablauf mit seinem Refresh-Token erneuert oder bei Client
  Credentials neu geholt.
- **Zurücksetzen** vergisst das Token.
- Token abrufen startet nicht, solange ein Feld eine Variable ohne Wert nennt, und sagt,
  welche.
- Eine orange Warnung erscheint, wenn die Token-URL oder die URL der Anfrage
  unverschlüsseltes `http://` zu einem anderen Computer verwendet, weil das Token dann
  unverschlüsselt übertragen würde.

## Zugangsdaten und Weiterleitungen

ReqDeck verfolgt Weiterleitungen selbst. Führt eine Weiterleitung zu einem **anderen
Server**, entfallen der Header `Authorization` und andere Zugangsdaten, und eine
Weiterleitung von `https` zu `http` wird nie verfolgt. Der Reiter **Gesendet** der Antwort
zeigt jeden Schritt mit verdeckten Zugangsdaten.
