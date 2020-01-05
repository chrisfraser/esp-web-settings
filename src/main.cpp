#include <DNSServer.h>
#include <FS.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include "captivePortal.h"

Preferences prefs;
DNSServer dnsServer;
AsyncWebServer server(80);

// This is the Wifi the ESP will try to connect to:
const char *ssid = "***********";
const char *password = "***********";

const char *STRING_SETTING = "STRING_SETTING";
const char *INT_SETTING = "INT_SETTING";
const char *FLOAT_SETTING = "FLOAT_SETTING";

// This function sets the templated parameters (eg. %STRING_SETTING%) in settings.html
String templateProcessor(const String &var)
{
    if (var == STRING_SETTING)
    {
        return prefs.getString(STRING_SETTING, "I <3 Unicorns!");
    }

    if (var == INT_SETTING)
    {
        return String(prefs.getInt(INT_SETTING, 42));
    }

    if (var == FLOAT_SETTING)
    {
        return String(prefs.getFloat(FLOAT_SETTING, 0.0));
    }

    return String("!!!You fucked up and forgot about this template variable.!!!");
}

void notFound(AsyncWebServerRequest *request)
{
    request->send(404, "text/plain", "Not found");
}

// This generates a unique hostName for SoftAP from chip MacId. This never changes and is unique to each device
String getUniqueHost()
{
    // This needs to be the lenght of the output string below
    char host[16];
    uint64_t chipid = ESP.getEfuseMac(); // The chip ID is essentially its MAC address(length: 6 bytes).

    snprintf(host, 16, "ESP-%04X%08X", (uint16_t)(chipid >> 32), (uint32_t)chipid);

    return String(host);
}

void setup()
{
    Serial.begin(115200);
    Serial.setDebugOutput(true);
    prefs.begin("settings");

    String host = getUniqueHost();
    Serial.print("host: ");
    Serial.println(host);

    // Set up the SoftAP, this is what you connect to if the ESP cannot connect to your network
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(host.c_str(), "SecurePass");

    // Try connect to the given credentials above
    WiFi.begin(ssid, password);
    if (WiFi.waitForConnectResult() != WL_CONNECTED)
    {
        Serial.printf("STA: Failed!\n");
        WiFi.disconnect(false);
        delay(1000);
        WiFi.begin(ssid, password);
    }

    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    // Start DNS server, this is so we can capture all requests in SoftAP mode for captive portal
    dnsServer.start(53, "*", WiFi.softAPIP());

    // Start file system
    SPIFFS.begin();

    // Handler for captive portal
    server.addHandler(new CaptiveRequestHandler(host)).setFilter(ON_AP_FILTER);

    // Root Endpoint for index page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/index.html", "text/html", false);
    });

    // Endpoint for settings page
    server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/settings.html", "text/html", false, templateProcessor);
    });

    // Endpoint to set settings
    server.on("/set", HTTP_POST, [](AsyncWebServerRequest *request) {
        String setting;

        // Check if the setting is in post params
        if (request->hasParam(STRING_SETTING, true))
        {
            // Get param
            setting = request->getParam(STRING_SETTING, true)->value();

            // Put new setting in prefs
            prefs.putString(STRING_SETTING, setting);
        }

        if (request->hasParam(INT_SETTING, true))
        {
            setting = request->getParam(INT_SETTING, true)->value();
            prefs.putInt(INT_SETTING, setting.toInt());
        }

        if (request->hasParam(FLOAT_SETTING, true))
        {
            setting = request->getParam(FLOAT_SETTING, true)->value();
            prefs.putFloat(FLOAT_SETTING, setting.toFloat());
        }

        request->send(200, "text/plain", "Success");
    });

    // Endpoint to clear settings
    server.on("/clear", HTTP_POST, [](AsyncWebServerRequest *request) {
        prefs.clear();
        request->send(200, "text/plain", "Success");
    });

    // Regex for *.css
    server.on("^(\\/.+\\.css)$", HTTP_GET, [](AsyncWebServerRequest *request) {
        String cssFile = request->pathArg(0);
        request->send(SPIFFS, cssFile, "text/css");
    });

    // Regex for *.ico
    server.on("^(\\/.+\\.ico)$", HTTP_GET, [](AsyncWebServerRequest *request) {
        String iconFile = request->pathArg(0);
        request->send(SPIFFS, iconFile, "image/x-icon");
    });

    server.onNotFound(notFound);

    server.begin();
}

void loop()
{
    dnsServer.processNextRequest();
}
