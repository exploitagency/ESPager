// ESPager by Hardcore Corey Harding
// From www.Exploit.Agency
// Tested on original Motorola Advisors (POCSAG and GSC versions)

//Arduino IDE v2.3.10
//esp32:esp32c3-libs@3.3.12
#include <WiFi.h>
#include "esp_wifi.h"
#include <esp_sleep.h>
#include <driver/gpio.h>

//3rd Party Libraries
#include <AsyncTCP.h> // v3.5.0 https://github.com/ESP32Async/AsyncTCP.git
#include <ESPAsyncWebServer.h> // v3.12.1 https://github.com/ESP32Async/ESPAsyncWebServer.git
#include <ElegantOTA.h> // https://github.com/ayushsharma82/ElegantOTA.git

//ESPager Libraries
#include "pocsag_website.h"
#include "gsc_website.h"
#include "settings.h"
#include "pocsag_tx.h"
#include "gsc_tx.h"

#include "pager_ascii_art.h"

AsyncWebServer server(80);

/*--------------------------------------------------------------------
  Utility functions
--------------------------------------------------------------------*/

// WiFiAPmode
void setWiFiAPmode() {
  WiFi.disconnect(true);  // Disconnect from any WiFi
  delay(500);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(APssid, APpassword);
  WiFi.softAPsetHostname(APhostname);
  // Print the AP IP address
  IPAddress APIP = WiFi.softAPIP();
  Serial.print("AP SSID: ");
  Serial.println(APssid);
  Serial.print("AP Password: ");
  Serial.println(APpassword);
  Serial.print("AP Hostname: ");
  Serial.println(APhostname);
  Serial.print("AP IP Address: ");
  Serial.println(APIP);
}

// WiFI STA Mode
void setWiFiSTAmode() {
  WiFi.disconnect(true);  // Disconnect from any WiFi
  delay(500);
  WiFi.setHostname(hostname);
  WiFi.mode(WIFI_MODE_STA);
  WiFi.begin(ssid, password);
  int WiFiRetries = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi...");
    if ((WiFiFallbackCount != 0) && (WiFiRetries == WiFiFallbackCount)) {
      // Unable to connect to network so starting ESP32 as Access Point
      setWiFiAPmode();
      WiFiFallback = true;
      break; //Break the loop
    }
    WiFiRetries++;
  }
  if (!WiFiFallback) {
    Serial.println();
    Serial.println("WiFi connected.");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }
}

// Sleep mode
static unsigned long lastISRTime = 0;
void IRAM_ATTR heartbeatISR() {
    unsigned long now = micros();
    if (now - lastISRTime > ISR_CHECK_INTERVAL_US) {  // debounce in uS
        beatCount++;
        lastHeartbeatTime = millis();
        lastISRTime = now;
    }
}

static String htmlEscape(const String &input) {
  String output;
  output.reserve(input.length() + 16);

  for (size_t i = 0; i < input.length(); ++i) {
    char c = input[i];

    switch (c) {
      case '&':
        output += "&amp;";
        break;

      case '<':
        output += "&lt;";
        break;

      case '>':
        output += "&gt;";
        break;

      case '"':
        output += "&quot;";
        break;

      case '\'':
        output += "&#39;";
        break;

      default:
        output += c;
        break;
    }
  }

  return output;
}

/*--------------------------------------------------------------------
  Setup
--------------------------------------------------------------------*/

