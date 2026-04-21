
# 🚀 ArogyaAI (CalAI) – AI-Powered Health & Nutrition Platform

ArogyaAI is a comprehensive AI-driven health ecosystem that combines **computer vision, IoT, and intelligent automation** to deliver personalized nutrition, fitness, and real-time health insights.

👉 Transforming health tracking into **proactive, data-driven decision-making**

---

## 🌟 Overview

Modern health applications are fragmented and require manual input. ArogyaAI solves this by creating a **fully automated and intelligent platform** that integrates:

- 📷 Food Intelligence  
- 🧠 AI-driven insights  
- 🏋️ Fitness tracking  
- 🧬 Genetic + demographic personalization  
- ⌚ Real-time biometric monitoring  

## # Features Overview

### # Core Nutrition & Health Tracking
- **# AI Meal Analysis** - Snap photos of meals for instant calorie/macro analysis using GPT-4o Vision
- **# Daily Dashboard** - Real-time calorie and macro tracking with progress visualization
- **# Weekly & Monthly Analytics** - Comprehensive health insights with charts and trends
- **# Goal Setting** - Personalized weight goals with automatic macro calculations
- **# Streak Tracking** - Daily logging motivation with achievement badges

### # Advanced Features Added

#### # ESP32 Biometric Sensor Integration
- **# Real-time Health Monitoring** - Heart rate, ECG, temperature, oxygen saturation
- **# Wireless Data Streaming** - ESP32 sensor connects via WiFi to mobile app
- **# Live Health Dashboard** - Real-time biometric data visualization
- **# Health Insights** - AI-powered analysis of biometric patterns
- **# Sensor Discovery** - Automatic ESP32 device detection on local network

#### # AI Doctor Consultation System
- **# Mobile AI Doctor Screen** - React Native interface with voice interaction
- **# Web AI Doctor Interface** - Professional web-based consultation platform
- **# Voice Interaction** - Speech-to-text and text-to-speech capabilities
- **# OpenAI Integration** - GPT-4o, Whisper, and TTS API integration
- **# Medical System Prompt** - Professional AI physician persona (Dr. Arjun Mehta/Dr. CalAI)
- **# Emergency Detection** - Automatic detection of emergency medical keywords
- **# Conversation History** - Persistent medical consultation records

#### # Automation & Notifications
- **# n8n Workflow Integration** - Automated email and notification system
- **# Personalized Email Campaigns** - Goal-specific meal reminders and encouragement
- **# Health Data Automation** - Daily nutrition reports and insights
- **# Multi-channel Notifications** - Email, SMS, and Slack integrations
- **# User Engagement Tracking** - Email logs and campaign analytics

---

## # Project Structure

```
calai-app/
# Backend Services
backend/
# Node.js + Express + PostgreSQL
src/
# server.js - Main Express server
routes/
# auth.js, meals.js, analytics.js, users.js, doctor.js
services/
# foodVision.js (OpenAI GPT-4o), DeviceDiscovery.js
middleware/
# auth.js, errorHandler.js
models/
# db.js, migrate.js
uploads/
# Food photos and sensor data
# n8n-workflows/
# user-email-automation.json, daily-nutrition-report-FIXED.json

# Mobile Application
mobile/
# React Native (Expo)
src/
screens/
# HomeScreen.js, CameraScreen.js, AnalyticsScreen.js
# AIDoctorScreen.js, ProfessionalAIDoctorScreen.js
# HealthInsightsScreen.js, ProfileScreen.js
navigation/
# RootNavigator, AuthNavigator, TabNavigator
context/
# AuthContext.js
services/
# api.js, HealthAPI.js, DeviceDiscovery.js
components/
# UI components and shared elements
utils/
# theme.js, storage.js

# ESP32 Sensor Code
esp32/
# sensor_firmware.ino - Biometric sensor implementation
# ESP32_SETUP_GUIDE.md - Complete setup instructions

# Automation Workflows
n8n-workflows/
# user-email-automation.json - Email automation workflow
# daily-nutrition-report-FIXED.json - Nutrition reporting
# email-logs-table.sql - Database schema for tracking
```

---

## # Quick Start Guide

### # Prerequisites
- Node.js 18+
- PostgreSQL 14+
- Expo CLI
- OpenAI API key (GPT-4o, Whisper, TTS access)
- ESP32 development board (for biometric features)
- n8n (for automation workflows)

### # 1. Backend Setup

```bash
cd backend
npm install
cp .env.example .env
```

**Environment Variables (.env):**
```env
DATABASE_URL=postgresql://postgres:@localhost:5432/calai_db
JWT_SECRET=your-super-secret-jwt-key
OPENAI_API_KEY=sk-your-openai-key-here
PORT=3001
NODE_ENV=development
```

