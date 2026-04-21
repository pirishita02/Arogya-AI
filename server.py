import os
import math
import json
import tempfile
import numpy as np
import uuid
from datetime import datetime, timedelta
from flask import Flask, render_template, request, redirect, url_for, flash, Response
from flask_login import LoginManager, login_user, logout_user, login_required, current_user
from flask_migrate import Migrate
from dotenv import load_dotenv
from openai import OpenAI
from sklearn.metrics.pairwise import cosine_similarity
import chromadb
from chromadb.config import Settings

from calorie_counter import get_calories_from_image
from models import db, User, Profile, MealLog, KnowledgeBase, DocumentEmbedding, RAGChatHistory, DailyTargets, DailyProgress, FoodItem, Nutrients, CalorieDetectionLog

load_dotenv()
openai_client = OpenAI()

app = Flask(__name__)
app.config["SECRET_KEY"] = os.getenv("SECRET_KEY", "change-this-secret")
app.config["SQLALCHEMY_DATABASE_URI"] = os.getenv("DATABASE_URL", "sqlite:///calorieapp.db")
app.config["SQLALCHEMY_TRACK_MODIFICATIONS"] = False

db.init_app(app)
migrate = Migrate(app, db)

login_manager = LoginManager(app)
login_manager.login_view = "login"

@login_manager.user_loader
def load_user(user_id):
    return User.query.get(int(user_id))

def calculate_bmr_tdee(profile: Profile):
    if not profile or not all([
        profile.weight_kg,
        profile.height_cm,
        profile.age,
        profile.sex,
        profile.activity_level,
    ]):
        return {"bmr": None, "tdee": None}

    w = profile.weight_kg
    h = profile.height_cm
    a = profile.age
    sex = profile.sex.lower()

    if sex == "male":
        bmr = 10 * w + 6.25 * h - 5 * a + 5
    else:
        bmr = 10 * w + 6.25 * h - 5 * a - 161

    activity_map = {
        "sedentary": 1.2,
        "light": 1.375,
        "moderate": 1.55,
        "active": 1.725,
        "athlete": 1.9,
    }
    tdee = bmr * activity_map.get(profile.activity_level, 1.2)
    return {"bmr": round(bmr), "tdee": round(tdee)}

# ===== YOLO realtime (lazy-loaded) =====
_yolo_model = None
def get_yolo_model():
    global _yolo_model
    if _yolo_model is None:
        try:
            from ultralytics import YOLO
            # Prefer a lightweight default; ultralytics will auto-download on first use
            _yolo_model = YOLO(os.getenv("YOLO_MODEL", "yolov8n.pt"))
        except Exception as e:
            _yolo_model = e  # stash the error to surface later
    return _yolo_model

@app.route("/")
def index():
    if current_user.is_authenticated:
        return redirect(url_for("dashboard"))
    return redirect(url_for("login"))

@app.route("/start", methods=["GET", "POST"])
def start():
    if request.method == "POST":
        # Create user
        name = request.form.get("name", "").strip()
        email = request.form.get("email", "").strip().lower()
        password = request.form.get("password", "")
        height_cm = request.form.get("height_cm")
        weight_kg = request.form.get("weight_kg")
        age = request.form.get("age")
        sex = request.form.get("sex")
        activity = request.form.get("activity_level")

        if not email or not password:
            flash("Email and password are required", "error")
            return redirect(url_for("start"))

        existing = User.query.filter_by(email=email).first()
        if existing:
            flash("Email already registered", "error")
            return redirect(url_for("start"))

        user = User(email=email)
        user.set_password(password)
        db.session.add(user)
        db.session.commit()

        prof = Profile(
            user_id=user.id,
            name=name if name else None,
            height_cm=float(height_cm) if height_cm else None,
            weight_kg=float(weight_kg) if weight_kg else None,
            age=int(age) if age else None,
            sex=sex,
            activity_level=activity,
            unit_system="metric",
        )
        db.session.add(prof)
        db.session.commit()
        login_user(user)
        return redirect(url_for("dashboard"))

    return render_template("start.html")

@app.route("/calories")
def calories_page():
    return render_template("index.html")

@app.route("/upload", methods=["POST"])
@login_required
def upload():
    image = request.files.get("image")
    if not image or image.filename == "":
        return {"error": "No image uploaded"}, 400

    temp_file = tempfile.NamedTemporaryFile()
    image.save(temp_file.name)
    calories = get_calories_from_image(temp_file.name)
    temp_file.close()

    if current_user.is_authenticated:
        try:
            # Extract nutrition data
            total_cals = 0
            protein = 0
            fat = 0
            carbs = 0
            food_items = []
            
            if isinstance(calories, dict):
                total_cals = float(calories.get("total", 0))
                protein = float(calories.get("protein", 0))
                fat = float(calories.get("fat", 0))
                carbs = float(calories.get("carbs", 0))
                food_items = calories.get("food_items", [])
            
            # Save to CalorieDetectionLog
            detection = CalorieDetectionLog(
                user_id=current_user.id,
                food_items=json.dumps(food_items),
                total_calories=total_cals,
                total_protein_g=protein,
                total_fat_g=fat,
                total_carbs_g=carbs,
                confidence_score=0.8  # Default confidence
            )
            db.session.add(detection)
            
            # Also create a meal log entry for backward compatibility
            meal_log = MealLog(
                user_id=current_user.id, 
                total_calories=total_cals,
                photo_url=""
            )
            db.session.add(meal_log)
            db.session.commit()
            
        except Exception as e:
            print(f"Error logging detection: {e}")
            # Still return the calories even if logging fails

    return {"calories": calories}

@app.route("/exercise", methods=["GET", "POST"])
@login_required
def exercise():
    answer = None
    if request.method == "POST":
        prompt = request.form.get("prompt", "")
        if prompt:
            try:
                resp = openai_client.chat.completions.create(
                    model="gpt-4o-mini",
                    messages=[
                        {"role": "system", "content": "You are a fitness coach. Provide safe, concise guidance."},
                        {"role": "user", "content": prompt},
                    ],
                )
                answer = resp.choices[0].message.content
            except Exception as e:
                answer = f"Error: {e}"
    return render_template("exercise.html", answer=answer)

@app.route("/chat", methods=["POST"]) 
def chat_api():
    data = request.get_json(silent=True) or {}
    msg = data.get("message", "")
    if not msg:
        return {"error": "message is required"}, 400
    try:
        resp = openai_client.chat.completions.create(
            model="gpt-4o-mini",
            messages=[
                {"role": "system", "content": "You are a helpful health and nutrition assistant."},
                {"role": "user", "content": msg},
            ],
        )
        content = resp.choices[0].message.content
        return {"reply": content}
    except Exception as e:
        return {"error": str(e)}, 500

