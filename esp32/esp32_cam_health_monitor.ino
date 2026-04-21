/**
 * ESP32-CAM Health Monitor with Sensor Integration
 * ================================================
 * Features:
 * - Live camera streaming with health alert triggers
 * - Fetches real-time health data from main ESP32 sensor node
 * - Automatic camera activation on abnormal health conditions
 * - Motion detection and posture monitoring
 * - Web interface for camera control and health status
 * 
 * Hardware:
 * - ESP32-CAM (AI-Thinker)
 * - Connects to main ESP32 health sensor at 10.37.112.48
 * - WiFi: realme 9 5G
 */

#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <soc/soc.h>
#include <soc/rtc_cntl_reg.h>

// -------- WIFI CONFIGURATION --------
const char* ssid = "realme 9 5G";
const char* password = "x6criudm";

// -------- MAIN ESP32 SENSOR API --------
const char* sensorURL = "http://10.171.201.48/health"; // Health data endpoint
const char* insightsURL = "http://10.171.201.48/insights"; // Health insights endpoint

// -------- CAMERA CONFIGURATION --------
#define CAMERA_MODEL_AI_THINKER

// Camera pin configuration for AI-Thinker ESP32-CAM
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

// -------- HEALTH DATA STRUCTURE --------
struct HealthData {
  int heartRate;
  int ecgBPM;
  float ecgValue;
  float ecgVoltage;
  float bodyTemperature;
  float ambientTemp;
  float humidity;
  float stressLevel;
  float stressIndex;
  float oxygenLevel;
  float spo2;
  int steps;
  bool isMoving;
  float motionLevel;
  bool leadsOff;
  bool max30102;
  String overallHealth;
  String overallStatus;
  String recommendation;
  unsigned long timestamp;
  int uptime;
};

// -------- ALERT SYSTEM --------
struct AlertSystem {
  bool heartRateAlert;
  bool ecgAlert;
  bool temperatureAlert;
  bool stressAlert;
  bool motionAlert;
  bool anyAlert;
  unsigned long lastAlertTime;
  String alertReason;
};

// -------- GLOBAL VARIABLES --------
HealthData healthData;
AlertSystem alerts;
WebServer server(80);
bool cameraActive = false;
bool autoStreamEnabled = true;
unsigned long lastSensorUpdate = 0;
unsigned long lastCameraCapture = 0;
unsigned long streamStartTime = 0;
int connectedClients = 0;

// -------- CAMERA INITIALIZATION --------
void initializeCamera() {
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

  // Set frame size and quality
  if (psramFound()) {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
    Serial.println("PSRAM found - Using high quality settings");
  } else {
    config.frame_size = FRAMESIZE_CIF;
    config.jpeg_quality = 12;
    config.fb_count = 1;
    Serial.println("No PSRAM - Using lower quality settings");
  }

  // Initialize camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  // Get camera sensor info
  sensor_t * s = esp_camera_sensor_get();
  s->set_brightness(s, 0);     // -2 to 2
  s->set_contrast(s, 0);       // -2 to 2
  s->set_saturation(s, 0);     // -2 to 2
  s->set_special_effect(s, 0); // 0 to 6 (0 - No Effect, 1 - Negative, 2 - Grayscale, 3 - Red Tint, 4 - Green Tint, 5 - Blue Tint, 6 - Sepia)
  s->set_whitebal(s, 1);       // 0 = disable , 1 = enable
  s->set_awb_gain(s, 1);       // 0 = disable , 1 = enable
  s->set_wb_mode(s, 0);        // 0 to 4 - if awb_gain enabled (0 - Auto, 1 - Sunny, 2 - Cloudy, 3 - Office, 4 - Home)
  s->set_exposure_ctrl(s, 1);  // 0 = disable , 1 = enable
  s->set_aec2(s, 0);           // 0 = disable , 1 = enable
  s->set_ae_level(s, 0);       // -2 to 2
  s->set_aec_value(s, 300);    // 0 to 1200
  s->set_gain_ctrl(s, 1);      // 0 = disable , 1 = enable
  s->set_agc_gain(s, 0);       // 0 to 30
  s->set_gainceiling(s, (gainceiling_t)0);  // 0 to 6
  s->set_bpc(s, 0);            // 0 = disable , 1 = enable
  s->set_wpc(s, 1);            // 0 = disable , 1 = enable
  s->set_raw_gma(s, 1);        // 0 = disable , 1 = enable
  s->set_lenc(s, 1);           // 0 = disable , 1 = enable
  s->set_hmirror(s, 0);        // 0 = disable , 1 = enable
  s->set_vflip(s, 0);          // 0 = disable , 1 = enable
  s->set_dcw(s, 1);            // 0 = disable , 1 = enable
  s->set_colorbar(s, 0);       // 0 = disable , 1 = enable

  Serial.println("Camera initialized successfully");
}

