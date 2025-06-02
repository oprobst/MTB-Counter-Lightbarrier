/*
 * Arduino Bike Counter - HTTP POST mit Basic Authentication
 * Für SIM7000E Modul
 * 
 * Sendet tägliche Fahrrad-Zähldaten an REST API
 */

#include <SoftwareSerial.h>
#include "base64.h" // Base64 Library für Authentication

// SIM7000E Konfiguration
SoftwareSerial sim7000(7, 8); // RX, TX Pins

// API Konfiguration
const char* API_HOST = "api.deine-domain.de";
const char* API_PATH = "/api/daily-report";
const int API_PORT = 443; // HTTPS
const char* API_USER = "bike_counter";
const char* API_PASSWORD = "super_secret_arduino_password_2024";

// Datenstrukturen
struct BikeData {
    String transmissionDate;
    String measurementDate;
    int startSecondOfDay;
    int endSecondOfDay;
    int dailyCount;
    long totalCountEver;
    float batteryVoltage;
    int rideTimestamps[500]; // Max 500 Fahrten pro Tag
    int timestampCount;
};

void setup() {
    Serial.begin(9600);
    sim7000.begin(9600);
    
    Serial.println("Bike Counter Arduino Starting...");
    
    // SIM7000E initialisieren
    initializeSIM7000();
}

void loop() {
    // Hier würde deine normale Zähllogik laufen
    // Am Ende des Tages (nach Sonnenuntergang) sendest du die Daten
    
    if (shouldTransmitData()) {
        BikeData todaysData = collectTodaysData();
        transmitDataToAPI(todaysData);
        
        // Nächste Übertragung erst am nächsten Tag
        delay(24 * 60 * 60 * 1000); // 24 Stunden warten
    }
    
    delay(1000);
}

void initializeSIM7000() {
    Serial.println("Initializing SIM7000E...");
    
    // Modul einschalten
    sim7000.println("AT");
    delay(1000);
    
    sim7000.println("AT+CPIN?");
    delay(1000);
    
    // Netzwerk-Registrierung prüfen
    sim7000.println("AT+CREG?");
    delay(2000);
    
    // GPRS aktivieren
    sim7000.println("AT+CGATT=1");
    delay(2000);
    
    // APN konfigurieren (Beispiel für deutsche Provider)
    sim7000.println("AT+CSTT=\"internet.telekom\",\"\",\"\""); // Telekom
    // sim7000.println("AT+CSTT=\"internet.vodafone.de\",\"vodafone\",\"vodafone\""); // Vodafone
    // sim7000.println("AT+CSTT=\"internet.eplus.de\",\"eplus\",\"gprs\""); // O2/eplus
    delay(1000);
    
    sim7000.println("AT+CIICR");
    delay(3000);
    
    sim7000.println("AT+CIFSR");
    delay(2000);
    
    Serial.println("SIM7000E initialized!");
}

BikeData collectTodaysData() {
    BikeData data;
    
    // Aktuelle Daten sammeln (Beispielwerte)
    data.transmissionDate = getCurrentDate();
    data.measurementDate = getCurrentDate(); // Oder gestern, je nach Logik
    data.startSecondOfDay = 6 * 3600 + 30 * 60; // 06:30 Uhr
    data.endSecondOfDay = 20 * 3600 + 15 * 60; // 20:15 Uhr
    data.dailyCount = 42; // Hier deine echten Zähldaten
    data.totalCountEver = 1337; // Hier dein echter Gesamtzähler
    data.batteryVoltage = readBatteryVoltage();
    
    // Beispiel-Zeitstempel (hier würdest du deine echten Daten laden)
    data.timestampCount = 5;
    data.rideTimestamps[0] = 7 * 3600; // 07:00
    data.rideTimestamps[1] = 8 * 3600; // 08:00
    data.rideTimestamps[2] = 9 * 3600; // 09:00
    data.rideTimestamps[3] = 17 * 3600; // 17:00
    data.rideTimestamps[4] = 18 * 3600; // 18:00
    
    return data;
}

String createBasicAuthHeader() {
    // Username:Password kombinieren
    String credentials = String(API_USER) + ":" + String(API_PASSWORD);
    
    // Base64 encodieren
    String encoded = base64::encode(credentials);
    
    return "Authorization: Basic " + encoded;
}

