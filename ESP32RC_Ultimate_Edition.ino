#include <Servo.h>
#include <Wire.h>
#include <MPU6050.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <Adafruit_NeoPixel.h>

// === HARDWAROVÁ KONFIGURACE (Uprav podle svého zapojení) ===
// Piny
const int PIN_THROTTLE_IN = 2;
const int PIN_STEERING_IN = 3;
const int PIN_MODE_IN     = 4;

const int PIN_ESC_LEFT    = 9;
const int PIN_ESC_RIGHT   = 10;

const int PIN_ENC_FL      = 5;
const int PIN_ENC_FR      = 6;
const int PIN_ENC_RL      = 7;
const int PIN_ENC_RR      = 8;

const int PIN_LEDS        = 13; // Pin pro WS2812B pásek
const int NUM_LEDS        = 8;  // Počet LED

// === OBJEKTY ===
Servo escLeft;
Servo escRight;
MPU6050 mpu;
WebServer server(80);
Preferences preferences;
Adafruit_NeoPixel strip(NUM_LEDS, PIN_LEDS, NEO_GRB + NEO_KHZ800);

// === PARAMETRY VOZIDLA & FYZIKA ===
const float WHEELBASE      = 0.26f;
const float TRACK_WIDTH    = 0.18f;
const float MAX_STEER_RAD  = 0.40f;
const float WHEEL_RADIUS   = 0.03f;
const float ENCODER_CPR    = 20.0f;

// === PARAMETRY LADĚNÍ (Budou se přepisovat z WiFi/Preferences) ===
float Kp_yaw = 0.20f;
float Ki_yaw = 0.80f;
float Kp_tc  = 0.40f;
float Ki_tc  = 0.50f;
float gamma_slip = 1.10f;     // Povolený skluz (běžný)
float gamma_launch = 1.30f;   // Povolený skluz (start)
float tv_factor = 1.0f;       // Síla torque vectoringu

// === STAVOVÉ PROMĚNNÉ ===
// Rychlosti (rad/s)
volatile float omega_FL=0, omega_FR=0, omega_RL=0, omega_RR=0;
volatile long enc_FL=0, enc_FR=0, enc_RL=0, enc_RR=0;

// IMU
float yaw_rate = 0.0f;
float lateral_accel = 0.0f;

// Regulátory
float yaw_err_int = 0.0f;
float slip_int_L = 0.0f;
float slip_int_R = 0.0f;

// Logika
bool launch_control_active = false;
bool is_braking = false;
unsigned long lastLoopTime = 0;
unsigned long lastTelemetryTime = 0;

// WiFi nastavení
const char* ssid = "ESP32-RC-Tuner";
const char* password = "torquemaster!";