void setup() {
  setCpuFrequencyMhz(80);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);  // Active-low LED

  Serial.begin(115200);
  delay(100);

  // Sleep mode
  // Clear all previous wakeup configurations
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  pinMode(SLEEP_PIN, INPUT_PULLDOWN);
  attachInterrupt(digitalPinToInterrupt(SLEEP_PIN), heartbeatISR, RISING);
  lastHeartbeatTime = millis();
  lastLoopCheck = millis();
  // Enable external GPIO wakeup on SLEEP_PIN
  if (ENABLE_SLEEP) {
  esp_sleep_enable_gpio_wakeup();
  gpio_wakeup_enable((gpio_num_t)SLEEP_PIN, GPIO_INTR_HIGH_LEVEL);
    Serial.println("Setup complete - GPIO wakeup enabled");
  } else {
    Serial.println("Setup complete - sleep mode disabled");
  }
  
  // Reduce WiFi current spikes
  WiFi.setTxPower(WIFI_POWER_DEFINE);

  // Start WiFi Network
  if (APMODE == 1) {
    setWiFiAPmode();
  }
  else {
    setWiFiSTAmode();
  }

  /*
    This file should contain your transmitter setup code, such as:

      pinMode(TX_PIN, OUTPUT);
      digitalWrite(TX_PIN, GSC_IDLE_LEVEL);

    It is included here exactly as in the original sketch.
  */
  #include "tx_setup.h"

  /*--------------------------------------------------------------------
    POCSAG route
  ------------------------------------------------------------------*/

  server.on("/pocsag", HTTP_GET,
    [](AsyncWebServerRequest *request) {

      int params = request->params();

      if (params > 0) {
        if (request->hasParam("msg")) {
          msg = request->getParam("msg")->value();
        }

        if (request->hasParam("cap")) {
          cap = request->getParam("cap")->value();
        }

        if (request->hasParam("baud")) {
          baud = request->getParam("baud")->value();
        }

        if (request->hasParam("function_code")) {
          function_code =
              request->getParam("function_code")->value();
        }

        if (request->hasParam("data_type")) {
          data_type =
              request->getParam("data_type")->value();
        } else {
          data_type = "0";
        }

        if (request->hasParam("inverted")) {
          inverted =
              request->getParam("inverted")->value();
        } else {
          inverted = "0";
        }

        if (request->hasParam("repeat")) {
          repeat =
              request->getParam("repeat")->value();
        }

        String html;

        html.reserve(3000);

// -------------------------------------------------
//  POCSAG Frame Queued – responsive, themed version
// -------------------------------------------------
html = F(
    "<!DOCTYPE html>"
    "<html lang=\"en\">"
    "<head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
    "<title>POCSAG Encoder</title>"
    "<style>"
    /* ----- colour variables (same as selection page) ----- */
    ":root{"
    "  --bg-dark:#0a0a12;"
    "  --bg-grad:#0d1a12;"
    "  --bg-deep:#060808;"
    "  --accent:#00ff88;"
    "  --text-main:#33ff66;"
    "  --text-sub:#e0e0e0;"
    "  --panel:#16213e;"
    "  --border:#0f3460;"
    "}"
    "body{"
    "  background-color:var(--bg-dark);"
    "  color:var(--text-main);"
    "  font-family:'Courier New',monospace;"
    "  margin:0;"
    "  padding:2rem;"
    "  text-shadow:"
    "    0 0 2px var(--text-main),"
    "    0 0 8px rgba(51,255,102,.4),"
    "    0 0 20px rgba(51,255,102,.15);"
    "}"
    /* ----- background layers ----- */
    "body::before,body::after,.vignette,.glass{content:\"\";position:fixed;inset:0;pointer-events:none;}"
    "body::before{"
    "  background:radial-gradient(ellipse at 50% 46%,var(--bg-grad) 0%,var(--bg-deep) 78%);"
    "  animation:hum 5.2s steps(6) infinite;"
    "  z-index:-2;"
    "}"
    "@keyframes hum{0%,100%{filter:brightness(0.96);}50%{filter:brightness(1.08);}}"
    "body::after{"
    "  background:repeating-linear-gradient(0deg,"
    "    rgba(0,0,0,.35) 0,"
    "    rgba(0,0,0,.35) 1px,"
    "    transparent 1px,"
    "    transparent 3px);"
    "  animation:scan-drift 2.6s linear infinite;"
    "  z-index:-1;"
    "}"
    "@keyframes scan-drift{to{background-position:0 3px;}}"
    ".vignette{background:radial-gradient(ellipse at 50% 50%,transparent 45%,rgba(0,0,0,.7) 100%);z-index:10;}"
    ".glass{background:radial-gradient(40% 30% at 22% 8%,rgba(255,255,255,.04) 0%,transparent 60%);z-index:11;}"
    "h1{color:var(--accent);margin-top:0;}"
    /* ----- stacked details form ----- */
    "form.details{"
    "  display:flex;"
    "  flex-direction:column;"
    "  gap:0.6rem;"
    "  max-width:40rem;"
    "  margin:1rem 0;"
    "}"
    "form.details label{"
    "  font-weight:bold;"
    "  color:#aaa;"
    "}"
    "form.details span{"
    "  color:var(--text-sub);"
    "  padding:0.4rem 0.6rem;"
    "  background:var(--panel);"
    "  border:1px solid var(--border);"
    "  border-radius:4px;"
    "  word-break:break-word;"
    "}"
    "form.action{margin-top:1rem;}"
    "input[type='submit']{"
    "  background:var(--border);"
    "  color:var(--accent);"
    "  border:1px solid var(--accent);"
    "  border-radius:4px;"
    "  padding:0.6rem 1.2rem;"
    "  font-family:monospace;"
    "  cursor:pointer;"
    "}"
    "input[type='submit']:hover{"
    "  background:var(--accent);"
    "  color:#1a1a2e;"
    "}"
    "@media (max-width:600px){"
    "  body{padding:1rem;}"
    "  h1{font-size:1.5rem;}"
    "}"
    "</style>"
    "</head>"
    "<body>"
    "<div class=\"vignette\"></div>"
    "<div class=\"glass\"></div>"
    "<h1>POCSAG Frame Queued</h1>"
    "<form class='details'>"
    "<label>MESSAGE:</label><span>"
);

html += htmlEscape(msg);

html += F(
    "</span>"
    "<label>CAPCODE:</label><span>"
);
html += htmlEscape(cap);

html += F(
    "</span>"
    "<label>BAUD RATE:</label><span>"
);
html += htmlEscape(baud);

html += F(
    "</span>"
    "<label>FUNCTION CODE:</label><span>"
);
html += htmlEscape(function_code);

html += F(
    "</span>"
    "<label>DATA TYPE:</label><span>"
);
html += htmlEscape(data_type);

html += F(
    "</span>"
    "<label>INVERTED:</label><span>"
);
html += htmlEscape(inverted);

html += F(
    "</span>"
    "<label>TIMES TO REPEAT:</label><span>"
);
html += htmlEscape(repeat);

html += F(
    "</span>"
    "</form>"
    "<form class='action' action='/pocsag' method='GET'>"
    "<input type='submit' value='Send Another POCSAG Page'>"
    "</form>"
    "<form class='action' action='/' method='GET'>"
    "<input type='submit' value='Return to Encoder Selection'>"
    "</form>"
    "</body>"
    "</html>"
);

        request->send(200, "text/html", html);

        WebsiteDelivered = true;
        TransmitPocsag = true;

        Serial.println("---------------");
        Serial.println("POCSAG FRAME");
        Serial.print("MESSAGE: ");
        Serial.println(msg);
        Serial.print("CAPCODE: ");
        Serial.println(cap);
        Serial.print("BAUD RATE: ");
        Serial.println(baud);
        Serial.print("FUNCTION CODE: ");
        Serial.println(function_code);
        Serial.print("DATA TYPE: ");
        Serial.println(data_type);
        Serial.print("INVERTED: ");
        Serial.println(inverted);
        Serial.print("TIMES TO REPEAT: ");
        Serial.println(repeat);
        Serial.println("---------------");
      }

      else {
        String html = String(pocsag_index_html);

        html.replace("{{MSG}}", msg);
        html.replace("{{CAP}}", cap);

        html.replace(
            "{{BAUD_512}}",
            baud == "512" ? "selected" : ""
        );

        html.replace(
            "{{BAUD_1200}}",
            baud == "1200" ? "selected" : ""
        );

        html.replace(
            "{{BAUD_2400}}",
            baud == "2400" ? "selected" : ""
        );

        html.replace(
            "{{FC_0}}",
            function_code == "0" ? "selected" : ""
        );

        html.replace(
            "{{FC_1}}",
            function_code == "1" ? "selected" : ""
        );

        html.replace(
            "{{FC_2}}",
            function_code == "2" ? "selected" : ""
        );

        html.replace(
            "{{FC_3}}",
            function_code == "3" ? "selected" : ""
        );

        html.replace(
            "{{data_type_0}}",
            data_type == "0" ? "selected" : ""
        );

        html.replace(
            "{{data_type_1}}",
            data_type == "1" ? "selected" : ""
        );

        html.replace(
            "{{data_type_2}}",
            data_type == "2" ? "selected" : ""
        );

        html.replace(
            "{{INVERTED}}",
            inverted == "1" ? "checked" : ""
        );

        html.replace("{{REPEAT}}", repeat);

        request->send(200, "text/html", html);
      }
    }
  );

  /*--------------------------------------------------------------------
    GSC route
  ------------------------------------------------------------------*/

  server.on("/gsc", HTTP_GET,
    [](AsyncWebServerRequest *request) {

      int params = request->params();

      if (params > 0) {
        if (request->hasParam("msg")) {
          gscMsg = request->getParam("msg")->value();
        }

        if (request->hasParam("cap")) {
          gscCap = request->getParam("cap")->value();
        }

        if (request->hasParam("function_code")) {
          gscFunctionCode =
              request->getParam("function_code")->value();
        }

        if (request->hasParam("data_type")) {
          gscDataType =
              request->getParam("data_type")->value();
        } else {
          gscDataType = "0";
        }

        String gscData_type =
            (gscDataType == "0") ? "Numeric" : "Alphanumeric";

        String html;

        html.reserve(3000);

// -------------------------------------------------
//  GSC Frame Queued – responsive, themed version
// -------------------------------------------------
html = F(
    "<!DOCTYPE html>"
    "<html lang=\"en\">"
    "<head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
    "<title>GSC Encoder</title>"
    "<style>"
    /* ----- colour variables (same as selection page) ----- */
    ":root{"
    "  --bg-dark:#0a0a12;"
    "  --bg-grad:#0d1a12;"
    "  --bg-deep:#060808;"
    "  --accent:#00ff88;"
    "  --text-main:#33ff66;"
    "  --text-sub:#e0e0e0;"
    "  --panel:#16213e;"
    "  --border:#0f3460;"
    "}"
    "body{"
    "  background-color:var(--bg-dark);"
    "  color:var(--text-main);"
    "  font-family:'Courier New',monospace;"
    "  margin:0;"
    "  padding:2rem;"
    "  text-shadow:"
    "    0 0 2px var(--text-main),"
    "    0 0 8px rgba(51,255,102,.4),"
    "    0 0 20px rgba(51,255,102,.15);"
    "}"
    /* ----- background layers (hum, scan‑lines, vignette, glass) ----- */
    "body::before,body::after,.vignette,.glass{content:\"\";position:fixed;inset:0;pointer-events:none;}"
    "body::before{"
    "  background:radial-gradient(ellipse at 50% 46%,var(--bg-grad) 0%,var(--bg-deep) 78%);"
    "  animation:hum 5.2s steps(6) infinite;"
    "  z-index:-2;"
    "}"
    "@keyframes hum{0%,100%{filter:brightness(0.96);}50%{filter:brightness(1.08);}}"
    "body::after{"
    "  background:repeating-linear-gradient(0deg,"
    "    rgba(0,0,0,.35) 0,"
    "    rgba(0,0,0,.35) 1px,"
    "    transparent 1px,"
    "    transparent 3px);"
    "  animation:scan-drift 2.6s linear infinite;"
    "  z-index:-1;"
    "}"
    "@keyframes scan-drift{to{background-position:0 3px;}}"
    ".vignette{background:radial-gradient(ellipse at 50% 50%,transparent 45%,rgba(0,0,0,.7) 100%);z-index:10;}"
    ".glass{background:radial-gradient(40% 30% at 22% 8%,rgba(255,255,255,.04) 0%,transparent 60%);z-index:11;}"
    "h1{color:var(--accent);margin-top:0;}"
    /* ----- stacked details form ----- */
    "form.details{"
    "  display:flex;"
    "  flex-direction:column;"
    "  gap:0.6rem;"
    "  max-width:40rem;"
    "  margin:1rem 0;"
    "}"
    "form.details label{"
    "  font-weight:bold;"
    "  color:#aaa;"
    "}"
    "form.details span{"
    "  color:var(--text-sub);"
    "  padding:0.4rem 0.6rem;"
    "  white-space:pre-wrap;"
    "  word-break:break-word;"
    "  background:var(--panel);"
    "  border:1px solid var(--border);"
    "  border-radius:4px;"
    "}"
    "form.action{margin-top:1rem;}"
    "input[type='submit']{"
    "  background:var(--border);"
    "  color:var(--accent);"
    "  border:1px solid var(--accent);"
    "  border-radius:4px;"
    "  padding:0.6rem 1.2rem;"
    "  font-family:monospace;"
    "  cursor:pointer;"
    "}"
    "input[type='submit']:hover{"
    "  background:var(--accent);"
    "  color:#1a1a2e;"
    "}"
    "@media (max-width:600px){"
    "  body{padding:1rem;}"
    "  h1{font-size:1.5rem;}"
    "}"
    "</style>"
    "</head>"
    "<body>"
    "<div class=\"vignette\"></div>"
    "<div class=\"glass\"></div>"
    "<h1>GSC Frame Queued</h1>"
    "<form class='details'>"
    "<label>MESSAGE:</label><span>"
);

html += htmlEscape(gscMsg);

html += F(
    "</span>"
    "<label>CAPCODE:</label><span>"
);

html += htmlEscape(gscCap);

html += F(
    "</span>"
    "<label>BAUD RATE:</label><span>600 bps</span>"
    "<label>FUNCTION CODE:</label><span>"
);

html += htmlEscape(gscFunctionCode);

html += F(
    "</span>"
    "<label>DATA TYPE:</label><span>"
);

html += gscData_type;   // already escaped when you set it

html += F(
    "</span>"
    "</form>"
    "<form class='action' action='/gsc' method='GET'>"
    "<input type='submit' value='Send Another GSC Page'>"
    "</form>"
    "<form class='action' action='/' method='GET'>"
    "<input type='submit' value='Return to Encoder Selection'>"
    "</form>"
    "</body>"
    "</html>"
);

        request->send(200, "text/html", html);

        Serial.println("---------------");
        Serial.println("GSC FRAME");
        Serial.print("MESSAGE: ");
        Serial.println(gscMsg);
        Serial.print("CAPCODE: ");
        Serial.println(gscCap);
        Serial.println("BAUD RATE: 600");
        Serial.print("FUNCTION CODE: ");
        Serial.println(gscFunctionCode);
        Serial.print("DATA TYPE: ");
        Serial.println(gscData_type);
        Serial.println("---------------");

        gscWebsiteDelivered = true;
        transmitGSC = true;

        Serial.println("---------------");
        Serial.println("GSC FRAME");
        Serial.print("MESSAGE: ");
        Serial.println(gscMsg);
        Serial.print("CAPCODE: ");
        Serial.println(gscCap);
        Serial.print("FUNCTION CODE: ");
        Serial.println(gscFunctionCode);
        Serial.print("DATA TYPE: ");
        Serial.println(gscData_type);
        Serial.println("---------------");
      }

      else {
        String html = String(gsc_index_html);

        html.replace("{{MSG}}", gscMsg);
        html.replace("{{CAP}}", gscCap);

        html.replace(
            "{{FC_0}}",
            gscFunctionCode == "0" ? "selected" : ""
        );

        html.replace(
            "{{FC_1}}",
            gscFunctionCode == "1" ? "selected" : ""
        );

        html.replace(
            "{{FC_2}}",
            gscFunctionCode == "2" ? "selected" : ""
        );

        html.replace(
            "{{FC_3}}",
            gscFunctionCode == "3" ? "selected" : ""
        );

        html.replace(
            "{{DATA_TYPE_ALPHA}}",
            gscDataType == "1" ? "selected" : ""
        );

        html.replace(
            "{{DATA_TYPE_NUMERIC}}",
            gscDataType == "0" ? "selected" : ""
        );

        request->send(200, "text/html", html);
      }
    }
  );

  /*--------------------------------------------------------------------
    Root website
  ------------------------------------------------------------------*/
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<!-- make the viewport follow the device width -->
<meta name="viewport" content="width=device-width, initial-scale=1">

