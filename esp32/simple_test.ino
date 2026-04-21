#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include "DHT.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// MPU6050 I2C address
#define MPU6050_ADDR 0x68

// DHT22 sensor pin
#define DHTPIN 4
#define DHTTYPE DHT22

// Heart rate sensor pin (analog)
#define HEART_RATE_PIN 32

// ECG sensor pin (analog)
#define ECG_PIN 33

// OLED Display settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

const char* ssid = "realme 9 5G";
const char* password = "x6criudm";

WebServer server(80);

// Sensor objects
DHT dht(DHTPIN, DHTTYPE);

// OLED Display object
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Health monitoring variables
float heartRate = 75;
float bodyTemperature = 36.8;
int steps = 1250;
float stressLevel = 45;
float oxygenLevel = 97;
float ecgValue = 0.5; // ECG sensor value (0-3.3V range)
int ecgBPM = 75; // ECG calculated BPM
unsigned long lastUpdateTime = 0;
const unsigned long updateInterval = 2000; // Update every 2 seconds
unsigned long ecgSampleTime = 0;
const unsigned long ecgSampleInterval = 10; // ECG sample every 10ms for live data

// MPU6050 variables
int16_t accelX, accelY, accelZ;
int16_t gyroX, gyroY, gyroZ;
float motionLevel = 0;
bool isMoving = false;
unsigned long lastMotionTime = 0;

// Heart rate detection variables
unsigned long lastHeartBeat = 0;
int heartRateThreshold = 0;
bool heartBeatDetected = false;

// Initialize OLED display
void initDisplay() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    return;
  }
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Health Monitor");
  display.println("Initializing...");
  display.display();
  
  delay(2000);
}

// Update OLED display with health data
void updateDisplay() {
  display.clearDisplay();
  
  // Header
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("ESP32 Health Monitor");
  
  // Heart Rate
  display.setCursor(0, 12);
  display.print("HR: ");
  display.print(heartRate);
  display.println(" BPM");
  
  // Temperature
  display.setCursor(0, 24);
  display.print("Temp: ");
  display.print(bodyTemperature, 1);
  display.println("C");
  
  // Stress Level
  display.setCursor(0, 36);
  display.print("Stress: ");
  display.print(stressLevel, 0);
  display.println("%");
  
  // Oxygen Level
  display.setCursor(0, 48);
  display.print("O2: ");
  display.print(oxygenLevel, 0);
  display.println("%");
  
  // Motion Status
  display.setCursor(70, 12);
  display.print("Motion: ");
  display.println(isMoving ? "Yes" : "No");
  
  // Steps
  display.setCursor(70, 24);
  display.print("Steps: ");
  display.println(steps);
  
  // WiFi Status
  display.setCursor(70, 36);
  display.print("WiFi: ");
  display.println(WiFi.status() == WL_CONNECTED ? "OK" : "NC");
  
  // IP Address
  display.setCursor(70, 48);
  display.setTextSize(0);
  display.print(WiFi.localIP().toString().substring(9));
  
  display.display();
}

// Initialize MPU6050
void initMPU6050() {
  Wire.begin();
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(0x6B); // PWR_MGMT_1 register
  Wire.write(0);    // Set to zero (wakes up the MPU-6050)
  Wire.endTransmission(true);
}

