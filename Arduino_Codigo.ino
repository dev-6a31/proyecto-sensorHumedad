#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <time.h>

// 1. Red Wi-Fi
const char* ssid = "Fastnet-103105_2.4";
const char* password = "76121105";

// 2. Broker MQTT
const char* mqtt_server = "192.168.1.6";
const int mqtt_port = 8883;

// Credenciales creadas con mosquitto_passwd
const char* mqtt_user = "user_230112174";
const char* mqtt_password = "ProyInt#123!";

// Certificado CA generado por el servidor
const char* ca_cert = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDLTCCAhWgAwIBAgIUf2KkUrN3iehHN0N/cmEh4nLYz9YwDQYJKoZIhvcNAQEL
BQAwPjELMAkGA1UEBhMCTVgxGDAWBgNVBAoMD1Byb3llY3RvIDYgTVFUVDEVMBMG
A1UEAwwMUHJveWVjdG82LUNBMB4XDTI2MTAwOTAwMjYwNVoXDTM2MTAwNzAwMjYw
NVowPjELMAkGA1UEBhMCTVgxGDAWBgNVBAoMD1Byb3llY3RvIDYgTVFUVDEVMBMG
A1UEAwwMUHJveWVjdG82LUNBMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKC
AQEAt8j9yuMKTlu92QxJDFP10uUiaeNM1/y/6KuOYW4bNAkBo47qyAoLeJ22yX+4
60l0gUt4k2sOhLBQ7X6fSFk3LHb1yd2ZlJujg0EjaAHyQl4gLrvqTniusXIeLcQ3
BdT+3ZWii7TnKX008RHrkNhhwYDA/b9sBIDePzS4xpICKX6aVbAzZI8tEBTV00b6
N1QcUnG6g5x6GZFgyoCTUf5Q1LaBAHDQSG1C1vXOauok6G14hLUo2xO0wVMZb776
DdfVTwtmh5UCW9/nho0TyxnUSoae/j2QH06MMQOhM+m2UGmL+lrMbXVPG28XiUBC
PxTyBqh0e0y5UW1s6yvwRZujYQIDAQABoyMwITAPBgNVHRMBAf8EBTADAQH/MA4G
A1UdDwEB/wQEAwIBhjANBgkqhkiG9w0BAQsFAAOCAQEAk/Jn4vShHB73iTe7uObf
v/aNq1Uk+7vPSB/uufsZvqqmNa6Z7n3AhN/Ywb+XB1OEkE7fiGXL8mOagLwgioMF
kdsFVmLrzgsH+sp1AXzwLtVqiyVVmZLoc4b2m989KEOkqTEMwUm4PY5LX8nBhf69
IcHqOXDqe6a/TqYXCa/Dq5jhR4MS5l+T1BmETZhewHuhZBZdDymgO2ABREfdUTIk
rlX9WZpyYbB2+fkO6JXkycZ/mhmnQF2qIJc1qYvI+bp+mCjGCGzOlc9/3S3fFQbz
wkXpnUSTpRIT/x8VmpfPxuxMkcWITM0F778t022lDj53gjddAckQkrV81B17j8xC
4g==
-----END CERTIFICATE-----
)EOF";

// Topics MQTT
const char* topic_sub = "privado/230112174/control";
const char* topic_hum = "privado/230112174/humedad";
const char* topic_bomba = "privado/230112174/bomba/estado";

// 3. Pines
const int LED_PIN = 2;
const int SENSOR_PIN = 34;
const int RELE_PIN = 26;

// Calibracion del sensor HL-01
const int VALOR_SECO = 4095;
const int VALOR_MOJADO = 1400;

// Estado del sensor
bool sensorActivado = false;

WiFiClientSecure espClient;
PubSubClient client(espClient);

unsigned long ultimoEnvio = 0;

