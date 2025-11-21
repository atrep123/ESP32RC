#include <Servo.h>
#include <Wire.h>
#include <MPU6050.h>

// === HARDWAROVÁ KONFIGURACE ===
// ESC výstupy
Servo escLeft;
Servo escRight;

// RC vstupy
const int THROTTLE_PIN = 2;
const int STEERING_PIN = 3;
const int MODE_PIN = 4;  // Přepínání režimů

// ESC piny
const int ESC_LEFT_PIN = 9;
const int ESC_RIGHT_PIN = 10;

// Enkodéry kol
const int ENCODER_FL_PIN = 5;
const int ENCODER_FR_PIN = 6;
const int ENCODER_RL_PIN = 7;
const int ENCODER_RR_PIN = 8;

// === KALIBRACE RC ===
const int THR_MIN = 1100;
const int THR_MAX = 1900;
const int THR_NEUTRAL = 1500;

const int STR_MIN = 1100;
const int STR_MAX = 1900;
const int STR_CENTER = 1500;

// === PARAMETRY VOZIDLA ===
const float WHEELBASE = 0.26f;        // Rozvor 260mm
const float TRACK_WIDTH = 0.18f;      // Rozchod 180mm
const float MAX_STEER_RAD = 0.40f;    // Max úhel rejdu ~23°
const float WHEEL_RADIUS = 0.03f;     // Poloměr kola 30mm
const float ENCODER_CPR = 20.0f;      // Pulzy na otáčku

// === JÍZDNÍ REŽIMY ===
enum DriveMode {
  MODE_NORMAL = 0,
  MODE_SPORT = 1,
  MODE_DRIFT = 2
};

struct ModeParameters {
  float kp_yaw;
  float ki_yaw;
  float kp_tc;
  float ki_tc;
  float gamma_slip;
  float max_dt;
  float tv_factor;
};

ModeParameters modeParams[3] = {
  // NORMAL
  {0.15f, 0.60f, 0.30f, 0.40f, 1.10f, 0.30f, 1.0f},
  // SPORT
  {0.25f, 1.00f, 0.40f, 0.50f, 1.20f, 0.50f, 1.2f},
  // DRIFT
  {0.05f, 0.10f, 0.10f, 0.20f, 1.40f, 0.70f, 1.5f}
};

// === STAVOVÉ PROMĚNNÉ ===
DriveMode currentMode = MODE_NORMAL;
MPU6050 mpu;

// Rychlosti kol (rad/s)
volatile float omega_FL = 0.0f;
volatile float omega_FR = 0.0f;
volatile float omega_RL = 0.0f;
volatile float omega_RR = 0.0f;

// Počítadla enkodérů
volatile long encoder_FL_count = 0;
volatile long encoder_FR_count = 0;
volatile long encoder_RL_count = 0;
volatile long encoder_RR_count = 0;

// IMU data
float yaw_rate = 0.0f;        // rad/s
float lateral_accel = 0.0f;   // m/s²

// Regulátory - integrální složky
float yaw_err_int = 0.0f;
float slip_int_L = 0.0f;
float slip_int_R = 0.0f;

// Telemetrie
struct TelemetryData {
  unsigned long timestamp;
  float vehicle_speed;
  float yaw_rate;
  float yaw_ref;
  float torque_left;
  float torque_right;
  float delta_torque;
  float slip_left;
  float slip_right;
  DriveMode mode;
  float throttle_cmd;
  float steering_cmd;
} telemetry;

// Fail-safe
unsigned long lastRCPulse = 0;
const unsigned long RC_TIMEOUT = 500; // ms

// === ISR PRO ENKODÉRY ===
void IRAM_ATTR encoderFL_ISR() { encoder_FL_count++; }
void IRAM_ATTR encoderFR_ISR() { encoder_FR_count++; }
void IRAM_ATTR encoderRL_ISR() { encoder_RL_count++; }
void IRAM_ATTR encoderRR_ISR() { encoder_RR_count++; }

// === POMOCNÉ FUNKCE ===
float mapPulseToNorm(int pulse, int min, int max, int center = -1) {
  if (center == -1) {
    return constrain((float)(pulse - min) / (max - min), 0.0f, 1.0f);
  } else {
    if (pulse >= center) {
      return constrain((float)(pulse - center) / (max - center), 0.0f, 1.0f);
    } else {
      return constrain((float)(pulse - center) / (center - min), -1.0f, 0.0f);
    }
  }
}