void transmitDataToAPI(BikeData data) {
    Serial.println("Starting data transmission...");
    
    // HTTP Connection aufbauen
    sim7000.println("AT+CIPSTART=\"TCP\",\"" + String(API_HOST) + "\"," + String(API_PORT));
    delay(3000);
    
    // JSON Payload erstellen
    String jsonPayload = createJSONPayload(data);
    String authHeader = createBasicAuthHeader();
    
    // HTTP Request Headers
    String httpRequest = "POST " + String(API_PATH) + " HTTP/1.1\r\n";
    httpRequest += "Host: " + String(API_HOST) + "\r\n";
    httpRequest += "Content-Type: application/json\r\n";
    httpRequest += authHeader + "\r\n";
    httpRequest += "Content-Length: " + String(jsonPayload.length()) + "\r\n";
    httpRequest += "Connection: close\r\n\r\n";
    httpRequest += jsonPayload;
    
    // Daten senden
    sim7000.println("AT+CIPSEND=" + String(httpRequest.length()));
    delay(1000);
    
    sim7000.print(httpRequest);
    delay(5000);
    
    // Antwort lesen
    String response = "";
    while (sim7000.available()) {
        response += sim7000.readString();
    }
    
    Serial.println("Server Response:");
    Serial.println(response);
    
    // Verbindung schließen
    sim7000.println("AT+CIPCLOSE");
    delay(1000);
    
    // Erfolg prüfen
    if (response.indexOf("HTTP/1.1 200") > -1) {
        Serial.println("? Data transmission successful!");
    } else if (response.indexOf("HTTP/1.1 401") > -1) {
        Serial.println("? Authentication failed! Check credentials.");
    } else {
        Serial.println("? Data transmission failed!");
        Serial.println("Response: " + response);
    }
}

String createJSONPayload(BikeData data) {
    String json = "{";
    json += "\"transmission_date\":\"" + data.transmissionDate + "\",";
    json += "\"measurement_date\":\"" + data.measurementDate + "\",";
    json += "\"start_second_of_day\":" + String(data.startSecondOfDay) + ",";
    json += "\"end_second_of_day\":" + String(data.endSecondOfDay) + ",";
    json += "\"daily_count\":" + String(data.dailyCount) + ",";
    json += "\"total_count_ever\":" + String(data.totalCountEver) + ",";
    json += "\"battery_voltage\":" + String(data.batteryVoltage, 2) + ",";
    json += "\"ride_timestamps\":[";
    
    for (int i = 0; i < data.timestampCount; i++) {
        json += String(data.rideTimestamps[i]);
        if (i < data.timestampCount - 1) json += ",";
    }
    
    json += "]}";
    return json;
}

String getCurrentDate() {
    // Hier würdest du das echte Datum vom RTC oder NTP holen
    // Beispiel: "2024-06-02"
    return "2024-06-02";
}

float readBatteryVoltage() {
    // Hier würdest du die echte Batteriespannung messen
    // Beispiel für 12V Bleiakku mit Spannungsteiler
    int adcValue = analogRead(A0);
    float voltage = (adcValue / 1023.0) * 5.0 * 3.0; // 3:1 Spannungsteiler
    return voltage;
}

bool shouldTransmitData() {
    // Hier würdest du prüfen, ob es Zeit für die Übertragung ist
    // Z.B. nach Sonnenuntergang und einmal pro Tag
    // Beispiel: return (hour() == 21 && minute() == 0);
    return true; // Für Demo-Zwecke
}

// Debugging Funktionen
void printResponse() {
    Serial.println("SIM7000 Response:");
    while (sim7000.available()) {
        Serial.write(sim7000.read());
    }
    Serial.println();
}

void testConnection() {
    Serial.println("Testing HTTP connection...");
    
    sim7000.println("AT+CIPSTART=\"TCP\",\"httpbin.org\",80");
    delay(3000);
    printResponse();
    
    String testRequest = "GET /get HTTP/1.1\r\nHost: httpbin.org\r\nConnection: close\r\n\r\n";
    sim7000.println("AT+CIPSEND=" + String(testRequest.length()));
    delay(1000);
    sim7000.print(testRequest);
    delay(3000);
    
    printResponse();
    
    sim7000.println("AT+CIPCLOSE");
    delay(1000);
}