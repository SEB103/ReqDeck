# Fehlerbehebung und FAQ

## Wenn etwas nicht funktioniert

| Was Sie sehen | Was es bedeutet |
|---|---|
| Connection refused | Unter dieser Adresse antwortet nichts. Läuft der Server? Stimmt der Port? |
| Host not found | Der Servername ist falsch oder das Netzwerk ist weg. |
| Timeout | Der Server hat zu lange gebraucht. Erhöhen Sie das Timeout oder versuchen Sie es später. |
| Nicht aufgelöste Variable | Ein `{{name}}` hat keinen Wert. Wählen Sie die richtige Umgebung oder geben Sie den Wert ein, wenn ReqDeck fragt. |
| Zertifikatsfehler | Dem HTTPS-Zertifikat des Servers wird nicht vertraut, bei Testservern üblich. Kennen Sie den Server, drücken Sie **Fortfahren**. |
| Proxy oder Firmennetz | ReqDeck nutzt den in Windows eingestellten Proxy, wie Ihr Browser. Anfragen an Ihren eigenen Computer gehen direkt. |
| Geheime Werte sind leer | Der Arbeitsbereich wurde auf einem anderen Computer oder von einem anderen Windows-Benutzer gespeichert. Geben Sie die Werte erneut ein und speichern Sie. |

*Ansicht → Protokollbereich einblenden* zeigt, was ReqDeck getan hat und was
schiefgegangen ist.

## Häufige Fragen

**Brauche ich ein Konto?**
Nein. Jede Funktion arbeitet ohne Anmeldung irgendwo.

**Sendet ReqDeck irgendwo etwas über mich?**
Nein. Es gibt keine Telemetrie. ReqDeck verbindet sich nur mit den Servern, die Ihre
Anfragen und Anmeldungen nennen, und mit GitHub, wenn Sie nach Updates suchen. Siehe
[Sicherheit und Datenschutz](Security-and-Privacy).

**Kann ich meinen Arbeitsbereich in Git halten?**
Ja. Eine Datei `.reqdeck` ist reines JSON. Werte geheimer Variablen, Cookies und Token
stehen nicht darin, sie lässt sich also committen. Werte, die direkt in Felder der
Authentifizierung eingegeben wurden, *stehen* darin: Verschieben Sie sie vorher in
geheime Variablen.

**Kann ich einen Arbeitsbereich mit Kollegen teilen?**
Ja, schicken Sie die Datei. Geheime Werte bleiben auf Ihrem Computer; das ReqDeck Ihres
Kollegen fragt beim ersten Senden danach.

**Warum warnt Windows beim Starten des Installers?**
Der Installer ist noch nicht signiert, daher kennt SmartScreen ihn nicht. Wählen Sie
*Weitere Informationen → Trotzdem ausführen* oder nehmen Sie die portable ZIP-Datei.

**Kann ReqDeck Skripte ausführen, wie die Pre-Request-Skripte von Postman?**
Nein, mit Absicht. ReqDeck führt keinen Code aus Arbeitsbereichen oder Importen aus. Um
einen Wert aus einer Antwort an die nächste Anfrage weiterzugeben, verwenden Sie
`{{response("Anfrage", "$.pfad")}}`; siehe
[Variablen und Umgebungen](Variables-and-Environments#anfragen-verketten-mit-response).

**Gibt es eine Version für macOS oder Linux?**
Nein. ReqDeck wird nur für Windows 10 und 11, 64 Bit, gebaut und getestet.

**Welche Sprachen spricht ReqDeck?**
Englisch und Deutsch. **EN** oder **DE** oben rechts schaltet die Oberfläche um; die
F1-Hilfe und dieses Handbuch folgen ihr. Ein deutsches Windows startet auf Deutsch.

**Wo liegen meine Einstellungen und der Verlauf?**
Siehe [Speicherorte](Installation#speicherorte).

## Nicht enthalten

- Skripte, Tests und Plugins;
- Cloud-Synchronisierung und Team-Konten;
- Mock-Server;
- WebSocket, gRPC und andere Protokolle außer HTTP;
- ein Programm für die Befehlszeile.

## Bekannte Grenzen

- Der Reiter **Gesendet** listet einige Header nicht, die die Netzwerkschicht selbst
  hinzufügt, etwa `Host` und `Accept-Encoding`.
- Werte für `response()` leben nur, bis ReqDeck beendet wird.
- Haben mehrere Anfragen denselben Namen, nimmt `response("Name", …)` die, deren Antwort
  zuletzt kam.
- Felder der Authentifizierung eines Ordners können geheime Variablen verwenden, aber dort
  gibt es keinen Hinweis *In einen geheimen Wert verschieben*.
- Eine Fehlermeldung, die vor dem Umschalten der Sprache angezeigt wurde, behält ihre
  Sprache, bis die Anfrage erneut gesendet wird.

## Ein Problem melden

Öffnen Sie ein Issue auf <https://github.com/SEB103/ReqDeck/issues> mit der Version aus
*Hilfe → Über ReqDeck…*, der Angabe, ob Sie die installierte oder die portable Kopie
verwenden, und den Schritten zum Nachstellen. Sicherheitsprobleme melden Sie vertraulich
über [Security Advisories](https://github.com/SEB103/ReqDeck/security/advisories/new).