void updateWheelSpeeds(float dt) {
  // Převod počtu pulzů na rychlost (rad/s)
  omega_FL = (encoder_FL_count * 2.0f * PI) / (ENCODER_CPR * dt);
  omega_FR = (encoder_FR_count * 2.0f * PI) / (ENCODER_CPR * dt);
  omega_RL = (encoder_RL_count * 2.0f * PI) / (ENCODER_CPR * dt);
  omega_RR = (encoder_RR_count * 2.0f * PI) / (ENCODER_CPR * dt);
  
  // Reset čítačů
  encoder_FL_count = 0;
  encoder_FR_count = 0;
  encoder_RL_count = 0;
  encoder_RR_count = 0;
}

void updateIMU() {
  int16_t ax, ay, az, gx, gy, gz;
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
  
  // Převod na fyzikální jednotky
  yaw_rate = gz / 131.0f * PI / 180.0f;  // rad/s
  lateral_accel = ay / 16384.0f * 9.81f; // m/s²
}

float calculateReferenceYawRate(float steering_norm, float vehicle_speed) {
  // Bicycle model - referenční yaw rate pro neutral steer
  if (fabs(vehicle_speed) < 0.1f) {
    return 0.0f;
  }
  
  float delta = steering_norm * MAX_STEER_RAD;
  float r_ref = (tanf(delta) / WHEELBASE) * vehicle_speed;
  
  // Aplikace TV faktoru podle režimu
  r_ref *= modeParams[currentMode].tv_factor;
  
  return r_ref;
}

float applyYawControl(float r_ref, float r_actual, float dt) {
  ModeParameters &params = modeParams[currentMode];
  
  // PI regulátor
  float yaw_err = r_ref - r_actual;
  yaw_err_int += yaw_err * dt;
  
  // Anti-windup
  yaw_err_int = constrain(yaw_err_int, -1.0f, 1.0f);
  
  float delta_torque = params.kp_yaw * yaw_err + params.ki_yaw * yaw_err_int;
  
  // Limitace
  delta_torque = constrain(delta_torque, -params.max_dt, params.max_dt);
  
  return delta_torque;
}

void applyTractionControl(float &t_left, float &t_right, float dt) {
  ModeParameters &params = modeParams[currentMode];
  
  // Průměrná rychlost předních kol
  float w_front = 0.5f * (omega_FL + omega_FR);
  
  // Ignorovat při velmi nízkých rychlostech
  if (w_front < 5.0f) {
    slip_int_L = 0.0f;
    slip_int_R = 0.0f;
    return;
  }
  
  float w_ref = params.gamma_slip * w_front;
  
  // Traction control - levé kolo
  if (omega_RL > w_ref) {
    float slip_err = w_ref - omega_RL;
    slip_int_L += slip_err * dt;
    slip_int_L = constrain(slip_int_L, -1.0f, 0.0f);
    
    float t_limit = params.kp_tc * slip_err + params.ki_tc * slip_int_L;
    t_left = min(t_left, max(0.0f, t_limit));
    
    telemetry.slip_left = (omega_RL - w_front) / max(w_front, 1.0f);
  } else {
    slip_int_L *= 0.95f; // Postupný decay
    telemetry.slip_left = 0.0f;
  }
  
  // Traction control - pravé kolo
  if (omega_RR > w_ref) {
    float slip_err = w_ref - omega_RR;
    slip_int_R += slip_err * dt;
    slip_int_R = constrain(slip_int_R, -1.0f, 0.0f);
    
    float t_limit = params.kp_tc * slip_err + params.ki_tc * slip_int_R;
    t_right = min(t_right, max(0.0f, t_limit));
    
    telemetry.slip_right = (omega_RR - w_front) / max(w_front, 1.0f);
  } else {
    slip_int_R *= 0.95f;
    telemetry.slip_right = 0.0f;
  }
}

