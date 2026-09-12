#include <WiFi.h>
#include <PubSubClient.h>

const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* mqtt_broker = "broker.mqttdashboard.com";
const int mqtt_port = 1883;

#define TOPIC_TRS_DATA   "pfa/trs/data"
#define TOPIC_COMMANDS   "pfa/trs/commands"

WiFiClient espClient;
PubSubClient client(espClient);

#define PIN_TRIG        23
#define PIN_ECHO        5
#define PIN_POT         34
#define PIN_BTN_RED     17
#define PIN_LED_GREEN   18
#define PIN_LED_RED     19
#define PIN_BUZZER      25

#define DEBOUNCE_MS     300
#define SEND_INTERVAL   2000
#define CURRENT_THRESHOLD 500
#define DISTANCE_THRESHOLD 15
#define SENSOR_COOLDOWN 1000

uint32_t goodCount = 0;
uint32_t rejectCount = 0;
uint32_t totalCount = 0;

bool machineState = false;
bool lastMachineState = false;
unsigned long lastSendTime = 0;
unsigned long lastRedPress = 0;
unsigned long lastObjectDetect = 0;
unsigned long shiftStartTime = 0;
unsigned long totalDowntimeSeconds = 0;
unsigned long currentDowntimeStart = 0;

float currentRPM = 0;
float targetRPM = 0;

int nodeRedNominalRPM = 1500;
int nodeRedPlannedTime = 480;
String nodeRedMachineName = "MACHINE";

int lastButtonState = LOW;
unsigned long lastDebounceTime = 0;

void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) message += (char)payload[i];
  
  if (String(topic) == TOPIC_COMMANDS) {
    if (message.indexOf("\"nominal_rpm\"") > 0) {
      int idx = message.indexOf("\"nominal_rpm\":");
      int start = message.indexOf(":", idx) + 1;
      int end = message.indexOf(",", start);
      if (end < 0) end = message.indexOf("}", start);
      nodeRedNominalRPM = message.substring(start, end).toInt();
    }
    if (message.indexOf("\"planned_time\"") > 0) {
      int idx = message.indexOf("\"planned_time\":");
      int start = message.indexOf(":", idx) + 1;
      int end = message.indexOf(",", start);
      if (end < 0) end = message.indexOf("}", start);
      nodeRedPlannedTime = message.substring(start, end).toInt();
    }
    if (message.indexOf("\"machine_name\"") > 0) {
      int idx = message.indexOf("\"machine_name\":\"");
      int start = idx + 16;
      int end = message.indexOf("\"", start);
      nodeRedMachineName = message.substring(start, end);
    }
  }
}

void connectWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
}

void connectMQTT() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    String clientId = "ESP32-TRS-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("connected!");
      client.subscribe(TOPIC_COMMANDS);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      delay(5000);
    }
  }
}

float readDistance() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  long duration = pulseIn(PIN_ECHO, HIGH, 30000);
  if (duration == 0) return 999;
  return duration * 0.034 / 2;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_POT, INPUT);
  pinMode(PIN_BTN_RED, INPUT);  // No internal pull-up (external resistor exists)
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  
  digitalWrite(PIN_TRIG, LOW);
  digitalWrite(PIN_LED_GREEN, LOW);
  digitalWrite(PIN_LED_RED, LOW);
  noTone(PIN_BUZZER);
  
  shiftStartTime = millis();
  
  connectWiFi();
  client.setServer(mqtt_broker, mqtt_port);
  client.setCallback(callback);
  
  Serial.println("========================================");
  Serial.println("TRS SYSTEM - BUTTON FIXED");
  Serial.println("Press the RED button - watch Serial Monitor");
  Serial.println("========================================");
}

