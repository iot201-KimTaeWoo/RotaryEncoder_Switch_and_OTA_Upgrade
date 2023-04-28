#include <Arduino.h>
#include <IO7F8266.h>
#include <SSD1306.h>


const int           pulseA = 12;
const int           pulseB = 13;
const int           pushSW = 2;
volatile int        lastEncoded = 0;
volatile long       encoderValue = 0;

volatile long last;
volatile bool pressed = false;



String user_html =
    "";
// for meta.XXXXX, this var is the C variable to hold the XXXXX
int customVar1;
// USER CODE EXAMPLE : your custom config variable

char* ssid_pfix = (char*)"TTTValve";
unsigned long lastPublishMillis = -pubInterval;
const int RELAY = 15;
IRAM_ATTR void handleRotary() {
    // Never put any long instruction
    int MSB = digitalRead(pulseA); //MSB = most significant bit
    int LSB = digitalRead(pulseB); //LSB = least significant bit

    int encoded = (MSB << 1) |LSB; //converting the 2 pin value to single number
    int sum  = (lastEncoded << 2) | encoded; //adding it to the previous encoded value
    if(sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) encoderValue ++;
    if(sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) encoderValue --;
    lastEncoded = encoded; //store this value for next time
    if (encoderValue > 255) {
        encoderValue = 255;
    } else if (encoderValue < 0 ) {
        encoderValue = 0;
    }
}

IRAM_ATTR void buttonClicked() {
    pressed = true;
    lastPublishMillis = -pubInterval;
}
void publishData() {
    StaticJsonDocument<512> root;
    JsonObject data = root.createNestedObject("d");
    // USER CODE EXAMPLE : command handling
    if(last + 8 < encoderValue)
    {
        data["switch"] = "on";
    }
    else if(last -8 > encoderValue)
    {
        data["switch"] = "off";
    }

    if(pressed)
    {
        data["pressed"] = "on";
        delay(200);
        pressed = false;
    }
    else
        data["pressed"] = "off";

    // USER CODE EXAMPLE : command handling

    serializeJson(root, msgBuffer);
    client.publish(evtTopic, msgBuffer);
}

void handleUserMeta() {
    // USER CODE EXAMPLE : meta data update
    // If any meta data updated on the Internet, it can be stored to local variable to use for the logic
    // in cfg["meta"]["XXXXX"], XXXXX should match to one in the user_html
    customVar1 = cfg["meta"]["yourVar"];
    // USER CODE EXAMPLE
}

void handleUserCommand(char* topic, JsonDocument* root) {
    JsonObject d = (*root)["d"];

    // USER CODE EXAMPLE : status/change update
    // code if any of device status changes to notify the change
    
    // USER CODE EXAMPLE
}

void setup() {
    Serial.begin(115200);
    pinMode(pushSW, INPUT_PULLUP);
    pinMode(pulseA, INPUT_PULLUP);
    pinMode(pulseB, INPUT_PULLUP);
    attachInterrupt(pushSW, buttonClicked, FALLING);
    attachInterrupt(pulseA, handleRotary, CHANGE);
    attachInterrupt(pulseB, handleRotary, CHANGE);
   
    // USER CODE EXAMPLE : Initialization

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
    userMeta = handleUserMeta;
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
        last = encoderValue;
    }
}