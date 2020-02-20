#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <U8g2lib.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <NTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <PubSubClient.h>

//flag for saving data
bool shouldSaveConfig = false;

//define your default values here, if there are different values in config.json, they are overwritten.
char mqtt_server[40] = "MQTT_Server";

////////////////////////////////////// WARNING //////////////////////////////////////
//port cannot be currently sepcified, I'm working on it... (Default port is 1883)
char mqtt_port[6] = "1883";
////////////////////////////////////// WARNING //////////////////////////////////////

char mqtt_user[60] = "Username";
char mqtt_pass[100] = "Password";

unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE  (50)
char msg[MSG_BUFFER_SIZE];
int value = 0;

//Start MQTT config
WiFiClient espClient;
PubSubClient client(espClient);


const long utcOffsetInSeconds = 3600;
unsigned long previousMillis = 0;        // will store last time LED was updated
const long interval = 30000;           // interval at which to blink (milliseconds)


Adafruit_BME280 bme; // use I2C interface
Adafruit_Sensor *bme_temp = bme.getTemperatureSensor();
Adafruit_Sensor *bme_pressure = bme.getPressureSensor();
Adafruit_Sensor *bme_humidity = bme.getHumiditySensor();


String temp = "50";
String humi = "42.5";

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_helvR08_tf);
  u8g2.drawStr(20, 20, "Configure me from");
  u8g2.drawStr(35, 40, "Your phone!");
  u8g2.drawStr(2, 60, "SSID:");
  u8g2.drawStr(33, 60, String(myWiFiManager->getConfigPortalSSID()).c_str());
  u8g2.sendBuffer();
}

void drawscr() {
  //Time
  u8g2.setFont(u8g2_font_helvR08_tf);
  if (String(timeClient.getHours()).length() == 1) {
    u8g2.drawStr(12, 10, "0");
    u8g2.drawStr(18, 10, String(timeClient.getHours()).c_str());
  }
  else {
    u8g2.drawStr(12, 10, String(timeClient.getHours()).c_str());
  }
  u8g2.drawStr(25, 10, ":");
  if (String(timeClient.getMinutes()).length() == 1) {
    u8g2.drawStr(30, 10, "0");
    u8g2.drawStr(36, 10, String(timeClient.getMinutes()).c_str());
  }
  else {
    u8g2.drawStr(30, 10, String(timeClient.getMinutes()).c_str());
  }

  //Wifi
  if ( WiFi.status() != WL_CONNECTED ) {
    u8g2.drawStr(45, 10, "Error!");
  }
  else {
    u8g2.drawStr(55, 10, "Conn.");
  }
  //Temperature
  u8g2.setFont(u8g2_font_helvR12_tf);
  u8g2.drawStr(2, 40, "Temp:");
  u8g2.drawStr(58, 40, temp.c_str());
  u8g2.drawStr(93, 40, "\xb0");
  u8g2.drawStr(103, 40, "C");
  //Humidity
  u8g2.drawStr(2, 60, "Hum:");
  u8g2.drawStr(58, 60, humi.c_str());
  u8g2.drawStr(103, 60, "%");
  u8g2.setFont(u8g2_font_open_iconic_all_1x_t);
  // Time Glyph
  u8g2.drawGlyph(2, 10, 123);
  //Wifi Glyph
  u8g2.drawGlyph(45, 10, 281);
  //MQTT Glyph
  u8g2.drawGlyph(90, 10, 128);
}

void getSensorInfo() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {

    previousMillis = currentMillis;

    sensors_event_t temp_event, pressure_event, humidity_event;
    bme_temp->getEvent(&temp_event);
    bme_pressure->getEvent(&pressure_event);
    bme_humidity->getEvent(&humidity_event);


    temp = String(int(temp_event.temperature));
    humi = String(int(humidity_event.relative_humidity));

    snprintf (msg, MSG_BUFFER_SIZE, temp.c_str());
    Serial.print("Publish message: ");
    Serial.println(msg);
    client.publish("habs/edu/sensor/temp", msg);

    snprintf (msg, MSG_BUFFER_SIZE, humi.c_str());
    Serial.print("Publish message: ");
    Serial.println(msg);
    client.publish("habs/edu/sensor/humi", msg);

  }
}

