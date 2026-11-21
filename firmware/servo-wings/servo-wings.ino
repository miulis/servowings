#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

// Datos de la red WiFi --> cambiar antes de usar
const char* ssid = "wifiname";
const char* password = "password";

WebServer server(80);
Servo wingLeft, wingRight;

// Pines de señal para cada servo
const int PIN_LEFT = 4;
const int PIN_RIGHT = 3;

bool flapping = false;
unsigned long startTime = 0;


const float CENTER_LEFT = 90;    // reposo del ala izq
const float CENTER_RIGHT = 90;   // reposo del ala der
const float AMPLITUDE = 25;      // cuantos grados se mueven desde el centro (espejado)
const float PERIOD_MS = 1800;    // tiempo que tarda en aletear una vez

// Web de control
const char HTML_INDEX[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Servo Wings</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      background-color: #0d0f12;
      color: #e2e8f0;
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
      display: flex;
      justify-content: center;
      align-items: center;
      min-height: 100vh;
      padding: 24px;
    }
    .card {
      background: #161b22;
      border: 1px solid #21262d;
      border-radius: 16px;
      padding: 32px 24px;
      width: 100%;
      max-width: 360px;
      box-shadow: 0 8px 24px rgba(0,0,0,0.5);
    }
    h1 {
      font-size: 1.1rem;
      font-weight: 600;
      color: #8b949e;
      text-transform: uppercase;
      letter-spacing: 0.1em;
      margin-bottom: 20px;
      text-align: center;
    }
    .status-badge {
      display: flex;
      align-items: center;
      justify-content: center;
      gap: 8px;
      padding: 10px 16px;
      border-radius: 20px;
      font-size: 0.85rem;
      font-weight: 600;
      letter-spacing: 0.04em;
      margin-bottom: 24px;
      background-color: #0d1117;
      border: 1px solid #30363d;
      transition: all 0.2s ease;
    }
    .dot {
      width: 8px;
      height: 8px;
      border-radius: 50%;
      background-color: #484f58;
      transition: background-color 0.2s ease;
    }
    .status-badge.active {
      border-color: #238636;
      color: #3fb950;
    }
    .status-badge.active .dot {
      background-color: #3fb950;
      box-shadow: 0 0 8px #3fb950;
    }
    .status-badge.inactive {
      border-color: #30363d;
      color: #8b949e;
    }
    .btn-group {
      display: flex;
      flex-direction: column;
      gap: 12px;
    }
    button {
      width: 100%;
      padding: 16px;
      font-size: 0.95rem;
      font-weight: 600;
      letter-spacing: 0.03em;
      border: 1px solid transparent;
      border-radius: 10px;
      cursor: pointer;
      transition: background-color 0.15s ease, border-color 0.15s ease;
      -webkit-tap-highlight-color: transparent;
    }
    .btn-on {
      background-color: #1f6feb;
      color: #ffffff;
      border-color: #388bfd;
    }
    .btn-on:active {
      background-color: #1158c7;
    }
    .btn-off {
      background-color: #21262d;
      color: #c9d1d9;
      border-color: #30363d;
    }
    .btn-off:active {
      background-color: #161b22;
      color: #8b949e;
    }
  </style>
</head>
<body>
  <div class="card">
    <h1>Servo Wings</h1>

    <div id="statusBadge" class="status-badge inactive">
      <span class="dot"></span>
      <span id="statusText">OFF</span>
    </div>

    <div class="btn-group">
      <button class="btn-on" onclick="sendCommand('/on')">Turn wings on</button>
      <button class="btn-off" onclick="sendCommand('/off')">Turn wings off</button>
    </div>
  </div>

  <script>
    function updateUI(data) {
      const badge = document.getElementById('statusBadge');
      const text = document.getElementById('statusText');

      if (data.flapping) {
        badge.className = 'status-badge active';
        text.innerText = 'ON';
      } else {
        badge.className = 'status-badge inactive';
        text.innerText = 'OFF';
      }
    }

    function sendCommand(path) {
      fetch(path)
        .then(res => res.json())
        .then(data => updateUI(data))
        .catch(err => console.error(err));
    }

    function getStatus() {
      fetch('/status')
        .then(res => res.json())
        .then(data => updateUI(data))
        .catch(err => console.error(err));
    }

   
    setInterval(getStatus, 3000);
    getStatus();
  </script>
</body>
</html>
)rawliteral";

// JSON simple avusa si están aleteando o no
void sendStatusJSON() {
  String json = String("{\"flapping\":") + (flapping ? "true" : "false") + "}";
  server.send(200, "application/json", json);
}

void setup() {
  //frecuencia estandar de los servos
  wingLeft.setPeriodHertz(50);
  wingRight.setPeriodHertz(50);

  // ancho de pulso de los servos
  wingLeft.attach(PIN_LEFT, 500, 2400);
  wingRight.attach(PIN_RIGHT, 500, 2400);

  // posicion inicial
  wingLeft.write(CENTER_LEFT);
  wingRight.write(CENTER_RIGHT);

  WiFi.softAP(ssid, password);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);

  server.on("/", []() {
    server.send_P(200, "text/html", HTML_INDEX);
  });

  server.on("/status", sendStatusJSON);

  server.on("/on", []() {
    if (!wingLeft.attached()) wingLeft.attach(PIN_LEFT, 500, 2400);
    if (!wingRight.attached()) wingRight.attach(PIN_RIGHT, 500, 2400);

    flapping = true;
    startTime = millis();
    sendStatusJSON();
  });

  server.on("/off", []() {
    flapping = false;
    wingLeft.detach();
    wingRight.detach();
    sendStatusJSON();
  });

  server.begin();
}

void loop() {
  server.handleClient();

  if (flapping) {
    //onda senoidal para movimiento fluido
    float t = (millis() - startTime) / PERIOD_MS;
    float offset = AMPLITUDE * sin(2 * PI * t);
    // movimiento espejado
    wingLeft.write(CENTER_LEFT + offset);
    wingRight.write(CENTER_RIGHT - offset);
  }

  delay(15);
}
