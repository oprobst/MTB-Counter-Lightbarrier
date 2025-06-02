<?php
/**
 * Bike Counter REST API
 * Arduino Fahrrad-Z�hler Schnittstelle
 * PHP 8.3 + MariaDB 10
 */

declare(strict_types=1);

// Error Reporting f�r Development
error_reporting(E_ALL);
ini_set('display_errors', '1');

// Headers f�r JSON API
header('Content-Type: application/json; charset=utf-8'); // Standard f�r JSON
header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Methods: GET, POST, OPTIONS');
header('Access-Control-Allow-Headers: Content-Type, Authorization');

// Handle preflight requests
if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    http_response_code(200);
    exit();
}

// Konfiguration laden
function loadConfig(): array {
    $config = [];
    if (file_exists('config.properties')) {
        $lines = file('config.properties', FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);
        foreach ($lines as $line) {
            if (strpos($line, '#') === 0) continue; // Kommentare �berspringen
            if (strpos(trim($line), '=') !== false) {
                [$key, $value] = explode('=', $line, 2);
                $config[trim($key)] = trim($value);
            }
        }
    }
    return $config;
}

$config = loadConfig();

// Datenbankverbindung
function getDatabase(array $config): PDO {
    $dsn = sprintf(
        "mysql:host=%s;port=%s;dbname=%s;charset=%s",
        $config['DB_HOST'] ?? 'localhost',
        $config['DB_PORT'] ?? '3306',
        $config['DB_NAME'] ?? 'bike_counter',
        $config['DB_CHARSET'] ?? 'utf8mb4'
    );
    
    $options = [
        PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
        PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
        PDO::ATTR_EMULATE_PREPARES => false,
    ];
    
    return new PDO($dsn, $config['DB_USER'], $config['DB_PASSWORD'], $options);
}

// Zeitzone setzen
date_default_timezone_set($config['TIMEZONE'] ?? 'Europe/Berlin');

// Authentifizierung f�r POST Endpunkte - Ionos kompatibel
function requireBasicAuth(array $config): void {
    $authUser = null;
    $authPassword = null;
    
    // Methode 1: Standard PHP_AUTH_* Variablen (funktioniert bei manchen Hostern)
    if (isset($_SERVER['PHP_AUTH_USER']) && isset($_SERVER['PHP_AUTH_PW'])) {
        $authUser = $_SERVER['PHP_AUTH_USER'];
        $authPassword = $_SERVER['PHP_AUTH_PW'];
    }
    // Methode 2: Authorization Header manuell parsen (Ionos-Fix)
    elseif (isset($_SERVER['HTTP_AUTHORIZATION'])) {
        $authHeader = $_SERVER['HTTP_AUTHORIZATION'];
        if (strpos($authHeader, 'Basic ') === 0) {
            $encoded = substr($authHeader, 6);
            $decoded = base64_decode($encoded);
            if ($decoded && strpos($decoded, ':') !== false) {
                [$authUser, $authPassword] = explode(':', $decoded, 2);
            }
        }
    }
    // Methode 3: Alternative Header-Namen probieren
    elseif (isset($_SERVER['REDIRECT_HTTP_AUTHORIZATION'])) {
        $authHeader = $_SERVER['REDIRECT_HTTP_AUTHORIZATION'];
        if (strpos($authHeader, 'Basic ') === 0) {
            $encoded = substr($authHeader, 6);
            $decoded = base64_decode($encoded);
            if ($decoded && strpos($decoded, ':') !== false) {
                [$authUser, $authPassword] = explode(':', $decoded, 2);
            }
        }
    }
    
    // Debug-Information f�r Troubleshooting
    if (($config['DEBUG_MODE'] ?? 'false') === 'true') {
        error_log('Auth Debug - Available headers: ' . json_encode([
            'PHP_AUTH_USER' => $_SERVER['PHP_AUTH_USER'] ?? 'not set',
            'HTTP_AUTHORIZATION' => $_SERVER['HTTP_AUTHORIZATION'] ?? 'not set',
            'REDIRECT_HTTP_AUTHORIZATION' => $_SERVER['REDIRECT_HTTP_AUTHORIZATION'] ?? 'not set'
        ]));
    }
    
    // Keine Credentials gefunden
    if (empty($authUser) || empty($authPassword)) {
        header('WWW-Authenticate: Basic realm="Bike Counter API"');
        errorResponse('Authentication required - No valid credentials found', 401);
    }
    
    $expectedUser = $config['BASIC_AUTH_USER'] ?? 'bike_counter';
    $expectedPassword = $config['BASIC_AUTH_PASSWORD'] ?? '';
    
    if (empty($expectedPassword)) {
        error_log('WARNING: BASIC_AUTH_PASSWORD not set in config!');
        errorResponse('Server configuration error', 500);
    }
    
    // Verwenden von hash_equals f�r sicheren Vergleich von Passw�rtern
    if (!hash_equals($expectedUser, $authUser) || !hash_equals($expectedPassword, $authPassword)) {
        // Log failed authentication attempts
        error_log(sprintf(
            'Failed authentication attempt from %s for user %s', 
            $_SERVER['REMOTE_ADDR'] ?? 'unknown',
            $authUser ?? 'none'
        ));
        
        header('WWW-Authenticate: Basic realm="Bike Counter API"');
        errorResponse('Invalid credentials', 401);
    }
    
    // Success - optional logging
    if (($config['DEBUG_MODE'] ?? 'false') === 'true') {
        error_log("Successful authentication for user: $authUser");
    }
}