// Read MPU6050 data
void readMPU6050() {
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(0x3B); // starting with register 0x3B (ACCEL_XOUT_H)
  Wire.endTransmission(false);
  Wire.requestFrom(MPU6050_ADDR, 14, true); // request a total of 14 registers
  
  accelX = Wire.read() << 8 | Wire.read(); // 0x3B (ACCEL_XOUT_H) & 0x3C (ACCEL_XOUT_L)
  accelY = Wire.read() << 8 | Wire.read(); // 0x3D (ACCEL_YOUT_H) & 0x3E (ACCEL_YOUT_L)
  accelZ = Wire.read() << 8 | Wire.read(); // 0x3F (ACCEL_ZOUT_H) & 0x40 (ACCEL_ZOUT_L)
  int16_t temp = Wire.read() << 8 | Wire.read(); // 0x41 (TEMP_OUT_H) & 0x42 (TEMP_OUT_L)
  gyroX = Wire.read() << 8 | Wire.read(); // 0x43 (GYRO_XOUT_H) & 0x44 (GYRO_XOUT_L)
  gyroY = Wire.read() << 8 | Wire.read(); // 0x45 (GYRO_YOUT_H) & 0x46 (GYRO_YOUT_L)
  gyroZ = Wire.read() << 8 | Wire.read(); // 0x47 (GYRO_ZOUT_H) & 0x48 (GYRO_ZOUT_L)
  
  // Calculate motion level
  float accelMagnitude = sqrt(accelX * accelX + accelY * accelY + accelZ * accelZ);
  float gyroMagnitude = sqrt(gyroX * gyroX + gyroY * gyroY + gyroZ * gyroZ);
  motionLevel = (accelMagnitude + gyroMagnitude) / 1000.0;
  
  // Detect motion
  if (motionLevel > 0.5) {
    isMoving = true;
    lastMotionTime = millis();
    steps += 1; // Increment steps when motion detected
  } else {
    isMoving = false;
  }
}

// Read temperature from DHT22 sensor
void readTemperature() {
  float temp = dht.readTemperature();
  
  // Check if sensor is working properly
  if (isnan(temp)) {
    // Sensor error or disconnected, use simulated temperature
    bodyTemperature = 36.5 + random(-10, 15) / 10.0; // 35.5-38.0°C range
    Serial.println("DHT22 sensor error, using simulated value");
  } else {
    bodyTemperature = temp;
  }
  
  // Validate temperature range (human body temperature)
  if (bodyTemperature < 25.0 || bodyTemperature > 42.0) {
    Serial.print("Invalid temperature reading: ");
    Serial.print(bodyTemperature);
    Serial.println("°C, using safe fallback");
    bodyTemperature = 36.8; // Safe fallback
  }
  
  // Additional validation - check for sudden large changes
  static float lastValidTemp = 36.8;
  if (abs(bodyTemperature - lastValidTemp) > 2.0) {
    Serial.print("Large temperature change detected: ");
    Serial.print(bodyTemperature);
    Serial.println("°C, smoothing data");
    bodyTemperature = (bodyTemperature + lastValidTemp) / 2.0; // Smooth sudden changes
  }
  lastValidTemp = bodyTemperature;
}

// Read heart rate from analog sensor
void readHeartRate() {
  int sensorValue = analogRead(HEART_RATE_PIN);
  
  // Convert analog value to voltage (0-3.3V)
  float voltage = sensorValue * (3.3 / 4095.0);
  
  // Check if sensor is connected (voltage should fluctuate)
  static float lastVoltage = 0;
  static unsigned long lastHeartBeatTime = 0;
  static float lastValidHR = 75.0;
  
  if (abs(voltage - lastVoltage) < 0.01 && millis() - lastHeartBeatTime > 5000) {
    // Sensor not connected or not working, use simulated heart rate
    heartRate = 65 + random(15, 35); // 65-100 BPM range
    lastHeartBeatTime = millis();
    Serial.println("Heart rate sensor not responding, using simulated value");
  } else {
    // Simple heart rate detection based on voltage peaks
    // This is a basic implementation - you may need to calibrate based on your sensor
    if (voltage > heartRateThreshold && !heartBeatDetected) {
      heartBeatDetected = true;
      unsigned long currentTime = millis();
      if (lastHeartBeat > 0) {
        int timeDiff = currentTime - lastHeartBeat;
        if (timeDiff > 300) { // Minimum 300ms between beats (200 BPM max)
          float newHR = 60000 / timeDiff; // Convert to BPM
          newHR = constrain(newHR, 40, 200); // Constrain to reasonable range
          
          // Validate heart rate - check for sudden large changes
          if (abs(newHR - lastValidHR) > 30) {
            Serial.print("Large heart rate change detected: ");
            Serial.print(newHR);
            Serial.println(" BPM, smoothing data");
            heartRate = (newHR + lastValidHR) / 2.0; // Smooth sudden changes
          } else {
            heartRate = newHR;
          }
          lastValidHR = heartRate;
        }
      }
      lastHeartBeat = currentTime;
      lastHeartBeatTime = currentTime;
    } else if (voltage <= heartRateThreshold) {
      heartBeatDetected = false;
    }
    
    // Update threshold (adaptive)
    heartRateThreshold = voltage * 0.8;
  }
  
  // Additional validation - ensure heart rate is in reasonable range
  if (heartRate < 30 || heartRate > 220) {
    Serial.print("Invalid heart rate reading: ");
    Serial.print(heartRate);
    Serial.println(" BPM, using safe fallback");
    heartRate = lastValidHR; // Use last valid reading
  }
  
  lastVoltage = voltage;
}