<style>
/* ---------- colours & utilities ---------- */
:root{
  --bg-dark:#0a0a12;
  --bg-grad:#0d1a12;
  --bg-deep:#060808;
  --accent:#00ff88;
  --text-main:#33ff66;
  --text-sub:#e0e0e0;
  --border:#0f3460;
}

/* ---------- page background ---------- */
body{
  background-color:var(--bg-dark);
  color:var(--text-main);
  font-family:'Courier New',monospace;
  min-height:100vh;
  margin:0;
  padding:2rem;
  text-shadow:
    0 0 2px var(--text-main),
    0 0 8px rgba(51,255,102,.4),
    0 0 20px rgba(51,255,102,.15);
}

/* hum (breathing) */
body::before{
  content:"";
  position:fixed;
  inset:0;
  background:radial-gradient(ellipse at 50% 46%,var(--bg-grad) 0%,var(--bg-deep) 78%);
  animation:hum 5.2s steps(6) infinite;
  z-index:-2;
}
@keyframes hum{
  0%,100%{filter:brightness(0.96);}
  50%{filter:brightness(1.08);}
}

/* scanlines */
body::after{
  content:"";
  position:fixed;
  inset:0;
  background:repeating-linear-gradient(
    0deg,
    rgba(0,0,0,.35) 0,
    rgba(0,0,0,.35) 1px,
    transparent 1px,
    transparent 3px);
  animation:scan-drift 2.6s linear infinite;
  z-index:-1;
}
@keyframes scan-drift{to{background-position:0 3px;}}

