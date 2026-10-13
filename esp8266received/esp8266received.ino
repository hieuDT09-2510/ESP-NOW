#include <espnow.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ===== CẤU HÌNH =====
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define BUZZER        D5
#define LED           D6

#define WIFI_SSID     ""
#define WIFI_PASS     ""
#define TB_SERVER     "thingsboard.cloud"
#define TB_TOKEN      ""
#define TB_TOPIC      "v1/devices/me/telemetry"
#define TB_PORT       1883

#define GAS_THRESHOLD     600
#define BLINK_INTERVAL_MS 300
#define SEND_INTERVAL_MS  5000   // Gửi ThingsBoard tối đa 1 lần/5s
#define MQTT_RETRY_DELAY  2000
#define LOOP_DELAY        50

// ===== OBJECTS =====
WiFiClient espClient;
PubSubClient mqttClient(espClient);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ===== DATA STRUCT =====
typedef struct {
  float temperature;
  float humidity;
  int   gas;
  int   light;
} SensorData;

SensorData sensorData;
volatile bool newData = false;

// ===== TRẠNG THÁI =====
unsigned long lastBlink    = 0;
unsigned long lastSendTime = 0;
bool blinkState = false;

// ===== WIFI =====
void setupWifi() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Dang ket noi WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi OK: " + WiFi.localIP().toString());
}

// ===== MQTT =====
void reconnectMQTT() {
  if (mqttClient.connected()) return;  // tránh gọi thừa
  Serial.print("Ket noi ThingsBoard...");
  if (mqttClient.connect("ESP8266_Receiver", TB_TOKEN, NULL)) {
    Serial.println("OK");
  } else {
    Serial.printf("Fail rc=%d, thu lai sau...\n", mqttClient.state());
    delay(MQTT_RETRY_DELAY);
  }
}

// ===== GỬI DỮ LIỆU =====
void sendToThingsBoard() {
  unsigned long now = millis();
  if (now - lastSendTime < SEND_INTERVAL_MS) return;  // rate-limit
  lastSendTime = now;

  char payload[128];
  snprintf(payload, sizeof(payload),
    "{\"temperature\":%.1f,\"humidity\":%.1f,\"gas\":%d,\"light\":%d}",
    sensorData.temperature, sensorData.humidity,
    sensorData.gas, sensorData.light
  );

  if (mqttClient.publish(TB_TOPIC, payload)) {
    Serial.println("Da gui ThingsBoard: " + String(payload));
  } else {
    Serial.println("Gui that bai!");
  }
}

// ===== ESP-NOW CALLBACK =====
void ICACHE_RAM_ATTR OnDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len) {
  if (len == sizeof(SensorData)) {
    memcpy(&sensorData, incomingData, sizeof(SensorData));
    newData = true;
  }
}

// ===== OLED =====
void updateDisplay() {
  display.clearDisplay();

  // Nhiệt độ (to)
  display.setTextSize(3);
  display.setTextColor(WHITE);
  display.setCursor(10, 0);
  display.print(sensorData.temperature, 1);
  display.setTextSize(1);
  display.setCursor(100, 5);
  display.print("C");

  // Độ ẩm (to)
  display.setTextSize(3);
  display.setCursor(10, 30);
  display.print(sensorData.humidity, 1);
  display.setTextSize(1);
  display.setCursor(100, 40);
  display.print("%RH");

  // Cảnh báo gas (nhấp nháy)
  if (sensorData.gas > GAS_THRESHOLD && blinkState) {
    display.setCursor(0, 55);
    display.print("!!! GAS ALERT !!!");
  }

  display.display();
}

// ===== XỬ LÝ GAS =====
void handleGas() {
  // Buzzer active-LOW: HIGH = tắt, LOW = bật
  digitalWrite(BUZZER, (sensorData.gas > GAS_THRESHOLD && blinkState) ? LOW : HIGH);
}

// ===== XỬ LÝ ĐÈN =====
void handleLight() {
  digitalWrite(LED, sensorData.light == 1 ? HIGH : LOW);
}

// ===== DEBUG =====
void debugSerial() {
  Serial.printf("T: %.1f | H: %.1f | Gas: %d | Light: %d\n",
    sensorData.temperature, sensorData.humidity,
    sensorData.gas, sensorData.light);
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);

  pinMode(BUZZER, OUTPUT);
  pinMode(LED, OUTPUT);
  digitalWrite(BUZZER, HIGH);  // Tắt buzzer ban đầu
  digitalWrite(LED, LOW);

  setupWifi();
  mqttClient.setServer(TB_SERVER, TB_PORT);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED loi! Dung lai.");
    while (true) delay(1000);
  }
  display.clearDisplay();
  display.display();

  if (esp_now_init() != 0) {
    Serial.println("ESP-NOW Init that bai!");
    return;
  }
  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  esp_now_register_recv_cb(OnDataRecv);

  Serial.println("Setup xong!");
}

// ===== LOOP =====
void loop() {
  // MQTT keepalive
  if (!mqttClient.connected()) reconnectMQTT();
  mqttClient.loop();

  // Blink timer
  unsigned long now = millis();
  if (now - lastBlink >= BLINK_INTERVAL_MS) {
    lastBlink = now;
    blinkState = !blinkState;
  }

  // Xử lý dữ liệu mới
  if (newData) {
    newData = false;
    debugSerial();
    sendToThingsBoard();
  }

  handleGas();
  handleLight();
  updateDisplay();

  delay(LOOP_DELAY);
}