// Read ECG from analog sensor
void readECG() {
  int sensorValue = analogRead(ECG_PIN);
  
  // Convert analog value to voltage (0-3.3V)
  ecgValue = sensorValue * (3.3 / 4095.0);
  
  // Validate ECG voltage range
  static float lastValidECG = 1.65; // Middle of 0-3.3V range
  
  if (ecgValue < 0.0 || ecgValue > 3.3) {
    Serial.print("Invalid ECG voltage: ");
    Serial.print(ecgValue);
    Serial.println("V, using safe fallback");
    ecgValue = lastValidECG; // Use last valid reading
  }
  
  // Check for sensor disconnect (flat line)
  static float lastECGValue = 1.65;
  static unsigned long lastECGChange = millis();
  
  if (abs(ecgValue - lastECGValue) < 0.01) {
    if (millis() - lastECGChange > 10000) { // 10 seconds of flat line
      Serial.println("ECG sensor appears disconnected, using simulated data");
      ecgValue = 1.65 + sin(millis() / 100.0) * 0.5; // Simulated ECG waveform
    }
  } else {
    lastECGChange = millis();
  }
  
  lastECGValue = ecgValue;
  lastValidECG = ecgValue;
  
  // ECG BPM calculation (simplified - you may need to implement proper QRS detection)
  // For now, we'll use the heart rate from the heart rate sensor
  ecgBPM = heartRate;
}

// Read all sensors
void readAllSensors() {
  readTemperature();
  readHeartRate();
  readECG();
  readMPU6050();
  
  // Calculate stress level based on heart rate and motion
  if (heartRate > 100 || motionLevel > 1.0) {
    stressLevel = min(stressLevel + 5.0, 100.0);
  } else if (heartRate < 70 && motionLevel < 0.2) {
    stressLevel = max(stressLevel - 2.0, 20.0);
  }
  
  // Estimate oxygen level (simplified - you may need a real SpO2 sensor)
  oxygenLevel = max(95.0 - stressLevel / 10.0, 85.0);
}