/* vignette & glass highlight */
.vignette,.glass{
  content:"";
  position:fixed;
  inset:0;
  pointer-events:none;
}
.vignette{
  background:radial-gradient(
    ellipse at 50% 50%,
    transparent 45%,
    rgba(0,0,0,.7) 100%);
  z-index:10;
}
.glass{
  background:radial-gradient(
    40% 30% at 22% 8%,
    rgba(255,255,255,.04) 0%,
    transparent 60%);
  z-index:11;
}

/* ---------- headings ---------- */
h1{
  color:var(--accent);
  margin-top:0;
}

/* ---------- form layout (flex) ---------- */
form{
  display:flex;
  flex-wrap:wrap;
  gap:0.5rem 1rem;
  max-width:40rem;
  margin:1rem 0;
}
form label{
  flex:1 0 8rem;            /* keep a sensible min width */
  font-weight:bold;
  color:#aaa;
  align-self:center;
}
form input[type=text],
form input[type=number],
form select{
  flex:2 1 0;
  min-width:0;
  background:#16213e;
  color:var(--text-sub);
  border:1px solid var(--border);
  border-radius:4px;
  padding:0.4rem 0.6rem;
  font-family:monospace;
  box-sizing:border-box;
}
form input:focus,
form select:focus{
  outline:none;
  border-color:var(--accent);
}
form input[type=submit]{
  flex:1 0 100%;            /* full‑width button on its own line */
  background:var(--border);
  color:var(--accent);
  border:1px solid var(--accent);
  border-radius:4px;
  padding:0.5rem 1rem;
  cursor:pointer;
  font-family:monospace;
}
form input[type=submit]:hover{
  background:var(--accent);
  color:#1a1a2e;
}

