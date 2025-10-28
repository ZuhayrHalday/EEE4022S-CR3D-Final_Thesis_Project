// ======================================================================
// CR3D Muon Logger (Arduino Nano / ATmega328P) - v1.4
// - A2: read peak-detector output
// - D8: non-blocking test pulser (10 us every 5000 ms) for RC injection
// - D13: blink on event
//
// Baseline modes:
//   FIXED
//   AUTO 
//
// Serial commands (newline-terminated, case-insensitive):
//   SET MODE AUTO
//   SET MODE FIXED
//   SET BASELINE_MV <float>
//   SET THRESHOLD_MV <float>
//   SET PULSER ON|OFF
//   SET PULSE_US <uint> 
//   SET PERIOD_MS <uint> 
//
// Output (newline-terminated JSON):
//   {"type":"hello", ...}
//   {"type":"sample","ts_us":...,"adc":...,"mv":...,"dht_C":null}
//   {"type":"event","ts_us":...,"adc_peak":...,"mv_peak":...,
//    "baseline_adc":...,"dead_us":...,"dht_C":null}
// ======================================================================

#include <Arduino.h>

// ---------------- Pins ----------------
const uint8_t PIN_ADC   = A2;
const uint8_t PIN_LED   = 13;
const uint8_t PIN_PULSE = 8;

// ---------------- ADC / scaling ----------------
const float   VREF_V  = 5.00;
const float   LSB_mV  = (VREF_V * 1000.0f) / 1023.0f; // ~4.887 mV/LSB

// ---------------- Detection parameters ----------------
volatile float THRESHOLD_mV   = 50.0f;    // over-baseline threshold (mV)
const uint32_t PEAK_HOLD_US   = 4000;     // local-peak search window
const uint32_t DEAD_TIME_US   = 300;      // ignore new events for this long
const uint32_t SAMPLE_EMIT_US = 1000;    // background telemetry sample every 20 ms

// ---------------- Baseline modes ----------------
#define BASELINE_MODE_AUTO  0
#define BASELINE_MODE_FIXED 1

uint8_t BASELINE_MODE      = BASELINE_MODE_FIXED; 
float   FIXED_BASELINE_mV  = 880.0f;              // ~0.88 V idle

// ---- Baseline estimation buffer (AUTO mode) ----
const uint8_t  BLEN = 31;
uint16_t bl_buf[BLEN];
uint8_t  bl_i = 0;
bool     bl_filled = false;
uint16_t medianCopy[BLEN];

uint16_t medianN() {
  uint8_t n = bl_filled ? BLEN : bl_i;
  for (uint8_t i = 0; i < n; ++i) medianCopy[i] = bl_buf[i];
  for (uint8_t i = 1; i < n; ++i) {
    uint16_t key = medianCopy[i];
    int8_t j = i - 1;
    while (j >= 0 && medianCopy[j] > key) { medianCopy[j+1] = medianCopy[j]; j--; }
    medianCopy[j+1] = key;
  }
  return medianCopy[n/2];
}

// ---------------- Non-blocking pulser on D8 ----------------
bool     PULSER_ENABLED = true;
uint32_t PULSE_US   = 10;        // HIGH width
uint32_t PERIOD_MS  = 5000;      // period

uint8_t  pulse_state = 0;        // 0=low, 1=high
uint32_t next_pulse_at_us = 0;
uint32_t pulse_end_us     = 0;

// Synchronized scope sample scheduling
volatile uint32_t scope_due_us = 0;
const uint16_t SCOPE_DELAY_US = 300; // time after rising edge to read A2

inline void pulser_init() {
  pinMode(PIN_PULSE, OUTPUT);
  digitalWrite(PIN_PULSE, LOW);
  next_pulse_at_us = micros() + 100000; // first pulse after 100 ms
}

inline void pulser_tick() {
  if (!PULSER_ENABLED) {
    if (pulse_state) { digitalWrite(PIN_PULSE, LOW); pulse_state = 0; }
    return;
  }
  uint32_t now = micros();
  if (pulse_state == 0) {
    if ((int32_t)(now - next_pulse_at_us) >= 0) {
      digitalWrite(PIN_PULSE, HIGH);
      pulse_state = 1;
      pulse_end_us = now + PULSE_US;
      scope_due_us = now + SCOPE_DELAY_US;
    }
  } else { // HIGH
    if ((int32_t)(now - pulse_end_us) >= 0) {
      digitalWrite(PIN_PULSE, LOW);
      pulse_state = 0;
      next_pulse_at_us = now + (PERIOD_MS * 1000UL);
    }
  }
}