@app.route("/gym")
@login_required
def gym():
    return render_template("gym.html")

@app.route("/food")
@login_required
def food():
    return render_template("food.html")

@app.route("/realtime")
@login_required
def realtime_page():
    return render_template("realtime.html")

@app.route("/realtime_feed")
@login_required
def realtime_feed():
    # Stream MJPEG with detections
    try:
        import cv2  # type: ignore
        import numpy as np  # noqa: F401
    except Exception:
        return Response("OpenCV (cv2) not installed. Please install opencv-python in your virtualenv.", status=500)

    model = get_yolo_model()
    if isinstance(model, Exception):
        return Response(f"Model load error: {model}", status=500)

    # Try multiple camera indices for robustness
    cap = None
    for idx in (0, 1, 2):
        c = cv2.VideoCapture(idx)
        if c.isOpened():
            cap = c
            break
        c.release()
    if cap is None:
        return Response("Webcam not available", status=500)

    def gen():
        try:
            while True:
                ok, frame = cap.read()
                if not ok:
                    break
                # Run detection with tunable params
                results = model.predict(source=frame, conf=float(os.getenv("YOLO_CONF", 0.25)), imgsz=int(os.getenv("YOLO_IMGSZ", 640)), verbose=False)
                # Draw boxes using ultralytics built-in plot()
                if results and len(results) > 0:
                    plotted = results[0].plot()
                else:
                    plotted = frame
                ret, jpeg = cv2.imencode('.jpg', plotted)
                if not ret:
                    continue
                data = jpeg.tobytes()
                yield (b'--frame\r\n'
                       b'Content-Type: image/jpeg\r\n\r\n' + data + b'\r\n')
        finally:
            cap.release()

    return Response(gen(), mimetype='multipart/x-mixed-replace; boundary=frame')

@app.route("/register", methods=["GET", "POST"])
def register():
    if request.method == "POST":
        email = request.form.get("email", "").strip().lower()
        password = request.form.get("password", "")
        if not email or not password:
            flash("Email and password are required", "error")
            return redirect(url_for("register"))
        existing = User.query.filter_by(email=email).first()
        if existing:
            flash("Email already registered", "error")
            return redirect(url_for("register"))
        user = User(email=email)
        user.set_password(password)
        db.session.add(user)
        db.session.commit()
        login_user(user)
        return redirect(url_for("profile"))
    return render_template("register.html")

@app.route("/login", methods=["GET", "POST"])
def login():
    if request.method == "POST":
        email = request.form.get("email", "").strip().lower()
        password = request.form.get("password", "")
        user = User.query.filter_by(email=email).first()
        if not user or not user.check_password(password):
            flash("Invalid credentials", "error")
            return redirect(url_for("login"))
        login_user(user)
        return redirect(url_for("dashboard"))
    return render_template("login.html")

@app.route("/logout")
@login_required
def logout():
    logout_user()
    return redirect(url_for("index"))

@app.route("/profile", methods=["GET", "POST"])
@login_required
def profile():
    profile = current_user.profile or Profile(user_id=current_user.id)
    if request.method == "POST":
        unit = request.form.get("unit_system", "metric")
        height = request.form.get("height_cm")
        weight = request.form.get("weight_kg")
        age = request.form.get("age")
        sex = request.form.get("sex")
        activity = request.form.get("activity_level")

        profile.unit_system = unit
        profile.height_cm = float(height) if height else None
        profile.weight_kg = float(weight) if weight else None
        profile.age = int(age) if age else None
        profile.sex = sex
        profile.activity_level = activity

        if not current_user.profile:
            db.session.add(profile)
        db.session.commit()
        flash("Profile saved", "success")
        return redirect(url_for("dashboard"))

    metrics = calculate_bmr_tdee(profile) if current_user.profile else {"bmr": None, "tdee": None}
    return render_template("profile.html", profile=profile, metrics=metrics)

@app.route("/dashboard")
@login_required
def dashboard():
    logs = MealLog.query.filter_by(user_id=current_user.id).order_by(MealLog.timestamp.desc()).limit(20).all()
    prof = current_user.profile
    metrics = calculate_bmr_tdee(prof) if prof else {"bmr": None, "tdee": None}
    today_total = sum(l.total_calories or 0 for l in logs if l.timestamp.date() == datetime.utcnow().date())
    
    # Get daily progress data
    today = datetime.now().date()
    targets = DailyTargets.query.filter_by(user_id=current_user.id, date=today).first()
    if not targets:
        targets = DailyTargets(
            user_id=current_user.id,
            date=today,
            target_calories=2000,
            target_protein_g=50,
            target_fat_g=65,
            target_carbs_g=250
        )
        db.session.add(targets)
        db.session.commit()

    # Get today's meals
    meals = MealLog.query.filter_by(user_id=current_user.id).filter(
        db.func.date(MealLog.timestamp) == today
    ).all()

    # Calculate consumed nutrition
    consumed_calories = sum(l.total_calories or 0 for l in meals)
    consumed_protein_g = 0  # Would need to join with nutrients for real data
    consumed_carbs_g = 0
    consumed_fat_g = 0
    
    # Get weekly data
    weekly_data = []
    for i in range(7):
        date = today - timedelta(days=i)
        day_meals = MealLog.query.filter_by(user_id=current_user.id).filter(
            db.func.date(MealLog.timestamp) == date
        ).all()
        day_calories = sum(l.total_calories or 0 for l in day_meals)
        weekly_data.append({
            'date': date.strftime('%Y-%m-%d'),
            'calories': day_calories,
            'meals_logged': len(day_meals)
        })
    
    return render_template("dashboard.html", 
                         logs=logs, 
                         metrics=metrics, 
                         today_total=round(today_total),
                         daily_progress={
                             'consumed_calories': consumed_calories,
                             'target_calories': targets.target_calories,
                             'consumed_protein_g': consumed_protein_g,
                             'target_protein_g': targets.target_protein_g,
                             'consumed_carbs_g': consumed_carbs_g,
                             'target_carbs_g': targets.target_carbs_g,
                             'consumed_fat_g': consumed_fat_g,
                             'target_fat_g': targets.target_fat_g
                         },
                         weekly_data=weekly_data)

def calculate_body_fat_percentage(sex, height_cm, waist_cm, neck_cm):
    """Calculate body fat percentage using US Navy formula"""
    try:
        if sex.lower() == 'male':
            # US Navy body fat formula for males
            body_fat = 86.010 * math.log10(waist_cm - neck_cm) - 70.041 * math.log10(height_cm) + 36.76
        else:
            # US Navy body fat formula for females (hip measurement needed, using approximation)
            body_fat = 163.34 * math.log10(waist_cm + neck_cm) - 97.68 * math.log10(height_cm) - 78.36
        
        return max(0, min(50, body_fat))  # Clamp between 0-50%
    except:
        return None

