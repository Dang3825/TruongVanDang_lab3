
#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_BMP085.h>
#include <math.h>

// =================================================
// 1. KHAI BAO CHAN
// =================================================

#define SDA_PIN 21
#define SCL_PIN 22

#define LED_PIN 18
#define BUZZER_PIN 19

// =================================================
// 2. CAU HINH WIFI
// =================================================

const char* ssid = "Wokwi-GUEST";
const char* password = "";

// =================================================
// 3. CAU HINH MQTT
// =================================================

const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;

// Topic du lieu cam bien
const char* topic_data = "runner/data";

// Topic dieu khien
const char* topic_led = "runner/control/led";
const char* topic_buzzer = "runner/control/buzzer";

// Topic trang thai
const char* topicLedStatus = "runner/status/led";
const char* topicBuzzerStatus = "runner/status/buzzer";
const char* topicLedMode = "runner/status/led_mode";

// =================================================
// 4. KHOI TAO DOI TUONG
// =================================================

WiFiClient espClient;
PubSubClient mqtt(espClient);
Adafruit_BMP085 bmp;

// =================================================
// 5. CAU TRUC DU LIEU
// =================================================

struct SensorData {
  float temperature;
  float pressure;
  float altitude;
};

struct AlertData {
  float temperature;
  float altitude;
};

// =================================================
// 6. KHAI BAO QUEUE
// =================================================

QueueHandle_t dataQueue;
QueueHandle_t dataAlert;

// =================================================
// 7. CAC BIEN TRANG THAI
// =================================================

volatile bool manualLed = false;
volatile bool manualBuzzer = false;

volatile bool autoAlert = false;

volatile bool actualLedState = false;
volatile bool actualBuzzerState = false;

portMUX_TYPE stateMux = portMUX_INITIALIZER_UNLOCKED;

// =================================================
// 8. NGUONG CANH BAO
// =================================================

const float TEMP_MIN = 10.0;
const float TEMP_MAX = 35.0;

const float ALTITUDE_LIMIT = 5.0;

// =================================================
// 9. HAM CAP NHAT LED VA BUZZER
// =================================================

void updateOutputs() {

  portENTER_CRITICAL(&stateMux);

  actualLedState = manualLed || autoAlert;
  actualBuzzerState = manualBuzzer || autoAlert;

  digitalWrite(LED_PIN, actualLedState ? HIGH : LOW);
  digitalWrite(BUZZER_PIN, actualBuzzerState ? HIGH : LOW);

  portEXIT_CRITICAL(&stateMux);
}

// =================================================
// 10. HAM MQTT CALLBACK
// =================================================

void mqttCallback(char* topic, byte* payload, unsigned int length) {

  String message = "";

  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  message.trim();

  bool validCommand = false;

  portENTER_CRITICAL(&stateMux);

  // Chi cho phep dieu khien khi o MANUAL
  if (!autoAlert) {

    if (strcmp(topic, topic_led) == 0) {

      if (message == "ON") {
        manualLed = true;
        validCommand = true;
      }
      else if (message == "OFF") {
        manualLed = false;
        validCommand = true;
      }
    }

    else if (strcmp(topic, topic_buzzer) == 0) {

      if (message == "ON") {
        manualBuzzer = true;
        validCommand = true;
      }
      else if (message == "OFF") {
        manualBuzzer = false;
        validCommand = true;
      }
    }
  }

  actualLedState = manualLed || autoAlert;
  actualBuzzerState = manualBuzzer || autoAlert;

  bool ledStatus = actualLedState;
  bool buzzerStatus = actualBuzzerState;

  digitalWrite(LED_PIN, ledStatus ? HIGH : LOW);
  digitalWrite(BUZZER_PIN, buzzerStatus ? HIGH : LOW);

  portEXIT_CRITICAL(&stateMux);

  // Gui phan hoi trang thai ngay ve Web
  if (validCommand) {
    mqtt.publish(topicLedStatus, ledStatus ? "ON" : "OFF", true);
    mqtt.publish(topicBuzzerStatus, buzzerStatus ? "ON" : "OFF", true);
  }

  Serial.print("MQTT command: ");
  Serial.print(topic);
  Serial.print(" = ");
  Serial.println(message);
}

