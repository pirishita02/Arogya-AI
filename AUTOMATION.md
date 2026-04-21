# 🤖 CalAI Automation with n8n

This guide shows how to set up powerful automations for your CalAI app using n8n.

## 🚀 What You Can Automate

### 📊 **Data & Analytics Automations**
- Daily nutrition reports via email/SMS
- Weekly progress summaries
- Goal achievement notifications
- Streak maintenance reminders

### 🔄 **User Management Automations**
- Welcome email sequences
- Inactive user re-engagement
- Premium upgrade prompts
- Birthday messages & rewards

### 🍎 **Nutrition Automations**
- Meal logging reminders
- Personalized recipe recommendations
- Macro deficit/surplus alerts
- Water intake reminders

### 💪 **Fitness Automations**
- Workout reminders based on schedule
- Performance progress tracking
- Recovery day suggestions
- Achievement badges

## 🛠️ Setup Instructions

### 1. Install n8n
```bash
# Using Docker (recommended)
docker run -it --rm \
  --name n8n \
  -p 5678:5678 \
  -v n8n_data:/home/node/.n8n \
  n8nio/n8n

# Or using npm
npm install n8n -g
n8n start
```

### 2. Access n8n
- Open: `http://localhost:5678`
- Create your account
- Get your API key from Settings

### 3. Connect to CalAI Database
Create PostgreSQL credentials in n8n:
- Host: `localhost` (or your DB host)
- Port: `5432`
- Database: `calai_db`
- User: `postgres`
- Password: (your DB password)

## 🎯 Automation Workflows

### Workflow 1: Daily Nutrition Report
**Trigger**: Every day at 8 PM
**Actions**:
1. Query yesterday's meal data
2. Calculate totals and compare to goals
3. Generate personalized insights
4. Send email/SMS report

### Workflow 2: Streak Maintenance
**Trigger**: User hasn't logged meals for 2 days
**Actions**:
1. Check user's last meal log
2. Send friendly reminder
3. Include motivational message
4. Suggest easy meal options

### Workflow 3: Weekly Progress Summary
**Trigger**: Every Sunday at 6 PM
**Actions**:
1. Aggregate week's nutrition data
2. Compare to previous week
3. Generate insights
4. Send comprehensive report

### Workflow 4: Water Intake Reminders
**Trigger**: Every 2 hours during day
**Actions**:
1. Check today's water intake
2. Compare to hourly goal
3. Send reminder if behind
4. Include hydration tips

### Workflow 5: Workout Performance Tracking
**Trigger**: After workout completion
**Actions**:
1. Analyze workout performance
2. Compare to previous sessions
3. Generate progress insights
4. Suggest next workout intensity

## 📧 Integration Examples

### Email Integration
```javascript
// SendGrid example
const emailNode = {
  credentials: {
    apiKey: 'your-sendgrid-key'
  },
  sendEmail: {
    to: 'user@example.com',
    subject: 'Your Daily Nutrition Report',
    html: generateNutritionReport(userData)
  }
};
```

### SMS Integration (Twilio)
```javascript
const smsNode = {
  credentials: {
    accountSid: 'your-twilio-sid',
    authToken: 'your-twilio-token'
  },
  sendSMS: {
    to: '+1234567890',
    body: 'Great job logging your meals today! 🎉'
  }
};
```

### Slack Integration
```javascript
const slackNode = {
  credentials: {
    webhookUrl: 'your-slack-webhook'
  },
  sendMessage: {
    channel: '#nutrition-updates',
    text: `User ${userName} completed their workout! 💪`
  }
};
```

## 🗄️ Database Queries for n8n

### Get User's Daily Nutrition
```sql
SELECT 
  u.email,
  u.name,
  COALESCE(SUM(mf.calories * mf.quantity), 0) as total_calories,
  COALESCE(SUM(mf.protein * mf.quantity), 0) as total_protein,
  COALESCE(SUM(mf.carbs * mf.quantity), 0) as total_carbs,
  COALESCE(SUM(mf.fat * mf.quantity), 0) as total_fat,
  up.daily_calorie_target,
  up.daily_protein_target,
  up.daily_carbs_target,
  up.daily_fat_target
FROM users u
LEFT JOIN user_profiles up ON u.id = up.user_id
LEFT JOIN meal_logs ml ON u.id = ml.user_id AND DATE(ml.logged_at) = CURRENT_DATE - INTERVAL '1 day'
LEFT JOIN meal_food_items mfi ON ml.id = mfi.meal_log_id
LEFT JOIN master_foods mf ON mfi.food_id = mf.id
WHERE u.id = {{ $json.user_id }}
GROUP BY u.id, u.email, u.name, up.daily_calorie_target, up.daily_protein_target, up.daily_carbs_target, up.daily_fat_target;
```

### Get User's Water Intake
```sql
SELECT 
  u.email,
  u.name,
  COALESCE(SUM(wl.amount), 0) as water_intake,
  2000 as water_goal
FROM users u
LEFT JOIN water_logs wl ON u.id = wl.user_id AND DATE(wl.created_at) = CURRENT_DATE
WHERE u.id = {{ $json.user_id }}
GROUP BY u.id, u.email, u.name;
```

