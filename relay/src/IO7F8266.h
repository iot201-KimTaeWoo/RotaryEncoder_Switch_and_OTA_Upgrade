// io7 IOT Device
#include <ConfigPortal8266.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266httpUpdate.h>
#include <PubSubClient.h>
#include <WiFiClient.h>

const char compile_date[] = __DATE__ " " __TIME__;
char cmdTopic[200]      = "iot3/%s/cmd/+/fmt/+";
char evtTopic[200]      = "iot3/%s/evt/status/fmt/json";
char connTopic[200]     = "iot3/%s/evt/connection/fmt/json";
char logTopic[200]      = "iot3/%s/mgmt/device/status";
char metaTopic[200]     = "iot3/%s/mgmt/device/meta";
char updateTopic[200]   = "iot3/%s/mgmt/device/update";
char rebootTopic[200]   = "iot3/%s/mgmt/initiate/device/reboot";
char resetTopic[200]    = "iot3/%s/mgmt/initiate/device/factory_reset";
char upgradeTopic[200]  = "iot3/%s/mgmt/initiate/firmware/update";

String user_config_html =
    ""
    "<p><input type='text' name='broker' placeholder='Broker'>"
    "<p><input type='text' name='devId' placeholder='Device Id'>"
    "<p><input type='text' name='token' placeholder='Device Token'>"
    "<p><input type='text' name='fingerprint' placeholder='No TLS' value='%s'>"
    "<p><input type='text' name='meta.pubInterval' placeholder='Publish Interval'>";

extern String       user_html;

ESP8266WebServer    server(80);
WiFiClientSecure    wifiClientSecure;
WiFiClient          wifiClient;
PubSubClient        client;
char                iot_server[100];
char                msgBuffer[JSON_CHAR_LENGTH];
unsigned long       pubInterval;
int                 mqttPort;
char                fpFile[] = "/fingerprint.txt";

void handleIOTCommand(char* topic, byte* payload, unsigned int payloadLength);
void (*userCommand)(char* topic, JsonDocument* root) = NULL;
void (*userMeta)() = NULL;

bool subscribeTopic(const char* topic) {
    if (client.subscribe(topic)) {
        Serial.printf("Subscription to %s OK\n", topic);
        return true;
    } else {
        Serial.printf("Subscription to %s Failed\n", topic);
        return false;
    }
}

void setDevId(char* topic, const char* devId) {
    char buffer[200];
    sprintf(buffer, topic, devId);
    strcpy(topic, buffer);
}

void initDevice() {
    loadConfig();
    String fingerprint;
    if (cfg.containsKey("fingerprint")) {
        fingerprint = (const char*)cfg["fingerprint"];
        fingerprint.trim();
        cfg.remove("fingerprint");
        save_config_json();
        File f = LittleFS.open(fpFile, "w");
        f.write(fingerprint.c_str());
        f.close();
    } else {
        if (LittleFS.exists(fpFile)) {
            File f = LittleFS.open(fpFile, "r");
            fingerprint = f.readString();
            fingerprint.trim();
            f.close();
        }
    }

    if (!cfg.containsKey("config") || strcmp((const char*)cfg["config"], "done")) {
        char buf[1000];
        sprintf(buf, user_config_html.c_str(), fingerprint.c_str());
        user_config_html = buf + user_html;
        configDevice();
    } else {
        const char* devId = (const char*)cfg["devId"];
        setDevId(evtTopic, devId);
        setDevId(logTopic, devId);
        setDevId(connTopic, devId);
        setDevId(metaTopic, devId);
        setDevId(cmdTopic, devId);
        setDevId(updateTopic, devId);
        setDevId(rebootTopic, devId);
        setDevId(resetTopic, devId);
        setDevId(upgradeTopic, devId);
        sprintf(iot_server, "%s", (const char*)cfg["broker"]);
        if (fingerprint.length() > 0) {
            wifiClientSecure.setFingerprint(fingerprint.c_str());
            client.setClient(wifiClientSecure);
            mqttPort = 8883;
        } else {
            client.setClient(wifiClient);
            mqttPort = 1883;
        }
    }
}

void set_iot_server() {
    client.setCallback(handleIOTCommand);
    if (mqttPort == 8883) {
        if (!wifiClientSecure.connect(iot_server, mqttPort)) {
            Serial.println("ssl connection failed");
            return;
        }
    } else {
        if (!wifiClient.connect(iot_server, mqttPort)) {
            Serial.println("connection failed");
            return;
        }
    }
    client.setServer(iot_server, mqttPort);  // IOT
}

void pubMeta() {
    JsonObject meta = cfg["meta"];
    StaticJsonDocument<512> root;
    JsonObject d = root.createNestedObject("d");
    JsonObject metadata = d.createNestedObject("metadata");
    for (JsonObject::iterator it = meta.begin(); it != meta.end(); ++it) {
        metadata[it->key().c_str()] = it->value();
    }
    JsonObject supports = d.createNestedObject("supports");
    supports["deviceActions"] = true;
    serializeJson(root, msgBuffer);
    Serial.printf("publishing device metadata: %s\n", msgBuffer);
    client.publish(metaTopic, msgBuffer, true);
}