def calculate_macro_split(target_calories, goal, exercise_level):
    """Calculate macronutrient split based on goals and activity level"""
    
    # Base macro ratios
    if goal == 'lose_weight':
        protein_ratio = 0.35  # Higher protein for satiety and muscle preservation
        carbs_ratio = 0.30
        fats_ratio = 0.35
    elif goal == 'gain_muscle':
        protein_ratio = 0.30  # High protein for muscle building
        carbs_ratio = 0.45    # High carbs for energy
        fats_ratio = 0.25
    elif goal == 'gain_weight':
        protein_ratio = 0.25
        carbs_ratio = 0.45
        fats_ratio = 0.30
    else:  # maintain_weight
        protein_ratio = 0.25
        carbs_ratio = 0.40
        fats_ratio = 0.35
    
    # Adjust based on exercise level
    if exercise_level in ['advanced', 'athlete']:
        carbs_ratio += 0.05
        fats_ratio -= 0.05
    elif exercise_level == 'beginner':
        protein_ratio += 0.05
        carbs_ratio -= 0.05
    
    # Calculate grams and calories
    protein_calories = target_calories * protein_ratio
    carbs_calories = target_calories * carbs_ratio
    fats_calories = target_calories * fats_ratio
    
    return {
        'protein_g': round(protein_calories / 4, 1),
        'carbs_g': round(carbs_calories / 4, 1),
        'fats_g': round(fats_calories / 9, 1),
        'protein_calories': round(protein_calories),
        'carbs_calories': round(carbs_calories),
        'fats_calories': round(fats_calories),
        'protein_percentage': round(protein_ratio * 100),
        'carbs_percentage': round(carbs_ratio * 100),
        'fats_percentage': round(fats_ratio * 100)
    }

def calculate_target_calories(tdee, goal, exercise_level):
    """Calculate target calories based on goal"""
    if goal == 'lose_weight':
        # 500 calorie deficit for ~1lb/week loss
        return tdee - 500
    elif goal == 'gain_weight':
        # 300-500 calorie surplus for weight gain
        return tdee + 400
    elif goal == 'gain_muscle':
        # Moderate surplus with focus on protein
        return tdee + 300
    else:  # maintain_weight
        return tdee

# ===== ADVANCED RAG SYSTEM WITH CHROMADB =====

# Initialize ChromaDB
chroma_client = chromadb.PersistentClient(path="./chroma_db")
collection = chroma_client.get_or_create_collection(
    name="nutrition_knowledge",
    metadata={"description": "Nutrition and fitness knowledge base"}
)

def generate_embedding(text):
    """Generate embedding for text using OpenAI"""
    try:
        response = openai_client.embeddings.create(
            model="text-embedding-ada-002",
            input=text
        )
        return response.data[0].embedding
    except Exception as e:
        print(f"Error generating embedding: {e}")
        return None

def add_document_to_chroma(doc_id, title, content, category, tags):
    """Add document to ChromaDB vector store"""
    try:
        # Generate embedding
        embedding = generate_embedding(content)
        if not embedding:
            return False
        
        # Add to ChromaDB
        collection.add(
            ids=[str(doc_id)],
            embeddings=[embedding],
            metadatas=[{
                "title": title,
                "category": category,
                "tags": tags,
                "source": "Internal Knowledge Base"
            }],
            documents=[content]
        )
        return True
    except Exception as e:
        print(f"Error adding document to ChromaDB: {e}")
        return False

def advanced_retrieve_relevant_context(query, category=None, limit=5, user_profile=None):
    """Advanced document retrieval with filtering and re-ranking"""
    try:
        # Generate query embedding
        query_embedding = generate_embedding(query)
        if not query_embedding:
            return []
        
        # Build where clause for category filtering
        where_clause = {}
        if category:
            where_clause["category"] = category
        
        # Query ChromaDB
        results = collection.query(
            query_embeddings=[query_embedding],
            n_results=limit * 2,  # Get more for re-ranking
            where=where_clause if where_clause else None,
            include=["metadatas", "documents", "distances"]
        )
        
        if not results or not results["ids"][0]:
            return []
        
        # Re-rank results based on user profile relevance
        documents = []
        for i, doc_id in enumerate(results["ids"][0]):
            metadata = results["metadatas"][0][i]
            document = results["documents"][0][i]
            distance = results["distances"][0][i]
            
            # Calculate relevance score based on user profile
            relevance_score = calculate_relevance_score(query, document, metadata, user_profile)
            
            documents.append({
                "id": doc_id,
                "title": metadata["title"],
                "content": document,
                "category": metadata["category"],
                "tags": metadata["tags"],
                "similarity_score": 1 - distance,  # Convert distance to similarity
                "relevance_score": relevance_score,
                "combined_score": (1 - distance) * 0.7 + relevance_score * 0.3
            })
        
        # Sort by combined score and return top results
        documents.sort(key=lambda x: x["combined_score"], reverse=True)
        return documents[:limit]
        
    except Exception as e:
        print(f"Error in advanced retrieval: {e}")
        return []

def calculate_relevance_score(query, document, metadata, user_profile):
    """Calculate relevance score based on user profile and query terms"""
    score = 0.5  # Base score
    
    if user_profile:
        # Boost score based on user goals and characteristics
        if user_profile.get("goal"):
            if user_profile["goal"].lower() in document.lower():
                score += 0.2
        
        if user_profile.get("exercise_level"):
            if user_profile["exercise_level"].lower() in document.lower():
                score += 0.15
        
        if user_profile.get("sex"):
            gender_terms = ["male", "female", "men", "women"]
            for term in gender_terms:
                if term in document.lower() and term == user_profile["sex"].lower():
                    score += 0.1
                    break
    
    # Boost score for exact query matches
    query_words = query.lower().split()
    for word in query_words:
        if word in document.lower():
            score += 0.05
    
    return min(score, 1.0)  # Cap at 1.0

def expand_query(query):
    """Expand query with related terms for better retrieval"""
    expansions = {
        "weight loss": ["fat loss", "calorie deficit", "slimming", "cutting"],
        "muscle gain": ["bulking", "mass building", "hypertrophy", "strength training"],
        "protein": ["amino acids", "muscle protein synthesis", "whey", "casein"],
        "exercise": ["workout", "training", "fitness", "physical activity"],
        "diet": ["nutrition", "meal plan", "eating plan", "food intake"]
    }
    
    expanded_query = query.lower()
    for key, terms in expansions.items():
        if key in expanded_query:
            expanded_query += " " + " ".join(terms)
    
    return expanded_query