// Hilfsfunktionen
function jsonResponse(array $data, int $statusCode = 200): void {
    http_response_code($statusCode);
    echo json_encode($data, JSON_PRETTY_PRINT | JSON_UNESCAPED_UNICODE);
    exit();
}

function errorResponse(string $message, int $statusCode = 400): void {
    jsonResponse(['error' => $message, 'timestamp' => date('c')], $statusCode);
}

function secondOfDayToTime(int $secondOfDay): string {
    $hours = intval($secondOfDay / 3600);
    $minutes = intval(($secondOfDay % 3600) / 60);
    $seconds = $secondOfDay % 60;
    return sprintf('%02d:%02d:%02d', $hours, $minutes, $seconds);
}

function validateBatteryVoltage(float $voltage, array $config): bool {
    $min = floatval($config['MIN_BATTERY_VOLTAGE'] ?? 10.0);
    $max = floatval($config['MAX_BATTERY_VOLTAGE'] ?? 15.0);
    return $voltage >= $min && $voltage <= $max;
}

// NEUE FUNKTION: CSV-Download
function csvDownload(string $filename, array $headers, array $data): void {
    // Headers f�r CSV-Download setzen
    header('Content-Type: text/csv; charset=utf-8'); // Explizit UTF-8 deklarieren
    header('Content-Disposition: attachment; filename="' . $filename . '"');
    header('Pragma: no-cache');
    header('Expires: 0');

    // Output Buffer leeren, falls vorhanden
    if (ob_get_level() > 0) {
        ob_end_clean();
    }

    $output = fopen('php://output', 'w'); // Direkt in den Output-Stream schreiben

    // UTF-8 BOM hinzuf�gen (wichtig f�r Excel, um UTF-8 korrekt zu erkennen)
    fwrite($output, "\xEF\xBB\xBF"); // Dies ist die UTF-8 BOM

    // CSV-Header schreiben
    fputcsv($output, $headers, ';'); // Semikolon als Trennzeichen f�r Excel-Kompatibilit�t in DE

    // Datenzeilen schreiben
    foreach ($data as $row) {
        // Sicherstellen, dass alle Werte als Strings behandelt werden,
        // um m�gliche PHP-Warnungen zu vermeiden und fputcsv zu helfen
        // und die Encoding-Probleme zu minimieren.
        $row_safe = array_map(function($value) {
            return (string) $value;
        }, $row);
        fputcsv($output, $row_safe, ';');
    }

    fclose($output);
    exit(); // Skript beenden, nachdem die Datei gesendet wurde
}

// Routing
$method = $_SERVER['REQUEST_METHOD'];
$path = parse_url($_SERVER['REQUEST_URI'], PHP_URL_PATH);
$pathParts = explode('/', trim($path, '/'));

