<?php
// Allow all origins (you can replace * with your domain to restrict it)
header('Content-Type: application/json');
header("Access-Control-Allow-Origin: https://teunroetman.design");
header("Access-Control-Allow-Methods: POST, OPTIONS");
header("Access-Control-Allow-Headers: Content-Type");

// Handle preflight OPTIONS request
if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    http_response_code(204); // No Content
    exit();
}

// Only allow POST requests beyond this point
if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    http_response_code(405); // Method Not Allowed
    echo json_encode(["status" => "error", "message" => "Only POST method allowed."]);
    exit();
}

// Get and decode JSON input
$file = '../messages.txt';
$data = json_decode(file_get_contents('php://input'), true);

if (isset($data['message']) && isset($data['time'])) {
    $timestamp = $data['time'];
    $message = $data['message'];

    $entry = "[" . $timestamp . "] " . $message . PHP_EOL;
    file_put_contents($file, $entry, FILE_APPEND | LOCK_EX);

    echo json_encode(['status' => 'success']);
} else {
    echo json_encode(['status' => 'error', 'message' => 'Invalid input']);
}

?>