/* ---------- responsive tweaks ---------- */
@media (max-width:600px){
  body{padding:1rem;}
  h1{font-size:1.5rem;}
  form{gap:0.4rem;}
}
</style>
</head>
<body>
  <div class="vignette"></div>
  <div class="glass"></div>

  <h1>ESPager: Select Encoder</h1>
)rawliteral";

html += pager_ascii_art;   // keep your ASCII art insertion

html += R"rawliteral(
  <br><br>
  <form class="action" action="/pocsag" method="GET">
    <input type="submit" value="Open POCSAG Encoder">
  </form>

  <br>

  <form class="action" action="/gsc" method="GET">
    <input type="submit" value="Open GSC Encoder">
  </form>

  <br>

  <form class="action" action="/update" method="GET">
    <input type="submit" value="Update ESPager Firmware">
  </form>

  <br>
<img src="data:image/png;base64,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" alt="" width="326" height="81" />
</body>
</html>
)rawliteral";

    request->send(200, "text/html", html);
  });

    /*
      Enable OTA if required.

      If ElegantOTA is enabled, this normally exposes the OTA page at
      /update.
    */
    ElegantOTA.begin(&server);

    server.begin();
    Serial.println("HTTP server started.");
    Serial.println("POCSAG page: /pocsag");
    Serial.println("GSC page:    /gsc");
  }