void applyAckermannDiff(float &t_left, float &t_right, float steering_norm) {
  // Základní Ackermann geometrie
  if (fabs(steering_norm) > 0.05f) {
    float acker_factor = 0.1f; // Síla efektu
    
    if (steering_norm > 0) {
      // Zatáčení doprava - levé kolo je vnější
      t_left *= (1.0f + acker_factor * steering_norm);
      t_right *= (1.0f - acker_factor * steering_norm * 0.5f);
    } else {
      // Zatáčení doleva - pravé kolo je vnější
      t_right *= (1.0f - acker_factor * steering_norm);
      t_left *= (1.0f + acker_factor * steering_norm * 0.5f);
    }
  }
}

void sendTelemetry() {
  // Formát pro snadné parsování
  Serial.print(telemetry.timestamp);
  Serial.print(",");
  Serial.print(telemetry.vehicle_speed, 3);
  Serial.print(",");
  Serial.print(telemetry.yaw_rate, 3);
  Serial.print(",");
  Serial.print(telemetry.yaw_ref, 3);
  Serial.print(",");
  Serial.print(telemetry.torque_left, 3);
  Serial.print(",");
  Serial.print(telemetry.torque_right, 3);
  Serial.print(",");
  Serial.print(telemetry.delta_torque, 3);
  Serial.print(",");
  Serial.print(telemetry.slip_left, 3);
  Serial.print(",");
  Serial.print(telemetry.slip_right, 3);
  Serial.print(",");
  Serial.print(telemetry.mode);
  Serial.print(",");
  Serial.print(telemetry.throttle_cmd, 3);
  Serial.print(",");
  Serial.println(telemetry.steering_cmd, 3);
}

void checkFailSafe() {
  if (millis() - lastRCPulse > RC_TIMEOUT) {
    // Fail-safe - zastavit motory
    escLeft.writeMicroseconds(THR_NEUTRAL);
    escRight.writeMicroseconds(THR_NEUTRAL);
    
    // Reset integrátorů
    yaw_err_int = 0.0f;
    slip_int_L = 0.0f;
    slip_int_R = 0.0f;
    
    Serial.println("FAIL-SAFE: RC signal lost!");
  }
}

DriveMode readDriveMode() {
  unsigned long modePulse = pulseIn(MODE_PIN, HIGH, 25000);
  
  if (modePulse < 1350) return MODE_NORMAL;
  else if (modePulse < 1650) return MODE_SPORT;
  else return MODE_DRIFT;
}

// === HLAVNÍ FUNKCE ===
void setup() {
  Serial.begin(115200);
  Serial.println("ESP32RC Advanced Starting...");
  
  // ESC
  escLeft.attach(ESC_LEFT_PIN);
  escRight.attach(ESC_RIGHT_PIN);
  escLeft.writeMicroseconds(THR_NEUTRAL);
  escRight.writeMicroseconds(THR_NEUTRAL);
  
  // RC vstupy
  pinMode(THROTTLE_PIN, INPUT);
  pinMode(STEERING_PIN, INPUT);
  pinMode(MODE_PIN, INPUT);
  
  // Enkodéry
  pinMode(ENCODER_FL_PIN, INPUT_PULLUP);
  pinMode(ENCODER_FR_PIN, INPUT_PULLUP);
  pinMode(ENCODER_RL_PIN, INPUT_PULLUP);
  pinMode(ENCODER_RR_PIN, INPUT_PULLUP);
  
  attachInterrupt(digitalPinToInterrupt(ENCODER_FL_PIN), encoderFL_ISR, RISING);
  attachInterrupt(digitalPinToInterrupt(ENCODER_FR_PIN), encoderFR_ISR, RISING);
  attachInterrupt(digitalPinToInterrupt(ENCODER_RL_PIN), encoderRL_ISR, RISING);
  attachInterrupt(digitalPinToInterrupt(ENCODER_RR_PIN), encoderRR_ISR, RISING);
  
  // IMU
  Wire.begin();
  mpu.initialize();
  
  if (!mpu.testConnection()) {
    Serial.println("MPU6050 connection failed!");
  } else {
    Serial.println("MPU6050 connected");
    mpu.setFullScaleGyroRange(MPU6050_GYRO_FS_250);
    mpu.setFullScaleAccelRange(MPU6050_ACCEL_FS_2);
  }
  
  delay(2000); // ESC inicializace
  Serial.println("Ready!");
}

