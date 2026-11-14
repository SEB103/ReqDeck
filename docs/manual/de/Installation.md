# Installation

ReqDeck läuft unter **Windows 10 und 11, 64 Bit**. Jedes Release hat zwei Pakete auf der
[Release-Seite](https://github.com/SEB103/ReqDeck/releases/latest):

| Paket | Datei | Wann |
|---|---|---|
| Installer | `ReqDeck-<Version>-Setup.exe` | Sie möchten Einträge im Startmenü und Updates über das Wartungstool |
| Portabel | `ReqDeck-<Version>-win64.zip` | Sie möchten nichts installieren oder ReqDeck auf einem USB-Stick mitnehmen |

Beide enthalten dasselbe Programm.

## Installer

1. Laden Sie `ReqDeck-<Version>-Setup.exe` herunter und starten Sie die Datei.
2. Der Installer ist **noch nicht signiert**, daher können Microsoft Edge und Windows
   SmartScreen warnen. Wählen Sie *Weitere Informationen → Trotzdem ausführen*.
3. Folgen Sie dem Assistenten: Ordner, Lizenz und eine optionale Verknüpfung auf dem
   Desktop. Der Standardordner ist `C:\Program Files\ReqDeck`.

Der Installer legt nur Folgendes an:

- den Programmordner;
- Einträge im Startmenü für ReqDeck und sein Wartungstool;
- eine optionale Verknüpfung auf dem Desktop;
- den Eintrag in der Windows-Liste der installierten Apps.

Er ändert keine Einstellungen anderer Programme.

### Stille Installation

```bat
ReqDeck-1.2.0-Setup.exe install --root "C:\Program Files\ReqDeck" --accept-licenses --default-answer --confirm-command
```

Für die Installation unter `Program Files` sind Administratorrechte nötig, für einen
Ordner im Benutzerprofil nicht.

### Deinstallation

Über *Einstellungen → Apps*, das Wartungstool im Startmenü oder die Befehlszeile:

```bat
"C:\Program Files\ReqDeck\ReqDeckMaintenanceTool.exe" purge --confirm-command --accept-licenses --default-answer
```

Bei der Deinstallation wird das Programm entfernt; Ihre Arbeitsbereiche und Einstellungen
bleiben erhalten.

## Portable Version

1. Entpacken Sie `ReqDeck-<Version>-win64.zip` in einen beliebigen Ordner.
2. Starten Sie `ReqDeck.exe`.

Eine Datei `portable.ini` neben der Programmdatei schaltet ReqDeck in den portablen
Modus: Alle Daten bleiben im Ordner, in `config\`, `data\` und `logs\`. Sie können den
Ordner frei verschieben oder kopieren, und unter `%LOCALAPPDATA%` wird nichts geschrieben.

## Speicherorte

| | Installiert | Portabel |
|---|---|---|
| Einstellungen (INI) | `%LOCALAPPDATA%\ReqDeck\ReqDeck` | `config\` |
| Daten: Verlauf, geheime Werte, Cookies, Token, Sicherungen, Beispiele | `%LOCALAPPDATA%\ReqDeck\ReqDeck` | `data\` |
| Protokolle | `%LOCALAPPDATA%\ReqDeck\ReqDeck\log` | `logs\` |
| Standardordner für neue Arbeitsbereiche | `Documents\ReqDeck` | `data\workspaces` |

Die Arbeitsbereichsdatei selbst liegt dort, wo Sie sie speichern. Siehe
[Arbeitsbereiche und Daten](Workspaces-and-Data).

## Updates

- *Hilfe → Nach Updates suchen…* fragt die GitHub-Releases-API nach dem neuesten Release.
- In einer **installierten** Kopie startet *Update installieren* das Wartungstool, das
  das Update aus dem Update-Repository auf GitHub Pages lädt
  (`https://seb103.github.io/ReqDeck/updates/stable/`). ReqDeck wird dann beendet.
- In einer **portablen** Kopie öffnet *Download-Seite öffnen* die Release-Seite; entpacken
  Sie die neue Version über eine Kopie des Ordners oder daneben.
- Die automatische Prüfung beim Start ist standardmäßig aus. Sie lässt sich in den
  Einstellungen einschalten.

## Erster Start

Der Startbildschirm zeigt Ihre zuletzt verwendeten Arbeitsbereiche und **Beispiele
öffnen**. Der Beispiel-Arbeitsbereich sendet Anfragen an zwei öffentliche Testdienste,
`httpbin.org` und `jsonplaceholder.typicode.com`. Weiter mit
[Erste Schritte](Getting-Started).