def chunk_document(content, chunk_size=500, overlap=100):
    """Chunk large documents for better retrieval"""
    if len(content) <= chunk_size:
        return [content]
    
    chunks = []
    start = 0
    
    while start < len(content):
        end = start + chunk_size
        if end > len(content):
            end = len(content)
        
        chunk = content[start:end]
        chunks.append(chunk)
        
        start = end - overlap
        if start >= len(content):
            break
    
    return chunks

def advanced_rag_augmented_prompt(query, retrieved_docs, user_context=None):
    """Create advanced RAG-augmented prompt with citations"""
    context = "\n\n".join([
        f"[Source {i+1}] {doc['title']} (Category: {doc['category']}, Relevance: {doc['combined_score']:.2f})\n{doc['content']}"
        for i, doc in enumerate(retrieved_docs)
    ])
    
    user_context_str = ""
    if user_context:
        user_context_str = f"\n\nUser Profile Context:\n{user_context}"
    
    augmented_prompt = f"""🧠 ADVANCED RAG SYSTEM ACTIVATED 🧠

You are an elite nutritionist and fitness trainer with access to a specialized knowledge base. The following documents have been retrieved and ranked by relevance to provide evidence-based guidance.

📚 RETRIEVED KNOWLEDGE BASE:
{context}

{user_context_str}

❓ USER QUERY: {query}

🎯 INSTRUCTIONS:
1. Use the retrieved documents as your primary source of information
2. Cite sources using [Source X] references
3. If documents don't fully address the query, supplement with your expertise
4. Provide specific, actionable advice based on the user's profile
5. Highlight when information comes from the knowledge base vs. your general knowledge
6. Include confidence levels for recommendations

✨ RESPONSE FORMAT:
- Start with a confidence indicator [High/Medium/Low]
- Use citations [Source X] when referencing retrieved documents
- Provide specific, numbered recommendations
- Include practical implementation steps

Answer:"""
    
    return augmented_prompt

def initialize_knowledge_base():
    """Initialize the advanced knowledge base with comprehensive nutrition and fitness information"""
    knowledge_items = [
        {
            "title": "Understanding Macronutrients",
            "content": "Macronutrients are nutrients that provide calories or energy. The three main types are: 1) Proteins - 4 calories per gram, essential for muscle repair and growth. Found in meat, fish, eggs, dairy, legumes. 2) Carbohydrates - 4 calories per gram, primary energy source. Found in grains, fruits, vegetables. 3) Fats - 9 calories per gram, energy storage and hormone production. Found in oils, nuts, seeds, fatty fish.",
            "category": "nutrition",
            "tags": "macronutrients,protein,carbs,fats,basics"
        },
        {
            "title": "Advanced Calorie Deficit Strategies",
            "content": "Strategic calorie deficit for sustainable weight loss: 500-750 calories daily deficit for 1-1.5 lbs/week loss. Use diet breaks every 8-12 weeks at maintenance calories. Incorporate refeed days with higher carbs (100-200g above baseline) every 2-3 weeks during extended deficits. Monitor metabolic rate and adjust deficits as weight loss progresses.",
            "category": "nutrition",
            "tags": "weight loss,calories,deficit,metabolic adaptation"
        },
        {
            "title": "Protein Timing and Distribution",
            "content": "Optimal protein distribution: 20-30g per meal across 3-4 meals daily. Pre-workout protein 20-30g 1-2 hours before training. Post-workout protein 25-40g within 30 minutes for muscle protein synthesis. Casein protein before bed for overnight recovery. Total daily protein: 1.6-2.2g/kg for strength athletes, 1.2-1.6g/kg for endurance athletes.",
            "category": "nutrition",
            "tags": "protein,timing,muscle synthesis,recovery"
        },
        {
            "title": "Body Composition Beyond BMI",
            "content": "Advanced body composition assessment: BMI limitations for athletes with high muscle mass. Waist-to-height ratio <0.5 for optimal health. Body fat percentage ranges: Essential fat 10-13% (men), 13-17% (women); Athletic 6-13% (men), 14-20% (women); Fitness 14-17% (men), 21-24% (women). Use DEXA, BodPod, or calipers for accurate measurements.",
            "category": "health",
            "tags": "BMI,body composition,waist ratio,body fat"
        },
        {
            "title": "Progressive Overload Training Principles",
            "content": "Progressive overload fundamentals: 1) Increase weight gradually (2.5-5lbs for upper body, 5-10lbs for lower body). 2) Increase repetitions within target range (8-12 for hypertrophy). 3) Increase training volume (sets x reps x weight). 4) Decrease rest periods. 5) Increase training frequency. Track workouts and apply 2-for-2 rule: if you can complete 2 extra reps on 2 consecutive workouts, increase weight.",
            "category": "exercise",
            "tags": "progressive overload,strength training,hypertrophy"
        },
        {
            "title": "Advanced Hydration Strategies",
            "content": "Precision hydration: Base intake 35ml/kg body weight daily. Add 500-750ml per hour of exercise. For workouts >90 minutes, add electrolytes (300-600mg sodium, 75-150mg potassium per liter). Pre-hydrate with 5-7ml/kg 2-4 hours before exercise. Monitor urine specific gravity (1.005-1.015 optimal). Consider sweat rate testing for individualized hydration plans.",
            "category": "health",
            "tags": "hydration,electrolytes,sweat rate,performance"
        },
        {
            "title": "Nutrient Timing for Performance",
            "content": "Strategic nutrient timing: Pre-workout meal 2-3 hours before: 1-2g carbs/kg, 0.2-0.3g protein/kg. Intra-workout carbs (60-90g/hour) for sessions >90 minutes. Post-workout anabolic window: 1-1.2g carbs/kg, 0.3-0.4g protein/kg within 30 minutes. Bedtime casein (20-40g) for overnight muscle protein synthesis.",
            "category": "nutrition",
            "tags": "nutrient timing,performance,carbs,protein"
        },
        {
            "title": "Complete Protein Food Database",
            "content": "Comprehensive protein sources: Animal proteins (complete amino acid profile) - whey isolate (90% protein, fast absorption), casein (80% protein, slow release), eggs (13g/100g), chicken breast (31g/100g), salmon (25g/100g). Plant proteins - soy isolate (90% protein, complete), quinoa (8g/100g complete), hemp seeds (25g/100g complete), rice + bean combinations. Digestibility scores: whey 99%, egg 97%, milk 95%, soy 91%, beef 90%.",
            "category": "nutrition",
            "tags": "protein,amino acids,food sources,complete proteins"
        },
        {
            "title": "Metabolic Adaptation Management",
            "content": "Combating metabolic adaptation during dieting: Implement diet breaks every 8-12 weeks at maintenance calories for 1-2 weeks. Use refeed days with 100-200g increased carbs every 2-3 weeks. Gradually increase calories (100-200 weekly) when reaching plateau. Monitor resting metabolic rate and adjust targets accordingly. Include high-carb days to replenish glycogen and boost leptin.",
            "category": "nutrition",
            "tags": "metabolic adaptation,diet breaks,refeed,leptin"
        },
        {
            "title": "Recovery and Sleep Optimization",
            "content": "Recovery optimization: 7-9 hours sleep quality for muscle recovery and hormone regulation. Sleep hygiene: cool room (65-68°F), complete darkness, no screens 1 hour before bed. Protein before sleep: 20-40g casein or slow-digesting protein. Magnesium (400mg) and zinc (30mg) before bed for recovery. Morning sunlight exposure for circadian rhythm regulation.",
            "category": "health",
            "tags": "recovery,sleep,magnesium,zinc,circadian"
        },
        {
            "title": "Supplement Evidence Guide",
            "content": "Evidence-based supplements: Creatine monohydrate (5g daily) - strength and power gains, extensive research support. Beta-alanine (3.2-6.4g daily) - muscular endurance, buffering capacity. Caffeine (3-6mg/kg) pre-workout - performance enhancement. Omega-3 fatty acids (2-4g EPA/DHA) - inflammation reduction. Vitamin D (2000-4000 IU) - hormone optimization, immune function.",
            "category": "health",
            "tags": "supplements,creatine,beta-alanine,caffeine,omega-3"
        },
        {
            "title": "Female-Specific Nutrition Strategies",
            "content": "Women's nutrition considerations: Iron needs higher (18mg pre-menopause, 8mg post-menopause). Calcium needs increase with age (1000-1200mg daily). Folate requirements (400-600mcg) during reproductive years. Consider menstrual cycle impacts on training: follicular phase better for high intensity, luteal phase better for moderate intensity. Adjust calories based on cycle phase.",
            "category": "nutrition",
            "tags": "female nutrition,iron,calcium,menstrual cycle"
        }
    ]
    
    # Clear existing collection for fresh start
    try:
        collection.delete()
        print("Cleared existing ChromaDB collection")
    except:
        pass
    
    # Add documents to ChromaDB
    for i, item in enumerate(knowledge_items):
        doc_id = f"doc_{i+1}"
        success = add_document_to_chroma(
            doc_id,
            item['title'],
            item['content'],
            item['category'],
            item['tags']
        )
        if success:
            print(f"Added to ChromaDB: {item['title']}")
        else:
            print(f"Failed to add: {item['title']}")
    
    print("Advanced knowledge base initialized successfully")

