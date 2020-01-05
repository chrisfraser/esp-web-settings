
#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

/** Is this an IP? */
boolean isIp(String str)
{
    for (size_t i = 0; i < str.length(); i++)
    {
        int c = str.charAt(i);
        if (c != '.' && (c < '0' || c > '9'))
        {
            return false;
        }
    }
    return true;
}

class CaptiveRequestHandler : public AsyncWebHandler
{
private:
    String _hostName;

public:
    CaptiveRequestHandler(String hostName)
    {
        _hostName = hostName;
    }
    virtual ~CaptiveRequestHandler() {}

    bool canHandle(AsyncWebServerRequest *request)
    {
        String host = request->host();
        return !isIp(host) && host != (String(_hostName) + ".local");
    }

    void handleRequest(AsyncWebServerRequest *request)
    {
        // You cannot get this to work using SPIFFS. I have tried for hours and failed
        AsyncResponseStream *response = request->beginResponseStream("text/html");
        response->print("<!DOCTYPE html><html><head><title>Gwa's Captive Portal</title>");
        // You should probably just leave this whole page blank and set the timout below to 0
        response->print("<script>setTimeout(function(){window.location.href=\"http://%SOFTAPIP%\";},30000);</script>");
        response->print("</head><body>");
        response->print("<h1>Captive Portal</h1>");
        response->print("<h4>This opens up magically when you connect to the wifi</h4>");
        response->printf("<p>This is the magic url that captive portal is loading: http://%s%s</p>", request->host().c_str(), request->url().c_str());
        response->printf("<p>You need to go to <a href='http://%s'>this link</a> to edit the device.</p>", WiFi.softAPIP().toString().c_str());
        response->print("<p>This will happen automatically in 30secs, but you can set the timeout to 0 and make it happen immediately.</p>");
        response->print("</body></html>");
        request->send(response);
    }
};