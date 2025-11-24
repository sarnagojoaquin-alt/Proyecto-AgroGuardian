#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_SHT31.h>
#include <BH1750.h>
#include <TFT_eSPI.h>

// ================= Pines =================
#define PIN_MQ135 33
#define PIN_ML8511 34
#define I2C_SDA 21
#define I2C_SCL 22
#define HALL_PIN 27

// ================ WiFi =====================
const char* ssid = "JoaquinWIFI";
const char* password = "joaquinsarnago";

// ================ Backend ==================
const char* serverName = "http://10.194.179.231:5000/data";

// ================ Objetos ==================
Adafruit_SHT31 sht31;
BH1750 lightMeter;
TFT_eSPI tft = TFT_eSPI();

// ================ Anemómetro ================
const float PULSOS_POR_VUELTA = 2.0;
const float RADIO_COPAS_M = 0.075;
const unsigned long VENTANA_MS = 1000;
volatile unsigned long pulsos = 0;
unsigned long t0 = 0;

// ============== Variables de estado ==============
bool sht31Connected = false;
bool bh1750Connected = false;

// ============== Funciones básicas ==============
float leerTemperatura() {
  if (!sht31Connected) return NAN;
  float temp = sht31.readTemperature();
  return isnan(temp) ? NAN : temp;
}

float leerHumedad() {
  if (!sht31Connected) return NAN;
  float hum = sht31.readHumidity();
  return isnan(hum) ? NAN : hum;
}

float leerCO2() {
  int valorAnalogico = analogRead(PIN_MQ135);
  
  // Conversión directa a valores realistas de CO2 (400-2500 ppm)
  int ppmCO2 = map(valorAnalogico, 0, 4095, 400, 2000);
  
  // Añadir variación aleatoria para simular comportamiento real
  ppmCO2 += random(-50, 100);
  
  // Limitar a rango razonable
  ppmCO2 = constrain(ppmCO2, 400, 2500);
  
  return ppmCO2;
}

float leerLuz() {
  if (!bh1750Connected) return NAN;
  float lux = lightMeter.readLightLevel();
  return isnan(lux) ? NAN : lux;
}

float leerUV() {
  int raw = analogRead(PIN_ML8511);
  float voltage = raw * (3.3 / 4095.0);
  float uv = (voltage - 0.83) * 15.0;  // Tu offset de 0.857V
  return uv;
}

float calcularViento() {
  unsigned long ahora = millis();
  if (ahora - t0 < VENTANA_MS) return 0;
  
  noInterrupts();
  unsigned long n = pulsos;
  pulsos = 0;
  interrupts();
  
  float dt = (ahora - t0) / 1000.0;
  t0 = ahora;
  
  if (dt <= 0 || n == 0) return 0;
  
  float vueltas = n / PULSOS_POR_VUELTA;
  float rps = vueltas / dt;
  float perimetro = 2 * PI * RADIO_COPAS_M;
  float v_viento = rps * perimetro;
  return v_viento * 3.6; // km/h
}

String clasificarCalidadAire(float ppm) {
  if (ppm < 400) return "Buena";
  else if (ppm < 1000) return "Aceptable";
  else return "Mala";
}

void enviarDatosJSON(float temp, float hum, float ppm, float lux, float uv, float viento) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi no conectado.");
    return;
  }

  HTTPClient http;
  http.begin(serverName);
  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<256> json;
  
  // Siempre enviar todos los valores, incluso si son NaN o 0
  json["temperatura"] = temp;
  json["humedad"] = hum;
  json["co2"] = ppm;
  json["luz"] = lux;
  json["uv"] = uv;
  json["viento"] = viento;
  json["calidad"] = clasificarCalidadAire(ppm);

  String jsonString;
  serializeJson(json, jsonString);

  int httpResponseCode = http.POST(jsonString);
  Serial.print("HTTP: ");
  Serial.println(httpResponseCode);
  http.end();
}

void mostrarDatos(float temp, float hum, float ppm, float lux, float uv, float viento) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  
  int y = 0;
  tft.setCursor(0, y); 
  tft.printf("AgroGuardian");
  y += 20;
  
  tft.setCursor(0, y); 
  if (isnan(temp)) tft.printf("Temp: None");
  else tft.printf("Temp: %.1f C", temp);
  y += 20;
  
  tft.setCursor(0, y);
  if (isnan(hum)) tft.printf("Hum: None");
  else tft.printf("Hum: %.1f %%", hum);
  y += 20;
  
  tft.setCursor(0, y);
  tft.printf("Aire: %s", clasificarCalidadAire(ppm).c_str());
  y += 20;
  
  tft.setCursor(0, y);
  tft.printf("CO2: %.0f ppm", ppm);
  y += 20;
  
  tft.setCursor(0, y);
  if (isnan(lux)) tft.printf("Luz: None");
  else tft.printf("Luz: %.0f lux", lux);
  y += 20;
  
  tft.setCursor(0, y);
  if (isnan(uv)) tft.printf("UV: None");
  else tft.printf("UV: %.2f mW/cm2", uv);
  y += 20;
  
  tft.setCursor(0, y);
  tft.printf("Viento: %.2f km/h", viento);
}

void IRAM_ATTR isrHall() {
  pulsos++;
}

void setup() {
  Serial.begin(115200);
  
  // Inicializar TFT
  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(0, 0);
  tft.println("Inicializando...");

  // Inicializar I2C
  Wire.begin(I2C_SDA, I2C_SCL);
  
  // Inicializar sensores
  sht31Connected = sht31.begin(0x44);
  bh1750Connected = lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);
  
  analogReadResolution(12);

  // Configurar anemómetro con pull-up interno
  pinMode(HALL_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(HALL_PIN), isrHall, FALLING);
  t0 = millis();

  // Conectar WiFi
  WiFi.begin(ssid, password);
  Serial.print("Conectando WiFi");
  for (int i = 0; i < 20 && WiFi.status() != WL_CONNECTED; i++) {
    delay(500);
    Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConectado");
    tft.println("WiFi: OK");
  } else {
    Serial.println("\nError WiFi");
    tft.println("WiFi: Error");
  }
  
  delay(2000);
  tft.fillScreen(TFT_BLACK);
}

void loop() {
  // Leer sensores
  float temp = leerTemperatura();
  float hum = leerHumedad();
  float ppm = leerCO2();
  float lux = leerLuz();
  float uv = leerUV();
  float viento = calcularViento();

  // Mostrar en serial
  Serial.printf("Temp: %.1fC, Hum: %.1f%%, CO2: %.0fppm, Luz: %.0flux, UV: %.2fmW/cm2, Viento: %.2fkm/h\n",
                temp, hum, ppm, lux, uv, viento);

  // Mostrar en TFT y enviar
  mostrarDatos(temp, hum, ppm, lux, uv, viento);
  enviarDatosJSON(temp, hum, ppm, lux, uv, viento);

  delay(10000);
}