@app.route("/body_analysis")
@login_required
def body_analysis():
    profile = Profile.query.filter_by(user_id=current_user.id).first()
    return render_template("body_analysis.html", profile=profile)

@app.route("/api/body_analysis", methods=["POST"])
@login_required
def analyze_body():
    data = request.get_json()
    try:
        # Extract and validate form data
        age = int(data.get('age'))
        sex = data.get('sex')
        height_cm = float(data.get('height_cm'))
        weight_kg = float(data.get('weight_kg'))
        waist_cm = float(data.get('waist_cm'))
        neck_cm = float(data.get('neck_cm'))
        goal = data.get('goal')
        exercise_level = data.get('exercise_level')
        work_activity_level = data.get('work_activity_level')
        activity_level = data.get('activity_level')
        
        # Calculate BMI
        height_m = height_cm / 100
        bmi = weight_kg / (height_m ** 2)
        
        # Calculate BMR and TDEE
        if sex.lower() == 'male':
            bmr = 10 * weight_kg + 6.25 * height_cm - 5 * age + 5
        else:
            bmr = 10 * weight_kg + 6.25 * height_cm - 5 * age - 161
        
        activity_multipliers = {
            'sedentary': 1.2,
            'light': 1.375,
            'moderate': 1.55,
            'active': 1.725,
            'athlete': 1.9
        }
        tdee = bmr * activity_multipliers.get(activity_level.lower(), 1.2)
        
        # Calculate target calories
        target_calories = calculate_target_calories(tdee, goal, exercise_level)
        
        # Calculate body fat percentage
        body_fat_percentage = calculate_body_fat_percentage(sex, height_cm, waist_cm, neck_cm)
        
        # Calculate macro split
        macros = calculate_macro_split(target_calories, goal, exercise_level)
        
        # Generate AI-powered analysis with ADVANCED RAG
        user_context = {
            "age": age,
            "sex": sex,
            "height_cm": height_cm,
            "weight_kg": weight_kg,
            "bmi": bmi,
            "body_fat_percentage": body_fat_percentage,
            "exercise_level": exercise_level,
            "work_activity_level": work_activity_level,
            "goal": goal,
            "target_calories": round(target_calories),
            "macros": macros
        }
        
        # Expand query for better retrieval
        expanded_query = expand_query(
            f"personalized nutrition plan for {goal} with {exercise_level} exercise level"
        )
        
        # Retrieve relevant documents using advanced RAG
        retrieved_docs = advanced_retrieve_relevant_context(
            expanded_query, 
            category="nutrition", 
            limit=3,
            user_profile=user_context
        )
        
        # Add exercise-related documents
        exercise_query = expand_query(
            f"exercise recommendations for {exercise_level} level"
        )
        exercise_docs = advanced_retrieve_relevant_context(
            exercise_query, 
            category="exercise", 
            limit=2,
            user_profile=user_context
        )
        retrieved_docs.extend(exercise_docs)
        
        # Add health-related documents
        health_query = expand_query(
            f"health tips for {goal}"
        )
        health_docs = advanced_retrieve_relevant_context(
            health_query, 
            category="health", 
            limit=2,
            user_profile=user_context
        )
        retrieved_docs.extend(health_docs)
        
        # Create advanced RAG-augmented prompt
        ai_prompt = advanced_rag_augmented_prompt(
            "Create a comprehensive, personalized nutrition and fitness plan based on detailed user metrics and goals. Include executive summary, nutrition strategy, exercise recommendations, weekly schedule, progress tracking, and advanced tips.",
            retrieved_docs,
            f"User Profile: Age {age}y, Gender {sex}, Height {height_cm}cm, Weight {weight_kg}kg, BMI {bmi:.1f}, Body Fat {body_fat_percentage:.1f}%, Exercise Level {exercise_level}, Goal {goal}, Target Calories {round(target_calories)}, Macros: Protein {macros['protein_g']}g, Carbs {macros['carbs_g']}g, Fats {macros['fats_g']}g"
        )
        
        response = openai_client.chat.completions.create(
            model="gpt-4",
            messages=[
                {
                    "role": "system",
                    "content": "You are an elite nutritionist and fitness trainer with expertise in metabolic health, body composition, and performance optimization. Provide evidence-based, personalized recommendations that are safe and effective."
                },
                {
                    "role": "user",
                    "content": ai_prompt
                }
            ],
            max_tokens=1500,
            temperature=0.7
        )
        
        # Update user profile with new measurements
        profile = Profile.query.filter_by(user_id=current_user.id).first()
        if profile:
            profile.age = age
            profile.sex = sex
            profile.height_cm = height_cm
            profile.weight_kg = weight_kg
            profile.waist_cm = waist_cm
            profile.neck_cm = neck_cm
            profile.activity_level = activity_level
            profile.exercise_level = exercise_level
            profile.work_activity_level = work_activity_level
            db.session.commit()
        
        # Save daily targets based on body analysis
        today = datetime.utcnow().date()
        existing_target = DailyTargets.query.filter_by(
            user_id=current_user.id, 
            date=today
        ).first()
        
        if not existing_target:
            daily_target = DailyTargets(
                user_id=current_user.id,
                date=today,
                target_calories=round(target_calories),
                target_protein_g=macros['protein_g'],
                target_fat_g=macros['fats_g'],
                target_carbs_g=macros['carbs_g']
            )
            db.session.add(daily_target)
            db.session.commit()
        
        return {
            "analysis": response.choices[0].message.content,
            "bmi": bmi,
            "tdee": tdee,
            "target_calories": target_calories,
            "body_fat_percentage": body_fat_percentage,
            "macros": macros,
            "rag_info": {
                "system_active": True,
                "retrieved_documents": len(retrieved_docs),
                "sources": [
                    {
                        "title": doc["title"],
                        "category": doc["category"],
                        "relevance_score": round(doc["combined_score"], 3),
                        "similarity_score": round(doc["similarity_score"], 3)
                    }
                    for doc in retrieved_docs
                ]
            }
        }
        
    except Exception as e:
        print(f"Error in body analysis: {str(e)}")
        return {"error": f"Analysis failed: {str(e)}"}

