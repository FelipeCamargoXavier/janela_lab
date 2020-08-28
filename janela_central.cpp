#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <esp_wpa2.h>

// Configuracao rede
#define EAP_IDENTITY "aRA@utfpr.edu.br"
#define EAP_PASSWORD "XXXXXXXXX"
const char* ssid = "UTFPRWEB";

IPAddress server(xxx, xxx, xxx, xxx);
WiFiClient espClient;
PubSubClient client(espClient);

// Configuracao ThingsBoard
#define TOKEN "xxxxxxxxxxxxxxxxxxxxxxx"
#define ID "xxxxxxxxxxxxxxxxxxxxxx" 

// Calibracao sensor de temperatura
#define Vcc 5.0                // Tensao alimentacao sensor
#define Vref 3.3               // Fundo de escala do conversor adc do esp32
#define T0 298.15              // Temperatura de referencia (25 ºC ou 298.15 K)
#define Rt 1500                // Resistor do divisor de tensao
#define R0 10000               // Resistencia do NTC a 25 ºC    (datasheet)
#define T1 273.15              // [K] 0 ºC                      (datasheet)
#define T2 373.15              // [K] 100 ºC                    (datasheet)
#define RT1 35563              // Resistencia em T1             (datasheet)                 
#define RT2 549                // Resistencia em T2             (datasheet)
float beta = 0.0;              // Parametro [K]         
float Rinf = 0.0;              // Parametro [Ohm]
float TempKelvin = 0.0;        
float TempCelsius = 0.0;
float SumTempCelsius = 0.0;
float Vin = 0.0;               // Tensao recebida 
float Rin = 0.0;               // Resistencia NTC

// Sensor abertura janela e sensor chuva
bool janela = false;
bool chuva = false;

char buf[5];
int timepublishTemp = 0; 

void setup() 
{
  pinMode(32, INPUT);
  pinMode(25, INPUT);
  pinMode(34, INPUT);
  
  Serial.begin(115200);

  //WiFi
  Serial.print("Connecting to ");
  Serial.println(ssid);

  // // EDUROADM
  WiFi.disconnect(true);  //disconnect form wifi to set new wifi connection
  WiFi.mode(WIFI_STA); //init wifi mode
  esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)EAP_IDENTITY, strlen(EAP_IDENTITY)); //provide identity
  esp_wifi_sta_wpa2_ent_set_username((uint8_t *)EAP_IDENTITY, strlen(EAP_IDENTITY)); //provide username --> identity and username is same
  esp_wifi_sta_wpa2_ent_set_password((uint8_t *)EAP_PASSWORD, strlen(EAP_PASSWORD)); //provide password
  esp_wpa2_config_t config = WPA2_CONFIG_INIT_DEFAULT(); //set config settings to default
  esp_wifi_sta_wpa2_ent_enable(&config); //set config settings to enable function
  WiFi.begin(ssid); //connect to wifi

  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }

 Serial.println("");
 Serial.println("WiFi connected");
 Serial.println("IP address: ");
 Serial.println(WiFi.localIP());

  client.setServer( server, 1883 );

  // Configuracao sensor temperatura
  analogReadResolution(12);                        // Resolucao adc 12 bits
  analogSetAttenuation(ADC_11db);                  // tensao referencia adc: 0 - 3.3 V
  beta = (log(RT1 / RT2)) / ((1 / T1) - (1 / T2)); // Calculo do beta NTC
  Rinf = R0 * exp(-beta / T0);                     // Calculo oo Rinf
}

void reconnect() 
{
  // Loop until we're reconnected
  while (!client.connected()) 
  {
    if ( client.connect(ID, TOKEN, NULL) ) {
      Serial.println( "[DONE]" );
    } 
    else {
      Serial.print( "[FAILED] [ rc = " );
      Serial.print( client.state() );
      Serial.println( " : retrying in 5 seconds]" );
      // Wait 5 seconds before retrying
      delay( 5000 );
    }
  }
}

void publishJanela(String key, String value)
{
  String payload = "{";
  payload += "\"";
  payload += key;
  payload += "\":"; 
  payload += value;
  payload += "}";

  char attributes[100];
  payload.toCharArray( attributes, 100 );
  client.publish( "v1/devices/me/attributes", attributes );
  Serial.println( attributes );
}

void publishJanela(String key, bool value)
{
  publishJanela(key, String(value));
}

void publishJanela(String key, float value)
{
  dtostrf(value, 5, 2, buf);
  Serial.print("buf temp: ");
  Serial.println(buf);
  publishJanela(key, String(buf));
}

void loop() 
{
  if ( !client.connected() ) {
    reconnect();
  }

  if(digitalRead(32) != janela)
  {
    janela = !janela;
    publishJanela("active", janela);
  }

  if(digitalRead(25) != chuva)
  {
    chuva = !chuva;
    publishJanela("chuva", chuva);
  }

  // Leitura sensor temperatura
  Vin = Vref * ((float)(analogRead(34)) / 4096.0);
  Rin = (Rt/Vin)*(Vcc-Vin);
  TempKelvin = (beta / log(Rin / Rinf));
  TempCelsius = TempKelvin - 273.15;

  if(timepublishTemp > 120)
  {
    TempCelsius = SumTempCelsius/timepublishTemp;
    Serial.print("Temperatura em Celsius: ");
    Serial.print(TempCelsius);
    Serial.print("   Vin: ");
    Serial.println(Vin);
    publishJanela("temp", TempCelsius);

    timepublishTemp = 0;
    SumTempCelsius = 0;
  }

  delay(1000);
  timepublishTemp++;
  SumTempCelsius += TempCelsius;

  client.loop();
}