void iot_connect() {
    while (!client.connected()) {
        int mqConnected = 0;
        mqConnected = client.connect((const char*)cfg["devId"],
                                     (const char*)cfg["devId"],
                                     (const char*)cfg["token"],
                                     connTopic,
                                     0,
                                     true,
                                     "{\"d\":{\"status\":\"offline\"}}",
                                     true);
        if (mqConnected) {
            Serial.println("MQ connected");
        } else {
            if (digitalRead(RESET_PIN) == 0) {
                reboot();
            }
            if (WiFi.status() == WL_CONNECTED) {
                if (client.state() == -2) {
                    set_iot_server();
                } else {
                    Serial.printf("MQ Connection fail RC = %d, try again in 5 seconds\n", client.state());
                }
                delay(5000);
            } else {
                Serial.println("Reconnecting to WiFi");
                WiFi.disconnect();
                WiFi.begin();
                int i = 0;
                while (WiFi.status() != WL_CONNECTED) {
                    Serial.print("*");
                    delay(5000);
                    if (i++ > 10) reboot();
                }
            }
        }
    }
    if (!subscribeTopic(rebootTopic)) return;
    if (!subscribeTopic(resetTopic)) return;
    if (!subscribeTopic(updateTopic)) return;
    if (!subscribeTopic(upgradeTopic)) return;
    if (!subscribeTopic(cmdTopic)) return;
    pubMeta();
    client.publish(connTopic, "{\"d\":{\"status\":\"online\"}}", true);
}

void update_progress(int cur, int total) {
    Serial.printf("CALLBACK:  HTTP update process at %d of %d bytes...\n", cur, total);
}

void update_error(int err) {
    Serial.printf("CALLBACK:  HTTP update fatal error code %d\n", err);
}

void handleIOTCommand(char* topic, byte* payload, unsigned int payloadLength) {
    byte2buff(msgBuffer, payload, payloadLength);
    StaticJsonDocument<512> root;
    deserializeJson(root, String(msgBuffer));
    JsonObject d = root["d"];

    if (strstr(topic, rebootTopic)) {  // rebooting
        reboot();
    } else if (strstr(topic, resetTopic)) {  // clear the configuration and reboot
        reset_config();
        ESP.restart();
    } else if (strstr(topic, updateTopic)) {
        JsonArray fields = d["fields"];
        for (JsonArray::iterator it = fields.begin(); it != fields.end(); ++it) {
            DynamicJsonDocument field = *it;
            const char* fieldName = field["field"];
            if (strstr(fieldName, "metadata")) {
                JsonObject fieldValue = field["value"];
                cfg.remove("meta");
                JsonObject meta = cfg.createNestedObject("meta");
                for (JsonObject::iterator fv = fieldValue.begin(); fv != fieldValue.end(); ++fv) {
                    meta[(char*)fv->key().c_str()] = fv->value();
                }
                save_config_json();
            }
        }
        pubMeta();
        pubInterval = cfg["meta"]["pubInterval"];
        if (userMeta) {
            userMeta();
        }
    } else if (strstr(topic, upgradeTopic)) {
        JsonObject upgrade = d["upgrade"];
        String response = "{\"OTA\":{\"status\":";
        if (upgrade.containsKey("fw_url")) {
            Serial.println("firmware upgrading");
            const char* fw_url = upgrade["fw_url"];
            ESPhttpUpdate.onProgress(update_progress);
            ESPhttpUpdate.onError(update_error);
            client.publish(logTopic, "{\"info\":{\"upgrade\":\"Device will be upgraded.\"}}");
            t_httpUpdate_return ret = ESPhttpUpdate.update(wifiClient, fw_url);
            switch (ret) {
                case HTTP_UPDATE_FAILED:
                    response += "\"[update] Update failed. " + String(fw_url) + "\"}}";
                    client.publish(logTopic, (char*)response.c_str());
                    Serial.println(response);
                    break;
                case HTTP_UPDATE_NO_UPDATES:
                    response += "\"[update] Update no Update.\"}}";
                    client.publish(logTopic, (char*)response.c_str());
                    Serial.println(response);
                    break;
                case HTTP_UPDATE_OK:
                    Serial.println("[update] Update ok.");  // may not called we reboot the ESP
                    break;
            }
        } else {
            response += "\"OTA Information Error\"}}";
            client.publish(logTopic, (char*)response.c_str());
            Serial.println(response);
        }
    } else if (strstr(topic, "/cmd/")) {
        if (d.containsKey("config")) {
            char maskBuffer[JSON_CHAR_LENGTH];
            cfg["compile_date"] = compile_date;
            maskConfig(maskBuffer);
            cfg.remove("compile_date");
            String info = String("{\"config\":") + String(maskBuffer) + String("}");
            client.publish(logTopic, info.c_str());
        } else if (userCommand) {
            userCommand(topic, &root);
        }
    }
}