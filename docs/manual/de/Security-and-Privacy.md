# Sicherheit und Datenschutz

Ein HTTP-Client hat den ganzen Tag mit Passwörtern, Token und API-Schlüsseln zu tun.
ReqDeck ist so gebaut, dass sie nur dorthin gehen, wohin Sie sie senden.

## Versprechen

- **Kein Konto.** Jede Funktion arbeitet ohne Anmeldung irgendwo.
- **Keine Telemetrie.** ReqDeck verbindet sich nur mit:
  - den Servern, die Ihre Anfragen und Anmeldungen nennen;
  - der GitHub-API, wenn Sie nach Updates suchen;
  - dem Update-Repository auf GitHub Pages, wenn der Installer oder das Wartungstool ein
    Update installiert.

  Die automatische Update-Prüfung ist aus, bis Sie sie einschalten.
- **Ihre Daten sind eine sichtbare Datei** dort, wo Sie sie ablegen.
- **Es installiert nur sich selbst** und ändert keine Einstellungen anderer Programme.
- **Keine Skripte, Plugins oder Cloud-Synchronisierung.** ReqDeck führt keinen Code aus
  Arbeitsbereichen oder Importen aus.

## Geheime Werte

| Was | Wo es aufbewahrt wird |
|---|---|
| Variablen, die als Geheim markiert sind | getrennt von der Arbeitsbereichsdatei, mit Windows DPAPI für Ihr Benutzerkonto verschlüsselt |
| OAuth-2.0-Token | ebenso |
| Dauerhafte Cookies | ebenso; Sitzungs-Cookies werden nie auf die Festplatte geschrieben |
| Werte, die direkt in Felder der Authentifizierung eingegeben werden | **in der Arbeitsbereichsdatei** – ReqDeck bietet *In einen geheimen Wert verschieben* an |

Verschlüsselte Dateien lassen sich auf einem anderen Computer oder von einem anderen
Windows-Benutzer nicht lesen. Dort fragt ReqDeck geheime Werte erneut ab. Eine Cookie-
oder Token-Datei, die es nicht lesen kann, bleibt unverändert; neue Cookies und Token
werden dann nur behalten, bis der Arbeitsbereich geschlossen wird.

## Was Ihren Computer verlässt und wie es gezeigt wird

- Der Reiter **Gesendet** listet jeden Schritt einer Anfrage, wie er hinausging: Methode,
  URL, Header und Größe des Body. Passwörter, Token, Cookie-Werte und API-Schlüssel sind
  **verdeckt**, zum Beispiel `Bearer ••••••`.
- Der **Verlauf** entfernt vor dem Speichern Werte von Feldern, deren Namen Zugangsdaten
  kennzeichnen. Andere Werte bleiben; leeren Sie den Verlauf bei Bedarf.

## Netzwerkverhalten

- **Weiterleitungen** verfolgt ReqDeck selbst, nicht die Netzwerkbibliothek, damit:
  - Zugangsdaten entfallen, wenn eine Weiterleitung den ursprünglichen Server verlässt;
  - eine Weiterleitung von `https` zu `http` nie verfolgt wird;
  - Dateien eines Formulardaten-Body nicht durch eine Weiterleitung `307`/`308` an einen
    anderen Server gehen.
- **TLS:** Ein Serverzertifikat, das die Prüfung nicht besteht, hält die Anfrage mit einem
  Hinweis an. Sie können es für **eine** Anfrage an diesen Server annehmen, für die Fehler,
  die es zeigte. Die Antwort trägt dann eine Warnung, und das Protokoll vermerkt es.
- **Proxy:** Anfragen nutzen den in Windows eingestellten Proxy, außer Anfragen an diesen
  Computer.
- **OAuth:** Eine Warnung erscheint, wenn ein Token über unverschlüsseltes `http://` zu
  einem anderen Computer gehen würde. Antworten des Token-Servers sind in der Größe
  begrenzt.

## Datei-Uploads

Ein Arbeitsbereich oder eine importierte Sammlung kann eine Datei auf Ihrer Festplatte zum
Hochladen nennen. Damit eine weitergegebene Datei Ihre Daten nicht unerwartet irgendwohin
schickt:

- eine Datei, die Sie mit **Durchsuchen…** gewählt haben, wird ohne Rückfrage gesendet;
- jeder andere Dateipfad löst vor dem ersten Upload dieser Datei an diesen Server die Frage
  **Dateien hochladen?** aus, mit dem Pfad und dem Namen des Servers;
- Netzwerkpfade (`\\server\freigabe`) werden nicht nur zur Prüfung geöffnet.

## Importe

Dateien aus Postman, Insomnia und OpenAPI werden **als Daten gelesen**. Skripte und Tests
darin laufen nicht; der Importbericht führt sie als nicht übernommen auf.

## Installer

Der Installer ist **noch nicht signiert**, daher kann Windows SmartScreen warnen. Jedes
Release listet seine Dateien auf der Release-Seite von GitHub, gebaut vom öffentlichen
Release-Workflow aus dem getaggten Quellcode.

## Sicherheitslücken melden

Bitte öffnen Sie **kein** öffentliches Issue. Melden Sie vertraulich über
[GitHub Security Advisories](https://github.com/SEB103/ReqDeck/security/advisories/new),
wie in [`SECURITY.md`](https://github.com/SEB103/ReqDeck/blob/master/SECURITY.md)
beschrieben. Nur das neueste Release erhält Sicherheitskorrekturen.