// -------- FETCH SENSOR DATA FROM MAIN ESP32 --------
void fetchSensorData() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected - cannot fetch sensor data");
    return;
  }

  HTTPClient http;
  http.begin(sensorURL);
  http.setTimeout(5000);

  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    
    // Parse JSON response
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      // Update health data structure with actual data
      healthData.heartRate = doc["hr"] | doc["heartRate"] | 0;
      healthData.ecgBPM = doc["ecgBPM"] | healthData.heartRate;
      healthData.ecgValue = doc["ecg"] | doc["ecgValue"] | 0.0;
      healthData.ecgVoltage = doc["ecgVoltage"] | 0.0;
      healthData.bodyTemperature = doc["temp"] | doc["bodyTemperature"] | 0.0;
      healthData.ambientTemp = doc["ambientTemp"] | 0.0;
      healthData.humidity = doc["humidity"] | 0.0;
      healthData.stressLevel = doc["stress"] | doc["stressLevel"] | doc["stressIndex"] | 0.0;
      healthData.stressIndex = doc["stressIndex"] | healthData.stressLevel;
      healthData.oxygenLevel = doc["o2"] | doc["oxygenLevel"] | doc["spo2"] | 0.0;
      healthData.spo2 = doc["spo2"] | healthData.oxygenLevel;
      healthData.steps = doc["steps"] | 0;
      healthData.isMoving = doc["moving"] | doc["isMoving"] | false;
      healthData.motionLevel = doc["motionLevel"] | 0.0;
      healthData.leadsOff = doc["leadsOff"] | false;
      healthData.max30102 = doc["max30102"] | false;
      healthData.timestamp = millis();
      
      Serial.println("=== Health Data Updated ===");
      Serial.printf("Heart Rate: %d BPM\n", healthData.heartRate);
      Serial.printf("ECG BPM: %d\n", healthData.ecgBPM);
      Serial.printf("Temperature: %.1f°C\n", healthData.bodyTemperature);
      Serial.printf("Stress Level: %.1f%%\n", healthData.stressLevel);
      Serial.printf("Oxygen Level: %.1f%%\n", healthData.oxygenLevel);
      Serial.printf("Steps: %d\n", healthData.steps);
      Serial.printf("Moving: %s\n", healthData.isMoving ? "Yes" : "No");
      
      lastSensorUpdate = millis();
      
    } else {
      Serial.print("JSON parsing failed: ");
      Serial.println(error.c_str());
    }
  } else {
    Serial.printf("HTTP error: %d\n", httpCode);
  }

  http.end();

  // Also fetch insights for recommendations
  fetchHealthInsights();
}

// -------- FETCH HEALTH INSIGHTS --------
void fetchHealthInsights() {
  HTTPClient http;
  http.begin(insightsURL);
  http.setTimeout(3000);

  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      healthData.overallHealth = doc["overallHealth"].as<String>();
      healthData.recommendation = doc["recommendation"].as<String>();
      
      Serial.printf("Overall Health: %s\n", healthData.overallHealth.c_str());
      Serial.printf("Recommendation: %s\n", healthData.recommendation.c_str());
    }
  }

  http.end();
}