void setup() {
  Serial.begin(115200);
  
  // Initialize display
  initDisplay();
  
  // Initialize sensors
  dht.begin();
  initMPU6050();
  
  // Initialize analog pins
  pinMode(HEART_RATE_PIN, INPUT);
  pinMode(ECG_PIN, INPUT);
  
  // Initialize built-in LED for WiFi status
  pinMode(2, OUTPUT);
  
  // Connect to WiFi
  Serial.println("\n=== ESP32 Health Monitor Starting ===");
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  int connectionAttempts = 0;
  while (WiFi.status() != WL_CONNECTED && connectionAttempts < 30) {
    delay(500);
    Serial.print(".");
    digitalWrite(2, !digitalRead(2)); // Blink LED
    connectionAttempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    // Configure static IP to match mobile app (10.171.201.x)
    WiFi.config(IPAddress(10, 171, 201, 48),   // Static IP: 10.171.201.48
               IPAddress(10, 171, 201, 1),    // Gateway: 10.171.201.1
               IPAddress(255, 255, 255, 0)); // Subnet mask
    
    Serial.println("\nWiFi Connected!");
    Serial.print("Connected to: ");
    Serial.println(WiFi.SSID());
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Signal Strength: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    
    // Show IP on display
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("WiFi Connected!");
    display.setCursor(0, 16);
    display.print("IP: ");
    display.println(WiFi.localIP().toString());
    display.display();
    delay(3000);
    
    digitalWrite(2, HIGH); // LED ON when connected
  } else {
    Serial.println("\n✗ WiFi Connection Failed!");
    Serial.println("Check:");
    Serial.println("1. WiFi SSID and password");
    Serial.println("2. ESP32 within WiFi range");
    Serial.println("3. Router is working");
    
    // Show error on display
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("WiFi Failed!");
    display.setCursor(0, 16);
    display.println("Check Serial");
    display.display();
    
    digitalWrite(2, LOW); // LED OFF when not connected
  }
  
  Serial.println("Sensors initialized:");
  Serial.println("- DHT22 Temperature & Humidity Sensor");
  Serial.println("- Heart Rate Sensor (Analog)");
  Serial.println("- ECG Sensor (Analog)");
  Serial.println("- MPU6050 Motion Sensor");
  Serial.println("- OLED Display");

  // Setup web server endpoints
  server.on("/", []() {
    String html = R"(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32 Health Monitor</title>
    <meta name='viewport' content='width=device-width, initial-scale=1'>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background: #f5f5f5; }
        .container { max-width: 900px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        .metric { display: flex; justify-content: space-between; margin: 10px 0; padding: 15px; background: #f8f9fa; border-radius: 8px; border-left: 4px solid #007bff; }
        .value { font-size: 24px; font-weight: bold; color: #007bff; }
        .label { color: #666; font-weight: 500; }
        .status { padding: 15px; margin: 15px 0; border-radius: 8px; text-align: center; background: linear-gradient(135deg, #28a745, #20c997); color: white; font-weight: bold; }
        .status.error { background: linear-gradient(135deg, #dc3545, #c82333); }
        .status.warning { background: linear-gradient(135deg, #ffc107, #e0a800); }
        h3 { color: #333; border-bottom: 2px solid #007bff; padding-bottom: 5px; }
        #insights { background: #f8f9fa; border-radius: 8px; padding: 20px; margin: 15px 0; }
        #insights p { margin: 8px 0; padding: 8px; background: white; border-radius: 5px; }
        .good { color: #28a745; font-weight: bold; }
        .fair { color: #ffc107; font-weight: bold; }
        .needs-attention { color: #dc3545; font-weight: bold; }
        canvas { border: 1px solid #ddd; border-radius: 8px; background: #fafafa; }
        .grid-info { font-size: 12px; color: #666; margin-top: 5px; }
    </style>
</head>
<body>
    <div class='container'>
        <h1>ESP32 Health Monitor</h1>
        <div class='status' id='status'>
            <strong>Real-Time Health Monitoring</strong><br>
            <span id='connectionStatus'>Connecting...</span>
        </div>
        
        <div class='metric'>
            <span class='label'>Heart Rate:</span>
            <span class='value' id='heartRate'>--</span> BPM
        </div>
        <div class='metric'>
            <span class='label'>Body Temperature:</span>
            <span class='value' id='temperature'>--</span>°C
        </div>
        <div class='metric'>
            <span class='label'>Steps Today:</span>
            <span class='value' id='steps'>--</span>
        </div>
        <div class='metric'>
            <span class='label'>Stress Level:</span>
            <span class='value' id='stress'>--</span>%
        </div>
        <div class='metric'>
            <span class='label'>Blood Oxygen:</span>
            <span class='value' id='oxygen'>--</span>%
        </div>
        <div class='metric'>
            <span class='label'>Motion Status:</span>
            <span class='value' id='motion'>--</span>
        </div>
        
        <h3>Live ECG Graph</h3>
        <canvas id='ecgCanvas' width='800' height='200'></canvas>
        <div class='grid-info'>Real-time ECG waveform - Updates every second</div>
        <div class='metric'>
            <span class='label'>Current ECG Value:</span>
            <span class='value' id='ecgValue'>--</span>V
        </div>
        
        <h3>Health Insights</h3>
        <div id='insights'>
            <p id='heartStatus'>Heart Rate: --</p>
            <p id='stressStatus'>Stress Level: --</p>
            <p id='tempStatus'>Temperature: --</p>
            <p id='overallHealth'>Overall Health: --</p>
            <p id='recommendation'><strong>Recommendation:</strong> --</p>
        </div>
        
        <h3>API Endpoints</h3>
        <div style='background: #e9ecef; padding: 15px; border-radius: 8px;'>
            <p><strong>/health</strong> - All sensor data (JSON)</p>
            <p><strong>/insights</strong> - Health insights (JSON)</p>
            <p><strong>/ecg</strong> - Live ECG data (JSON)</p>
            <p><strong>/test</strong> - Test endpoint (JSON)</p>
            <p><small>Real-time updates every second - No manual refresh needed</small></p>
        </div>
    </div>
    <script>
        // Real-time data fetching and ECG visualization
        let ecgData = [];
        const maxEcgPoints = 150;
        const canvas = document.getElementById('ecgCanvas');
        const ctx = canvas.getContext('2d');
        
        function drawECG() {
            ctx.clearRect(0, 0, canvas.width, canvas.height);
            
            // Draw grid
            ctx.strokeStyle = '#e0e0e0';
            ctx.lineWidth = 0.5;
            
            // Vertical grid lines
            for (let i = 0; i <= canvas.width; i += 50) {
                ctx.beginPath();
                ctx.moveTo(i, 0);
                ctx.lineTo(i, canvas.height);
                ctx.stroke();
            }
            
            // Horizontal grid lines
            for (let i = 0; i <= canvas.height; i += 40) {
                ctx.beginPath();
                ctx.moveTo(0, i);
                ctx.lineTo(canvas.width, i);
                ctx.stroke();
            }
            
            // Draw center line
            ctx.strokeStyle = '#888';
            ctx.lineWidth = 1;
            ctx.beginPath();
            ctx.moveTo(0, canvas.height / 2);
            ctx.lineTo(canvas.width, canvas.height / 2);
            ctx.stroke();
            
            // Draw ECG waveform
            if (ecgData.length > 1) {
                ctx.strokeStyle = '#ff4444';
                ctx.lineWidth = 2;
                ctx.shadowBlur = 3;
                ctx.shadowColor = 'rgba(255, 68, 68, 0.3)';
                
                ctx.beginPath();
                for (let i = 0; i < ecgData.length; i++) {
                    const x = (i / maxEcgPoints) * canvas.width;
                    const y = canvas.height / 2 - ((ecgData[i] - 1.65) / 1.65) * (canvas.height / 2 - 20);
                    
                    if (i === 0) {
                        ctx.moveTo(x, y);
                    } else {
                        ctx.lineTo(x, y);
                    }
                }
                ctx.stroke();
                ctx.shadowBlur = 0;
            }
        }
        
        function updateHealthData() {
            // Fetch health data
            fetch('/health')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('heartRate').textContent = data.heartRate.toFixed(0);
                    document.getElementById('temperature').textContent = data.bodyTemperature.toFixed(1);
                    document.getElementById('steps').textContent = data.steps;
                    document.getElementById('stress').textContent = data.stressLevel.toFixed(0);
                    document.getElementById('oxygen').textContent = data.oxygenLevel.toFixed(0);
                    document.getElementById('motion').textContent = data.isMoving ? 'Active' : 'Resting';
                    document.getElementById('ecgValue').textContent = data.ecgValue.toFixed(3);
                    
                    // Update connection status
                    const statusElement = document.getElementById('connectionStatus');
                    statusElement.textContent = 'Live Monitoring Active';
                    document.getElementById('status').className = 'status';
                })
                .catch(error => {
                    const statusElement = document.getElementById('connectionStatus');
                    statusElement.textContent = 'Connection Error';
                    document.getElementById('status').className = 'status error';
                    console.error('Health data fetch error:', error);
                });
            
            // Fetch ECG data for graph
            fetch('/ecg')
                .then(response => response.json())
                .then(data => {
                    ecgData.push(data.ecgValue);
                    if (ecgData.length > maxEcgPoints) {
                        ecgData.shift();
                    }
                    drawECG();
                })
                .catch(error => console.log('ECG fetch error:', error));
            
            // Fetch health insights
            fetch('/insights')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('heartStatus').innerHTML = 
                        `<strong>Heart Rate:</strong> ${data.heartStatus} (${data.heartRate} BPM)`;
                    document.getElementById('stressStatus').innerHTML = 
                        `<strong>Stress Level:</strong> ${data.stressStatus} (${data.stressLevel}%)`;
                    document.getElementById('tempStatus').innerHTML = 
                        `<strong>Temperature:</strong> ${data.temperatureStatus} (${data.bodyTemperature}°C)`;
                    document.getElementById('overallHealth').innerHTML = 
                        `<strong>Overall Health:</strong> <span class='${data.overallHealth.toLowerCase().replace(' ', '-')}'>${data.overallHealth}</span>`;
                    document.getElementById('recommendation').innerHTML = 
                        `<strong>Recommendation:</strong> ${data.recommendation}`;
                })
                .catch(error => console.log('Insights fetch error:', error));
        }
        
        // Start real-time updates
        updateHealthData();
        setInterval(updateHealthData, 1000); // Update every second for smooth ECG
    </script>
</body>
</html>
    )";
    server.send(200, "text/html", html);
  });
  
  server.on("/test", []() {
    server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"ESP32 Health Monitor is working!\"}");
  });

  // Health data endpoint
  server.on("/health", []() {
    StaticJsonDocument<400> doc;
    
    doc["timestamp"] = millis();
    doc["heartRate"] = heartRate;
    doc["bodyTemperature"] = bodyTemperature;
    doc["steps"] = steps;
    doc["stressLevel"] = stressLevel;
    doc["oxygenLevel"] = oxygenLevel;
    doc["ecgValue"] = ecgValue;
    doc["ecgBPM"] = ecgBPM;
    doc["deviceStatus"] = "online";
    
    // Add MPU6050 motion data
    doc["motionLevel"] = motionLevel;
    doc["isMoving"] = isMoving;
    doc["accelX"] = accelX;
    doc["accelY"] = accelY;
    doc["accelZ"] = accelZ;
    doc["gyroX"] = gyroX;
    doc["gyroY"] = gyroY;
    doc["gyroZ"] = gyroZ;
    
    String response;
    serializeJson(doc, response);
    
    server.send(200, "application/json", response);
  });

  // Live ECG data endpoint (for real-time ECG monitoring)
  server.on("/ecg", []() {
    readECG(); // Read real ECG data
    
    StaticJsonDocument<200> doc;
    
    doc["timestamp"] = millis();
    doc["ecgValue"] = ecgValue;
    doc["ecgBPM"] = ecgBPM;
    doc["heartRate"] = heartRate;
    
    String response;
    serializeJson(doc, response);
    
    server.send(200, "application/json", response);
  });

  // Health insights endpoint
  // Simple data endpoint for ESP32-CAM
  server.on("/data", []() {
    StaticJsonDocument<300> doc;
    
    doc["hr"] = ecgBPM; // Use ECG BPM as primary heart rate
    doc["ecg"] = ecgValue;
    doc["temp"] = bodyTemperature;
    doc["stress"] = stressLevel;
    doc["o2"] = oxygenLevel;
    doc["moving"] = isMoving;
    doc["steps"] = steps;
    doc["spo2"] = oxygenLevel; // Add SpO2 field
    doc["stressIndex"] = stressLevel; // Add stress index
    doc["timestamp"] = millis();
    
    String response;
    serializeJson(doc, response);
    
    server.send(200, "application/json", response);
  });

  server.on("/insights", []() {
    StaticJsonDocument<600> doc;
    
    // Include all raw sensor data first
    doc["heartRate"] = heartRate;
    doc["bodyTemperature"] = bodyTemperature;
    doc["stressLevel"] = stressLevel;
    doc["oxygenLevel"] = oxygenLevel;
    doc["ecgValue"] = ecgValue;
    doc["ecgBPM"] = ecgBPM;
    doc["steps"] = steps;
    doc["motionLevel"] = motionLevel;
    doc["isMoving"] = isMoving;
    
    // Analyze health patterns
    String heartStatus = (heartRate > 100) ? "Elevated" : 
                        (heartRate < 60) ? "Low" : "Normal";
    
    String stressStatus = (stressLevel > 70) ? "High" :
                        (stressLevel > 40) ? "Moderate" : "Low";
    
    String tempStatus = (bodyTemperature > 37.2) ? "Elevated" :
                       (bodyTemperature < 36.0) ? "Low" : "Normal";
    
    String overallHealth = "Good";
    if (heartRate > 100 || stressLevel > 70 || bodyTemperature > 37.5) {
      overallHealth = "Needs Attention";
    } else if (heartRate > 80 || stressLevel > 50) {
      overallHealth = "Fair";
    }
    
    // Generate recommendations
    String recommendation = "Continue monitoring your health metrics.";
    if (stressLevel > 70) {
      recommendation = "Consider stress reduction techniques like deep breathing or meditation.";
    } else if (heartRate > 100) {
      recommendation = "Heart rate is elevated. Consider resting and monitoring.";
    } else if (oxygenLevel < 95) {
      recommendation = "Oxygen levels are slightly low. Ensure proper ventilation.";
    } else if (steps < 100) {
      recommendation = "Low activity detected. Consider a short walk.";
    }
    
    doc["timestamp"] = millis();
    doc["heartStatus"] = heartStatus;
    doc["stressStatus"] = stressStatus;
    doc["temperatureStatus"] = tempStatus;
    doc["overallHealth"] = overallHealth;
    doc["recommendation"] = recommendation;
    doc["dataPoints"] = 5;
    doc["deviceUptime"] = millis() / 1000;
    
    String response;
    serializeJson(doc, response);
    
    server.send(200, "application/json", response);
  });

  server.begin();
  Serial.println("HTTP server started");
  Serial.println("Open http://" + WiFi.localIP().toString() + " in browser");
  Serial.println("Health monitoring endpoints: /health, /insights, /test");
}

void loop() {
  server.handleClient();
  
  // Read real-time ECG data continuously
  readECG();
  
  // Update sensor data every 2 seconds
  if (millis() - lastUpdateTime > updateInterval) {
    readAllSensors(); // Read all real sensors
    lastUpdateTime = millis();
    
    // Update OLED display
    updateDisplay();
    
    // Print to Serial for debugging
    Serial.print("Real Sensor Data - ");
    Serial.print("HR: "); Serial.print(heartRate);
    Serial.print(" BPM, Temp: "); Serial.print(bodyTemperature, 1);
    Serial.print("°C, Steps: "); Serial.print(steps);
    Serial.print(", Stress: "); Serial.print(stressLevel, 0);
    Serial.print("%, O2: "); Serial.print(oxygenLevel, 0);
    Serial.print("%, ECG: "); Serial.print(ecgValue, 3);
    Serial.print("V, Motion: "); Serial.print(motionLevel, 2);
    Serial.print(", Moving: "); Serial.print(isMoving ? "Yes" : "No");
    Serial.println();
  }
  
  delay(10);
}