**Database Setup:**
```bash
createdb calai_db
node src/models/migrate.js
```

**Start Backend:**
```bash
npm run dev
```

### # 2. Mobile App Setup

```bash
cd mobile
npm install
```

**Update API Configuration:**
```js
// src/services/api.js
export const API_BASE_URL = 'http://YOUR_LOCAL_IP:3001';
```

**Start Expo:**
```bash
npx expo start
```

### # 3. ESP32 Sensor Setup

1. **Hardware Setup:**
   - Connect MAX30102 (heart rate/spO2)
   - Connect AD8232 (ECG)
   - Connect DS18B20 (temperature)
   - Setup WiFi connection

2. **Upload Firmware:**
   ```bash
   # Open esp32/sensor_firmware.ino in Arduino IDE
   # Upload to ESP32 board
   ```

3. **Configure Sensor:**
   - Sensor IP: `10.171.201.48` (static IP)
   - WiFi credentials in firmware
   - Test sensor endpoints

### # 4. n8n Automation Setup

```bash
# Install n8n
npm install -g n8n

# Start n8n
npx n8n

# Access at http://localhost:5678
```

**Import Workflows:**
1. Import `user-email-automation.json`
2. Import `daily-nutrition-report-FIXED.json`
3. Set up credentials (PostgreSQL, SMTP, OpenAI)

---

## # API Reference

### # Authentication
| Method | Endpoint | Description |
|--------|----------|-------------|
| POST | `/api/auth/register` | User registration |
| POST | `/api/auth/login` | User login |
| GET | `/api/auth/me` | Get current user profile |
| PUT | `/api/auth/profile` | Update user profile and goals |

### # Meals & Nutrition
| Method | Endpoint | Description |
|--------|----------|-------------|
| POST | `/api/meals/analyze` | AI meal photo analysis |
| POST | `/api/meals` | Save meal to database |
| GET | `/api/meals/today` | Today's meals and summary |
| GET | `/api/meals/history` | Historical meal data |
| DELETE | `/api/meals/:id` | Delete specific meal |

### # Health & Biometrics
| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/health/data` | Get latest biometric data |
| GET | `/api/health/insights` | AI health insights |
| GET | `/api/health/ecg` | ECG data points |
| POST | `/api/health/test-connection` | Test ESP32 connectivity |

### # AI Doctor
| Method | Endpoint | Description |
|--------|----------|-------------|
| POST | `/api/doctor/chat` | AI doctor consultation |
| GET | `/api/doctor/history` | Consultation history |
| POST | `/api/doctor/transcribe` | Voice transcription |

### # Analytics
| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/analytics/weekly` | 7-day health analytics |
| GET | `/api/analytics/monthly` | 30-day health trends |
| GET | `/api/analytics/macros/today` | Today's macro vs targets |
| GET | `/api/analytics/streak` | Current logging streak |

---

## # Feature Deep Dive

### # ESP32 Biometric Monitoring

**Real-time Data Collection:**
- Heart Rate: 60-100 BPM (normal range)
- Blood Oxygen: 95-100% (healthy range)
- ECG: Real-time cardiac rhythm monitoring
- Body Temperature: 36.1-37.2°C (normal range)

**Mobile Integration:**
- Automatic device discovery on local network
- Real-time data streaming with 10-second intervals
- Visual data representation with charts and graphs
- Health trend analysis and insights

**Technical Implementation:**
- WiFi communication via HTTP requests
- JSON data format for sensor readings
- Error handling and reconnection logic
- Battery status monitoring

### # AI Doctor Consultation

**Mobile Experience:**
- Voice-first interaction with speech-to-text
- Professional medical assistant interface
- Emergency keyword detection
- Conversation history and context

**Web Experience:**
- Professional medical consultation UI
- OpenAI API integration (GPT-4o, Whisper, TTS)
- Real-time voice recording and playback
- Medical system prompt with detailed persona

**Medical Features:**
- Symptom analysis and recommendations
- Preventive care guidance
- Lifestyle medicine advice
- Emergency detection and alerts

### # Automation Workflows

**Email Campaigns:**
- Personalized meal reminders based on user goals
- Positive reinforcement for active users
- Weekly nutrition summaries
- Goal-specific content (weight loss, muscle gain, maintenance)

**Data Processing:**
- User activity monitoring
- Meal logging pattern analysis
- Health trend identification
- Automated report generation

**Multi-channel Integration:**
- SMTP email delivery
- SMS notifications (Twilio)
- Slack team notifications
- Custom webhook support

---

## # Technology Stack

### # Frontend (Mobile)
- **React Native** with Expo
- **React Navigation** v6
- **React Query** for state management
- **Expo Camera** for meal photos
- **Expo Speech** for AI doctor voice
- **Linear Gradients** for UI design