// Control de mensajes MQTT
void callback(char* topic, byte* payload, unsigned int length) {
  String mensaje = "";

  for (unsigned int i = 0; i < length; i++) {
    mensaje += (char)payload[i];
  }

  mensaje.trim();

  Serial.print("Mensaje recibido: ");
  Serial.println(mensaje);

  if (mensaje == "ON") {
    sensorActivado = true;
    digitalWrite(LED_PIN, HIGH);

    Serial.println("-> Sensor y LED ACTIVADOS");

  } else if (mensaje == "OFF") {
    sensorActivado = false;
    digitalWrite(LED_PIN, LOW);
    digitalWrite(RELE_PIN, LOW);

    client.publish(topic_bomba, "🔴 APAGADA");

    Serial.println("-> Sensor, LED y bomba DESACTIVADOS");
  }
}

// Sincronizar hora para TLS
void sincronizarHora() {
  Serial.println("Sincronizando hora para TLS...");

  configTime(
    0,
    0,
    "pool.ntp.org",
    "time.nist.gov"
  );

  struct tm timeinfo;

  while (!getLocalTime(&timeinfo)) {
    delay(500);
  }

  Serial.println("Hora sincronizada correctamente.");
}

// Reconectar con MQTT TLS
void reconectarMQTT() {
  while (!client.connected()) {

    Serial.print("Conectando a MQTT TLS por puerto 8883...");

    String clientId =
      "ESP32_Tilin_" + String(random(0xffff), HEX);

    if (client.connect(
          clientId.c_str(),
          mqtt_user,
          mqtt_password
        )) {

      Serial.println(" ¡Conectado mediante TLS!");

      client.subscribe(topic_sub);

      Serial.print("Suscrito a: ");
      Serial.println(topic_sub);

    } else {
      Serial.print(" Fallo rc=");
      Serial.print(client.state());

      Serial.println(" reintentando en 5 segundos...");

      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  pinMode(SENSOR_PIN, INPUT);
  pinMode(RELE_PIN, OUTPUT);

  digitalWrite(LED_PIN, LOW);
  digitalWrite(RELE_PIN, LOW);

  // Conexion Wi-Fi
  Serial.print("Conectando a Wi-Fi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("Wi-Fi conectado.");

  Serial.print("IP del ESP32: ");
  Serial.println(WiFi.localIP());

  // Sincronizar hora para validar certificado TLS
  sincronizarHora();

  // Cargar certificado de seguridad
  espClient.setCACert(ca_cert);

  Serial.println("Certificado CA cargado.");
  Serial.println("TLS habilitado.");
  Serial.println("Puerto MQTT seguro: 8883");

  // Configuracion MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconectarMQTT();
  }

  client.loop();

  // Leer el sensor y controlar la bomba
  if (sensorActivado) {

    int lecturaCruda = analogRead(SENSOR_PIN);

    // Encender la bomba dependiendo de la humedad
    if (lecturaCruda <= VALOR_MOJADO) {
      digitalWrite(RELE_PIN, HIGH);
    } else {
      digitalWrite(RELE_PIN, LOW);
    }

    // Enviar humedad cada 5 segundos
    if (millis() - ultimoEnvio >= 5000) {
      ultimoEnvio = millis();

      int porcentaje = map(
        lecturaCruda,
        VALOR_SECO,
        VALOR_MOJADO,
        0,
        100
      );

      porcentaje = constrain(porcentaje, 0, 100);

      char humStr[8];
      itoa(porcentaje, humStr, 10);

      client.publish(topic_hum, humStr);

      Serial.print("Lectura D34 simulada: ");
      Serial.print(lecturaCruda);

      Serial.print(" -> Humedad enviada por TLS: ");
      Serial.print(humStr);
      Serial.println("%");

      if (lecturaCruda <= VALOR_MOJADO) {

        Serial.println("Bomba APAGADA");

        client.publish(
          topic_bomba,
          "🔴 APAGADA"
        );

      } else {

        Serial.println("Bomba ENCENDIDA");

        client.publish(
          topic_bomba,
          "🟢 ENCENDIDA"
        );
      }
    }

  } else {
    digitalWrite(RELE_PIN, LOW);
  }
}