void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void setupSpiffs() {
  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);
          strcpy(mqtt_user, json["mqtt_user"]);
          strcpy(mqtt_pass, json["mqtt_pass"]);


        } else {
          Serial.println("failed to load json config");
        }
        configFile.close();
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read

}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived in topic: ");
  Serial.println(topic);

  Serial.print("Message:");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("outTopic", "hello world");
      // ... and resubscribe
      client.subscribe("inTopic");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup(void) {

  //Serial Init
  Serial.begin(9600);



  //Initialize OLED panel
  u8g2.begin();
  u8g2.setFont(u8g2_font_helvR12_tf);
  u8g2.drawStr(20, 40, "Loading...");
  u8g2.sendBuffer();

  setupSpiffs();

  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 6);
  WiFiManagerParameter custom_mqtt_user("user", "mqtt user", mqtt_user, 60);
  WiFiManagerParameter custom_mqtt_pass("pass", "mqtt pass", mqtt_pass, 100);


  //Start WifiManager
  WiFiManager wifiManager;

  //WifiManager Configuration
  wifiManager.setConfigPortalTimeout(180);
  wifiManager.setCustomHeadElement("<style>.c { text-align: center;}button {border:0;border-radius:0.5rem;color:white;line-height:2.4rem;font-size:1.2rem;width:100%;background: linear-gradient(to right, #ee0979, #ff6a00);} div,input {padding:10px;font-size:1em;background: white;border-radius: 5px;/*! top: auto; */}input { width: 90%}body { text-align: center; font-family:verdana; background: linear-gradient(to right, #36d1dc, #5b86e5); /* W3C, IE 10+/ Edge, Firefox 16+, Chrome 26+, Opera 12+, Safari 7+ */ /* W3C, IE 10+/ Edge, Firefox 16+, Chrome 26+, Opera 12+, Safari 7+ */}button { border:0; border-radius:0.5rem; color:white; line-height:2.4rem; font-size:1.2rem; width:100%;  background: linear-gradient(to right, #00c6ff, #0072ff); /* W3C, IE 10+/ Edge, Firefox 16+, Chrome 26+, Opera 12+, Safari 7+ */}.q { float: right; width: 64px; text-align: right;}   </style>");
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //Extra Params for MQTT
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_mqtt_user);
  wifiManager.addParameter(&custom_mqtt_pass);


  u8g2.begin();
  u8g2.setFont(u8g2_font_helvR12_tf);
  u8g2.drawStr(20, 40, "Connecting...");
  u8g2.sendBuffer();

  //WifiManager Manage Connection
  wifiManager.setAPCallback(configModeCallback);
  if (!wifiManager.autoConnect()) {
    Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(1000);
  }
  Serial.println("connected...yeey :)");

  WiFi.mode(WIFI_STA);

  //read updated parameters
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(mqtt_user, custom_mqtt_user.getValue());
  strcpy(mqtt_pass, custom_mqtt_pass.getValue());

  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"]   = mqtt_port;
    json["mqtt_user"]   = mqtt_user;
    json["mqtt_pass"]   = mqtt_pass;

    // json["ip"]          = WiFi.localIP().toString();
    // json["gateway"]     = WiFi.gatewayIP().toString();
    // json["subnet"]      = WiFi.subnetMask().toString();

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.prettyPrintTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
    shouldSaveConfig = false;
  }

  //Check for BME280
  if (!bme.begin(0x76)) {
    Serial.println(F("Could not find a valid BME280 sensor, check wiring!"));
    while (1) delay(10);
  }

  //Get Sensor Info and update Vars
  sensors_event_t temp_event, humidity_event;
  bme_temp->getEvent(&temp_event);
  bme_humidity->getEvent(&humidity_event);

  Serial.println(mqtt_server);
  Serial.println(mqtt_port);
  Serial.println(mqtt_user);
  Serial.println(mqtt_pass);


  temp = String(int(temp_event.temperature));
  humi = String(int(humidity_event.relative_humidity));

  

  //Sync RTC
  timeClient.begin();

  WiFi.mode(WIFI_STA);

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  while (!client.connected()) {
    u8g2.begin();
    u8g2.setFont(u8g2_font_helvR12_tf);
    u8g2.drawStr(20, 40, "MQTT Wait...");
    u8g2.sendBuffer();
    Serial.println("Connecting to MQTT...");

    String clientId = "PhotonHTensor-";
    clientId += String(ESP.getChipId());

    if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {

      Serial.println("connected");

    } else {

      Serial.print("failed with state ");
      Serial.print(client.state());
      delay(2000);

    }
  }

  client.publish("habs/edu/sensor/boot", "Booted");

  snprintf (msg, MSG_BUFFER_SIZE, temp.c_str());
  Serial.print("Publish message: ");
  Serial.println(msg);
  client.publish("habs/edu/sensor/temp", msg);

  snprintf (msg, MSG_BUFFER_SIZE, humi.c_str());
  Serial.print("Publish message: ");
  Serial.println(msg);
  client.publish("habs/edu/sensor/humi", msg);
}

void loop() {

  //Update time
  timeClient.update();

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  //Check to refresh sensor
  getSensorInfo();

  //clear previous information
  u8g2.clearBuffer();

  //Update Screen
  drawscr();

  //Send Updates
  u8g2.sendBuffer();
}