### Get User's Workout Sessions
```sql
SELECT 
  u.email,
  u.name,
  COUNT(ws.id) as total_sessions,
  SUM(ws.duration_minutes) as total_minutes,
  AVG(ws.exercises_completed) as avg_exercises
FROM users u
LEFT JOIN workout_sessions ws ON u.id = ws.user_id AND DATE(ws.completed_at) >= CURRENT_DATE - INTERVAL '7 days'
WHERE u.id = {{ $json.user_id }}
GROUP BY u.id, u.email, u.name;
```

## 🔧 n8n Node Setup

### PostgreSQL Node Configuration
```json
{
  "operation": "executeQuery",
  "database": "calai_db",
  "query": "SELECT * FROM users WHERE last_login > NOW() - INTERVAL '24 hours'"
}
```

### HTTP Request Node (for API calls)
```json
{
  "method": "POST",
  "url": "https://calai-app.loca.lt/api/analytics/weekly",
  "headers": {
    "Content-Type": "application/json",
    "Authorization": "Bearer {{ $json.token }}"
  },
  "body": {
    "user_id": "{{ $json.user_id }}"
  }
}
```

### Function Node (Custom JavaScript)
```javascript
// Calculate nutrition insights
const items = $input.all();
const userData = items[0].json;

const insights = [];
if (userData.total_calories < userData.daily_calorie_target * 0.8) {
  insights.push("You're under your calorie goal. Consider adding healthy snacks!");
}
if (userData.total_protein < userData.daily_protein_target * 0.8) {
  insights.push("Increase your protein intake with lean meats or plant-based proteins.");
}

return [{
  json: {
    ...userData,
    insights: insights,
    report_date: new Date().toISOString()
  }
}];
```

## 📱 Mobile App Integration

### Add n8n Webhook Support
```javascript
// Add to your API routes
app.post('/api/webhooks/nutrition-report', async (req, res) => {
  const { user_id, report_data } = req.body;
  
  // Store report in database or send push notification
  await saveNutritionReport(user_id, report_data);
  
  res.json({ success: true });
});
```

### Push Notification Integration
```javascript
// Using Expo Push Notifications
const sendPushNotification = async (userToken, message) => {
  const response = await fetch('https://exp.host/--/api/v2/push/send', {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
    },
    body: JSON.stringify({
      to: userToken,
      sound: 'default',
      body: message,
      data: { type: 'nutrition_report' }
    })
  });
};
```

## 🎯 Advanced Automations

### Smart Meal Recommendations
```javascript
// Based on user's nutrition gaps
const recommendMeals = (macroGaps) => {
  const recommendations = [];
  
  if (macroGaps.protein > 0) {
    recommendations.push({
      type: 'high_protein',
      meals: ['Grilled Chicken Salad', 'Protein Smoothie', 'Greek Yogurt Parfait']
    });
  }
  
  if (macroGaps.carbs > 0) {
    recommendations.push({
      type: 'complex_carbs',
      meals: ['Quinoa Bowl', 'Sweet Potato', 'Brown Rice']
    });
  }
  
  return recommendations;
};
```

### Workout Adaptation
```javascript
// Adjust workout intensity based on performance
const adaptWorkout = (performanceData) => {
  if (performanceData.completion_rate > 0.9) {
    return { intensity: 'increase', suggestions: ['Add 5% more weight', 'Increase reps by 2'] };
  } else if (performanceData.completion_rate < 0.7) {
    return { intensity: 'decrease', suggestions: ['Reduce weight by 10%', 'Focus on form'] };
  }
  return { intensity: 'maintain', suggestions: ['Keep current intensity'] };
};
```

## 📊 Monitoring & Analytics

### Track Automation Performance
```javascript
// Log automation metrics
const logAutomationMetrics = async (workflowId, userId, success, duration) => {
  await query(`
    INSERT INTO automation_logs 
    (workflow_id, user_id, success, duration_ms, created_at)
    VALUES ($1, $2, $3, $4, NOW())
  `, [workflowId, userId, success, duration]);
};
```

### Create Automation Dashboard
```sql
-- Automation performance table
CREATE TABLE IF NOT EXISTS automation_logs (
  id SERIAL PRIMARY KEY,
  workflow_id VARCHAR(100),
  user_id INTEGER REFERENCES users(id),
  success BOOLEAN,
  duration_ms INTEGER,
  error_message TEXT,
  created_at TIMESTAMP DEFAULT NOW()
);
```

## 🚀 Getting Started

1. **Install n8n** using Docker or npm
2. **Set up database credentials** in n8n
3. **Import the workflow templates** below
4. **Configure your integrations** (email, SMS, etc.)
5. **Test with sample data**
6. **Activate your workflows**

## 📋 Workflow Templates

Import these JSON templates into n8n to get started quickly:

- `daily-nutrition-report.json`
- `streak-maintenance.json`
- `weekly-progress-summary.json`
- `water-intake-reminders.json`
- `workout-performance.json`

## 🎉 Benefits

- **24/7 Automation**: Never miss a reminder or report
- **Personalization**: Tailored messages based on user data
- **Scalability**: Handle thousands of users automatically
- **Integration**: Connect with 200+ services via n8n
- **Analytics**: Track automation performance and user engagement

This setup will make your CalAI app feel like a premium service with intelligent, timely automations! 🚀
