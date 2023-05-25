#include <Arduino.h>
#include "BLEDevice.h"
#include "WiFi.h"
#include "PubSubClient.h"

static boolean BLEWasConectedLastLoopCycle = false;
static BLEAddress *pServerAddress;
const uint8_t notificationOn[] = {0x1, 0x0};
const uint8_t notificationOff[] = {0x0, 0x0};
static BLERemoteCharacteristic *temperatureCharacteristic;
const char *mqtt_server = "192.168.86.53";
WiFiClient espClient;
PubSubClient mqttClient(espClient);
BLEClient *pClient;
bool doConnect = false;




static void log(const char *msg)
{
  Serial.println(msg);
  mqttClient.publish("grilltemperature/log", msg);
}

static float convertTemperature(uint8_t b[])
{
  float tmp = (((b[0] & 255)) | ((((b[1] & 255)) << 8)));
  return (tmp / 10) - 40;
}

// When the BLE Server sends a new temperature reading with the notify property
static void temperatureNotifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic,
                                      uint8_t *pData, size_t length, bool isNotify)
{
  if (isNotify)
  {
    uint8_t internalTemp[2];
    uint8_t outsideTemp[2];

    char *value = 0;
    char internalTempChar[10];
    char outsideTempChar[10];

    internalTemp[0] = *(pData + 2);
    internalTemp[1] = *(pData + 3);

    outsideTemp[0] = *(pData + 4);
    outsideTemp[1] = *(pData + 5);

    dtostrf(convertTemperature(internalTemp), 0, 2, internalTempChar);
    dtostrf(convertTemperature(outsideTemp), 0, 2, outsideTempChar);
    Serial.println(internalTempChar);
    Serial.println(outsideTempChar);
    Serial.println(convertTemperature(internalTemp));
    Serial.println(convertTemperature(outsideTemp));
    mqttClient.publish("grilltemperature/grillprobe/insideprobe", internalTempChar);
    mqttClient.publish("grilltemperature/grillprobe/outsideprobe", outsideTempChar);
  }
}

// Connect to the BLE Server that has the name, Service, and Characteristics
bool connectToBLE(BLEAddress pAddress)
{
  if (pClient->isConnected())
  {
    log("Already connected to BLE");
    return true;
  }

  log("Trying to connecto to BLE device");
  pClient->connect(pAddress);
  log(" - Connected to server");

  // Obtain a reference to the service we are after in the remote BLE server.
  BLERemoteService *pRemoteService = pClient->getService("0000FB00-0000-1000-8000-00805F9B34FB");

  if (pRemoteService == nullptr)
  {
    log("Failed to find our service UUID: ");
    return (false);
  }

  // Obtain a reference to the characteristics in the service of the remote BLE server.
  temperatureCharacteristic = pRemoteService->getCharacteristic("0000FB02-0000-1000-8000-00805F9B34FB");

  if (temperatureCharacteristic == nullptr)
  {
    return false;
  }
  log(" - Found our characteristics");

  // Assign callback functions for the Characteristics
  temperatureCharacteristic->registerForNotify(temperatureNotifyCallback);
  digitalWrite(5, HIGH);
  return true;
}

void disconnectBLE()
{
  if (pClient->isConnected())
  {
    pClient->disconnect();
    digitalWrite(5, LOW);
  }
}

class MyClientCallback : public BLEClientCallbacks
{
  void onConnect(BLEClient* pclient)
  {
     log(" - Connected to server 2");
  }

  void onDisconnect(BLEClient* pclient)
  {
    if(doConnect) {
        log("Disconnected, reconnecting to BLE");
        connectToBLE(*pServerAddress);
    }
  }
};

void callback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message arrived in topic: ");
  Serial.println(topic);

  Serial.print("Message:");
  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }

  if (strcmp(topic, "data/enablegrill/command") == 0)
  {

    if (payload[0] == '1')
    {
      connectToBLE(*pServerAddress);
      doConnect = true;
      mqttClient.publish("data/enablegrill/state", "1");
    }
    else
    {
      doConnect = false;
      disconnectBLE();
      mqttClient.publish("data/enablegrill/state", "0");
    }
  }
  else
  {
    Serial.print("ERROR");
  }
}

void initWiFi()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin("MartinWireless", "");
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
}

void setup()
{
  try
  {
    pinMode(5, OUTPUT); // sets the digital pin 13 as output
    // Start serial communication
    Serial.begin(115200);
    Serial.println("Starting Arduino BLE Client application...");
    pServerAddress = new BLEAddress("2A:03:60:44:8D:7E");
    // Init BLE device
    BLEDevice::init("testestset");
    Serial.println("Init successfull");
    initWiFi();
    pClient = BLEDevice::createClient();
    pClient->setClientCallbacks(new MyClientCallback());
    mqttClient.setServer(mqtt_server, 1883);
    mqttClient.setCallback(callback);
  }
  catch (const std::exception &e)
  {
    Serial.println("geh");
  }
}

void reconnect()
{
  // Loop until we're reconnected
  while (!mqttClient.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqttClient.connect("ESP32GRILL"))
    {
      log("connected to MQTT");
      mqttClient.subscribe("data/enablegrill/command");
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");

      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void loop()
{
  try
  {
    // if (doConnect && pClient->isConnected() == false)
    // {
    //   log("connecting to BLE");
    //   connectToBLE(*pServerAddress);
    //   // Serial.println("publish disconnect msg");
    //   // mqttClient.publish("data/enablegrill/state", "0");
    //   // mqttClient.publish("data/enablegrill/command", "0");
    //   // digitalWrite(5, LOW);
    // }

    if (!mqttClient.connected())
    {
      reconnect();
    }

    mqttClient.loop();

    delay(1000); // Delay a second between loops.
  }
  catch (const std::exception &e)
  {
    Serial.println("hej");
  }
}