# ===== RAG ENDPOINTS =====

@app.route("/rag_chat")
@login_required
def rag_chat():
    return render_template("rag_chat.html")

@app.route("/api/rag_chat", methods=["POST"])
@login_required
def rag_chat_api():
    data = request.get_json()
    message = data.get('message', '')
    session_id = data.get('session_id', 'default')
    
    if not message:
        return {"error": "Message is required"}, 400
    
    try:
        # Get user profile for context
        profile = Profile.query.filter_by(user_id=current_user.id).first()
        user_context = {}
        if profile:
            user_context = {
                "age": profile.age,
                "sex": profile.sex,
                "weight_kg": profile.weight_kg,
                "height_cm": profile.height_cm,
                "goal": profile.exercise_level,
                "exercise_level": profile.exercise_level
            }
        
        # Store user message
        user_chat = RAGChatHistory(
            user_id=current_user.id,
            session_id=session_id,
            message_type='user',
            content=message
        )
        db.session.add(user_chat)
        
        # Expand query for better retrieval
        expanded_query = expand_query(message)
        
        # Retrieve relevant documents using advanced RAG
        retrieved_docs = advanced_retrieve_relevant_context(
            expanded_query, 
            limit=5,
            user_profile=user_context
        )
        
        # Create advanced RAG-augmented prompt
        ai_prompt = advanced_rag_augmented_prompt(
            message, 
            retrieved_docs, 
            f"User Profile: {user_context}"
        )
        
        # Get AI response
        response = openai_client.chat.completions.create(
            model="gpt-4o-mini",
            messages=[
                {
                    "role": "system",
                    "content": "🧠 ADVANCED RAG SYSTEM ACTIVATED 🧠 You are a professional nutritionist and fitness trainer with access to a specialized knowledge base. Use retrieved documents as primary sources and cite them appropriately. Always prioritize user safety and recommend consulting healthcare professionals for medical concerns."
                },
                {
                    "role": "user",
                    "content": ai_prompt
                }
            ],
            max_tokens=1000,
            temperature=0.7
        )
        
        ai_response = response.choices[0].message.content
        
        # Store AI response with retrieved documents
        assistant_chat = RAGChatHistory(
            user_id=current_user.id,
            session_id=session_id,
            message_type='assistant',
            content=ai_response,
            retrieved_documents=json.dumps([doc["id"] for doc in retrieved_docs])
        )
        db.session.add(assistant_chat)
        db.session.commit()
        
        return {
            "response": ai_response,
            "rag_info": {
                "system_active": True,
                "retrieved_documents": len(retrieved_docs),
                "sources": [
                    {
                        "title": doc["title"],
                        "category": doc["category"],
                        "relevance_score": round(doc["combined_score"], 3),
                        "similarity_score": round(doc["similarity_score"], 3),
                        "content_preview": doc["content"][:200] + "..." if len(doc["content"]) > 200 else doc["content"]
                    }
                    for doc in retrieved_docs
                ]
            }
        }
        
    except Exception as e:
        print(f"Error in RAG chat: {str(e)}")
        return {"error": f"Chat failed: {str(e)}"}

@app.route("/api/rag_history/<session_id>")
@login_required
def rag_chat_history(session_id):
    try:
        messages = RAGChatHistory.query.filter_by(
            user_id=current_user.id,
            session_id=session_id
        ).order_by(RAGChatHistory.timestamp.asc()).all()
        
        return {
            "messages": [
                {
                    "type": msg.message_type,
                    "content": msg.content,
                    "timestamp": msg.timestamp.isoformat()
                }
                for msg in messages
            ]
        }
    except Exception as e:
        return {"error": str(e)}

# ===== FOOD LOGGING ENDPOINTS =====

@app.route("/food_logging")
@login_required
def food_logging():
    return render_template("food_logging.html")

@app.route("/api/search_food", methods=["POST"])
@login_required
def search_food():
    """Search for food items in database"""
    try:
        data = request.get_json()
        query = data.get('query', '')
        
        if not query:
            return {"error": "Search query is required"}, 400
        
        # Search food items
        foods = FoodItem.query.filter(
            FoodItem.name.ilike(f"%{query}%")
        ).limit(20).all()
        
        # Get nutrients for each food
        results = []
        for food in foods:
            nutrients = Nutrients.query.filter_by(food_id=food.id).first()
            if nutrients:
                results.append({
                    "id": food.id,
                    "name": food.name,
                    "brand": food.brand,
                    "serving_qty": food.serving_qty,
                    "serving_unit": food.serving_unit,
                    "calories": nutrients.calories,
                    "protein_g": nutrients.protein_g,
                    "fat_g": nutrients.fat_g,
                    "carbs_g": nutrients.carbs_g,
                    "fiber_g": nutrients.fiber_g
                })
        
        return {"foods": results}
        
    except Exception as e:
        return {"error": str(e)}

