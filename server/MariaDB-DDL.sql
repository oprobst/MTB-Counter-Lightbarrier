-- Bike Counter Database Schema
-- MariaDB 10.x compatible

CREATE DATABASE IF NOT EXISTS bike_counter CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE bike_counter;

-- Haupttabelle für die täglichen Übertragungen
CREATE TABLE daily_reports (
    id INT AUTO_INCREMENT PRIMARY KEY,
    transmission_date DATE NOT NULL COMMENT 'Datum der Übertragung',
    measurement_date DATE NOT NULL COMMENT 'Datum des gemessenen Tages',
    start_time TIME NOT NULL COMMENT 'Einschaltzeitpunkt (Morgengrauen)',
    end_time TIME NOT NULL COMMENT 'Abschaltzeitpunkt (Sonnenuntergang)',
    daily_count INT NOT NULL COMMENT 'Gesamtzahl Fahrten am Tag',
    total_count_ever INT NOT NULL COMMENT 'Gesamtzahl aller Fahrten seit Installation',
    battery_voltage DECIMAL(4,2) NOT NULL COMMENT 'Aktuelle Batteriespannung in Volt',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    
    -- Eindeutigkeit pro Messtag sicherstellen
    UNIQUE KEY unique_measurement_date (measurement_date),
    
    -- Indizes für bessere Performance
    INDEX idx_transmission_date (transmission_date),
    INDEX idx_measurement_date (measurement_date),
    INDEX idx_created_at (created_at)
) ENGINE=InnoDB COMMENT='Tägliche Berichte vom Arduino Fahrrad-Zähler';

-- Tabelle für einzelne Fahrten mit Zeitstempeln
CREATE TABLE bike_rides (
    id INT AUTO_INCREMENT PRIMARY KEY,
    daily_report_id INT NOT NULL,
    measurement_date DATE NOT NULL COMMENT 'Datum der Fahrt',
    ride_time TIME NOT NULL COMMENT 'Uhrzeit der Fahrt (konvertiert von SecondOfTheDay)',
    second_of_day INT NOT NULL COMMENT 'Ursprünglicher SecondOfTheDay Wert',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    
    -- Foreign Key zu daily_reports
    FOREIGN KEY (daily_report_id) REFERENCES daily_reports(id) ON DELETE CASCADE,
    
    -- Indizes
    INDEX idx_measurement_date (measurement_date),
    INDEX idx_ride_time (ride_time),
    INDEX idx_daily_report_id (daily_report_id)
) ENGINE=InnoDB COMMENT='Einzelne Fahrten mit Zeitstempeln';

-- View für einfache Abfragen der Fahrten pro Tag
CREATE VIEW daily_ride_summary AS
SELECT 
    dr.measurement_date,
    dr.transmission_date,
    dr.start_time,
    dr.end_time,
    dr.daily_count,
    dr.total_count_ever,
    dr.battery_voltage,
    COUNT(br.id) as recorded_rides,
    MIN(br.ride_time) as first_ride,
    MAX(br.ride_time) as last_ride
FROM daily_reports dr
LEFT JOIN bike_rides br ON dr.id = br.daily_report_id
GROUP BY dr.id, dr.measurement_date, dr.transmission_date, dr.start_time, dr.end_time, dr.daily_count, dr.total_count_ever, dr.battery_voltage;

-- Beispiel-Daten für Tests (optional)
-- INSERT INTO daily_reports (transmission_date, measurement_date, start_time, end_time, daily_count, total_count_ever, battery_voltage) 
-- VALUES ('2024-06-02', '2024-06-01', '06:30:00', '20:15:00', 42, 1337, 12.45);