#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <DHT.h>

// Configuración del DHT11
#define DHTPIN 2 // Pin D4 en NodeMCU
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Pines para el sensor de humedad del suelo y el relé
int sensorHumedadSuelo = A0; // Pin analógico A0
int pinRele = 5;  // Pin GPIO 5 (D1 en NodeMCU) para el relé

// Datos de la red Wi-Fi
const char* ssid = "CLARO1_6D6CBD";     // Cambia por el nombre de tu red WiFi
const char* password = "42161OESzc";   // Cambia por tu contraseña WiFi

// URL del endpoint PHP
const char* serverName = "http://35.172.116.139/api/sensor";

// Umbrales para el control de los sensores
int umbralHumedadSueloBajo = 400;  // Suelo seco (activar bomba)
int umbralHumedadSueloAlto = 550;  // Suelo suficientemente húmedo (desactivar bomba)
int umbralTemperaturaAlta = 25;    // Temperatura alta (activar bomba)
int umbralTemperaturaBaja = 18;    // Temperatura baja (desactivar bomba)
int umbralHumedadAmbienteBaja = 30;  // Humedad ambiente baja (activar bomba)
int umbralHumedadAmbienteAlta = 70;  // Humedad ambiente alta (desactivar bomba)

// Variables para los datos de los sensores
int valorHumedadSuelo;
float temperatura;
float humedadAmbiente;

// Variables de temporización
unsigned long tiempoUltimaLectura = 0;
unsigned long tiempoUltimoEnvio = 0;
unsigned long tiempoBombaActivada = 0;
const unsigned long intervaloLectura = 10000;  // 30 segundos
const unsigned long intervaloEnvio = 30000;    // 30 segundos
const unsigned long duracionBombaActiva = 60000;  // La bomba se activa por 30 segundos

// Bandera para indicar si la bomba está activa
bool bombaEncendida = false;

// Función para conectarse a la red WiFi
void conectarWiFi() {
  WiFi.begin(ssid, password);
  Serial.println("Conectando a WiFi...");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nConectado a WiFi");
}

// Función para leer los sensores
void leerSensores() {
  // Leer humedad del suelo (YL-69)
  valorHumedadSuelo = analogRead(sensorHumedadSuelo);
  
  // Leer temperatura y humedad del ambiente (DHT11)
  temperatura = dht.readTemperature();
  humedadAmbiente = dht.readHumidity();

  // Imprimir los valores en el monitor serial
  Serial.print("Humedad del suelo: ");
  Serial.println(valorHumedadSuelo);
  
  Serial.print(F("%  Temperatura: "));
  Serial.print(temperatura);
  Serial.println(F("°C"));
  
  Serial.print("Humedad del ambiente: ");
  Serial.print(humedadAmbiente);
  Serial.println(" %");
}

// Separar las condiciones de cada sensor para activar la bomba
void controlarBomba() {
  // Verificar si la bomba ya ha estado activa por más tiempo del permitido
  if (bombaEncendida && ( valorHumedadSuelo < 550 )) {
    digitalWrite(pinRele, HIGH);
    bombaEncendida = false;
    Serial.println("Bomba DESACTIVADA automáticamente por condiciones del ambiente o humedad del suelo.");
  }

  // Evaluar si el sensor de humedad del suelo requiere activar la bomba
  if (!bombaEncendida && valorHumedadSuelo >= 550) {
    digitalWrite(pinRele, LOW);
    bombaEncendida = true;
    Serial.println("Bomba ACTIVADA por humedad del suelo.");
  }

  // Evaluar si el sensor de temperatura ambiente requiere activar la bomba
  if (!bombaEncendida && temperatura >= 25) {
    digitalWrite(pinRele, LOW);
    bombaEncendida = true;
    Serial.println("Bomba ACTIVADA por temperatura alta.");
  }

}

// Función para enviar datos al servidor en formato JSON
void enviarDatos() {
  if(WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    WiFiClient client;

    // Comenzar la solicitud HTTP con WiFiClient y la URL
    http.begin(client, serverName);  // Cambiar la línea obsoleta

    http.addHeader("Content-Type", "application/json");  // Tipo de contenido JSON

    // Crear la estructura JSON con los nombres correctos
    String jsonData = "{\"humedadSuelo\":" + String(valorHumedadSuelo) + 
                      ", \"temperatura\":" + String(temperatura, 1) +   // 1 decimal para la temperatura
                      ", \"humedadAmbiente\":" + String(humedadAmbiente, 1) + 
                      ", \"bombaActivada\":" + String(digitalRead(pinRele) == HIGH ? "true" : "false") + "}";

    // Enviar solicitud POST con el JSON
    int httpResponseCode = http.POST(jsonData);

    // Imprimir la respuesta del servidor
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("Respuesta del servidor: " + response);
    } else {
      Serial.println("Error en la solicitud POST");
    }

    // Finalizar la conexión HTTP
    http.end();
  } else {
    Serial.println("Error en la conexión WiFi");
  }
}

void setup() {
  // Iniciar comunicación serial
  Serial.begin(9600);
  // Iniciar el sensor DHT11
  dht.begin();
  // Definir el pin del relé como salida
  pinMode(pinRele, OUTPUT);
  // Inicialmente la bomba está apagada
  digitalWrite(pinRele, LOW);
  // Conectar a la red WiFi
  conectarWiFi();
}

void loop() {
  unsigned long tiempoActual = millis();
  if(tiempoActual - tiempoUltimaLectura >= intervaloLectura){
    leerSensores();
    tiempoUltimaLectura = tiempoActual;
  }
  controlarBomba();
  // Verificar si es tiempo de enviar datos (cada 30 segundos)
  if (tiempoActual - tiempoUltimoEnvio >= intervaloEnvio) {
    enviarDatos();
    tiempoUltimoEnvio = tiempoActual;  // Actualizar tiempo de último envío
  }
}