/*--------------------------------------------------------------------
  Main loop
--------------------------------------------------------------------*/

void loop() {
  // Sleep mode
  unsigned long now = millis();
  // Non-blocking timer check
  if (ENABLE_SLEEP && (now - lastLoopCheck >= LOOP_CHECK_INTERVAL)) {
    lastLoopCheck = now;
    unsigned long timeSinceLastBeat = now - lastHeartbeatTime;
    // Enter sleep only if timeout exceeded and sleep is enabled
    if (ENABLE_SLEEP && timeSinceLastBeat > SLEEP_TIMEOUT) {
      Serial.println("ESP32 entering light sleep (GPIO wakeup only)...");
      Serial.flush();
      disableCore0WDT();  // Disable before sleeping
      esp_light_sleep_start();
      Serial.println("ESP32 woke from light sleep");
      Serial.flush();
    }
  }

  ElegantOTA.loop();

  /*--------------------------------------------------------------
    POCSAG transmission
  --------------------------------------------------------------*/

  if (WebsiteDelivered) {
    WebsiteDelivered = false;

    if (TransmitPocsag) {
      TransmitPocsag = false;

      bool success = txPOCSAGMessage(
          static_cast<long>(cap.toInt()),
          msg.c_str(),
          baud.toInt(),
          function_code.toInt(),
          data_type.toInt(),
          inverted.toInt(),
          repeat.toInt()
      );

      if (success) {
        Serial.println("POCSAG transmission complete.");
      } else {
        Serial.println("POCSAG transmission failed.");
      }
    }
  }

  /*--------------------------------------------------------------
    GSC transmission
  --------------------------------------------------------------*/

  if (gscWebsiteDelivered) {
    gscWebsiteDelivered = false;

    if (transmitGSC) {
      transmitGSC = false;

      uint32_t capcode =
          static_cast<uint32_t>(gscCap.toInt());

      uint8_t functionCode =
          static_cast<uint8_t>(gscFunctionCode.toInt());

      GSCMessageType messageType =
          gscDataType == "0"
              ? GSC_MESSAGE_NUMERIC
              : GSC_MESSAGE_ALPHA;

      bool success = sendGSCFrame(
          capcode,
          gscMsg.c_str(),
          messageType,
          functionCode
      );

      if (success) {
        Serial.println("GSC transmission complete.");
      } else {
        Serial.println("GSC transmission failed.");
      }
    }
  }
}