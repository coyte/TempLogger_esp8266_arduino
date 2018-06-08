 /**
 * TempLogger
 *
 *  Created on: 04.02.2018
 *  Based on ESPIOT work
 *  VERSION 0.01
 **/

#include <stdint.h>
#include <HardwareSerial.h>
#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#include "PubSubClient.h"


//#include "SparkFunHTU21D.h"
#include "Adafruit_HTU21DF.h"

#include <ArduinoJson.h>
#include "FS.h"

extern "C" {
#include <user_interface.h>
}

#define FPM_SLEEP_MAX_TIME 0xFFFFFFF

// DEFINES
#define DONEPIN 14
#define CONFIGPIN 12 // switch high at boot to enter config mode

// GLOBAL VARIABLES
String VERSION = "0.01";
const char* WIFISSIDValue;
const char* WIFIpassword;
String urlServer;
const char* HTTPUser;
const char* HTTPPass;
const char* jsonIp;
char ip1, ip2, ip3, ip4;
char mask1, mask2, mask3, mask4;
char gw1, gw2, gw3, gw4;
bool configOK = false;
bool cmdComplete = false;
String SerialCMD;
char wificonfig[512];
IPAddress ip, gateway, subnet;
uint8_t MAC_array[6];
char MAC_char[18];
Adafruit_HTU21DF mySensor;
ESP8266WebServer server(80);
//mqtt variables
const char* mqttServer = "192.168.15.25";
const int mqttPort = 1883;
const char* mqttUser = "slave";
const char* mqttPassword = "mypass";
WiFiClient espClient;
PubSubClient client(espClient);

 

char serverIndex[512] = "<h1>TempLogger Config</h1><ul><li><a href='params'>Config "
                        "TempLogger</a></li><li><a href='update'>Flash "
                        "TempLogger</a></li></ul><br><br>Version: 0.01<br><a "
                        "href='http://www.teekens.info' "
                        "target='_blank'>Documentation</a>";

char serverIndexUpdate[256] = "<h1>TempLogger Config</h1><h2>Update Firmware</h2><form method='POST' "
                              "action='/updateFile' enctype='multipart/form-data'><input type='file' "
                              "name='update'><input type='submit' value='Update'></form>";
char serverIndexConfig[1024] = "<h1>TempLogger Config</h1><h2>Config TempLogger</h2><form method='POST' "
                               "action='/config'>SSID : <br><input type='text' name='ssid'><br>Pass : "
                               "<br><input type='password' name='pass'><br>@IP : <br><input type='text' "
                               "name='ip'><br>Mask : <br><input type='text' name='mask'><br>@GW : "
                               "<br><input type='text' name='gw'><br>Http user : <br><input type='text' "
                               "name='userhttp'><br>Http pass : <br><input type='text' "
                               "name='passhttp'><br>URL : <br><input type='text' name='url'><br><input "
                               "type='submit' value='OK'></form>";

const char* Style = "<style>body {  text-align: center; font-family:Arial, Tahoma;  "
                    "background-color:#f0f0f0;}ul li { border:1px solid gray;  height:30px;  "
                    "padding:3px;  list-style: none;}ul li a { text-decoration: none;}input{ "
                    "border:1px solid gray;  height:25px;}input[type=text] { width:150px;  "
                    "padding:5px;  font-size:10px;}#url {  width:300px;}</style>";

//Function prototypes
bool loadConfig();
void PowerDown();
float readADC_avg();
void serverWebCfg();
void setupWifiAP();