// ---------------- Command parser ----------------
void handleCommandLine(const String& sIn) {
  String s = sIn; s.trim(); if (!s.length()) return;
  String u = s; u.toUpperCase();

  if (u.startsWith("SET MODE ")) {
    if (u.endsWith("AUTO"))  { BASELINE_MODE = BASELINE_MODE_AUTO;
      Serial.println(F("{\"type\":\"ack\",\"cmd\":\"MODE\",\"val\":\"AUTO\"}"));
    } else if (u.endsWith("FIXED")) { BASELINE_MODE = BASELINE_MODE_FIXED;
      Serial.println(F("{\"type\":\"ack\",\"cmd\":\"MODE\",\"val\":\"FIXED\"}"));
    }
  } else if (u.startsWith("SET BASELINE_MV ")) {
    float v = s.substring(16).toFloat();
    if (v >= 0.0f) { FIXED_BASELINE_mV = v;
      Serial.print(F("{\"type\":\"ack\",\"cmd\":\"BASELINE_MV\",\"val\":")); Serial.print(FIXED_BASELINE_mV,1); Serial.println('}');
    }
  } else if (u.startsWith("SET THRESHOLD_MV ")) {
    float v = s.substring(17).toFloat();
    if (v >= 0.0f) { THRESHOLD_mV = v;
      Serial.print(F("{\"type\":\"ack\",\"cmd\":\"THRESHOLD_MV\",\"val\":")); Serial.print(THRESHOLD_mV,1); Serial.println('}');
    }
  } else if (u.startsWith("SET PULSER ")) {
    if (u.endsWith("ON"))  { PULSER_ENABLED = true;  Serial.println(F("{\"type\":\"ack\",\"cmd\":\"PULSER\",\"val\":\"ON\"}")); }
    if (u.endsWith("OFF")) { PULSER_ENABLED = false; Serial.println(F("{\"type\":\"ack\",\"cmd\":\"PULSER\",\"val\":\"OFF\"}")); }
  } else if (u.startsWith("SET PULSE_US ")) {
    uint32_t v = (uint32_t)s.substring(13).toInt();
    if (v > 0) { PULSE_US = v; Serial.print(F("{\"type\":\"ack\",\"cmd\":\"PULSE_US\",\"val\":")); Serial.print(PULSE_US); Serial.println('}'); }
  } else if (u.startsWith("SET PERIOD_MS ")) {
    uint32_t v = (uint32_t)s.substring(14).toInt();
    if (v > 0) { PERIOD_MS = v; Serial.print(F("{\"type\":\"ack\",\"cmd\":\"PERIOD_MS\",\"val\":")); Serial.print(PERIOD_MS); Serial.println('}'); }
  }
}

// ---------------- Helpers ----------------
inline uint16_t adc_toss_then_read(uint16_t pin, uint16_t settle_us) {
  (void)analogRead(pin);              // toss first after mux switch
  if (settle_us) delayMicroseconds(settle_us);
  return analogRead(pin);
}

// ---------------- Setup ----------------
void setup() {
  // Keep analog input high-Z (disable global pull-ups)
  MCUCR |= _BV(PUD);
  pinMode(PIN_ADC, INPUT); digitalWrite(PIN_ADC, LOW);
  pinMode(PIN_LED, OUTPUT); digitalWrite(PIN_LED, LOW);

  analogReference(DEFAULT);
  Serial.begin(115200);
  delay(20);

  for (uint8_t i=0; i<BLEN; ++i) { bl_buf[i] = adc_toss_then_read(PIN_ADC, 150); delay(5); }
  bl_filled = true;

  pulser_init();

  // Handshake
  Serial.print(F("{\"type\":\"hello\",\"ver\":\"1.4\",\"unit\":\"CR3D-nano-a2\",\"vref_V\":"));
  Serial.print(VREF_V,3);
  Serial.print(F(",\"baseline_mode\":\""));
  Serial.print(BASELINE_MODE==BASELINE_MODE_FIXED ? F("FIXED") : F("AUTO"));
  Serial.print(F("\",\"fixed_baseline_mV\":"));
  Serial.print(FIXED_BASELINE_mV,1);
  Serial.print(F(",\"threshold_mV\":"));
  Serial.print(THRESHOLD_mV,1);
  Serial.print(F(",\"pulser\":{\"enabled\":"));
  Serial.print(PULSER_ENABLED ? F("true") : F("false"));
  Serial.print(F(",\"pulse_us\":")); Serial.print(PULSE_US);
  Serial.print(F(",\"period_ms\":")); Serial.print(PERIOD_MS);
  Serial.println(F("}}"));
}