// -------- HEALTH ALERT MONITORING --------
void checkHealthAlerts() {
  alerts.anyAlert = false;
  alerts.alertReason = "";

  // Heart rate alerts
  if (healthData.heartRate > 120 || healthData.heartRate < 40) {
    alerts.heartRateAlert = true;
    alerts.anyAlert = true;
    alerts.alertReason += "Heart Rate Abnormal; ";
  } else {
    alerts.heartRateAlert = false;
  }

  // ECG alerts
  if (healthData.ecgValue > 2.5 || healthData.ecgValue < 0.5) {
    alerts.ecgAlert = true;
    alerts.anyAlert = true;
    alerts.alertReason += "ECG Signal Abnormal; ";
  } else {
    alerts.ecgAlert = false;
  }

  // Temperature alerts
  if (healthData.bodyTemperature > 38.0 || healthData.bodyTemperature < 35.0) {
    alerts.temperatureAlert = true;
    alerts.anyAlert = true;
    alerts.alertReason += "Temperature Abnormal; ";
  } else {
    alerts.temperatureAlert = false;
  }

  // Stress alerts
  if (healthData.stressLevel > 80) {
    alerts.stressAlert = true;
    alerts.anyAlert = true;
    alerts.alertReason += "High Stress; ";
  } else {
    alerts.stressAlert = false;
  }

  // Motion alerts (no movement for extended time)
  static unsigned long lastMotionTime = millis();
  if (healthData.isMoving) {
    lastMotionTime = millis();
  } else if (millis() - lastMotionTime > 300000) { // 5 minutes no movement
    alerts.motionAlert = true;
    alerts.anyAlert = true;
    alerts.alertReason += "No Movement Detected; ";
  }

  // Log alerts
  if (alerts.anyAlert) {
    if (millis() - alerts.lastAlertTime > 30000) { // Throttle alerts to every 30 seconds
      Serial.println("=== HEALTH ALERT TRIGGERED ===");
      Serial.printf("Reason: %s\n", alerts.alertReason.c_str());
      Serial.printf("Heart Rate: %d BPM\n", healthData.heartRate);
      Serial.printf("ECG: %.3fV\n", healthData.ecgValue);
      Serial.printf("Temperature: %.1f°C\n", healthData.bodyTemperature);
      Serial.printf("Stress: %.1f%%\n", healthData.stressLevel);
      
      alerts.lastAlertTime = millis();
      
      // Auto-activate camera if enabled
      if (autoStreamEnabled && !cameraActive) {
        Serial.println("Auto-activating camera due to health alert");
        cameraActive = true;
        streamStartTime = millis();
      }
    }
  }
}

// -------- CAMERA STREAM HANDLER --------
void handleStream(WiFiClient client) {
  Serial.println("Client connected for camera stream");
  connectedClients++;

  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: multipart/x-mixed-replace; boundary=frame");
  client.println("Access-Control-Allow-Origin: *");
  client.println();

  // Stream duration (5 minutes max for auto-activated streams)
  unsigned long streamDuration = alerts.anyAlert ? 300000 : 600000; // 5 min alert, 10 min manual
  unsigned long streamStart = millis();

  while (client.connected() && (millis() - streamStart < streamDuration)) {
    if (!cameraActive && !alerts.anyAlert) {
      break; // Stop streaming if camera deactivated and no alerts
    }

    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      delay(1000);
      continue;
    }

    client.println("--frame");
    client.println("Content-Type: image/jpeg");
    client.println("Content-Length: " + String(fb->len));
    client.println();
    
    client.write(fb->buf, fb->len);
    client.println();
    
    esp_camera_fb_return(fb);
    
    // Add health status overlay info every 10 frames
    static int frameCount = 0;
    frameCount++;
    if (frameCount % 10 == 0) {
      String statusInfo = "HR:" + String(healthData.heartRate) + 
                         " STRESS:" + String(healthData.stressLevel, 0) + "%" +
                         " " + (alerts.anyAlert ? "ALERT" : "OK");
      Serial.println("Stream Status: " + statusInfo);
    }

    delay(100); // ~10 fps
  }

  client.stop();
  connectedClients--;
  Serial.println("Client disconnected from camera stream");

  // Auto-deactivate camera if it was auto-activated
  if (alerts.anyAlert && millis() - streamStartTime > 300000) {
    cameraActive = false;
    Serial.println("Auto-deactivating camera after alert duration");
  }
}