void loop() {
  unsigned long now = millis();
  
  if (!client.connected()) connectMQTT();
  client.loop();
  
  // ========== READ BUTTON EVERY LOOP WITH DEBUG ==========
  int buttonReading = digitalRead(PIN_BTN_RED);
  
  // Print button state every second for debugging
  static unsigned long lastDebugPrint = 0;
  if (millis() - lastDebugPrint > 1000) {
    lastDebugPrint = millis();
    Serial.print("Button value: ");
    Serial.print(buttonReading);
    Serial.print(" | Machine: ");
    Serial.println(machineState ? "ON" : "OFF");
  }
  
  // Detect button press (when value changes to HIGH - depends on wiring)
  if (buttonReading == HIGH && lastButtonState == LOW && (millis() - lastDebounceTime) > DEBOUNCE_MS) {
    lastDebounceTime = millis();
    Serial.println("*** BUTTON CLICK DETECTED ***");
    
    if (machineState) {
      rejectCount++;
      totalCount = goodCount + rejectCount;
      
      digitalWrite(PIN_LED_RED, HIGH);
      delay(150);
      digitalWrite(PIN_LED_RED, LOW);
      tone(PIN_BUZZER, 2000, 200);
      
      Serial.print("🔴 REJECT! Bad: ");
      Serial.print(rejectCount);
      Serial.print(" | Good: ");
      Serial.print(goodCount);
      Serial.print(" | Total: ");
      Serial.println(totalCount);
    } else {
      Serial.println("Machine OFF - reject ignored");
    }
  }
  lastButtonState = buttonReading;
  
  // Read potentiometer
  int rawPot = analogRead(PIN_POT);
  float current_mA = map(rawPot, 0, 4095, 0, 5000);
  float potPercent = (rawPot / 4095.0) * 100.0;
  
  bool newMachineState = (current_mA > CURRENT_THRESHOLD);
  targetRPM = newMachineState ? map(potPercent, 10, 100, 200, nodeRedNominalRPM) : 0;
  currentRPM += (targetRPM - currentRPM) * 0.05;
  if (currentRPM < 10) currentRPM = 0;
  
  if (newMachineState != lastMachineState) {
    if (!newMachineState && lastMachineState) {
      currentDowntimeStart = now;
      Serial.println("Machine STOPPED");
    }
    if (newMachineState && !lastMachineState) {
      unsigned long downtimeDuration = (now - currentDowntimeStart) / 1000;
      totalDowntimeSeconds += downtimeDuration;
      Serial.print("Machine STARTED, +");
      Serial.print(downtimeDuration);
      Serial.println("s downtime");
    }
    lastMachineState = newMachineState;
  }
  
  unsigned long currentTotalDowntime = totalDowntimeSeconds;
  if (!newMachineState && currentDowntimeStart > 0) {
    currentTotalDowntime += (now - currentDowntimeStart) / 1000;
  }
  machineState = newMachineState;
  
  // Ultrasonic sensor
  float distance = readDistance();
  bool objectDetected = (distance > 0 && distance < DISTANCE_THRESHOLD);
  
  if (objectDetected && machineState && (now - lastObjectDetect > SENSOR_COOLDOWN)) {
    goodCount++;
    totalCount = goodCount + rejectCount;
    lastObjectDetect = now;
    digitalWrite(PIN_LED_GREEN, HIGH);
    delay(100);
    digitalWrite(PIN_LED_GREEN, LOW);
    Serial.print("🟢 GOOD! Good: ");
    Serial.print(goodCount);
    Serial.print(" | Total: ");
    Serial.println(totalCount);
  }
  
  // TRS Calculations
  float downtimeMinutes = currentTotalDowntime / 60.0;
  float TD = max(0.0, min(100.0, ((nodeRedPlannedTime - downtimeMinutes) / nodeRedPlannedTime) * 100.0));
  float TP = (machineState && nodeRedNominalRPM > 0) ? min((currentRPM / nodeRedNominalRPM) * 100.0, 120.0) : 0;
  float TQ = (totalCount > 0) ? ((float)goodCount / (float)totalCount) * 100.0 : 0;
  float TRS = (TD / 100.0) * (TP / 100.0) * (TQ / 100.0) * 100.0;
  
  // Alerts
  if (TRS > 0 && TRS < 45) {
    if ((now / 500) % 2 == 0) {
      tone(PIN_BUZZER, 1000, 100);
      digitalWrite(PIN_LED_RED, HIGH);
    } else {
      digitalWrite(PIN_LED_RED, LOW);
    }
  } else if (TRS >= 85) {
    digitalWrite(PIN_LED_GREEN, HIGH);
    digitalWrite(PIN_LED_RED, LOW);
    noTone(PIN_BUZZER);
  } else {
    noTone(PIN_BUZZER);
    digitalWrite(PIN_LED_RED, LOW);
    if (TRS < 85) digitalWrite(PIN_LED_GREEN, LOW);
  }
  
  // MQTT Send
  if (now - lastSendTime >= SEND_INTERVAL) {
    lastSendTime = now;
    
    String weakest = "TD";
    if (TP <= TD && TP <= TQ) weakest = "TP";
    if (TQ <= TD && TQ <= TP) weakest = "TQ";
    
    String json = "{";
    json += "\"td\":" + String(TD, 1) + ",";
    json += "\"tp\":" + String(TP, 1) + ",";
    json += "\"tq\":" + String(TQ, 1) + ",";
    json += "\"trs\":" + String(TRS, 1) + ",";
    json += "\"vitesse_rpm\":" + String(currentRPM, 0) + ",";
    json += "\"downtime_min\":" + String(downtimeMinutes, 1) + ",";
    json += "\"machine_state\":\"" + String(machineState ? "ON" : "OFF") + "\",";
    json += "\"good_count\":" + String(goodCount) + ",";
    json += "\"reject_count\":" + String(rejectCount) + ",";
    json += "\"total\":" + String(totalCount) + ",";
    json += "\"current_mA\":" + String(current_mA, 0) + ",";
    json += "\"distance_cm\":" + String(distance, 1) + ",";
    json += "\"machine_name\":\"" + nodeRedMachineName + "\",";
    json += "\"weakest\":\"" + weakest + "\"";
    json += "}";
    
    client.publish(TOPIC_TRS_DATA, json.c_str());
  }
  
  delay(50);
}