// HTML pro WebServer
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP32 RC Telemetry</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: sans-serif; text-align: center; background: #1a1a1a; color: #fff; }
    .card { background: #333; padding: 20px; margin: 10px; border-radius: 10px; }
    input[type=range] { width: 100%; height: 30px; }
    .val { color: #0f0; font-weight: bold; font-size: 1.2em; }
    button { padding: 15px; width: 100%; background: #e63946; border: none; color: white; font-size: 18px; border-radius: 5px; margin-top: 10px;}
  </style>
</head>
<body>
  <h1>🏎️ RC Ultimate Tuner</h1>
  
  <div class="card">
    <h3>Yaw Control (Zatáčení)</h3>
    <label>P Gain: <span id="kp" class="val">%KP%</span></label>
    <input type="range" min="0" max="500" value="%KP_VAL%" oninput="upd('kp', this.value/100)">
    <br>
    <label>I Gain: <span id="ki" class="val">%KI%</span></label>
    <input type="range" min="0" max="500" value="%KI_VAL%" oninput="upd('ki', this.value/100)">
    <br>
    <label>TV Factor: <span id="tv" class="val">%TV%</span></label>
    <input type="range" min="0" max="200" value="%TV_VAL%" oninput="upd('tv', this.value/100)">
  </div>

  <div class="card">
    <h3>Traction & Launch</h3>
    <label>Max Slip (Jízda): <span id="sl" class="val">%SL%</span></label>
    <input type="range" min="100" max="200" value="%SL_VAL%" oninput="upd('sl', this.value/100)">
    <br>
    <label>Max Slip (Launch): <span id="lc" class="val">%LC%</span></label>
    <input type="range" min="100" max="200" value="%LC_VAL%" oninput="upd('lc', this.value/100)">
  </div>

  <button onclick="save()">💾 ULOŽIT DO PAMĚTI</button>

<script>
function upd(p, v) {
  document.getElementById(p).innerText = v;
  fetch("/set?p="+p+"&v="+v).catch(console.log);
}
function save() {
  fetch("/save").then(()=>alert("Uloženo!")).catch(()=>alert("Chyba!"));
}
</script>
</body></html>
)rawliteral";

// === ISR INTERRUPTS ===
void IRAM_ATTR isr_FL() { enc_FL++; }
void IRAM_ATTR isr_FR() { enc_FR++; }
void IRAM_ATTR isr_RL() { enc_RL++; }
void IRAM_ATTR isr_RR() { enc_RR++; }

// === POMOCNÉ FUNKCE ===
float mapPulse(int p, int min, int max, int center) {
  if (p < center) return map(p, min, center, -100, 0) / 100.0f;
  return map(p, center, max, 0, 100) / 100.0f;
}

void updateSensors(float dt) {
  // Rychlosti kol
  omega_FL = (enc_FL * 2*PI) / (ENCODER_CPR * dt);
  omega_FR = (enc_FR * 2*PI) / (ENCODER_CPR * dt);
  omega_RL = (enc_RL * 2*PI) / (ENCODER_CPR * dt);
  omega_RR = (enc_RR * 2*PI) / (ENCODER_CPR * dt);
  enc_FL=0; enc_FR=0; enc_RL=0; enc_RR=0;

  // IMU
  int16_t ax,ay,az,gx,gy,gz;
  mpu.getMotion6(&ax,&ay,&az,&gx,&gy,&gz);
  yaw_rate = gz / 131.0f * PI/180.0f; 
}

// === SETUP ===
void setup() {
  Serial.begin(115200);
  
  // WiFi & Preferences
  preferences.begin("rc-car", false);
  Kp_yaw = preferences.getFloat("kp", 0.20f);
  Ki_yaw = preferences.getFloat("ki", 0.80f);
  tv_factor = preferences.getFloat("tv", 1.0f);
  gamma_slip = preferences.getFloat("sl", 1.10f);
  
  WiFi.softAP(ssid, password);
  Serial.print("Tuner IP: "); Serial.println(WiFi.softAPIP());

  server.on("/", [](){
    String s = index_html;
    s.replace("%KP%",String(Kp_yaw)); s.replace("%KP_VAL%",String(int(Kp_yaw*100)));
    s.replace("%KI%",String(Ki_yaw)); s.replace("%KI_VAL%",String(int(Ki_yaw*100)));
    s.replace("%TV%",String(tv_factor)); s.replace("%TV_VAL%",String(int(tv_factor*100)));
    s.replace("%SL%",String(gamma_slip)); s.replace("%SL_VAL%",String(int(gamma_slip*100)));
    s.replace("%LC%",String(gamma_launch)); s.replace("%LC_VAL%",String(int(gamma_launch*100)));
    server.send(200, "text/html", s);
  });

  server.on("/set", [](){
    String p = server.arg("p"); float v = server.arg("v").toFloat();
    if(p=="kp") Kp_yaw=v; if(p=="ki") Ki_yaw=v;
    if(p=="tv") tv_factor=v; if(p=="sl") gamma_slip=v; if(p=="lc") gamma_launch=v;
    server.send(200,"text/plain","ok");
  });

  server.on("/save", [](){
    preferences.putFloat("kp", Kp_yaw); preferences.putFloat("ki", Ki_yaw);
    preferences.putFloat("tv", tv_factor); preferences.putFloat("sl", gamma_slip);
    server.send(200,"text/plain","saved");
  });
  server.begin();

  // Hardware
  escLeft.attach(PIN_ESC_LEFT); escRight.attach(PIN_ESC_RIGHT);
  pinMode(PIN_THROTTLE_IN, INPUT); pinMode(PIN_STEERING_IN, INPUT);
  
  pinMode(PIN_ENC_FL, INPUT_PULLUP); attachInterrupt(digitalPinToInterrupt(PIN_ENC_FL), isr_FL, RISING);
  // ... (nastav ostatní interrupt piny analogicky) ...
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_FR), isr_FR, RISING);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_RL), isr_RL, RISING);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_RR), isr_RR, RISING);

  Wire.begin();
  mpu.initialize();
  strip.begin(); strip.show();
}

