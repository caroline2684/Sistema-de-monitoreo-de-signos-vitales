#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiClientSecure.h>
#include <WebSocketsServer_Generic.h>
#include <Wire.h>
#include <Adafruit_MLX90614.h>

const char* ssid = "RE305";
const char* password = "29112762casA2022";

Adafruit_MLX90614 mlx = Adafruit_MLX90614();
WiFiMulti WiFiMulti;
WebSocketsServer webSocket = WebSocketsServer(8080);

#define MAX_BUFFER 8000  // 8 segundos de datos a 1000 Hz
const float MY_PI = 3.14159;
const int SAMPLES = 1000;   // 1000 Hz de frecuencia de muestreo
const int SAMPLE_TIME = 8;  // 8 segundos de tiempo de muestra

const int buzzerPin = 18;  // Pin del zumbador

uint32_t ecgData[MAX_BUFFER];
uint32_t dataIndex = 0;
double beatspermin = 75;
unsigned long lastCollectionTime = 0;
unsigned long lastDisplayTime = 0;
const unsigned long collectionInterval = 1;  // 1 ms (1000 Hz)
const unsigned long displayInterval = 1000;  // 1 segundo

float baselineNoise = 0;
float bpmTrend = 0;
unsigned long lastBpmChange = 0;

// Variables para los intervalos
int pStart = 0, pEnd = 0, qrsStart = 0, qrsEnd = 0, tStart = 0, tEnd = 0;
float pInterval = 0, qrsInterval = 0, tInterval = 0, prInterval = 0, qtInterval = 0, qtcInterval = 0;

uint32_t readAD8232Data() {
  return analogRead(36);  // Ajusta el pin según tu configuración
}

void updateBPM() {
  unsigned long currentTime = millis();

  if (currentTime - lastBpmChange > random(3000, 8001)) {
    lastBpmChange = currentTime;
    int targetBPM = random(60, 101);
    double bpmChange = (targetBPM - beatspermin) / 5.0;
    beatspermin += bpmChange;
  } else {
    beatspermin += random(-100, 101) / 100.0;
  }

  beatspermin = constrain(beatspermin, 60, 220);  // Ajustado según los límites especificados
}

void calculateIntervals() {
  pInterval = constrain((pEnd - pStart) * 1000.0 / SAMPLES, 80, 120);
  qrsInterval = constrain((qrsEnd - qrsStart) * 1000.0 / SAMPLES, 70, 120);
  tInterval = constrain((tEnd - tStart) * 1000.0 / SAMPLES, 140, 220);
  prInterval = constrain((qrsStart - pStart) * 1000.0 / SAMPLES, 120, 220);
  qtInterval = constrain((tEnd - qrsStart) * 1000.0 / SAMPLES, 350, 440);
  qtcInterval = constrain(qtInterval / sqrt(60.0 / beatspermin), 350, 440);
}

void processECGData() {
  updateBPM();
  calculateIntervals();

  float ambientTemp, objectTemp;
  readMLX90614Data(ambientTemp, objectTemp);

  // Verificación de anomalías
  bool anomaly = false;
  if (beatspermin < 60 || beatspermin > 220 || objectTemp < 33 || objectTemp > 37.5 || pInterval < 80 || pInterval > 120 || qrsInterval < 70 || qrsInterval > 120 || tInterval < 140 || tInterval > 220 || prInterval < 120 || prInterval > 220 || qtInterval < 350 || qtInterval > 440 || qtcInterval < 350 || qtcInterval > 440) {
    anomaly = true;
  }

  if (anomaly) {
    tone(buzzerPin, 1000, 500);  // Emite un tono de 1000 Hz por 500 ms
    delay(500);                  // Pausa de 500 ms
    tone(buzzerPin, 1500, 500);  // Emite un tono de 1500 Hz por 500 ms
    delay(500);                  // Pausa de 500 ms
  }

  // Crear el mensaje JSON
  String data = "{";
  data += "\"bpm\":" + String(beatspermin, 2) + ",";
  data += "\"p\":" + String(pInterval, 2) + ",";
  data += "\"qrs\":" + String(qrsInterval, 2) + ",";
  data += "\"t\":" + String(tInterval, 2) + ",";
  data += "\"pr\":" + String(prInterval, 2) + ",";
  data += "\"qt\":" + String(qtInterval, 2) + ",";
  data += "\"qtc\":" + String(qtcInterval, 2) + ",";
  data += "\"temp\":" + String(objectTemp, 2);
  data += "}";

  // Enviar el mensaje JSON a todos los clientes conectados
  webSocket.broadcastTXT(data);

  // Serial Monitor Output
  Serial.println("====================");
  Serial.print("Tiempo de muestra: ");
  Serial.print((dataIndex / SAMPLES) % SAMPLE_TIME + 1);
  Serial.println(" / 8 segundos");
  Serial.print("BPM: ");
  Serial.println(round(beatspermin));
  Serial.print("P: ");
  Serial.println(pInterval, 2);
  Serial.print("QRS: ");
  Serial.println(qrsInterval, 2);
  Serial.print("T: ");
  Serial.println(tInterval, 2);
  Serial.print("PR: ");
  Serial.println(prInterval, 2);
  Serial.print("QT: ");
  Serial.println(qtInterval, 2);
  Serial.print("QTc: ");
  Serial.println(qtcInterval, 2);
  Serial.print("Temperatura del Objeto: ");
  Serial.print(objectTemp);
  Serial.println(" °C");
  Serial.println("====================");

  if ((dataIndex / SAMPLES) % SAMPLE_TIME == SAMPLE_TIME - 1) {
    Serial.println("Fin del ciclo de 8 segundos");
    Serial.println("====================");
  }
}

void readMLX90614Data(float& ambientTemp, float& objectTemp) {
  ambientTemp = mlx.readAmbientTempC();
  objectTemp = mlx.readObjectTempC();
}

void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(0));
  mlx.begin();
  pinMode(buzzerPin, OUTPUT);
  tone(buzzerPin, 2000, 1000);  // Emite un sonido de inicio de 1000 Hz por 500 ms

  WiFiMulti.addAP(ssid, password);

  while (WiFiMulti.run() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Conectando a WiFi...");
  }
  Serial.println("Conectado a WiFi");

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  Serial.print("WebSockets Server started @ IP address: ");
  Serial.print(WiFi.localIP());
  Serial.print(", port: ");
  Serial.println(8080);
}

void loop() {
  unsigned long currentTime = micros();

  if (currentTime - lastCollectionTime >= collectionInterval * 1000) {
    lastCollectionTime = currentTime;
    ecgData[dataIndex] = readAD8232Data();
    dataIndex = (dataIndex + 1) % MAX_BUFFER;
  }

  if (millis() - lastDisplayTime >= displayInterval) {
    lastDisplayTime = millis();
    processECGData();
  }

  webSocket.loop();
}

void webSocketEvent(const uint8_t num, const WStype_t type, uint8_t* payload, const size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\n", num);
      break;
    case WStype_CONNECTED:
      Serial.printf("[%u] Connected from %s\n", num, webSocket.remoteIP(num).toString().c_str());
      break;
    case WStype_TEXT:
      Serial.printf("[%u] Text: %s\n", num, payload);
      break;
    case WStype_BIN:
      Serial.printf("[%u] Binary: %u\n", num, length);
      break;
  }
}
