/**
 * ESP32-CAM Simple Test - Basic Connectivity
 * ==========================================
 * Minimal firmware to test ESP32-CAM connectivity
 * - Simple web server
 * - Basic status endpoint
 * - WiFi connection test
 */

#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>

// WiFi Configuration
const char* ssid = "realme 9 5G";
const char* password = "x6criudm";

// Camera pins for AI-Thinker ESP32-CAM
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

WebServer server(80);
bool cameraInitialized = false;

void setupCamera() {
  Serial.println("Initializing camera...");
  
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;

  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_CIF;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    cameraInitialized = false;
    return;
  }
  
  Serial.println("Camera initialized successfully");
  cameraInitialized = true;
}

void handleRoot() {
  String html = R"(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32-CAM Test</title>
    <meta name='viewport' content='width=device-width, initial-scale=1'>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background: #1a1a1a; color: #fff; }
        .container { max-width: 600px; margin: 0 auto; background: #2a2a2a; padding: 20px; border-radius: 10px; }
        .status { padding: 15px; margin: 10px 0; border-radius: 8px; text-align: center; }
        .status.ok { background: #2d5a2d; color: #90ee90; }
        .status.error { background: #5a2d2d; color: #ff6b6b; }
        .metric { display: flex; justify-content: space-between; margin: 10px 0; padding: 15px; background: #333; border-radius: 5px; }
        .value { font-size: 24px; font-weight: bold; color: #00ff8c; }
        .label { color: #ccc; }
    </style>
</head>
<body>
    <div class='container'>
        <h1>ESP32-CAM Test Server</h1>
        <div class='status ok'>
            <strong>ESP32-CAM is Running!</strong>
        </div>
        
        <div class='metric'>
            <span class='label'>Camera Status:</span>
            <span class='value'>)" + String(cameraInitialized ? "OK" : "ERROR") + R"(</span>
        </div>
        
        <div class='metric'>
            <span class='label'>WiFi Status:</span>
            <span class='value'>Connected</span>
        </div>
        
        <div class='metric'>
            <span class='label'>IP Address:</span>
            <span class='value'>)" + WiFi.localIP().toString() + R"(</span>
        </div>
        
        <div class='metric'>
            <span class='label'>Free Memory:</span>
            <span class='value'>)" + String(ESP.getFreeHeap() / 1024) + R"( KB</span>
        </div>
        
        <div class='metric'>
            <span class='label'>Uptime:</span>
            <span class='value'>)" + String(millis() / 1000) + R"( s</span>
        </div>
        
        <h3>Test Endpoints:</h3>
        <p><a href='/status' style='color: #00ff8c;'>/status</a> - JSON status</p>
        <p><a href='/camera' style='color: #00ff8c;'>/camera</a> - Camera info</p>
        <p><a href='/test' style='color: #00ff8c;'>/test</a> - Test endpoint</p>
    </div>
</body>
</html>
  )";
  
  server.send(200, "text/html", html);
}

void handleStatus() {
  String json = "{";
  json += "\"status\":\"ok\",";
  json += "\"cameraInitialized\":" + String(cameraInitialized ? "true" : "false") + ",";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  json += "\"freeHeap\":" + String(ESP.getFreeHeap()) + ",";
  json += "\"uptime\":" + String(millis()) + ",";
  json += "\"connectedClients\":0,";
  json += "\"anyAlert\":false,";
  json += "\"cameraActive\":false,";
  json += "\"autoStreamEnabled\":true,";
  json += "\"heartRate\":75,";
  json += "\"ecgValue\":1.5,";
  json += "\"bodyTemperature\":36.8,";
  json += "\"stressLevel\":25";
  json += "}";
  
  server.send(200, "application/json", json);
}

void handleCamera() {
  String json = "{";
  json += "\"status\":\"ok\",";
  json += "\"cameraInitialized\":" + String(cameraInitialized ? "true" : "false") + ",";
  json += "\"model\":\"AI-Thinker ESP32-CAM\",";
  json += "\"resolution\":\"CIF (400x300)\",";
  json += "\"quality\":\"Medium\"";
  json += "}";
  
  server.send(200, "application/json", json);
}

void handleTest() {
  server.send(200, "text/plain", "ESP32-CAM Test Working!");
}

void setup() {
  // Disable brownout detector
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  
  Serial.begin(115200);
  Serial.println();
  Serial.println("=== ESP32-CAM Simple Test Starting ===");

  // Initialize WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println("WiFi connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Signal Strength: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  } else {
    Serial.println();
    Serial.println("WiFi connection failed!");
    return;
  }

  // Initialize camera
  setupCamera();

  // Setup web server
  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.on("/camera", handleCamera);
  server.on("/test", handleTest);

  server.begin();
  Serial.println("HTTP server started");
  Serial.println("Test endpoints available:");
  Serial.println("  http://" + WiFi.localIP().toString() + "/");
  Serial.println("  http://" + WiFi.localIP().toString() + "/status");
  Serial.println("  http://" + WiFi.localIP().toString() + "/camera");
  Serial.println("  http://" + WiFi.localIP().toString() + "/test");
}

void loop() {
  server.handleClient();
  
  // Print status every 30 seconds
  static unsigned long lastStatus = 0;
  if (millis() - lastStatus > 30000) {
    Serial.println("=== Status Update ===");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("Camera: ");
    Serial.println(cameraInitialized ? "OK" : "ERROR");
    Serial.print("Free Heap: ");
    Serial.print(ESP.getFreeHeap());
    Serial.println(" bytes");
    lastStatus = millis();
  }
  
  delay(100);
}
