#define TINY_GSM_MODEM_SIM800 // Definimos el tipo de módem para TinyGSM
#include <TinyGsmClient.h>
#include <PubSubClient.h>

// Definicir de pines y hardware
const int RXD2 = 16;
const int TXD2 = 17;
const int PIN_LED = 4;

// Configurar de Usuario y Credenciales Telcel
const String NUMERO_CELULAR = "+523122011685"; 
const String MENSAJE_SMS = "Prueba ESP32 - Actividad VII: nensaje";

const char apn[] = "internet.itelcel.com";
const char user[] = "webgprs";
const char pass[] = "webgprs2002";

// Configurar el Broker MQTT
const char broker[] = "broker.hivemq.com";
const int puertoMQTT = 1883;
const char topicLED[] = "Equipo2/led";

// Instancias de los objetos de red y cliente MQTT
HardwareSerial sim800Serial(2);
TinyGsm modem(sim800Serial);
TinyGsmClient client(modem);
PubSubClient mqtt(client);

// Declaración de funciones
void enviarComandoAT(String comando, const int tiempoEspera);
void callbackMQTT(char* topic, byte* payload, unsigned int length);
void conectarMQTT();

void setup() {
  Serial.begin(115200);
  sim800Serial.begin(115200, SERIAL_8N1, RXD2, TXD2);
  
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);

  delay(3000); 
  Serial.println("\n--- Iniciando Secuencia Autónoma GSM ---");

  // Configurar SMS en modo texto
  enviarComandoAT("AT+CMGF=1", 1000);

  // Enviar SMS
  Serial.println("Enviando SMS...");
  sim800Serial.print("AT+CMGS=\"");
  sim800Serial.print(NUMERO_CELULAR);
  sim800Serial.println("\"");
  delay(1000);
  sim800Serial.print(MENSAJE_SMS);
  delay(100);
  sim800Serial.write(26); // Ctrl+Z
  
  delay(5000); 
  while(sim800Serial.available()) {
    Serial.write(sim800Serial.read());
  }

  // Realizar Llamada
  Serial.println("\nRealizando llamada de voz...");
  String comandoLlamada = "ATD" + NUMERO_CELULAR + ";";
  enviarComandoAT(comandoLlamada, 1000);

  // Corte programado
  Serial.println("Esperando 15 segundos antes de colgar...");
  delay(15000); 
  
  Serial.println("Colgando llamada...");
  enviarComandoAT("ATH", 1000);

  // FASE MQTT / GPRS 
  Serial.println("\n--- Iniciando Conexión GPRS y Broker MQTT ---");
  
  modem.restart();
  
  // Esperar a que la red celular esté lista tras el reinicio para evitar errores GPRS
  Serial.println("Esperando red celular...");
  if (!modem.waitForNetwork()) {
    Serial.println("No se pudo registrar en la red");
    return;
  }
  Serial.println("¡Red celular lista!");

  Serial.print("Conectando a APN: ");
  Serial.println(apn);
  if (!modem.gprsConnect(apn, user, pass)) {
    Serial.println("Fallo en la conexión GPRS");
    return;
  }
  Serial.println("¡Conectado con éxito!");

  IPAddress localIP = modem.localIP();
  Serial.print("IP Local asignada por Telcel: ");
  Serial.println(localIP);
  
  Serial.println("[GPRS] Estabilizando canal de red (5 segundos)...");
  delay(5000);

  // Configurar cliente MQTT
  mqtt.setServer(broker, puertoMQTT);
  mqtt.setCallback(callbackMQTT);
}

void loop() {
  // Mantener la conexión MQTT activa
  if (!mqtt.connected()) {
    conectarMQTT();
  }
  mqtt.loop();
}

// Función callback 
// Procesa los mensajes entrantes del tópico
void callbackMQTT(char* topic, byte* payload, unsigned int length) {
  String mensaje = "";
  for (unsigned int i = 0; i < length; i++) {
    mensaje += (char)payload[i];
  }
  
  Serial.print("Mensaje recibido en [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(mensaje);

  // Control físico del actuador según el payload recibido
  if (mensaje == "ON") {
    digitalWrite(PIN_LED, HIGH);
    Serial.println("LED ENCENDIDO");
  } else if (mensaje == "OFF") {
    digitalWrite(PIN_LED, LOW);
    Serial.println("LED APAGADO");
  }
}

// Rutina de reconexión al broker público
void conectarMQTT() {
  while (!mqtt.connected()) {
    Serial.print("Intentando conexión al broker...");
    String clientId = "ESP32-SIM800-Client-";
    clientId += String(random(0xffff), HEX);
    
    if (mqtt.connect(clientId.c_str())) {
      Serial.println(" ¡Conectado!");
      mqtt.subscribe(topicLED);
      Serial.print("Suscrito al tópico: ");
      Serial.println(topicLED);
    } else {
      Serial.print(" Fallo, rc=");
      Serial.print(mqtt.state());
      Serial.println(" Reintentando en 5 segundos...");
      delay(5000);
    }
  }
}

// Función auxiliar para comandos AT directos
void enviarComandoAT(String comando, const int tiempoEspera) {
  sim800Serial.println(comando);
  delay(tiempoEspera);
  while (sim800Serial.available()) {
    Serial.write(sim800Serial.read());
  }
}