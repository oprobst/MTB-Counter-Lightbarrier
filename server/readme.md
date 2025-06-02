🚴 Bike Counter API (Arduino Fahrrad-Zähler Schnittstelle)
Diese API dient als Schnittstelle für einen Arduino-basierten Fahrradzähler. Sie empfängt tägliche Zähldaten und einzelne Fahrten vom Arduino und speichert sie in einer MariaDB-Datenbank. Darüber hinaus bietet sie verschiedene Endpunkte zum Abrufen von Zähldaten, Tageszusammenfassungen und zum Export in CSV-Formaten.

🌟 Features
POST-Endpunkt: Empfängt tägliche Berichte (Gesamtzählung, Batteriestatus, Start-/Endzeiten und individuelle Fahrten-Zeitstempel) vom Arduino.
GET-Endpunkte:
Tägliche Zusammenfassungen der Fahrten.
Einzelne Fahrten für spezifische Tage (JSON).
CSV-Export von einzelnen Fahrten (/api/rides-csv) mit Zählernummer, Datum und Uhrzeit.
CSV-Export von täglichen Berichten (/api/report-csv) mit aggregierten Tagesdaten.
Abfrage des aktuellen Batteriestatus.
Abfrage des aktuellen Gesamtzählerstands.
API-Statusprüfung.
Authentifizierung: Basic Authentication für den POST-Endpunkt.
Datenbank: MariaDB (getestet mit MariaDB 10).
Technologie: PHP 8.3.
🛠️ Installation
Voraussetzungen
Webserver (z.B. Apache, Nginx) mit PHP 8.3 oder höher
MariaDB 10 oder höher Datenbankserver
Ein Datenbankbenutzer mit den notwendigen Berechtigungen (SELECT, INSERT, UPDATE, DELETE auf die bike_counter Datenbank)
1. Datenbank einrichten
Die benötigte Datenbank- und Tabellenstruktur ist in der Datei MariaDB-DDL.sql im Projektverzeichnis definiert. Führe dieses Skript auf deinem MariaDB-Server aus:

Bash

mysql -u your_db_user -p < MariaDB-DDL.sql
Oder führe den Inhalt manuell über ein Tool wie phpMyAdmin oder DBeaver aus.

SQL

-- Inhalt von MariaDB-DDL.sql
CREATE DATABASE IF NOT EXISTS `bike_counter` CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

USE `bike_counter`;

