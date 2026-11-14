# Import und Export

## cURL

### Import

*Datei → cURL importieren…* nimmt einen `curl`-Befehl entgegen, zum Beispiel einen aus
den Entwicklerwerkzeugen des Browsers (*Copy as cURL*). Der Dialog prüft den Befehl schon
beim Tippen und zeigt, was er anlegen wird.

Unterstützt werden unter anderem:

- Methode und URL;
- Header;
- Daten und Formularfelder;
- Formulardaten mit Dateien (`-F`);
- eine Datei als Body (`--data-binary @datei`, `-T`);
- Benutzer und Passwort.

### Export

*Anfrage → Als cURL kopieren* kopiert die aktuelle Anfrage als Befehl für ein Terminal.
Es fügt `-L` und `-m` hinzu, damit der Befehl Weiterleitungen folgt und nach demselben
Timeout aufhört wie ReqDeck.

## Sammlungen aus anderen Werkzeugen

*Datei → Sammlung importieren…* liest diese Formate, als JSON oder YAML, wo das Format es
erlaubt:

| Quelle | Formate |
|---|---|
| Postman | Sammlungen v2.1 und Umgebungen |
| Insomnia | Exporte v4 |
| OpenAPI | OpenAPI-3-Dokumente |

Was passiert:

1. ReqDeck liest die Datei und zeigt einen **Bericht**, bevor sich etwas ändert: was
   angelegt wird und was nicht übernommen wird.
2. Nach Ihrer Bestätigung kommt der Inhalt in einen **neuen Ordner** des aktuellen
   Arbeitsbereichs; Umgebungen werden als Umgebungen hinzugefügt.
3. Ordner, Anfragen, Header, Bodys, Einstellungen der Authentifizierung (OAuth 2.0
   eingeschlossen) und Variablen werden übernommen, wo ReqDeck eine Entsprechung hat.

> [!WARNING]
> **Nichts in einer importierten Datei wird ausgeführt.** Pre-Request-Skripte, Tests und
> anderer Code aus Postman oder Insomnia laufen nicht. Sie stehen im Bericht als nicht
> übernommen, weil ReqDeck überhaupt keine Skripte hat.

## Umstieg von Postman

Sie müssen nicht von vorn anfangen:

1. Exportieren Sie in Postman die Sammlung als *Collection v2.1* und jede Umgebung, die
   Sie brauchen. In Insomnia verwenden Sie *Export Data* im Format *Insomnia v4 (JSON)*.
   Für eine API-Beschreibung nehmen Sie deren `openapi.json` oder `openapi.yaml`.
2. Wählen Sie in ReqDeck *Datei → Sammlung importieren…* und die Datei.
3. Lesen Sie die Liste dessen, was nicht übernommen wird, und drücken Sie **Importieren**.
4. Speichern Sie den Arbeitsbereich mit **Strg+S**, um den neuen Ordner zu behalten.

| Datei | Was Sie bekommen |
|---|---|
| Postman-Sammlung | Ordner, Anfragen, Header, Bodys, Authentifizierung (OAuth 2.0 eingeschlossen) und Sammlungsvariablen; die Variablen werden Einstellungen des neuen Ordners |
| Postman-Umgebung | Eine neue Umgebung; geheime Werte kommen leer an, geben Sie sie erneut ein |
| Insomnia-Export | Der erste Workspace mit seinen Ordnern und Anfragen; die Basisumgebung wird zu Ordnervariablen, die anderen Umgebungen werden ReqDeck-Umgebungen |
| OpenAPI-3-Dokument | Eine Anfrage je Operation, in Ordnern nach Tag, mit Beispielwerten; die Serveradresse wird zur Variablen `{{baseUrl}}` |

Eine am Ordner festgelegte Authentifizierung gilt für jede Anfrage darin mit *Vom Ordner
erben*. Rechtsklicken Sie auf einen Ordner und wählen Sie *Ordnereinstellungen…*, um sie
zu sehen oder zu ändern.

Beim Senden fragt ReqDeck nach den Werten, die der Import nicht füllen konnte, etwa
`{{token}}`, und kann sie in der aktiven Umgebung behalten. Um einen Wert aus einer
Antwort an die nächste Anfrage weiterzugeben, hat ReqDeck statt Skripten
`{{response(…)}}` (siehe
[Variablen und Umgebungen](Variables-and-Environments#anfragen-verketten-mit-response)).

## Dateien in importierten Anfragen

Eine Anfrage, die eine Datei hochlädt, nennt die Datei über ihren Pfad. Kam ein solcher
Pfad aus einer Datei oder einem Import statt über **Durchsuchen…**, fragt ReqDeck vor dem
ersten Upload dieser Datei an diesen Server. Es zeigt den Pfad und den Server. Ein
relativer Pfad meint eine Datei neben dem Arbeitsbereich. Siehe
[Sicherheit und Datenschutz](Security-and-Privacy).