void loop() {
  static unsigned long lastLoopTime = micros();
  unsigned long now = micros();
  float dt = (now - lastLoopTime) / 1e6f;
  lastLoopTime = now;
  
  // Minimální dt pro stabilitu
  if (dt <= 0 || dt > 0.1f) dt = 0.01f;
  
  // === 1. ČTENÍ VSTUPŮ ===
  
  // RC signály
  unsigned long thrPulse = pulseIn(THROTTLE_PIN, HIGH, 25000);
  unsigned long strPulse = pulseIn(STEERING_PIN, HIGH, 25000);
  
  // Kontrola platnosti signálu
  if (thrPulse == 0 || strPulse == 0) {
    checkFailSafe();
    return;
  }
  
  lastRCPulse = millis();
  
  // Normalizace vstupů
  float throttle = mapPulseToNorm(thrPulse, THR_MIN, THR_MAX);
  float steering = mapPulseToNorm(strPulse, STR_MIN, STR_MAX, STR_CENTER);
  
  telemetry.throttle_cmd = throttle;
  telemetry.steering_cmd = steering;
  
  // Režim jízdy
  DriveMode newMode = readDriveMode();
  if (newMode != currentMode) {
    currentMode = newMode;
    // Reset integrátorů při změně režimu
    yaw_err_int = 0.0f;
    slip_int_L = 0.0f;
    slip_int_R = 0.0f;
    Serial.print("Mode changed to: ");
    Serial.println(currentMode);
  }
  
  // === 2. AKTUALIZACE SENZORŮ ===
  
  updateWheelSpeeds(dt);
  updateIMU();
  
  // Výpočet rychlosti vozidla
  float wheelSpeedFront = 0.5f * (omega_FL + omega_FR);
  float vehicleSpeed = wheelSpeedFront * WHEEL_RADIUS;
  
  telemetry.vehicle_speed = vehicleSpeed;
  telemetry.yaw_rate = yaw_rate;
  
  // === 3. VÝPOČET ŘÍZENÍ ===
  
  // Referenční yaw rate
  float r_ref = calculateReferenceYawRate(steering, vehicleSpeed);
  telemetry.yaw_ref = r_ref;
  
  // Yaw control -> delta torque
  float delta_torque = applyYawControl(r_ref, yaw_rate, dt);
  telemetry.delta_torque = delta_torque;
  
  // Základní rozdělení výkonu
  float t_base = throttle;
  float t_left = t_base - delta_torque;
  float t_right = t_base + delta_torque;
  
  // Ackermann diferenciál
  applyAckermannDiff(t_left, t_right, steering);
  
  // Traction control
  applyTractionControl(t_left, t_right, dt);
  
  // === 4. BEZPEČNOSTNÍ LIMITY ===
  
  // Saturace výstupů
  t_left = constrain(t_left, 0.0f, 1.0f);
  t_right = constrain(t_right, 0.0f, 1.0f);
  
  // Kontrola maximálního rozdílu
  float max_diff = modeParams[currentMode].max_dt;
  float actual_diff = fabs(t_right - t_left);
  if (actual_diff > max_diff) {
    float scale = max_diff / actual_diff;
    float avg = (t_left + t_right) * 0.5f;
    t_left = avg + (t_left - avg) * scale;
    t_right = avg + (t_right - avg) * scale;
  }
  
  telemetry.torque_left = t_left;
  telemetry.torque_right = t_right;
  
  // === 5. VÝSTUP NA MOTORY ===
  
  int leftPulse = THR_NEUTRAL + (int)(t_left * (THR_MAX - THR_NEUTRAL));
  int rightPulse = THR_NEUTRAL + (int)(t_right * (THR_MAX - THR_NEUTRAL));
  
  escLeft.writeMicroseconds(leftPulse);
  escRight.writeMicroseconds(rightPulse);
  
  // === 6. TELEMETRIE ===
  
  static unsigned long lastTelemetry = 0;
  if (millis() - lastTelemetry > 50) { // 20Hz
    telemetry.timestamp = millis();
    telemetry.mode = currentMode;
    sendTelemetry();
    lastTelemetry = millis();
  }
}