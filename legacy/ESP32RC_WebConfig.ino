#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

// Nastavení WiFi (nebo Access Point módu)
const char* ssid = "ESP32-RC-Tuner";
const char* password = "torquemaster!";

WebServer server(80);
Preferences preferences;

// Globální proměnné pro ladění (ty, co už máš v hlavním kódu)
extern float Kp_yaw;
extern float Ki_yaw;
extern float gamma_slip;
extern float tv_factor;

// HTML stránka uložená v PROGMEM (pro úsporu RAM)
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP32 RC Tuner</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial; text-align: center; background-color: #222; color: #eee; }
    .slider-container { width: 80%; margin: 20px auto; }
    input[type=range] { width: 100%; }
    .value { font-size: 1.2em; color: #00ff00; font-weight: bold; }
    button { padding: 15px; background-color: #4CAF50; color: white; border: none; border-radius: 5px; font-size: 16px; margin-top: 20px;}
  </style>
</head>
<body>
  <h2>🏎️ RC Dynamics Tuner</h2>
  
  <div class="slider-container">
    <label>Yaw Kp (Agresivita): <span id="valKp">%KP%</span></label>
    <input type="range" min="0" max="200" value="%KP_VAL%" oninput="update('kp', this.value/100)">
  </div>

  <div class="slider-container">
    <label>Yaw Ki (Stabilita): <span id="valKi">%KI%</span></label>
    <input type="range" min="0" max="200" value="%KI_VAL%" oninput="update('ki', this.value/100)">
  </div>

  <div class="slider-container">
    <label>Max Slip (Trakce): <span id="valSlip">%SLIP%</span></label>
    <input type="range" min="100" max="200" value="%SLIP_VAL%" oninput="update('slip', this.value/100)">
  </div>

  <div class="slider-container">
    <label>Torque Vectoring (%): <span id="valTv">%TV%</span></label>
    <input type="range" min="0" max="200" value="%TV_VAL%" oninput="update('tv', this.value/100)">
  </div>
  
  <button onclick="save()">💾 Uložit do paměti</button>

<script>
function update(param, val) {
  document.getElementById('val'+param.charAt(0).toUpperCase() + param.slice(1)).innerHTML = val;
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/set?param="+param+"&value="+val, true);
  xhr.send();
}
function save() {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/save", true);
  xhr.send();
  alert("Nastavení uloženo!");
}
</script>
</body>
</html>
)rawliteral";

void setupWiFi() {
  // Vytvoříme vlastní WiFi síť
  WiFi.softAP(ssid, password);
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());

  // Načtení uložených hodnot
  preferences.begin("rc-config", false);
  Kp_yaw = preferences.getFloat("kp", 0.20f);
  Ki_yaw = preferences.getFloat("ki", 0.80f);
  gamma_slip = preferences.getFloat("slip", 1.10f);
  tv_factor = preferences.getFloat("tv", 1.0f);

  // Routy webserveru
  server.on("/", []() {
    String html = index_html;
    // Nahrazení placeholderů aktuálními hodnotami
    html.replace("%KP%", String(Kp_yaw));
    html.replace("%KP_VAL%", String(Kp_yaw * 100));
    html.replace("%KI%", String(Ki_yaw));
    html.replace("%KI_VAL%", String(Ki_yaw * 100));
    html.replace("%SLIP%", String(gamma_slip));
    html.replace("%SLIP_VAL%", String(gamma_slip * 100));
    html.replace("%TV%", String(tv_factor));
    html.replace("%TV_VAL%", String(tv_factor * 100));
    server.send(200, "text/html", html);
  });

  // API pro změnu hodnot
  server.on("/set", []() {
    String param = server.arg("param");
    float value = server.arg("value").toFloat();

    if (param == "kp") Kp_yaw = value;
    else if (param == "ki") Ki_yaw = value;
    else if (param == "slip") gamma_slip = value;
    else if (param == "tv") tv_factor = value;

    server.send(200, "text/plain", "OK");
    Serial.print("Tuning update: "); Serial.print(param); Serial.print(" = "); Serial.println(value);
  });

  // Uložení do EEPROM
  server.on("/save", []() {
    preferences.putFloat("kp", Kp_yaw);
    preferences.putFloat("ki", Ki_yaw);
    preferences.putFloat("slip", gamma_slip);
    preferences.putFloat("tv", tv_factor);
    server.send(200, "text/plain", "Saved");
  });

  server.begin();
}

// Funkce volat v loop()
void handleWiFi() {
  server.handleClient();
}