@app.route("/api/log_food", methods=["POST"])
@login_required
def log_food():
    """Log a food item for the user"""
    try:
        data = request.get_json()
        food_id = data.get('food_id')
        quantity = data.get('quantity', 1.0)
        meal_type = data.get('meal_type', 'snack')  # breakfast, lunch, dinner, snack
        
        if not food_id:
            return {"error": "Food ID is required"}, 400
        
        # Get food and nutrients
        food = FoodItem.query.get(food_id)
        if not food:
            return {"error": "Food not found"}, 404
        
        nutrients = Nutrients.query.filter_by(food_id=food_id).first()
        if not nutrients:
            return {"error": "Nutrition data not found"}, 404
        
        # Calculate total nutrients based on quantity
        multiplier = quantity / food.serving_qty if food.serving_qty else quantity
        total_calories = nutrients.calories * multiplier
        total_protein = nutrients.protein_g * multiplier
        total_fat = nutrients.fat_g * multiplier
        total_carbs = nutrients.carbs_g * multiplier
        
        # Create meal log
        meal_log = MealLog(
            user_id=current_user.id,
            food_id=food_id,
            quantity=quantity,
            portion_text=f"{quantity} {food.serving_unit or 'serving'}",
            total_calories=total_calories
        )
        
        db.session.add(meal_log)
        db.session.commit()
        
        # Update daily progress
        today = datetime.utcnow().date()
        progress = DailyProgress.query.filter_by(
            user_id=current_user.id,
            date=today
        ).first()
        
        if not progress:
            progress = DailyProgress(
                user_id=current_user.id,
                date=today
            )
            db.session.add(progress)
        
        # Update consumed values
        progress.consumed_calories += total_calories
        progress.consumed_protein_g += total_protein
        progress.consumed_fat_g += total_fat
        progress.consumed_carbs_g += total_carbs
        progress.meals_logged += 1
        progress.last_updated = datetime.utcnow()
        
        db.session.commit()
        
        return {
            "success": True,
            "meal_id": meal_log.id,
            "total_calories": total_calories,
            "total_protein": total_protein,
            "total_fat": total_fat,
            "total_carbs": total_carbs
        }
        
    except Exception as e:
        db.session.rollback()
        return {"error": str(e)}

@app.route("/api/daily_progress")
@login_required
def daily_progress():
    today = datetime.now().date()

    # Get or create daily targets
    targets = DailyTargets.query.filter_by(user_id=current_user.id, date=today).first()
    if not targets:
        # Default targets if none set
        targets = DailyTargets(
            user_id=current_user.id,
            date=today,
            target_calories=2000,
            target_protein_g=50,
            target_fat_g=65,
            target_carbs_g=250
        )
        db.session.add(targets)
        db.session.commit()

    # Get today's meals
    meals = MealLog.query.filter_by(user_id=current_user.id).filter(
        db.func.date(MealLog.timestamp) == today
    ).all()

    # Calculate consumed nutrition
    consumed = {
        'calories': sum(meal.total_calories for meal in meals),
        'protein_g': sum(meal.protein_g for meal in meals),
        'fat_g': sum(meal.fat_g for meal in meals),
        'carbs_g': sum(meal.carbs_g for meal in meals)
    }

    # Calculate percentages
    percentages = {
        'calories': (consumed['calories'] / targets.calories * 100) if targets.calories > 0 else 0,
        'protein_g': (consumed['protein_g'] / targets.protein_g * 100) if targets.protein_g > 0 else 0,
        'fat_g': (consumed['fat_g'] / targets.fat_g * 100) if targets.fat_g > 0 else 0,
        'carbs_g': (consumed['carbs_g'] / targets.carbs_g * 100) if targets.carbs_g > 0 else 0
    }

    # Get detected calories from dashboard (from today's uploads)
    detected_calories = get_today_detected_calories()

    return jsonify({
        'targets': {
            'calories': targets.calories,
            'protein_g': targets.protein_g,
            'fat_g': targets.fat_g,
            'carbs_g': targets.carbs_g
        },
        'consumed': consumed,
        'percentages': percentages,
        'meals': [{
            'id': meal.id,
            'food_name': meal.food_name,
            'brand': meal.brand,
            'quantity': meal.quantity,
            'portion_text': meal.portion_text,
            'calories': meal.total_calories,
            'protein_g': meal.protein_g,
            'fat_g': meal.fat_g,
            'carbs_g': meal.carbs_g,
            'timestamp': meal.timestamp.isoformat()
        } for meal in meals],
        'detected_calories': detected_calories
    })

def get_today_detected_calories():
    """Get calories from today's dashboard detections"""
    try:
        today = datetime.now().date()
        
        # Get today's detections from database
        detections = CalorieDetectionLog.query.filter_by(
            user_id=current_user.id, 
            date=today
        ).all()
        
        total_detected = sum(detection.total_calories for detection in detections)
        return total_detected
        
    except Exception as e:
        print(f"Error getting detected calories: {e}")
        return 0

@app.route("/api/log_detected_foods_to_meals", methods=["POST"])
@login_required
def log_detected_foods_to_meals():
    """Log dashboard calorie detections to food logging meals"""
    try:
        today = datetime.now().date()
        
        # Get today's unlogged detections
        detections = CalorieDetectionLog.query.filter_by(
            user_id=current_user.id,
            date=today,
            logged_to_meals=False
        ).all()
        
        meals_logged = 0
        total_calories_logged = 0
        
        for detection in detections:
            try:
                # Parse food items from JSON
                food_items = json.loads(detection.food_items) if detection.food_items else []
                
                for food_item in food_items:
                    # Create a meal log entry for each detected food
                    meal = MealLog(
                        user_id=current_user.id,
                        food_name=food_item.get('name', 'Detected Food'),
                        brand=food_item.get('brand', 'AI Detection'),
                        quantity=food_item.get('quantity', 1),
                        portion_text=food_item.get('portion', '1 serving'),
                        total_calories=food_item.get('calories', 0),
                        protein_g=food_item.get('protein', 0),
                        fat_g=food_item.get('fat', 0),
                        carbs_g=food_item.get('carbs', 0),
                        photo_url="",
                        meal_type="snack"  # Default to snack for detected foods
                    )
                    db.session.add(meal)
                    meals_logged += 1
                    total_calories_logged += food_item.get('calories', 0)
                
                # Mark this detection as logged
                detection.logged_to_meals = True
                
            except Exception as e:
                print(f"Error logging detection {detection.id}: {e}")
                continue
        
        db.session.commit()
        
        return jsonify({
            'success': True,
            'meals_logged': meals_logged,
            'total_calories_logged': total_calories_logged,
            'message': f'Successfully logged {meals_logged} detected foods to meals'
        })
        
    except Exception as e:
        db.session.rollback()
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500
@app.route("/api/weekly_progress")
@login_required
def get_weekly_progress():
    """Get weekly nutrition progress history"""
    try:
        # Get last 7 days
        end_date = datetime.utcnow().date()
        start_date = end_date - timedelta(days=6)
        
        weekly_data = []
        
        for i in range(7):
            current_date = start_date + timedelta(days=i)
            
            # Get targets for this date
            targets = DailyTargets.query.filter_by(
                user_id=current_user.id,
                date=current_date
            ).first()
            
            # Get progress for this date
            progress = DailyProgress.query.filter_by(
                user_id=current_user.id,
                date=current_date
            ).first()
            
            weekly_data.append({
                "date": current_date.isoformat(),
                "day_name": current_date.strftime('%A'),
                "targets": {
                    "calories": targets.target_calories if targets else 2000,
                    "protein_g": targets.target_protein_g if targets else 50,
                    "fat_g": targets.target_fat_g if targets else 65,
                    "carbs_g": targets.target_carbs_g if targets else 250
                },
                "consumed": {
                    "calories": progress.consumed_calories if progress else 0,
                    "protein_g": progress.consumed_protein_g if progress else 0,
                    "fat_g": progress.consumed_fat_g if progress else 0,
                    "carbs_g": progress.consumed_carbs_g if progress else 0
                },
                "meals_logged": progress.meals_logged if progress else 0
            })
        
        return {"weekly_data": weekly_data}
        
    except Exception as e:
        return {"error": str(e)}

