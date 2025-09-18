

#include <Arduino.h>
#include <WiFi.h>
#include <WebOTA.h>

const char* ssidSTA = "lan_home_new";
const char* passwordSTA = "3002new1987lsda";

const char* ssidAP = "WeighingBatcher";
const char* passwordAP = "1w3r5v7z9";

void taskOta(void *pvParameter){
    Serial.println("Start taskOta");
    int count = 0;
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssidSTA, passwordSTA);
    WiFi.config(IPAddress(192, 168, 0, 222), IPAddress(192, 168, 0, 1), IPAddress(255, 255, 255, 0));
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print("...");
        count++;
        if(count > 20){
            break;
        }

    }
    if(count > 20){
        WiFi.mode(WIFI_MODE_AP);
        WiFi.softAP(ssidAP, passwordAP);
        WiFi.softAPConfig(IPAddress(192, 168, 0, 222), IPAddress(192, 168, 0, 1), IPAddress(255, 255, 255, 0));
    }
    Serial.println("");


    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    Serial.println("HTTP server started");
    webota.init(80, "/update");
    for(;;){
        webota.handle();
        delay(1);
    }
}


