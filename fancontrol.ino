#if defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#elif defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
#include <WebServer.h>
#endif
#include <time.h>
#include <AutoConnect.h>
#include <OneWire.h>

#define D1 5 // fan assigning the ESP8266 pin to arduino pin
#define D2 4

int fanPin = 5;
int fan2Pin = 4;
int dutyCycle = 0;
int fanSpeedPercent = 0;
int fan2SpeedPercent = 0;

static const char AUX_TIMEZONE[] PROGMEM = R"(
{
  "title": "TimeZone",
  "uri": "/timezone",
  "menu": true,
  "element": [
    {
      "name": "caption",
      "type": "ACText",
      "value": "Sets the time zone to get the current local time.",
      "style": "font-family:Arial;font-weight:bold;text-align:center;margin-bottom:10px;color:DarkSlateBlue"
    },
    {
      "name": "timezone",
      "type": "ACSelect",
      "label": "Select TZ name",
      "option": [],
      "selected": 10
    },
    {
      "name": "newline",
      "type": "ACElement",
      "value": "<br>"
    },
    {
      "name": "start",
      "type": "ACSubmit",
      "value": "OK",
      "uri": "/start"
    }
  ]
}
)";

typedef struct {
  const char* zone;
  const char* ntpServer;
  int8_t      tzoff;
} Timezone_t;

static const Timezone_t TZ[] = {
  { "Europe/London", "europe.pool.ntp.org", 0 },
  { "Europe/Berlin", "europe.pool.ntp.org", 1 },
  { "Europe/Helsinki", "europe.pool.ntp.org", 2 },
  { "Europe/Moscow", "europe.pool.ntp.org", 3 },
  { "Asia/Dubai", "asia.pool.ntp.org", 4 },
  { "Asia/Karachi", "asia.pool.ntp.org", 5 },
  { "Asia/Dhaka", "asia.pool.ntp.org", 6 },
  { "Asia/Jakarta", "asia.pool.ntp.org", 7 },
  { "Asia/Manila", "asia.pool.ntp.org", 8 },
  { "Asia/Tokyo", "asia.pool.ntp.org", 9 },
  { "Australia/Brisbane", "oceania.pool.ntp.org", 10 },
  { "Pacific/Noumea", "oceania.pool.ntp.org", 11 },
  { "Pacific/Auckland", "oceania.pool.ntp.org", 12 },
  { "Atlantic/Azores", "europe.pool.ntp.org", -1 },
  { "America/Noronha", "south-america.pool.ntp.org", -2 },
  { "America/Araguaina", "south-america.pool.ntp.org", -3 },
  { "America/Blanc-Sablon", "north-america.pool.ntp.org", -4},
  { "America/New_York", "north-america.pool.ntp.org", -5 },
  { "America/Chicago", "north-america.pool.ntp.org", -6 },
  { "America/Denver", "north-america.pool.ntp.org", -7 },
  { "America/Los_Angeles", "north-america.pool.ntp.org", -8 },
  { "America/Anchorage", "north-america.pool.ntp.org", -9 },
  { "Pacific/Honolulu", "north-america.pool.ntp.org", -10 },
  { "Pacific/Samoa", "oceania.pool.ntp.org", -11 }
};

#if defined(ARDUINO_ARCH_ESP8266)
ESP8266WebServer Server;
#elif defined(ARDUINO_ARCH_ESP32)
WebServer Server;
#endif

AutoConnect       Portal(Server);
AutoConnectConfig Config;       // Enable autoReconnect supported on v0.9.4
AutoConnectAux    Timezone;