// -------- WEB SERVER HANDLERS --------
void handleRoot() {
  String html = R"(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32-CAM Health Monitor</title>
    <meta name='viewport' content='width=device-width, initial-scale=1'>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background: #1a1a1a; color: #fff; }
        .container { max-width: 900px; margin: 0 auto; background: #2a2a2a; padding: 20px; border-radius: 10px; }
        .status { padding: 15px; margin: 10px 0; border-radius: 8px; text-align: center; }
        .status.ok { background: #2d5a2d; color: #90ee90; }
        .status.alert { background: #5a2d2d; color: #ff6b6b; }
        .metric { display: flex; justify-content: space-between; margin: 10px 0; padding: 15px; background: #333; border-radius: 5px; }
        .value { font-size: 24px; font-weight: bold; color: #00ff8c; }
        .label { color: #ccc; }
        .camera-controls { margin: 20px 0; text-align: center; }
        .button { padding: 10px 20px; margin: 5px; border: none; border-radius: 5px; cursor: pointer; font-weight: bold; }
        .button.start { background: #28a745; color: white; }
        .button.stop { background: #dc3545; color: white; }
        .stream-container { margin: 20px 0; text-align: center; }
        .stream-img { max-width: 100%; border-radius: 8px; }
    </style>
</head>
<body>
    <div class='container'>
        <h1>ESP32-CAM Health Monitor</h1>
        <div class='status )" + (alerts.anyAlert ? "alert" : "ok") + R"(' id='status'>
            <strong>)" + (alerts.anyAlert ? "ALERT TRIGGERED" : "System Normal") + R"(</strong><br>
            <span id='alertReason'>)" + alerts.alertReason + R"(</span>
        </div>
        
        <div class='metric'>
            <span class='label'>Heart Rate:</span>
            <span class='value'>)" + String(healthData.heartRate) + R"( BPM</span>
        </div>
        <div class='metric'>
            <span class='label'>ECG Value:</span>
            <span class='value'>)" + String(healthData.ecgValue, 3) + R"( V</span>
        </div>
        <div class='metric'>
            <span class='label'>Temperature:</span>
            <span class='value'>)" + String(healthData.bodyTemperature, 1) + R"(°C</span>
        </div>
        <div class='metric'>
            <span class='label'>Stress Level:</span>
            <span class='value'>)" + String(healthData.stressLevel, 1) + R"(%</span>
        </div>
        <div class='metric'>
            <span class='label'>Oxygen Level:</span>
            <span class='value'>)" + String(healthData.oxygenLevel, 1) + R"(%</span>
        </div>
        <div class='metric'>
            <span class='label'>Steps:</span>
            <span class='value'>)" + String(healthData.steps) + R"(</span>
        </div>
        <div class='metric'>
            <span class='label'>Status:</span>
            <span class='value'>)" + (healthData.isMoving ? "Active" : "Resting") + R"(</span>
        </div>
        
        <div class='camera-controls'>
            <button class='button start' onclick='startCamera()'>Start Camera</button>
            <button class='button stop' onclick='stopCamera()'>Stop Camera</button>
            <button class='button start' onclick='toggleAuto()'>Toggle Auto-Stream</button>
        </div>
        
        <div class='stream-container'>
            <img id='stream' class='stream-img' src='' alt='Camera stream will appear here'>
        </div>
        
        <div style='text-align: center; margin-top: 20px;'>
            <p><strong>Auto-Stream:</strong> )" + (autoStreamEnabled ? "Enabled" : "Disabled") + R"(</p>
            <p><strong>Connected Clients:</strong> <span id='clients'>)" + String(connectedClients) + R"(</span></p>
            <p><strong>Last Update:</strong> <span id='lastUpdate'>)" + String(millis() - lastSensorUpdate) + R"( ms ago</span></p>
        </div>
    </div>
    
    <script>
        function startCamera() {
            document.getElementById('stream').src = 'http://' + window.location.hostname + ':81/stream';
        }
        
        function stopCamera() {
            document.getElementById('stream').src = '';
        }
        
        function toggleAuto() {
            fetch('/toggleAuto')
                .then(() => location.reload());
        }
        
        // Auto-refresh page every 30 seconds
        setTimeout(() => location.reload(), 30000);
    </script>
</body>
</html>
  )";
  
  server.send(200, "text/html", html);
}

void handleStatus() {
  StaticJsonDocument<512> doc;
  
  doc["heartRate"] = healthData.heartRate;
  doc["ecgBPM"] = healthData.ecgBPM;
  doc["ecgValue"] = healthData.ecgValue;
  doc["bodyTemperature"] = healthData.bodyTemperature;
  doc["stressLevel"] = healthData.stressLevel;
  doc["oxygenLevel"] = healthData.oxygenLevel;
  doc["steps"] = healthData.steps;
  doc["isMoving"] = healthData.isMoving;
  doc["overallHealth"] = healthData.overallHealth;
  doc["recommendation"] = healthData.recommendation;
  doc["cameraActive"] = cameraActive;
  doc["autoStreamEnabled"] = autoStreamEnabled;
  doc["connectedClients"] = connectedClients;
  doc["anyAlert"] = alerts.anyAlert;
  doc["alertReason"] = alerts.alertReason;
  doc["lastSensorUpdate"] = lastSensorUpdate;
  doc["uptime"] = millis();
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleToggleAuto() {
  autoStreamEnabled = !autoStreamEnabled;
  String message = "Auto-stream " + String(autoStreamEnabled ? "enabled" : "disabled");
  server.send(200, "text/plain", message);
  Serial.println(message);
}

void handleCameraControl() {
  if (server.hasArg("action")) {
    String action = server.arg("action");
    if (action == "start") {
      cameraActive = true;
      streamStartTime = millis();
      server.send(200, "text/plain", "Camera started");
    } else if (action == "stop") {
      cameraActive = false;
      server.send(200, "text/plain", "Camera stopped");
    } else {
      server.send(400, "text/plain", "Invalid action");
    }
  } else {
    server.send(400, "text/plain", "Missing action parameter");
  }
}

// -------- SETUP --------
void setup() {
  // Disable brownout detector
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  
  Serial.begin(115200);
  Serial.println();
  Serial.println("=== ESP32-CAM Health Monitor Starting ===");

  // Initialize WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println();
  Serial.println("WiFi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Signal Strength: ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");

  // Initialize camera
  initializeCamera();

  // Initialize health data
  memset(&healthData, 0, sizeof(healthData));
  memset(&alerts, 0, sizeof(alerts));

  // Setup web server routes
  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.on("/toggleAuto", handleToggleAuto);
  server.on("/camera", handleCameraControl);

  server.begin();
  Serial.println("HTTP server started");
  Serial.println("Camera stream available at: http://" + WiFi.localIP().toString() + ":81/stream");
  Serial.println("Web interface at: http://" + WiFi.localIP().toString());
  
  // Initial sensor data fetch
  delay(2000); // Wait for main ESP32 to be ready
  fetchSensorData();
}

// -------- MAIN LOOP --------
void loop() {
  server.handleClient();

  // Fetch sensor data every 3 seconds
  if (millis() - lastSensorUpdate > 3000) {
    fetchSensorData();
  }

  // Check for health alerts
  checkHealthAlerts();

  // Handle camera streaming client
  WiFiClient client = server.available();
  if (client) {
    Serial.println("New client connected");
    handleStream(client);
  }

  // Print status every 30 seconds
  static unsigned long lastStatusPrint = 0;
  if (millis() - lastStatusPrint > 30000) {
    Serial.println("=== Status Update ===");
    Serial.printf("Heart Rate: %d BPM\n", healthData.heartRate);
    Serial.printf("Camera Active: %s\n", cameraActive ? "Yes" : "No");
    Serial.printf("Auto-Stream: %s\n", autoStreamEnabled ? "Yes" : "No");
    Serial.printf("Connected Clients: %d\n", connectedClients);
    Serial.printf("Alert Status: %s\n", alerts.anyAlert ? "ALERT" : "Normal");
    Serial.printf("Free Heap: %d bytes\n", ESP.getFreeHeap());
    lastStatusPrint = millis();
  }

  delay(100);
}
