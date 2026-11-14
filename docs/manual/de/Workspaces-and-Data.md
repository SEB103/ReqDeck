# Arbeitsbereiche und Daten

## Die Arbeitsbereichsdatei

Alle Anfragen, Ordner und Umgebungen eines Projekts liegen in einer einzigen
**Arbeitsbereichsdatei** mit der Endung `.reqdeck`. Sie ist reines JSON und lässt sich
deshalb:

- neben dem Code aufbewahren;
- unter Versionskontrolle stellen und in einem Diff prüfen;
- an Kolleginnen und Kollegen schicken.

ReqDeck speichert sie **atomar**: Zuerst wird eine neue Datei geschrieben, die dann die
alte ersetzt, so hinterlässt ein Absturz nie eine halbe Datei. Speichern ohne Änderungen
lässt die Datei unberührt.

### Format

Das aktuelle Format ist **Schema 2**. Ein gekürztes Beispiel:

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

Die wichtigsten Teile einer Anfrage:

| Schlüssel | Inhalt |
|---|---|
| `method`, `url` | die Anfragezeile; `url` darf `{{Variablen}}` enthalten |
| `queryParams`, `headers` | Listen aus Zeilen `{ "key", "value", "enabled" }` |
| `body` | `mode`: `none`, `raw` (mit `rawType` `json`, `xml` oder `text`), `form`, `formData` (mit `parts`) oder `file` |
| `auth` | `type`: `none`, `inherit`, `basic`, `bearer`, `apiKey` oder `oauth2` mit seinen Feldern |
| `timeoutMs` | Timeout der Übertragung; `0` bedeutet den Standard von 30 Sekunden |

Ordner können eigene `auth` und `variables` haben; die Anfragen darin übernehmen sie
(siehe [Variablen und Umgebungen](Variables-and-Environments)).

ReqDeck prüft eine Datei beim Öffnen und lehnt eine ab, die es nicht sicher lesen kann,
mit einer Meldung, die den Grund nennt.

### Formatänderungen und Sicherungen

Bevor eine neue Version von ReqDeck eine Datei in einem neueren Format schreibt, behält
sie eine Kopie der alten Datei und ihrer geheimen Werte. *Datei → Vorherige Kopie
wiederherstellen…* holt diese Kopie zurück. ReqDeck 1.0.0 hat Dateien auf diese Weise von
Schema 1 auf Schema 2 umgestellt.

## Was außerhalb der Arbeitsbereichsdatei liegt

Manche Daten dürfen nicht mit der Datei reisen. ReqDeck hält sie in seinem Datenordner
(siehe [Speicherorte](Installation#speicherorte)), ein Satz Dateien je `id` eines
Arbeitsbereichs:

| Daten | Datei | Schutz |
|---|---|---|
| Werte von Variablen, die als **Geheim** markiert sind | `secrets\<id>.json` | verschlüsselt für den Windows-Benutzer (DPAPI) |
| Dauerhafte Cookies | `secrets\<id>.cookies.json` | verschlüsselt (DPAPI); Sitzungs-Cookies werden nie geschrieben |
| OAuth-2.0-Token | `secrets\<id>.tokens.json` | verschlüsselt (DPAPI) |
| Kopien vor einer Formatänderung | `backups\` | die geheimen Werte darin bleiben verschlüsselt |
| Verlauf gesendeter Anfragen | `history.json` | reines JSON; Werte von Zugangsdaten entfernt |

> [!NOTE]
> Weil die Verschlüsselung an Ihr Windows-Konto gebunden ist, funktionieren geheime Werte
> auf einem anderen Computer nicht. Dort fragt ReqDeck die Werte erneut ab, und
> Speichern funktioniert weiterhin.

## Verlauf

- Der Verlauf behält die letzten **200** gesendeten Anfragen, ohne die Bodys der
  Antworten.
- Werte von Feldern, deren Namen Zugangsdaten kennzeichnen, etwa `Authorization`,
  `token`, `password` oder `api_key`, werden vor dem Speichern entfernt. Andere Werte
  bleiben, wie sie gesendet wurden.
- Zwei ReqDeck-Fenster überschreiben sich nicht gegenseitig den Verlauf.
- *Verlauf leeren* im Bereich Verlauf entfernt alles.

## Beispiele

Der Beispiel-Arbeitsbereich wird beim ersten Gebrauch in den Datenordner kopiert, so
berühren Ihre Änderungen daran den Programmordner nicht.