void rootPage() {
  String  content =
    "<html>"
    "<head>"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
    "<script type=\"text/javascript\">"
    "setTimeout(\"location.reload()\", 1000);"
    "</script>"
    "</head>"
    "<body style=\"font-family: Arial, Verdana\">"
    "<h2 align=\"center\" style=\"margin:20px;\">ESP wifi pwm fan control</h2>"
    "<p align=\"center\">Set fan speed in percent:</p>"
    "<p align=\"center\">http://espip/fan/1?speed=30</p>"
    "<p align=\"center\">http://espip/fan/2?speed=0</p>"
    "<p></p><p style=\"padding-top:15px;text-align:center\">" AUTOCONNECT_LINK(COG_24) "</p>"
    "</body>"
    "</html>";
  Server.send(200, "text/html", content);
}

void startPage() {
  // Retrieve the value of AutoConnectElement with arg function of WebServer class.
  // Values are accessible with the element name.
  String  tz = Server.arg("timezone");

  for (uint8_t n = 0; n < sizeof(TZ) / sizeof(Timezone_t); n++) {
    String  tzName = String(TZ[n].zone);
    if (tz.equalsIgnoreCase(tzName)) {
      configTime(TZ[n].tzoff * 3600, 0, TZ[n].ntpServer);
      Serial.println("Time zone: " + tz);
      Serial.println("ntp server: " + String(TZ[n].ntpServer));
      break;
    }
  }

  // The /start page just constitutes timezone,
  // it redirects to the root page without the content response.
  Server.sendHeader("Location", String("http://") + Server.client().localIP().toString() + String("/"));
  Server.send(302, "text/plain", "");
  Server.client().flush();
  Server.client().stop();
}

void setup() {
  delay(1000);
  Serial.begin(115200);
  Serial.println();

  pinMode(fanPin, OUTPUT); // sets the pins as outputs:
  pinMode(fan2Pin, OUTPUT); // sets the pins as outputs:

  // Enable saved past credential by autoReconnect option,
  // even once it is disconnected.
  Config.autoReconnect = true;
  Config.ota = AC_OTA_BUILTIN;
  Portal.config(Config);

  // Load aux. page
  Timezone.load(AUX_TIMEZONE);
  // Retrieve the select element that holds the time zone code and
  // register the zone mnemonic in advance.
  AutoConnectSelect&  tz = Timezone["timezone"].as<AutoConnectSelect>();
  for (uint8_t n = 0; n < sizeof(TZ) / sizeof(Timezone_t); n++) {
    tz.add(String(TZ[n].zone));
  }

  Portal.join({ Timezone });        // Register aux. page

  // Behavior a root path of ESP8266WebServer.
  Server.on("/", rootPage);
  Server.on("/start", startPage);   // Set NTP server trigger handler

  Server.on("/fan/1", [](){
    char response[50];
    snprintf(response, 50, Server.arg("speed").c_str() );
    fanSpeedPercent = atoi( Server.arg("speed").c_str() );
    Server.send(200, "text/plain", response);
  });

  Server.on("/fan/2", [](){
    char response[50];
    snprintf(response, 50, Server.arg("speed").c_str() );
    fan2SpeedPercent = atoi( Server.arg("speed").c_str() );
    Server.send(200, "text/plain", response);
  });

  analogWriteRange(100); // to have a range 1 - 100 for the fan
  analogWriteFreq(10000);

  // Establish a connection with an autoReconnect option.
  if (Portal.begin()) {
    Serial.println("WiFi connected: " + WiFi.localIP().toString());
  }
}

void controlFanSpeed (int fanSpeedPercent) {
  Serial.print("Fan 1 Speed: ");
  Serial.print(fanSpeedPercent);
  Serial.println("%");
  analogWrite(fanPin, fanSpeedPercent); // set the fan speed
}

void controlFan2Speed (int fan2SpeedPercent) {
  Serial.print("Fan 2 Speed: ");
  Serial.print(fan2SpeedPercent);
  Serial.println("%");
  analogWrite(fan2Pin, fan2SpeedPercent); // set the fan speed
}

void loop() {
  Portal.handleClient();

  controlFanSpeed (fanSpeedPercent); // Update fan speed
  controlFan2Speed (fan2SpeedPercent); // Update fan speed
  delay(1000);
}