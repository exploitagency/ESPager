// ESPager by Hardcore Corey Harding
//Board used is ESP32 C3 SuperMini
//Choose LOLIN C3 Mini in Arduino Board Manager

//ESP32 Hardware Settings
#define SLEEP_PIN 3 //ESP32 pin that listens for the signal that pager is on
#define TX_PIN 7    //ESP32 pin that outputs the pocsag signal to the pager
//The pagers radio board is typically removed or modified and replaced with esp32
//For Motorola Advisor (Classic)
//With the pager screen down, radio removed, header up, and the battery compartment on the right side
//Pin1 on the pagers radio header is to the left and pin 8 to the right
//Pin2 is a GND - I use the ground at the battery tray but this is the same ground plane
//Pin3 //Pin4 is connected to the ESP32 C3 TX_PIN defined above
//Pin5 //Pin6 //Pin7 (heartbeat when pager is on) is connected to the ESP32 C3 SLEEP_PIN defined above
//Pin8

//Put the ESP32 to sleep when pager is turned off
#define ENABLE_SLEEP true   // Set to false to disable sleep mode
#define SLEEP_TIMEOUT 5000  // value in mS before sleeping
#define LOOP_CHECK_INTERVAL_MS 1000 // value in mS to check for pulse
#define ISR_CHECK_INTERVAL_US 900000 // value in uS for isr interval

//Networking
#define APMODE 0 // 1 Starts AP Mode at boot, 0 Starts STA mode at boot
#define WIFI_POWER_DEFINE WIFI_POWER_8_5dBm // 8.5dBm, set the TX Power of WiFi chip (Default 20dBm)
//STATION MODE // Your WiFi SSID and Password
const char* ssid = "router";
const char* password = "Password";
const char* hostname = "ESPager";
#define WiFiFallbackCount 25 // # of times to try to connect to the network above before ESP32 creates its own access point
//Set to zero above to disable fallback to AP mode
//AP MODE // Access ESPager at http://192.168.4.1 when in AP Mode
const char* APssid     = "ESPager";
const char* APpassword = "password"; // Minimum 8 characters
const char* APhostname = "ESPager";

//POCSAG Defaults
String msg = "  GRAND CENTRAL       HACK THE PLANET"; //Each line on advisor is 20 characters, pad with spaces
String cap = "1337331"; //7 digits
String baud = "1200"; //512,1200,2400
String function_code = "3"; //0,1,2,3
String data_type = "0"; // Alphanumeric(0), Numeric(1), Tone(2)
String inverted = "0";
String repeat = "1"; //# Of times to send transmission

//GSC Defaults
String gscMsg = "  GRAND CENTRAL       HACK THE PLANET";
String gscCap = "313371"; //6 digits
String gscFunctionCode = "3"; //0,1,2,3
String gscDataType = "1";   // 1 = alphanumeric, 0 = numeric

//ESPager Version
#define VERSION_MAJOR 1
#define VERSION_MINOR 33
#define VERSION_PATCH 7331

//Initializing Variables
bool TransmitPocsag = false;
bool WebsiteDelivered = false;
bool gscWebsiteDelivered = false;
bool transmitGSC = false;
//Sleep Mode
volatile unsigned long lastHeartbeatTime = 0;
volatile int beatCount = 0;
unsigned long lastLoopCheck = 0;
const unsigned long LOOP_CHECK_INTERVAL = LOOP_CHECK_INTERVAL_MS;  // ms
bool WiFiFallback = false;