@app.route("/api/detect_food_from_image", methods=["POST"])
@login_required
def detect_food_from_image():
    """Detect food and calories from uploaded image"""
    try:
        if 'image' not in request.files:
            return {"error": "No image file provided"}, 400
        
        file = request.files['image']
        if file.filename == '':
            return {"error": "No image file selected"}, 400
        
        # Save uploaded file temporarily
        with tempfile.NamedTemporaryFile(delete=False, suffix='.jpg') as tmp_file:
            file.save(tmp_file.name)
            image_path = tmp_file.name
        
        try:
            # Use existing calorie detection
            detection_result = get_calories_from_image(image_path)
            
            if 'error' in detection_result:
                return {"error": "Failed to detect food: " + detection_result['error']}, 500
            
            # Format results for frontend
            result = {
                "success": True,
                "reasoning": detection_result.get('reasoning', ''),
                "food_items": detection_result.get('food_items', []),
                "nutrition": detection_result.get('nutrition', {}),
                "total_calories": detection_result.get('total', 0)
            }
            
            return result
            
        finally:
            # Clean up temporary file
            if os.path.exists(image_path):
                os.unlink(image_path)
        
    except Exception as e:
        return {"error": str(e)}

@app.route("/api/log_detected_food", methods=["POST"])
@login_required
def log_detected_food():
    """Log AI-detected food items to the database"""
    try:
        data = request.get_json()
        food_items = data.get('food_items', [])
        meal_type = data.get('meal_type', 'snack')
        
        if not food_items:
            return {"error": "No food items to log"}, 400
        
        logged_items = []
        total_nutrition = {
            'calories': 0,
            'protein': 0,
            'fat': 0,
            'carbs': 0
        }
        
        for item in food_items:
            # Create or find food item in database
            food = FoodItem.query.filter_by(name=item['name']).first()
            if not food:
                # Create new food item
                food = FoodItem(
                    source='ai_detected',
                    name=item['name'],
                    brand='AI Detected',
                    serving_qty=1,
                    serving_unit='serving'
                )
                db.session.add(food)
                db.session.flush()  # Get ID without committing
                
                # Create nutrients (estimated from calories)
                estimated_macros = estimate_macros_from_calories(item['calories'])
                nutrients = Nutrients(
                    food_id=food.id,
                    calories=item['calories'],
                    protein_g=estimated_macros['protein'],
                    fat_g=estimated_macros['fat'],
                    carbs_g=estimated_macros['carbs'],
                    fiber_g=estimated_macros['fiber']
                )
                db.session.add(nutrients)
            
            # Log the meal
            meal_log = MealLog(
                user_id=current_user.id,
                food_id=food.id,
                quantity=1,
                portion_text="1 serving (AI detected)",
                total_calories=item['calories']
            )
            
            db.session.add(meal_log)
            
            # Add to totals
            total_nutrition['calories'] += item['calories']
            if food.nutrients:
                total_nutrition['protein'] += food.nutrients.protein_g
                total_nutrition['fat'] += food.nutrients.fat_g
                total_nutrition['carbs'] += food.nutrients.carbs_g
            
            logged_items.append({
                'name': item['name'],
                'calories': item['calories'],
                'confidence': item.get('confidence', 0)
            })
        
        # Update daily progress
        today = datetime.utcnow().date()
        progress = DailyProgress.query.filter_by(
            user_id=current_user.id,
            date=today
        ).first()
        
        if not progress:
            progress = DailyProgress(
                user_id=current_user.id,
                date=today
            )
            db.session.add(progress)
        
        # Update consumed values
        progress.consumed_calories += total_nutrition['calories']
        progress.consumed_protein_g += total_nutrition['protein']
        progress.consumed_fat_g += total_nutrition['fat']
        progress.consumed_carbs_g += total_nutrition['carbs']
        progress.meals_logged += len(logged_items)
        progress.last_updated = datetime.utcnow()
        
        db.session.commit()
        
        return {
            "success": True,
            "logged_items": logged_items,
            "total_nutrition": total_nutrition,
            "meals_logged": len(logged_items)
        }
        
    except Exception as e:
        db.session.rollback()
        return {"error": str(e)}

def estimate_macros_from_calories(calories):
    """Estimate macronutrients from total calories using typical ratios"""
    # Typical ratio: 30% protein, 25% fat, 45% carbs
    protein_calories = calories * 0.30
    fat_calories = calories * 0.25
    carb_calories = calories * 0.45
    
    return {
        'protein': protein_calories / 4,  # 4 cal per gram
        'fat': fat_calories / 9,          # 9 cal per gram
        'carbs': carb_calories / 4,       # 4 cal per gram
        'fiber': max(2, calories * 0.01)  # Estimate fiber
    }

if __name__ == "__main__":
    with app.app_context():
        db.create_all()
        # Initialize knowledge base
        initialize_knowledge_base()
        
        # Lightweight schema guard for SQLite upgrades (adds 'name' to profile if missing)
        try:
            engine = db.engine
            if engine.url.drivername.startswith("sqlite"):
                insp = db.inspect(engine)
                cols = [c['name'] for c in insp.get_columns('profile')]
                if 'name' not in cols:
                    with engine.begin() as conn:
                        conn.exec_driver_sql("ALTER TABLE profile ADD COLUMN name VARCHAR(255)")
        except Exception:
            # Ignore to avoid blocking startup; errors will surface during requests
            pass
    
    app.run(debug=True, port=5002)
