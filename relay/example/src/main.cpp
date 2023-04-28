#include <Arduino.h>
#include <IO7F8266.h>

String user_html =
    "";
// for meta.XXXXX, this var is the C variable to hold the XXXXX

// USER CODE EXAMPLE : your custom config variable

char* ssid_pfix = (char*)"TAEWOO_RELAY";
unsigned long lastPublishMillis = -pubInterval;
const int RELAY = 15;

void publishData() {
    StaticJsonDocument<512> root;
    JsonObject data = root.createNestedObject("d");

    // USER CODE EXAMPLE : command handling
    data["relay"] = digitalRead(RELAY) == 1 ? "on" : "off";
    // USER CODE EXAMPLE : command handling

    serializeJson(root, msgBuffer);
    client.publish(evtTopic, msgBuffer);
}


void handleUserCommand(char* topic, JsonDocument* root) {
    JsonObject d = (*root)["d"];

    // USER CODE EXAMPLE : status/change update
    // code if any of device status changes to notify the change
    if(d.containsKey("relay"))
    {
        if(strstr(d["relay"], "on"))
        {
            digitalWrite(RELAY, HIGH);
        }
        else if(strstr(d["relay"], "toggle"))
        {
            digitalWrite(RELAY, !digitalRead(RELAY));
        }
        else{
            digitalWrite(RELAY, LOW);
        }
    
    }
    // USER CODE EXAMPLE
    lastPublishMillis = -pubInterval;
}

void setup() {
    Serial.begin(115200);
    // USER CODE EXAMPLE : Initialization
    pinMode(RELAY, OUTPUT);
    // USER CODE EXAMPLE

    initDevice();
    // USER CODE EXAMPLE : meta data to local variable
    JsonObject meta = cfg["meta"];
    pubInterval = meta.containsKey("pubInterval") ? meta["pubInterval"] : 0;
    lastPublishMillis = -pubInterval;
    // USER CODE EXAMPLE

    WiFi.mode(WIFI_STA);
    WiFi.begin((const char*)cfg["ssid"], (const char*)cfg["w_pw"]);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    // main setup
    Serial.printf("\nIP address : ");
    Serial.println(WiFi.localIP());

    // if there is any user Meta Data, implement the funtcion and assign it to userMeta
    // if there is any user Command, implement the funtcion and assign it to userCommand
    userCommand = handleUserCommand;
    set_iot_server();
    iot_connect();
}

void loop() {
    if (!client.connected()) {
        iot_connect();
    }
    // USER CODE EXAMPLE : main loop
    //     you can put any main code here, for example,
    //     the continous data acquistion and local intelligence can be placed here
    // USER CODE EXAMPLE
    client.loop();
    if ((pubInterval != 0) && (millis() - lastPublishMillis > pubInterval)) {
        publishData();
        lastPublishMillis = millis();
    }
}