void setup()
{
    pinMode(DONEPIN, OUTPUT);
    digitalWrite(DONEPIN, LOW);

    Serial.begin(9600);
    Serial.setDebugOutput(true);
    Serial.println("Start setup");

    if (!SPIFFS.begin()) {
        Serial.println("Failed to mount file system");
        return;
    }

    if (!loadConfig()) {
        Serial.println("Failed to load config");
    }
    else {
        configOK = true;
        Serial.println("Config loaded");
    }

    if (configOK) {
        Serial.println("Set WIFI_STA mode");
        WiFi.mode(WIFI_STA);
        printf("WIFI connecting now using SSID: %s and password: %s\n", WIFISSIDValue,WIFIpassword);
        WiFi.begin(WIFISSIDValue, WIFIpassword);
        //WiFi.config(ip, gateway, subnet); //Comment out this line to use DHCP
        while (WiFi.status() != WL_CONNECTED) {
              delay(500);
              printf("Wait for WiFi connection to become ready.\n");
            }
        Serial.print("IP adress in use: ");
        Serial.println(WiFi.localIP());

        WiFi.macAddress(MAC_array);
        for (int i = 0; i < sizeof(MAC_array); ++i) {
            sprintf(MAC_char, "%s%02x:", MAC_char, MAC_array[i]);
        }


        //Connecting to MQTT server
        client.setServer(mqttServer, mqttPort);
        //client.setCallback(callback);
        while (!client.connected()) {
            Serial.println("Connecting to MQTT...");
            if (client.connect("ESP8266Client", mqttUser, mqttPassword )) {
                Serial.println("connected");  
            } else {
                Serial.print("failed with state ");
                Serial.print(client.state());
                delay(2000);
 
            }
        }

        //Test mqtt
        //client.publish("esp/test", "Hello from ESP8266");


        Serial.println("Setting up I2C");
        Wire.begin(2,0);
        //mySensor.settings.commInterface = I2C_MODE;
        //mySensor.settings.I2CAddress = 0x76;
        //mySensor.settings.runMode = 3;
        //mySensor.settings.tStandby = 0;
        //mySensor.settings.filter = 0;
        //mySensor.settings.tempOverSample = 1;
        //mySensor.settings.humidOverSample = 1;
        //mySensor.settings.pressOverSample = 1;
        delay(10);
        Serial.println("Start I2C sensor");
        if (!mySensor.begin()) {
            Serial.println("Couldn't find sensor!");
            while (1);
            }        
    }
    else {
        Serial.println("Config not ok, starting AP mode");
        setupWifiAP(); //Start AP mode
        serverWebCfg(); //Start webserver

    }
}





void loop()
{
    char strtemp[6];
    //Serial.print("Temp: "); Serial.print(mySensor.readTemperature());
    //Serial.print("\t\tHum: "); Serial.println(mySensor.readHumidity());
    server.handleClient();
    // Read sensor data
    float temp=0;
    temp = mySensor.readTemperature();
    //strtemp = "Temperatuur: " + String(temp);
    dtostrf(temp,3,1,strtemp);
    printf("Temperature: %s\n", strtemp);
    client.publish("esp/test", strtemp);
    printf("Pushed to MQTT, now waiting for 5s\n");
    delay(5000);
//PowerDown();
}



//FUNCTIONS

// Read config file
bool loadConfig()
{
    File configFile = SPIFFS.open("/config.json", "r");
    if (!configFile) {
        printf("Failed to open config file\n");
        return false;
    }

    size_t size = configFile.size();
    if (size > 1024) {
        printf("Config file size is too large\n");
        return false;
    }

    // Allocate a buffer to store contents of the file.
    std::unique_ptr<char[]> buf(new char[size]);

    // We don't use String here because ArduinoJson library requires the input
    // buffer to be mutable. If you don't use ArduinoJson, you may as well
    // use configFile.readString instead.
    configFile.readBytes(buf.get(), size);

    StaticJsonBuffer<512> jsonBuffer;
    JsonObject& json = jsonBuffer.parseObject(buf.get());

    if (!json.success()) {
        printf("Failed to parse config file\n");
        return false;
    }

    if (json.containsKey("WIFISSID")) {
        WIFISSIDValue = json["WIFISSID"];
    }
    if (json.containsKey("WIFIpass")) {
        WIFIpassword = json["WIFIpass"];
    }
    if (json.containsKey("Ip")) {
        jsonIp = json["Ip"];
        ip.fromString(jsonIp);
    }
    if (json.containsKey("Mask")) {
        jsonIp = json["Mask"];
        subnet.fromString(jsonIp);
    }
    if (json.containsKey("GW")) {
        jsonIp = json["GW"];
        gateway.fromString(jsonIp);
    }
    if (json.containsKey("urlServer")) {
        //urlServer = json["urlServer"].asString();
        urlServer = json["urlServer"].as<char*>();
    }
    if (json.containsKey("HTTPUser")) {
        //HTTPUser = json["HTTPUser"].asString();
        HTTPUser = json["HTTPUser"].as<char*>();
    }
    if (json.containsKey("HTTPPass")) {
        //HTTPPass = json["HTTPPass"].asString();
        HTTPPass = json["HTTPPass"].as<char*>();
    }


    uint8_t tmp[6];
    char tmpMAC[18];
    WiFi.macAddress(tmp);
    for (int i = 0; i < sizeof(tmp); ++i) {
        sprintf(tmpMAC, "%s%02x", tmpMAC, tmp[i]);
    }
    json["ID"] = String(tmpMAC);
    json.printTo(wificonfig, sizeof(wificonfig));

    return true;
}


//Powerdown
void PowerDown()
{
    digitalWrite(DONEPIN, HIGH);
    delay(1);
    digitalWrite(DONEPIN, LOW);
}


//Read battery voltage and average it
float readADC_avg()
{
    int battery = 0;
    for (int i = 0; i < 10; i++) {
        battery = battery + analogRead(A0);
    }
    return (((battery / 10) * 4.2) / 1024);
}

