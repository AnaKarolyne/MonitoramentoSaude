#include <WiFi.h>
#include <PubSubClient.h>
#include <DHTesp.h>

// Pinos dos componentes
const int DHT_PIN = 15;     // Pino ao qual o sensor DHT22 está conectado
const int ledTempPin = 2;   // Pino ao qual o LED vermelho está conectado
const int ledHumPin = 5;    // Pino ao qual o LED amarelo está conectado

DHTesp dht; 

// Configurações de rede MQTT
const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* mqtt_server = "test.mosquitto.org";

// Cliente WiFi e cliente MQTT
WiFiClient espClient;
PubSubClient client(espClient);

// Variáveis de controle
unsigned long lastMsg = 0;
float temp = 0;
float hum = 0;

void setup_wifi() { 
  delay(10);
  Serial.println();
  Serial.print("Conectando-se a ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi conectado");
  Serial.println("Endereço IP: ");
  Serial.println(WiFi.localIP());
}

// Callback chamada quando uma mensagem MQTT é recebida
void callback(char* topic, byte* payload, unsigned int length) { //perintah untuk menampilkan data ketika esp32 di setting sebagai subscriber
  Serial.print("Mensagem recebida [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) { // Exibe o conteúdo da mensagem
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Ligar ou desligar o LED com base na mensagem recebida
  if ((char)payload[0] == '1') {
    digitalWrite(2, LOW);   // Ligar o LED (LOW é o nível de voltagem)
  } else {
    digitalWrite(2, HIGH); // Desligar o LED (HIGH é o nível de voltagem)
  }
}

void reconnect() { //perintah koneksi esp32 ke mqtt broker baik itu sebagai publusher atau subscriber
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Tentando conectar ao MQTT...");
    // perintah membuat client id agar mqtt broker mengenali board yang kita gunakan
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("Connected");
      // Once connected, publish an announcement...
      client.publish("/saude/monitoramento/mqtt", "Indobot"); //perintah publish data ke alamat topik yang di setting
      // ... and resubscribe
      client.subscribe("/saude/monitoramento/mqtt"); //perintah subscribe data ke mqtt broker
    } else {
      Serial.print("Falha, rc=");
      Serial.print(client.state());
      Serial.println(" tentando novamente em 5 segundos");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  // Inicializar pinos como saída
  pinMode(2, OUTPUT);  // LED vermelho
  pinMode(5, OUTPUT);  // LED amarelo
  
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  
  // Inicializar comunicação com o sensor DHT22
  dht.setup(DHT_PIN, DHTesp::DHT22);
}

// Função para controlar o LED com base na frequência
void controlarLED(int pin, int frequencia) {
  digitalWrite(pin, HIGH);
  delay(frequencia);
  digitalWrite(pin, LOW);
  delay(frequencia);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long now = millis();

  // Publicar dados a cada segundo
  if (now - lastMsg > 1000) { //perintah publish data
    lastMsg = now;

    // Obter dados do sensor DHT22
    TempAndHumidity  data = dht.getTempAndHumidity();

    // Publicar temperatura
    String temp = String(data.temperature, 2); //membuat variabel temp untuk di publish ke broker mqtt
    client.publish("/saude/monitoramento/temperature", temp.c_str()); //publish data dari varibel temp ke broker mqtt
    
    // Publicar umidade
    String hum = String(data.humidity, 1); //membuat variabel hum untuk di publish ke broker mqtt
    client.publish("/saude/monitoramento/humidity", hum.c_str()); //publish data dari varibel hum ke broker mqtt

    Serial.print("Temperatura: ");
    Serial.println(temp);
    Serial.print("Umidade: ");
    Serial.println(hum);

    // Controle do LED com base na temperatura
    if (data.temperature > 35) {
      int frequenciaTempLED = map(data.temperature, -40, 80, 50, 500);
      controlarLED(ledTempPin, frequenciaTempLED);

      // Publicar mensagem dependendo da temperatura
      if (data.temperature > 40) {
        client.publish("/saude/monitoramento/alertatemp", "Temperatura muito alta!");
      } else {
        client.publish("/saude/monitoramento/alertatemp", "Temperatura elevada.");
      }
    } else {
      client.publish("/saude/monitoramento/alertatemp", "Temperatura normal.");
    }

    // Controle do LED com base na umidade
    if (data.humidity < 35) {
      int frequenciaHumLED = map(data.humidity, 0, 100, 10, 100);
      controlarLED(ledHumPin, frequenciaHumLED);
    
      // Publicar mensagem dependendo da umidade
      if (data.humidity < 20) {
        client.publish("/saude/monitoramento/alertahum", "Umidade muito baixa!");
      } else {
        client.publish("/saude/monitoramento/alertahum", "Umidade baixa.");
      }
    } else {
      client.publish("/saude/monitoramento/alertahum", "Umidade normal.");
    }
  }
}