### # Backend
- **Node.js** + **Express**
- **PostgreSQL** for data storage
- **OpenAI API** (GPT-4o, Whisper, TTS)
- **JWT** for authentication
- **Multer** for file uploads
- **Axios** for HTTP requests

### # Hardware & IoT
- **ESP32** microcontroller
- **MAX30102** pulse oximeter
- **AD8232** ECG sensor
- **DS18B20** temperature sensor
- **WiFi** communication protocol

### # Automation
- **n8n** workflow automation
- **SMTP** for email delivery
- **Twilio** for SMS
- **Slack** API for team notifications

---

## # Configuration Details

### # ESP32 Sensor Configuration

**Static IP Setup:**
```cpp
// Static IP configuration
IPAddress local_IP(10, 171, 201, 48);
IPAddress gateway(10, 171, 201, 1);
IPAddress subnet(255, 255, 255, 0);
```

**Sensor Endpoints:**
- `GET /health` - All biometric data
- `GET /ecg` - ECG waveform data
- `GET /status` - Sensor status and battery

### # AI Doctor OpenAI Integration

**API Keys Required:**
```env
OPENAI_API_KEY=sk-your-key-here
WHISPER_MODEL=whisper-1
CHAT_MODEL=gpt-4o
TTS_MODEL=tts-1
TTS_VOICE=onyx
```

**System Prompt:**
```javascript
const DOCTOR_SYSTEM_PROMPT = `You are Dr. Arjun Mehta, a compassionate and experienced General Physician with 18 years of clinical practice...`;
```

### # n8n Workflow Credentials

**PostgreSQL:**
```json
{
  "host": "localhost",
  "port": 5432,
  "database": "calai_db",
  "user": "postgres",
  "password": ""
}
```

**SMTP (Gmail):**
```json
{
  "host": "smtp.gmail.com",
  "port": 587,
  "secure": false,
  "user": "your-email@gmail.com",
  "password": "your-app-password"
}
```

---

## # Troubleshooting

### # Common Issues

**ESP32 Connection Problems:**
- Ensure sensor and mobile are on same WiFi network
- Check static IP configuration (10.171.201.48)
- Verify sensor power and wiring connections
- Test with browser: `http://10.171.201.48/health`

**AI Doctor Voice Issues:**
- Check OpenAI API key permissions
- Verify microphone permissions on mobile
- Test with text input if voice fails
- Check network connectivity

**n8n Workflow Failures:**
- Verify PostgreSQL connection settings
- Check SMTP credentials and app passwords
- Test individual nodes separately
- Review workflow execution logs

**Mobile App Network Issues:**
- Update API_BASE_URL with correct local IP
- Ensure backend server is running
- Check firewall settings
- Verify same WiFi network connection

---

## # Production Deployment

### # Backend Deployment
**Recommended Platforms:**
- **Railway** - Easy PostgreSQL integration
- **Render** - Good free tier
- **Heroku** - Established platform

**Deployment Steps:**
1. Set all environment variables
2. Configure production database
3. Update API URLs in mobile app
4. Set up SSL certificates
5. Configure domain and DNS

### # Mobile App Deployment
**EAS Build Process:**
```bash
npm install -g eas-cli
eas build --platform ios
eas build --platform android
eas submit --platform ios
eas submit --platform android
```

**Store Preparation:**
- Update app icons and splash screens
- Configure app store listings
- Set up in-app purchases if needed
- Test on physical devices

### # Automation Deployment
**n8n Cloud:**
- Migrate workflows to n8n.cloud
- Set up production credentials
- Configure error monitoring
- Set up workflow scheduling

---

## # Contributing

### # Development Workflow
1. Fork the repository
2. Create feature branch
3. Test all components
4. Submit pull request
5. Code review and merge

### # Code Standards
- ESLint for JavaScript
- Prettier for formatting
- TypeScript for new features
- Comprehensive testing

---

## # License & Support

**License:** MIT License  
**Support:** Create GitHub issues for bugs and feature requests  
**Community:** Join our Discord for development discussions  

---

## # Future Roadmap

### # Upcoming Features
- **Apple Health Integration** - Sync with HealthKit
- **Google Fit Integration** - Android health data sync
- **Wearable Support** - Apple Watch, Galaxy Watch
- **Multi-language Support** - International expansion
- **Advanced AI Models** - GPT-5 integration when available
- **Social Features** - Community challenges and sharing

### # Platform Expansion
- **Web Dashboard** - Full web application
- **Desktop App** - Electron application
- **API Platform** - Third-party integrations
- **Enterprise Features** - Corporate wellness programs

---

# # Get Started Now!

1. **Clone the repository**
2. **Set up backend** (5 minutes)
3. **Configure mobile app** (2 minutes)
4. **Start tracking your health** (instant)

# # Built with # by the CalAI Team

 transforming health tracking with AI technology
