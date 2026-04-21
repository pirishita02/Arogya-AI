#!/usr/bin/env python3
"""
Database update script to add CalorieDetectionLog model
"""

import os
import sys
from flask import Flask
from flask_sqlalchemy import SQLAlchemy
from datetime import datetime

# Add the current directory to the path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from models import db, CalorieDetectionLog

def update_database():
    """Update the database with the new CalorieDetectionLog model"""
    app = Flask(__name__)
    app.config["SQLALCHEMY_DATABASE_URI"] = "sqlite:///calorieapp.db"
    app.config["SQLALCHEMY_TRACK_MODIFICATIONS"] = False
    
    db.init_app(app)
    
    with app.app_context():
        try:
            # Create the new table
            print("Creating CalorieDetectionLog table...")
            CalorieDetectionLog.__table__.create(db.engine, checkfirst=True)
            print("✅ CalorieDetectionLog table created successfully!")
            
            # Verify the table exists
            inspector = db.inspect(db.engine)
            tables = inspector.get_table_names()
            if 'calorie_detection_log' in tables:
                print("✅ Table verified in database!")
            else:
                print("❌ Table not found in database!")
                
        except Exception as e:
            print(f"❌ Error updating database: {e}")
            return False
    
    return True

if __name__ == "__main__":
    print("🔄 Updating database with CalorieDetectionLog model...")
    success = update_database()
    
    if success:
        print("✅ Database update completed successfully!")
        print("🎯 You can now use the enhanced food logging system!")
    else:
        print("❌ Database update failed!")
        sys.exit(1)