try {
    $pdo = getDatabase($config);
    $linkdepth= $config['API_DEPTH'] ?? 2;

    switch ($method) {
        case 'POST':
            // Authentifizierung f�r alle POST Endpunkte erforderlich
            requireBasicAuth($config);
            
            if ($pathParts[0+$linkdepth] === 'api' && $pathParts[1+$linkdepth] === 'daily-report') {
                handleDailyReport($pdo, $config);
            } else {
                errorResponse('Unknown POST endpoint', 404);
            }
            break;
            
        case 'GET':
            if ($pathParts[0+$linkdepth] === 'api') {
                switch ($pathParts[1+$linkdepth] ?? '') {
                    case 'daily-summary':
                        handleDailySummary($pdo);
                        break;
                    case 'rides':
                        handleGetRides($pdo);
                        break;
                    case 'rides-csv': // ENDPUNKT UMBENANNT
                        handleGetRidesCsv($pdo); // Funktion umbenannt
                        break;
                    case 'report-csv': // NEUER ENDPUNKT
                        handleGetDailyReportsCsv($pdo); 
                        break;
                    case 'battery-status':
                        handleBatteryStatus($pdo);
                        break;
                    case 'total-count':
                        handleTotalCount($pdo);
                        break;
                    case 'status':
                        jsonResponse(['status' => 'OK', 'version' => $config['API_VERSION'] ?? '1.0']);
                        break;
                    default:
                        errorResponse('Unknown GET endpoint', 404);
                }
            } else {
                errorResponse('API endpoint required', 404);
            }
            break;
            
        default:
            errorResponse('Method not allowed', 405);
    }
    
} catch (Exception $e) {
    error_log("Bike Counter API Error: " . $e->getMessage());
    errorResponse('Internal server error', 500);
}