// === HLAVNÍ LOOP ===
void loop() {
  server.handleClient();
  
  unsigned long now = micros();
  float dt = (now - lastLoopTime) / 1e6f;
  if(dt < 0.01f) return; // 100Hz loop
  lastLoopTime = now;

  // 1. Čtení RC
  float thr = mapPulse(pulseIn(PIN_THROTTLE_IN, HIGH), 1100, 1900, 1500);
  float str = mapPulse(pulseIn(PIN_STEERING_IN, HIGH), 1100, 1900, 1500);
  
  // Detekce brzdění / zpátečky
  is_braking = (thr < -0.1f);

  updateSensors(dt);
  float v_car = (omega_FL + omega_FR) * 0.5f * WHEEL_RADIUS;

  // 2. LAUNCH CONTROL LOGIKA
  // Pokud stojíme, držíme brzdu a přidáme plyn -> aktivace Launch mode
  static bool lc_ready = false;
  if (fabs(v_car) < 0.1f && is_braking && thr < -0.8f) {
    lc_ready = true;
    // Vizuální signalizace - bliká modře
    strip.fill(strip.Color(0,0,255)); strip.show();
  } else if (lc_ready && thr > 0.1f) {
    launch_control_active = true;
    lc_ready = false;
  }
  
  // Vypnutí LC po dosažení rychlosti
  if (launch_control_active && v_car > 3.0f) launch_control_active = false;

  float current_gamma = launch_control_active ? gamma_launch : gamma_slip;


  // 3. TORQUE VECTORING (YAW CONTROL)
  float r_ref = 0;
  if (!is_braking && fabs(v_car) > 0.2f) {
    r_ref = (tanf(str * MAX_STEER_RAD) / WHEELBASE) * v_car * tv_factor;
  }
  
  float yaw_err = r_ref - yaw_rate;
  yaw_err_int += yaw_err * dt;
  yaw_err_int = constrain(yaw_err_int, -0.5f, 0.5f);
  
  float delta_T = Kp_yaw * yaw_err + Ki_yaw * yaw_err_int;
  delta_T = constrain(delta_T, -0.4f, 0.4f); // Max 40% přenos síly


  // 4. VÝPOČET VÝKONU KOL
  float t_base = constrain(thr, -1.0f, 1.0f);
  float t_left = t_base;
  float t_right = t_base;

  if (!is_braking && thr > 0) {
    // Aplikujeme TV jen pod plynem
    t_left -= delta_T;
    t_right += delta_T;
  }

  // 5. TRACTION CONTROL & ABS
  bool tc_active = false;
  bool abs_active = false;

  if (!is_braking && thr > 0) {
    // === TRACTION CONTROL ===
    float w_front = max((omega_FL + omega_FR) * 0.5f, 1.0f);
    float w_limit = current_gamma * w_front;

    // Levé
    if (omega_RL > w_limit) {
      float slip = w_limit - omega_RL;
      slip_int_L += slip * dt;
      t_left = min(t_left, max(0.0f, Kp_tc * slip + Ki_tc * slip_int_L));
      tc_active = true;
    } else slip_int_L = 0;

    // Pravé
    if (omega_RR > w_limit) {
      float slip = w_limit - omega_RR;
      slip_int_R += slip * dt;
      t_right = min(t_right, max(0.0f, Kp_tc * slip + Ki_tc * slip_int_R));
      tc_active = true;
    } else slip_int_R = 0;

  } else if (is_braking) {
    // === ABS (Anti-lock Braking) ===
    // Pokud se auto hýbe (>0.5 m/s), ale kolo stojí (<1 rad/s) -> uvolni brzdu
    if (v_car > 0.5f) {
      if (omega_RL < 1.0f) { t_left *= 0.2f; abs_active = true; } // Povol brzdu na 20%
      if (omega_RR < 1.0f) { t_right *= 0.2f; abs_active = true; }
    }
  }


  // 6. OVLÁDÁNÍ MOTORŮ
  int pulseL = 1500 + constrain(t_left, -1.0f, 1.0f) * 400;
  int pulseR = 1500 + constrain(t_right, -1.0f, 1.0f) * 400;
  
  escLeft.writeMicroseconds(pulseL);
  escRight.writeMicroseconds(pulseR);


  // 7. VIZUÁLNÍ SIGNALIZACE (LED)
  if (millis() - lastTelemetryTime > 50) {
    strip.clear();
    if (lc_ready) {
       // Modrá pulzuje - připraveno ke startu
       int b = (millis() / 5) % 255; 
       strip.fill(strip.Color(0, 0, b));
    } 
    else if (tc_active || abs_active) {
       // Červené záblesky - ztráta trakce
       strip.fill(strip.Color(255, 0, 0));
    } 
    else {
       // Normální jízda - indikace plynu (zelená) + TV (bílá na stranách)
       int throttleLed = map(abs(thr)*100, 0, 100, 0, 255);
       strip.setPixelColor(3, strip.Color(0, throttleLed, 0)); // Střed
       strip.setPixelColor(4, strip.Color(0, throttleLed, 0)); 
       
       // Indikace Torque Vectoringu
       if (delta_T > 0.05f) strip.setPixelColor(7, strip.Color(255, 255, 255)); // Pravá zabírá
       if (delta_T < -0.05f) strip.setPixelColor(0, strip.Color(255, 255, 255)); // Levá zabírá
    }
    strip.show();
    lastTelemetryTime = millis();
  }
}