// =================================================
// 11. TASK DOC DU LIEU CAM BIEN - CORE 1
// =================================================

void TaskReadData(void *pvParameters) {

  SensorData sensor;
  AlertData alert;

  while (true) {

    // Doc du lieu BMP180
    sensor.temperature = bmp.readTemperature();
    sensor.pressure = bmp.readPressure() / 100.0;
    sensor.altitude = bmp.readAltitude(101325);

    if (isfinite(sensor.temperature) &&
        isfinite(sensor.pressure) &&
        isfinite(sensor.altitude) &&
        sensor.pressure > 0) {

      // Hien thi du lieu qua Serial
      Serial.println("\n========== DU LIEU RUNNER ==========");

      Serial.print("Nhiet do: ");
      Serial.print(sensor.temperature);
      Serial.println(" *C");

      Serial.print("Ap suat: ");
      Serial.print(sensor.pressure);
      Serial.println(" hPa");

      Serial.print("Do cao: ");
      Serial.print(sensor.altitude);
      Serial.println(" m");

      // Gui du lieu day du vao dataQueue
      xQueueOverwrite(dataQueue, &sensor);

      // Gui du lieu canh bao vao dataAlert
      alert.temperature = sensor.temperature;
      alert.altitude = sensor.altitude;

      xQueueOverwrite(dataAlert, &alert);

    } else {

      Serial.println("Loi doc cam bien BMP180!");
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

// =================================================
// 12. TASK CANH BAO - CORE 1
// =================================================

void TaskAlert(void *pvParameters) {

  AlertData alert;

  float previousAltitude = 0;
  bool firstSample = true;

  while (true) {

    if (xQueueReceive(dataAlert, &alert, portMAX_DELAY) == pdTRUE) {

      // Kiem tra nhiet do
      bool temperatureWarning =
        (alert.temperature < TEMP_MIN ||
         alert.temperature > TEMP_MAX);

      // Kiem tra thay doi do cao
      bool altitudeWarning = false;

      if (!firstSample) {

        float altitudeDifference =
          fabs(alert.altitude - previousAltitude);

        if (altitudeDifference > ALTITUDE_LIMIT) {
          altitudeWarning = true;
        }
      }

      previousAltitude = alert.altitude;
      firstSample = false;

      portENTER_CRITICAL(&stateMux);

      bool previousAuto = autoAlert;

      autoAlert = temperatureWarning || altitudeWarning;

    // Khi chuyển từ AUTO sang MANUAL:
    // Buộc LED tắt và xóa trạng thái LED thủ công cũ.
      if (previousAuto && !autoAlert) {
           manualLed = false;
      }

      portEXIT_CRITICAL(&stateMux);

      updateOutputs();

      Serial.println("========== CANH BAO ==========");

      Serial.print("Nhiet do: ");
      Serial.println(temperatureWarning ? "BAT THUONG" : "BINH THUONG");

      Serial.print("Do cao: ");
      Serial.println(altitudeWarning ? "THAY DOI DOT NGOT" : "BINH THUONG");

      Serial.print("Che do: ");
      Serial.println(autoAlert ? "AUTO" : "MANUAL");

      Serial.print("LED: ");
      Serial.println(actualLedState ? "ON" : "OFF");

      Serial.print("BUZZER: ");
      Serial.println(actualBuzzerState ? "ON" : "OFF");
    }
  }
}

// =================================================
// 13. KET NOI WIFI
// =================================================

void connectWiFi() {

  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("Dang ket noi WiFi");

  while (WiFi.status() != WL_CONNECTED) {

    Serial.print(".");
    vTaskDelay(pdMS_TO_TICKS(500));
  }

  Serial.println("\nWiFi ket noi thanh cong!");

  Serial.print("Dia chi IP: ");
  Serial.println(WiFi.localIP());
}

// =================================================
// 14. KET NOI MQTT
// =================================================


void reconnectMQTT() {

  static unsigned long lastAttempt = 0;

  if (mqtt.connected()) {
    return;
  }

  // Moi 2 giay moi thu ket noi lai mot lan
  if (millis() - lastAttempt < 2000) {
    return;
  }

  lastAttempt = millis();

  Serial.print("Dang ket noi MQTT...");

  String clientId = "ESP32-Runner-";
  clientId += String((uint32_t)ESP.getEfuseMac(), HEX);

  if (mqtt.connect(clientId.c_str())) {

    Serial.println("MQTT ket noi thanh cong!");

    mqtt.subscribe(topic_led);
    mqtt.subscribe(topic_buzzer);

    publishStatus();

  } else {

    Serial.print("Loi MQTT: ");
    Serial.println(mqtt.state());
  }
}

// =================================================
// 15. GUI TRANG THAI LED, BUZZER VA CHE DO
// =================================================

void publishStatus() {

  bool ledState;
  bool buzzerState;
  bool modeState;

  portENTER_CRITICAL(&stateMux);

  ledState = actualLedState;
  buzzerState = actualBuzzerState;
  modeState = autoAlert;

  portEXIT_CRITICAL(&stateMux);

  mqtt.publish(topicLedStatus, ledState ? "ON" : "OFF", true);

  mqtt.publish(topicBuzzerStatus,
               buzzerState ? "ON" : "OFF", true);

  mqtt.publish(topicLedMode,
               modeState ? "AUTO" : "MANUAL", true);
}

// =================================================
// 16. TASK TRUYEN DU LIEU - CORE 0
// =================================================

void TaskTransmitData(void *pvParameters) {

  SensorData sensor;

  unsigned long lastStatusTime = 0;
  unsigned long lastSensorTime = 0;

  mqtt.setServer(mqtt_server, mqtt_port);
  mqtt.setCallback(mqttCallback);

  while (true) {

    // 1. Kiem tra WiFi
    if (WiFi.status() != WL_CONNECTED) {
      connectWiFi();
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    // 2. Kiem tra MQTT
   if (!mqtt.connected()) {
      reconnectMQTT();
      vTaskDelay(pdMS_TO_TICKS(5));
      continue;
    }

    // 3. Xu ly MQTT lien tuc
    mqtt.loop();

    // 4. Gui du lieu cam bien
    if (xQueueReceive(dataQueue, &sensor, 0) == pdTRUE) {

      char payload[160];

      snprintf(payload, sizeof(payload),
        "{\"temperature\":%.2f,"
        "\"pressure\":%.2f,"
        "\"altitude\":%.2f}",
        sensor.temperature,
        sensor.pressure,
        sensor.altitude
      );

      mqtt.publish(topic_data, payload);
    }

    // 5. Dong bo trang thai LED va BUZZER
    if (millis() - lastStatusTime >= 200) {

      publishStatus();

      lastStatusTime = millis();
    }

    // 6. Nghi ngan de nhuong CPU
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

// =================================================
// 17. SETUP
// =================================================

void setup() {

  Serial.begin(115200);

  // Khoi tao chan
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  // Khoi tao I2C
  Wire.begin(SDA_PIN, SCL_PIN);

  // Khoi tao BMP180
  if (!bmp.begin()) {

    Serial.println("Khong tim thay cam bien BMP180!");

    while (true) {
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }

  Serial.println("BMP180 khoi tao thanh cong!");

  // Tao dataQueue
  dataQueue = xQueueCreate(1, sizeof(SensorData));

  // Tao dataAlert
  dataAlert = xQueueCreate(1, sizeof(AlertData));

  if (dataQueue == NULL || dataAlert == NULL) {

    Serial.println("Loi khoi tao Queue!");

    while (true) {
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }

  // Task doc du lieu - Core 1
  xTaskCreatePinnedToCore(
    TaskReadData,
    "TaskReadData",
    4096,
    NULL,
    2,
    NULL,
    1
  );

  // Task canh bao - Core 1
  xTaskCreatePinnedToCore(
    TaskAlert,
    "TaskAlert",
    4096,
    NULL,
    2,
    NULL,
    1
  );

  // Task truyen du lieu - Core 0
  xTaskCreatePinnedToCore(
    TaskTransmitData,
    "TaskTransmitData",
    6144,
    NULL,
    1,
    NULL,
    0
  );

  Serial.println("He thong Runner da khoi dong!");
}

// =================================================
// 18. LOOP
// =================================================

void loop() {

  vTaskDelay(pdMS_TO_TICKS(1000));
}