// POST: Täglichen Bericht vom Arduino empfangen
function handleDailyReport(PDO $pdo, array $config): void {
    $input = json_decode(file_get_contents('php://input'), true);
    
    if (!$input) {
        errorResponse('Invalid JSON payload');
    }
    
    // Validierung der erforderlichen Felder
    $required = [
        'transmission_date', 'measurement_date', 'start_second_of_day', 
        'end_second_of_day', 'daily_count', 'total_count_ever', 
        'battery_voltage', 'ride_timestamps'
    ];
    
    foreach ($required as $field) {
        if (!isset($input[$field])) {
            errorResponse("Missing required field: $field");
        }
    }
    
    // Datenvalidierung
    if (!validateBatteryVoltage((float)$input['battery_voltage'], $config)) {
        errorResponse('Battery voltage out of valid range');
    }
    
    if (!is_array($input['ride_timestamps'])) {
        errorResponse('ride_timestamps must be an array');
    }
    
    if (count($input['ride_timestamps']) != $input['daily_count']) {
        errorResponse('Number of timestamps must match daily_count');
    }
    
    // Zeitkonvertierung
    $startTime = secondOfDayToTime((int)$input['start_second_of_day']);
    $endTime = secondOfDayToTime((int)$input['end_second_of_day']);
    
    $pdo->beginTransaction();
    
    try {
        // Daily Report einfügen/aktualisieren
        $stmt = $pdo->prepare("
            INSERT INTO daily_reports 
            (transmission_date, measurement_date, start_time, end_time, daily_count, total_count_ever, battery_voltage)
            VALUES (?, ?, ?, ?, ?, ?, ?)
            ON DUPLICATE KEY UPDATE
            transmission_date = VALUES(transmission_date),
            start_time = VALUES(start_time),
            end_time = VALUES(end_time),
            daily_count = VALUES(daily_count),
            total_count_ever = VALUES(total_count_ever),
            battery_voltage = VALUES(battery_voltage),
            updated_at = CURRENT_TIMESTAMP
        ");
        
        $stmt->execute([
            $input['transmission_date'],
            $input['measurement_date'],
            $startTime,
            $endTime,
            $input['daily_count'],
            $input['total_count_ever'],
            $input['battery_voltage']
        ]);
        
        $reportId = $pdo->lastInsertId() ?: $pdo->query("SELECT id FROM daily_reports WHERE measurement_date = " . $pdo->quote($input['measurement_date']))->fetchColumn();
        
        // Alte Fahrten f�r diesen Tag l�schen (bei Update)
        $stmt = $pdo->prepare("DELETE FROM bike_rides WHERE daily_report_id = ?");
        $stmt->execute([$reportId]);
        
        // Neue Fahrten einf�gen
        $stmt = $pdo->prepare("
            INSERT INTO bike_rides (daily_report_id, measurement_date, ride_time, second_of_day)
            VALUES (?, ?, ?, ?)
        ");
        
        foreach ($input['ride_timestamps'] as $secondOfDay) {
            $rideTime = secondOfDayToTime((int)$secondOfDay);
            $stmt->execute([$reportId, $input['measurement_date'], $rideTime, $secondOfDay]);
        }
        
        $pdo->commit();
        
        jsonResponse([
            'success' => true,
            'message' => 'Daily report saved successfully',
            'report_id' => $reportId,
            'rides_saved' => count($input['ride_timestamps']),
            'timestamp' => date('c')
        ]);
        
    } catch (Exception $e) {
        $pdo->rollBack();
        throw $e;
    }
}

// GET: Fahrten-Zusammenfassung f�r Zeitraum
function handleDailySummary(PDO $pdo): void {
    $startDate = $_GET['start_date'] ?? date('Y-m-d', strtotime('-7 days'));
    $endDate = $_GET['end_date'] ?? date('Y-m-d');
    
    $stmt = $pdo->prepare("
        SELECT measurement_date, daily_count, total_count_ever, battery_voltage,
               start_time, end_time, transmission_date, recorded_rides,
               first_ride, last_ride
        FROM daily_ride_summary
        WHERE measurement_date BETWEEN ? AND ?
        ORDER BY measurement_date DESC
    ");
    
    $stmt->execute([$startDate, $endDate]);
    $results = $stmt->fetchAll();
    
    jsonResponse([
        'period' => ['start' => $startDate, 'end' => $endDate],
        'total_days' => count($results),
        'total_rides' => array_sum(array_column($results, 'daily_count')),
        'daily_summaries' => $results
    ]);
}

// GET: Alle Fahrten f�r einen bestimmten Tag
function handleGetRides(PDO $pdo): void {
    $date = $_GET['date'] ?? date('Y-m-d');
    
    $stmt = $pdo->prepare("
        SELECT br.ride_time, br.second_of_day, dr.daily_count, dr.battery_voltage,
               dr.start_time, dr.end_time, br.measurement_date
        FROM bike_rides br
        JOIN daily_reports dr ON br.daily_report_id = dr.id
        WHERE br.measurement_date = ?
        ORDER BY br.ride_time ASC
    ");
    
    $stmt->execute([$date]);
    $rides = $stmt->fetchAll();
    
    if (empty($rides)) {
        jsonResponse([
            'date' => $date,
            'message' => 'No rides found for this date',
            'rides' => []
        ]);
    }
    
    jsonResponse([
        'date' => $date,
        'total_rides' => count($rides),
        'daily_total' => $rides[0]['daily_count'] ?? 0,
        'period' => [
            'start' => $rides[0]['start_time'] ?? null,
            'end' => $rides[0]['end_time'] ?? null
        ],
        'battery_voltage' => $rides[0]['battery_voltage'] ?? null,
        'rides' => array_map(function($ride) {
            return [
                'time' => $ride['ride_time'],
                'second_of_day' => $ride['second_of_day']
            ];
        }, $rides)
    ]);
}

// Export der Fahrten als CSV (umbenannt von handleGetRidesExcel)
function handleGetRidesCsv(PDO $pdo): void {
    $startDate = $_GET['start_date'] ?? null;
    $endDate = $_GET['end_date'] ?? null;

    $query = "
        SELECT 
            br.measurement_date,
            br.ride_time,
            ROW_NUMBER() OVER (PARTITION BY br.measurement_date ORDER BY br.ride_time ASC) AS ride_number_daily
        FROM bike_rides br
    ";
    
    $params = [];
    $whereClauses = [];

    if ($startDate && $endDate) {
        $whereClauses[] = "br.measurement_date BETWEEN ? AND ?";
        $params[] = $startDate;
        $params[] = $endDate;
    } elseif ($startDate) {
        $whereClauses[] = "br.measurement_date >= ?";
        $params[] = $startDate;
    } elseif ($endDate) {
        $whereClauses[] = "br.measurement_date <= ?";
        $params[] = $endDate;
    }

    if (!empty($whereClauses)) {
        $query .= " WHERE " . implode(' AND ', $whereClauses);
    }

    $query .= " ORDER BY br.measurement_date ASC, br.ride_time ASC";
    
    $stmt = $pdo->prepare($query);
    $stmt->execute($params);
    $results = $stmt->fetchAll();

    if (empty($results)) {
        errorResponse('No ride data available for the specified period.', 404);
    }

    $headers = [
        'Nr', 
        'Datum', 
        'Uhrzeit'
    ];

    $data = [];
    foreach ($results as $row) {
        $data[] = [
            $row['ride_number_daily'],
            $row['measurement_date'],
            $row['ride_time']
        ];
    }
    
    $filename = 'bike_rides_report_' . date('Ymd_His') . '.csv';
    csvDownload($filename, $headers, $data);
}

// NEUE FUNKTION: Export der t�glichen Berichte als CSV
function handleGetDailyReportsCsv(PDO $pdo): void {
    $startDate = $_GET['start_date'] ?? null;
    $endDate = $_GET['end_date'] ?? null;

    $query = "
        SELECT 
            dr.measurement_date,
            dr.daily_count,
            dr.total_count_ever,
            dr.battery_voltage,
            dr.start_time,
            dr.end_time,
            dr.transmission_date
        FROM daily_reports dr
    ";
    
    $params = [];
    $whereClauses = [];

    if ($startDate && $endDate) {
        $whereClauses[] = "dr.measurement_date BETWEEN ? AND ?";
        $params[] = $startDate;
        $params[] = $endDate;
    } elseif ($startDate) {
        $whereClauses[] = "dr.measurement_date >= ?";
        $params[] = $startDate;
    } elseif ($endDate) {
        $whereClauses[] = "dr.measurement_date <= ?";
        $params[] = $endDate;
    }

    if (!empty($whereClauses)) {
        $query .= " WHERE " . implode(' AND ', $whereClauses);
    }

    $query .= " ORDER BY dr.measurement_date ASC";
    
    $stmt = $pdo->prepare($query);
    $stmt->execute($params);
    $results = $stmt->fetchAll();

    if (empty($results)) {
        errorResponse('No daily report data available for the specified period.', 404);
    }

    $headers = [
        'Messdatum', 
        'Tägliche Fahrten', 
        'Gesamtzahl Fahrten (ever)', 
        'Batteriespannung (V)', 
        'Startzeitpunkt Tag', 
        'Endzeitpunkt Tag', 
        'Übertragungsdatum'
    ];

    $data = [];
    foreach ($results as $row) {
        $data[] = [
            $row['measurement_date'],
            $row['daily_count'],
            $row['total_count_ever'],
            str_replace('.', ',', (string)$row['battery_voltage']), // Komma f�r Excel
            $row['start_time'],
            $row['end_time'],
            $row['transmission_date']
        ];
    }
    
    $filename = 'daily_reports_' . date('Ymd_His') . '.csv';
    csvDownload($filename, $headers, $data);
}

// GET: Letzter Batteriestand
function handleBatteryStatus(PDO $pdo): void {
    $stmt = $pdo->query("
        SELECT battery_voltage, measurement_date, transmission_date
        FROM daily_reports
        ORDER BY transmission_date DESC, id DESC
        LIMIT 1
    ");
    
    $result = $stmt->fetch();
    
    if (!$result) {
        jsonResponse(['message' => 'No battery data available']);
    }
    
    jsonResponse([
        'battery_voltage' => (float)$result['battery_voltage'],
        'measurement_date' => $result['measurement_date'],
        'last_transmission' => $result['transmission_date'],
        'status' => $result['battery_voltage'] >= 11.5 ? 'OK' : 'LOW'
    ]);
}

// GET: Letzter Gesamtz�hler
function handleTotalCount(PDO $pdo): void {
    $stmt = $pdo->query("
        SELECT total_count_ever, measurement_date, transmission_date
        FROM daily_reports
        ORDER BY transmission_date DESC, id DESC
        LIMIT 1
    ");
    
    $result = $stmt->fetch();
    
    if (!$result) {
        jsonResponse(['message' => 'No count data available']);
    }
    
    jsonResponse([
        'total_count_ever' => (int)$result['total_count_ever'],
        'measurement_date' => $result['measurement_date'],
        'last_transmission' => $result['transmission_date']
    ]);
}