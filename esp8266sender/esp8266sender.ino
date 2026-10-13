#include <espnow.h>
#include <ESP8266WiFi.h>
#include <DHT.h>

// ===== CẤU HÌNH =====
#define DHTPIN        D1
#define DHTTYPE       DHT11
#define MQ2PIN        A0
#define LDRPIN        D8

#define PREHEAT_MS    30000
#define NUM_SAMPLES   10
#define SEND_DELAY    2000

// ===== MAC THIẾT BỊ NHẬN =====
uint8_t MAC[] = {0x58, 0xBF, 0x25, 0xC4, 0xAD, 0xCF};

// ===== STRUCT =====
typedef struct {
  float temperature;
  float humidity;
  int   gas;
  int   light;
} SensorData;

SensorData myData;
DHT dht(DHTPIN, DHTTYPE);

// ===== ĐỌC GAS CÓ LỌC NHIỄU =====
int readGasFiltered() {
  long sum = 0;
  for (int i = 0; i < NUM_SAMPLES; i++) {
    sum += analogRead(MQ2PIN);
    delay(5);
  }
  return (int)(sum / NUM_SAMPLES);
}

// ===== CALLBACK GỬI =====
void OnDataSent(uint8_t *mac_addr, uint8_t status) {
  Serial.println(status == 0 ? "Gui: OK" : "Gui: FAIL");
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  dht.begin();
  pinMode(LDRPIN, INPUT);  // Module đã có mạch kéo sẵn

  // Chờ MQ2 làm nóng
  Serial.println("Cho MQ2 on dinh...");
  for (int i = PREHEAT_MS / 1000; i > 0; i--) {
    Serial.printf("  Con %d giay...\n", i);
    delay(1000);
  }
  Serial.println("MQ2 san sang!");

  // Khởi tạo ESP-NOW
  if (esp_now_init() != 0) {
    Serial.println("ESP-NOW Init that bai!");
    return;
  }
  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
  esp_now_add_peer(MAC, ESP_NOW_ROLE_SLAVE, 1, NULL, 0);
  esp_now_register_send_cb(OnDataSent);
}

// ===== LOOP =====
void loop() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (isnan(t) || isnan(h)) {
    Serial.println("Doc DHT loi, thu lai...");
    delay(2000);
    return;
  }

  myData.temperature = t;
  myData.humidity    = h;
  myData.gas         = readGasFiltered();
  myData.light       = digitalRead(LDRPIN); // Nếu LED sai logic thì đổi thành !digitalRead(LDRPIN)

  // Gửi ESP-NOW
  uint8_t result = esp_now_send(MAC, (uint8_t*)&myData, sizeof(myData));
  Serial.println(result == 0 ? "ESP-NOW: Confirmed" : "ESP-NOW: Error");

  // Debug
  Serial.printf("T: %.1f | H: %.1f | Gas: %d | Light: %d\n",
    myData.temperature, myData.humidity,
    myData.gas, myData.light);
  Serial.println("---");

  delay(SEND_DELAY);
}