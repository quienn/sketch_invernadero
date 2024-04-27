#include <HTTPClient.h>
#include <DHT.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <cmath>

#define WIFI_SSID "ESP32"
#define WIFI_PASSWORD ""
#define GREENHOUSE_ID 1

#define SOILPIN 34
#define DHTPIN 22
#define PUMPOUT 33

#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define API_URL "http://192.168.80.216:3000/api/greenhouses"

unsigned long previousMillis = 0;
unsigned long currentMillis = 0;
int interval = 20 * 1000;

JsonDocument doc;
HTTPClient http;

void setup()
{
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  // Espera a que la conexión a Wi-Fi se establezca
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(100);
  }

  Serial.println("Conectado a Wi-Fi!");
  Serial.print("IP:");
  Serial.println(WiFi.localIP());

  // Configura los pines de entrada y salida
  pinMode(PUMPOUT, OUTPUT);

  digitalWrite(PUMPOUT, HIGH);

  // Inicializa el sensor DHT11
  dht.begin();
}

// Función para regar el invernadero
void waterNow()
{
  Serial.println("Regando invernadero...");
  digitalWrite(PUMPOUT, LOW);
  delay(5000);
  digitalWrite(PUMPOUT, HIGH);
}

void loop()
{
  currentMillis = millis();

  doc.clear();

  float h = dht.readHumidity();    // Humedad del Ambiente
  float t = dht.readTemperature(); // Temperatura
  float f = dht.readTemperature(true);
  float hif = dht.computeHeatIndex(f, h);
  float hic = dht.computeHeatIndex(t, h, false);

  if (isnan(h) || isnan(t) || isnan(f))
  {
    Serial.println("Error obteniendo los datos del sensor DHT11");
    return;
  }

  int soilValue = analogRead(SOILPIN);
  int m = map(soilValue, 4095, 0, 0, 100); // Nivel (porcentaje) de humedad del suelo

  doc["id"] = GREENHOUSE_ID;
  doc["humidity"] = h;
  doc["temperature"] = round(t);
  doc["moisture"] = m;

  // Petición HTTP PUT enviando los datos del invernadero
  if (WiFi.status() == WL_CONNECTED)
  {
    http.begin(API_URL);

    String payload;
    serializeJson(doc, payload);

    int httpResponseCode = http.PUT(payload);

    if (httpResponseCode > 0)
    {
      if (currentMillis - previousMillis >= interval == true)
      {
        String response = http.getString();

        doc.clear();
        deserializeJson(doc, response);

        bool shouldAutoWater = doc["data"]["shouldAutoWater"].as<bool>();

        if (shouldAutoWater == true && m < 20)
        {
          waterNow();
        }
        else
        {
          Serial.println("[INFO] No se riega invernadero, opción en el servidor desactivada.");
        }

        previousMillis = currentMillis;
      }

      Serial.println("Petición HTTP exitosa");
    }
    else
    {
      Serial.print("Error en la solicitud HTTP: ");
      Serial.println(httpResponseCode);
    }

    http.end();
  }
  else
  {
    Serial.println("Error de conexión a Wi-Fi");
  }

  delay(1000);
}