// ---------------- Main loop ----------------
void loop() {
  // Commands
  while (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    if (line.length()) handleCommandLine(line);
  }

  // Pulser (non-blocking)
  pulser_tick();

  // ---- Synchronized scope sample  ----
  uint32_t now_us = micros();
  if (scope_due_us && (int32_t)(now_us - scope_due_us) >= 0) {
    scope_due_us = 0;
    uint16_t adc_scope = adc_toss_then_read(PIN_ADC, 150);
    float mv_scope = adc_scope * LSB_mV;

    Serial.print(F("{\"type\":\"sample\",\"ts_us\":")); Serial.print(now_us);
    Serial.print(F(",\"adc\":")); Serial.print(adc_scope);
    Serial.print(F(",\"mv\":")); Serial.print(mv_scope, 2);
    Serial.print(F(",\"dht_C\":null"));
    Serial.println('}');
  }
  
  static uint32_t last_sample_emit = 0;
  if ((uint32_t)(now_us - last_sample_emit) >= SAMPLE_EMIT_US) {
    last_sample_emit = now_us;

    uint16_t adc_bg = adc_toss_then_read(PIN_ADC, 150);
    float mv_bg = adc_bg * LSB_mV;

    // Update AUTO baseline
    bl_buf[bl_i] = adc_bg;
    bl_i = (bl_i + 1) % BLEN;
    if (bl_i == 0) bl_filled = true;

    Serial.print(F("{\"type\":\"sample\",\"ts_us\":")); Serial.print(now_us);
    Serial.print(F(",\"adc\":")); Serial.print(adc_bg);
    Serial.print(F(",\"mv\":")); Serial.print(mv_bg, 2);
    Serial.print(F(",\"dht_C\":null"));
    Serial.println('}');
  }

  // ---- Baseline in raw & mV ----
  uint16_t bl_adc;
  float bl_mV;
  if (BASELINE_MODE == BASELINE_MODE_FIXED) {
    bl_mV  = FIXED_BASELINE_mV;
    bl_adc = (uint16_t)((bl_mV / LSB_mV) + 0.5f);
  } else {
    bl_adc = medianN();
    bl_mV  = bl_adc * LSB_mV;
  }

  static uint32_t last_event_time = 0;
  bool in_dead = (uint32_t)(now_us - last_event_time) < DEAD_TIME_US;

  // Current sample for trigger decision 
  uint16_t adc_now = adc_toss_then_read(PIN_ADC, 150);
  float mv_now = adc_now * LSB_mV;
  float over_mV = mv_now - bl_mV;

  if (!in_dead && over_mV >= THRESHOLD_mV) {
    uint16_t peak_adc = adc_now;
    uint32_t start = micros();
    while ((uint32_t)(micros() - start) < PEAK_HOLD_US) {
      pulser_tick(); // keep pulses regular
      uint16_t a = adc_toss_then_read(PIN_ADC, 100);
      if (a > peak_adc) peak_adc = a;
    }

    uint32_t ts_event = micros();
    uint32_t dead_us  = ts_event - last_event_time;
    last_event_time   = ts_event;

    // LED blink
    digitalWrite(PIN_LED, HIGH);
    delayMicroseconds(200);
    digitalWrite(PIN_LED, LOW);

    // Emit event
    Serial.print(F("{\"type\":\"event\",\"ts_us\":")); Serial.print(ts_event);
    Serial.print(F(",\"adc_peak\":")); Serial.print(peak_adc);
    Serial.print(F(",\"mv_peak\":")); Serial.print(peak_adc * LSB_mV, 2);
    Serial.print(F(",\"baseline_adc\":")); Serial.print(bl_adc);
    Serial.print(F(",\"dead_us\":")); Serial.print(dead_us);
    Serial.print(F(",\"dht_C\":null"));
    Serial.println('}');
  }
}
