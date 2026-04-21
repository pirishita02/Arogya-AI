console.log("✅ JS loaded correctly!");
const imageInput = document.querySelector("#image-input");
const calorieCount = document.querySelector("#calorie-count");
const spinner = document.querySelector("#spinner");
const uploadArea = document.querySelector("#upload-area");
const itemsCol = document.querySelector("#items-col");
const nutritionCol = document.querySelector("#nutrition-col");

// When image is selected
imageInput.addEventListener("change", () => {
    const file = imageInput.files[0];
    if (!file) return;

    startLoading();

    const fd = new FormData();
    fd.append("image", file);

    fetch("/upload", {
        method: "POST",
        body: fd,
    })
        .then((response) => response.json())
        .then((data) => {
            stopLoading();
            if (data.error) {
                calorieCount.textContent = "❌ Error: " + data.error;
                return;
            }

            // Normalize response shape
            const payload = data.calories || data;
            const total = safeNumber(payload.total) ?? 0;
            const foods = Array.isArray(payload.food_items) ? payload.food_items : (Array.isArray(payload.items) ? payload.items : []);
            const nutrition = payload.nutrition || {};

            // Update total calories
            calorieCount.textContent = `${Math.round(total)} calories 🍽️`;

            // Populate detected items list
            if (itemsCol) {
                itemsCol.innerHTML = "";
                if (foods.length === 0) {
                    itemsCol.innerHTML = `<li class="muted">No items detected</li>`;
                } else {
                    foods.forEach((f) => {
                        const name = f.name || f.label || "Item";
                        const cals = f.calories != null ? Math.round(Number(f.calories)) : null;
                        const acc = f.confidence != null ? ` · ${Math.round(Number(f.confidence) * 100)}%` : (f.accuracy != null ? ` · ${Math.round(Number(f.accuracy))}%` : "");
                        const right = cals != null ? `${cals} kcal` : "";
                        const li = document.createElement("li");
                        li.innerHTML = `<span>${escapeHtml(name)}${acc}</span><strong>${right}</strong>`;
                        itemsCol.appendChild(li);
                    });
                }
            }

            // Populate nutrition facts
            if (nutritionCol) {
                nutritionCol.innerHTML = "";
                const facts = buildNutritionFacts(nutrition, total);
                facts.forEach(([label, value]) => {
                    const li = document.createElement("li");
                    li.innerHTML = `<span>${label}</span><strong>${value}</strong>`;
                    nutritionCol.appendChild(li);
                });
            }
        })
        .catch(() => {
            stopLoading();
            calorieCount.textContent = "⚠️ Upload failed, try again!";
        });
});

function startLoading() {
    spinner.style.display = "block";
    calorieCount.style.display = "none";
    uploadArea.style.opacity = "0.5";
}

function stopLoading() {
    spinner.style.display = "none";
    calorieCount.style.display = "block";
    uploadArea.style.opacity = "1";
}

function safeNumber(v){
    const n = Number(v);
    return Number.isFinite(n) ? n : null;
}

function buildNutritionFacts(nutrition, fallbackCalories){
    const rows = [];
    const energy = safeNumber(nutrition.calories) ?? fallbackCalories ?? 0;
    rows.push(["Calories", `${Math.round(energy)} kcal`]);
    if (nutrition.protein_g != null) rows.push(["Protein", `${Math.round(Number(nutrition.protein_g))} g`]);
    if (nutrition.carbs_g != null) rows.push(["Carbs", `${Math.round(Number(nutrition.carbs_g))} g`]);
    if (nutrition.fat_g != null) rows.push(["Fat", `${Math.round(Number(nutrition.fat_g))} g`]);
    if (nutrition.fiber_g != null) rows.push(["Fiber", `${Math.round(Number(nutrition.fiber_g))} g`]);
    if (nutrition.sugar_g != null) rows.push(["Sugar", `${Math.round(Number(nutrition.sugar_g))} g`]);
    if (nutrition.sodium_mg != null) rows.push(["Sodium", `${Math.round(Number(nutrition.sodium_mg))} mg`]);
    if (rows.length === 1) {
        rows.push(["Protein", "—"], ["Carbs", "—"], ["Fat", "—"]);
    }
    return rows;
}

function escapeHtml(str){
    return String(str).replace(/[&<>"]+/g, s => ({"&":"&amp;","<":"&lt;",">":"&gt;","\"":"&quot;"}[s]));
}