CREATE TABLE IF NOT EXISTS `daily_reports` (
    `id` INT AUTO_INCREMENT PRIMARY KEY,
    `transmission_date` DATETIME NOT NULL,
    `measurement_date` DATE NOT NULL UNIQUE,
    `start_time` TIME NOT NULL,
    `end_time` TIME NOT NULL,
    `daily_count` INT NOT NULL DEFAULT 0,
    `total_count_ever` INT NOT NULL DEFAULT 0,
    `battery_voltage` DECIMAL(4, 2) NOT NULL,
    `created_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    `updated_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `bike_rides` (
    `id` INT AUTO_INCREMENT PRIMARY KEY,
    `daily_report_id` INT NOT NULL,
    `measurement_date` DATE NOT NULL,
    `ride_time` TIME NOT NULL,
    `second_of_day` INT NOT NULL,
    `created_at` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT `fk_daily_report`
        FOREIGN KEY (`daily_report_id`) REFERENCES `daily_reports`(`id`)
        ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Optional: Falls eine View daily_ride_summary existiert oder erstellt werden soll
-- CREATE VIEW daily_ride_summary AS ...;
2. API-Dateien hochladen
Lade die bike_counter.php-Datei (und alle anderen relevanten Dateien wie openapi.yaml, config-template.properties, MariaDB-DDL.sql in dein Webserver-Verzeichnis hoch (z.B. htdocs/mtb-club/). Die öffentliche URL sollte dann etwa https://www.your-domain.de/mtb-club/bike_counter.php/ sein.

3. Konfiguration (config.properties)
Das Projekt enthält die Datei config-template.properties. Um die API zu konfigurieren, führe folgende Schritte aus:

Benenne die Datei config-template.properties um in config.properties.
Fülle die umbenannte Datei mit deinen tatsächlichen Zugangsdaten und Einstellungen.
<!-- end list -->

Properties

# Inhalt von config-template.properties (Beispiel)
# Datenbank-Zugangsdaten
DB_HOST=localhost
DB_PORT=3306
DB_NAME=bike_counter
DB_USER=your_db_user
DB_PASSWORD=your_db_password
DB_CHARSET=utf8mb4

# API Basic Authentication
BASIC_AUTH_USER=your_api_username
BASIC_AUTH_PASSWORD=your_api_password

# API Pfad-Tiefe (Anzahl der Segmente vor /api/ im URL-Pfad, z.B. 2 für /mtb-club/bike_counter.php/api)
API_DEPTH=2 # Passe dies an deine Server-Umgebung an!

# Zeitzone für Zeitstempel (z.B. Europe/Berlin, America/New_York)
TIMEZONE=Europe/Berlin

# Minimale und maximale Batteriespannung für Validierung (optional)
MIN_BATTERY_VOLTAGE=10.0
MAX_BATTERY_VOLTAGE=15.0

# Debug-Modus (optional, 'true' oder 'false')
DEBUG_MODE=false

# API Version (optional)
API_VERSION=1.0
Wichtig: Die config.properties sollte niemals in einem öffentlichen Repository eingecheckt werden, da sie sensible Daten (Passwörter) enthält. Sie ist in der .gitignore-Datei dieses Projekts aufgeführt.

4. Datei-Encoding prüfen
Stelle sicher, dass die bike_counter.php-Datei selbst als UTF-8 ohne BOM gespeichert ist. Dies ist entscheidend für die korrekte Anzeige von Umlauten und Sonderzeichen in der API-Ausgabe und den CSV-Dateien.

🚀 Nutzung der API
Alle API-Endpunkte beginnen mit https://www.oliver-probst.de/mtb-club/bike_counter.php/api/.

POST-Endpunkte
POST /daily-report: Empfängt einen JSON-Payload mit täglichen Zähldaten und einzelnen Fahrten.
Authentifizierung: Basic Auth erforderlich.
Siehe OpenAPI Spezifikation für Details zum Request Body.
GET-Endpunkte
GET /daily-summary: Ruft tägliche Zusammenfassungen der Fahrten ab.
Query-Parameter: start_date=YYYY-MM-DD, end_date=YYYY-MM-DD
GET /rides: Ruft einzelne Fahrten für einen bestimmten Tag ab (JSON-Format).
Query-Parameter: date=YYYY-MM-DD
GET /rides-csv: Lädt eine CSV-Datei mit allen einzelnen Fahrten herunter (enthält Zählernummer pro Durchfahrt, Datum, Uhrzeit).
Query-Parameter: start_date=YYYY-MM-DD, end_date=YYYY-MM-DD
GET /report-csv: Lädt eine CSV-Datei mit den täglichen Berichten (Tageszusammenfassungen) herunter.
Query-Parameter: start_date=YYYY-MM-DD, end_date=YYYY-MM-DD
GET /battery-status: Ruft den letzten aufgezeichneten Batteriestatus ab.
GET /total-count: Ruft den letzten aufgezeichneten Gesamtzählerstand ab.
GET /status: Überprüft den API-Status und die Version.
📄 OpenAPI Spezifikation
Eine vollständige OpenAPI 3.0 Spezifikation der API befindet sich im Projektverzeichnis. Du kannst sie in Tools wie Swagger UI oder Postman importieren, um die API interaktiv zu erkunden und zu testen.

Link zur openapi.yaml

📝 Lizenz
Dieses Projekt ist unter der MIT-Lizenz lizenziert. Weitere Informationen findest du in der LICENSE-Datei. ---
Hinweis: Ersetze die Platzhalter wie https://www.your-domain.de/mtb-club/bike_counter.php/ und den Pfad zum Logo durch deine tatsächlichen Werte. Du solltest auch eine LICENSE-Datei hinzufügen, wenn du eine Lizenz angeben möchtest.