//Run AP mode
void setupWifiAP()
{
    WiFi.mode(WIFI_AP);

    //Create SSID from name and mac address
    uint8_t mac[WL_MAC_ADDR_LENGTH];
    WiFi.softAPmacAddress(mac);
    String macID = String(mac[WL_MAC_ADDR_LENGTH - 2], HEX) + String(mac[WL_MAC_ADDR_LENGTH - 1], HEX);
    macID.toUpperCase();
    String AP_NameString = "TEMPLOGGER-" + macID;

    char AP_NameChar[AP_NameString.length() + 1];
    memset(AP_NameChar, 0, AP_NameString.length() + 1);

    for (int i = 0; i < AP_NameString.length(); i++)
        AP_NameChar[i] = AP_NameString.charAt(i);
    //Set password
    String WIFIPASSSTR = "admin" + macID;
    char WIFIPASS[WIFIPASSSTR.length() + 1];
    memset(WIFIPASS, 0, WIFIPASSSTR.length() + 1);
    for (int i = 0; i < WIFIPASSSTR.length(); i++)
        WIFIPASS[i] = WIFIPASSSTR.charAt(i);
    printf("Starting AP with SSID: %s and password: %s\n", AP_NameChar,WIFIPASS);
    WiFi.softAP(AP_NameChar, WIFIPASS);
    Serial.print("IP address: ");
    Serial.println(WiFi.softAPIP());

} //RUN ACCESSPOINT MODE




void serverWebCfg()
{
   printf("Starting function serverWebCfg\n");

    //Assign hostname
    char host[] = "TempLogger";

    MDNS.begin(host);
    server.on("/", HTTP_GET, []() {
        server.sendHeader("Connection", "close");
        server.send(200, "text/html", strcat(serverIndex, Style));
    });
    server.on("/update", HTTP_GET, []() {
        server.sendHeader("Connection", "close");
        server.send(200, "text/html", strcat(serverIndexUpdate, Style));
    });
    server.on("/params", HTTP_GET, []() {
        server.sendHeader("Connection", "close");
        server.send(200, "text/html", strcat(serverIndexConfig, Style));
    });
    server.on("/config", HTTP_POST, []() {

        String StringConfig;
        String ssid = server.arg("ssid");
        String pass = server.arg("pass");
        String ip = server.arg("ip");

        String mask = server.arg("mask");

        String gw = server.arg("gw");

        String userhttp = server.arg("userhttp");
        String passhttp = server.arg("passhttp");
        String url = server.arg("url");
        uint8_t tmp[6];
        char tmpMAC[18];
        WiFi.macAddress(tmp);
        for (int i = 1; i < sizeof(tmp); ++i) {
            sprintf(tmpMAC, "%s%02x:", tmpMAC, tmp[i]);
        }

        StringConfig = "{\"ID\":\"" + String(tmpMAC) + "\",\"WIFISSID\":\"" + ssid + "\",\"WIFIpass\":\"" + pass + "\",\"Ip\":\"" + ip + "\",\"Mask\":\"" + mask + "\",\"GW\":\"" + gw + "\",\"urlServer\":\"" + url + "\",\"HTTPUser\":\"" + userhttp + "\",\"HTTPPass\":\"" + passhttp + "\"}";

        StaticJsonBuffer<512> jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(StringConfig);
        File configFile = SPIFFS.open("/config.json", "w");
        if (!configFile) {
            Serial.println("Failed to open config file for writing");
        }
        else {
            json.printTo(configFile);
        }

        server.send(200, "text/plain", "OK");
    });

    server.on(
        "/updateFile", HTTP_POST,
        []() {
            server.sendHeader("Connection", "close");
            server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
            ESP.restart();
        },
        []() {
            HTTPUpload& upload = server.upload();
            if (upload.status == UPLOAD_FILE_START) {
                Serial.setDebugOutput(true);
                WiFiUDP::stopAll();
                Serial.printf("Update: %s\n", upload.filename.c_str());
                uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
                if (!Update.begin(maxSketchSpace)) { // start with max available size
                    Update.printError(Serial);
                }
            }
            else if (upload.status == UPLOAD_FILE_WRITE) {
                if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                    Update.printError(Serial);
                }
            }
            else if (upload.status == UPLOAD_FILE_END) {
                if (Update.end(true)) { // true to set the size to the current
                    // progress
                    Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
                }
                else {
                    Update.printError(Serial);
                }
                Serial.setDebugOutput(false);
            }
            yield();
        });
    server.begin();
    MDNS.addService("http", "tcp", 80);
    printf("End function serverWebCfg\n");

}
