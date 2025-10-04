// Pablo Garrido v1

#include <WiFi.h>
#include <PubSubClient.h>
#include <time.h>
#include <WebServer.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <HTTPClient.h>

#define sensores_maximos 4

float temperaturaC[sensores_maximos];
char fecha[11];
char horaF[9];
char fechaHoraRFC[30];

// Configuración Wi-Fi
const char* ssid =  "ssid"; //Sustituir SSID
const char* password = "contraseña"; //Sustituir Contraseña

// Configuración MQTT
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
WiFiClient espClient;
PubSubClient client(espClient);

// Configuración NTP
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -14400;  // Venezuela UTC-4
const int daylightOffset_sec = 0;
unsigned long LastSync = 0;

// Web Server
WebServer server(80);

// One Wire
#define ONE_WIRE_BUS 4  // Pin conectado a Data Query
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// DeviceSensorAddress
DeviceAddress sensor1 = { 0x28, 0x24, 0x13, 0xB2, 0x00, 0x00, 0x00, 0xD0 };
DeviceAddress sensor2 = { 0x28, 0xA6, 0x40, 0x47, 0x00, 0x00, 0x00, 0x7F };
DeviceAddress sensor3 = { 0x28, 0x6A, 0x8D, 0xB2, 0x00, 0x00, 0x00, 0xA3 };
DeviceAddress sensor4 = { 0x28, 0xFF, 0x64, 0x1E, 0xCD, 0x07, 0x57, 0x1E };

// TelegramBOT
String botToken = "token"; //Sustituir token bot
String chatID = "chatid";  //Sustituir chatid
String mensaje = "N/A";


void handleRoot() {
  String selectHTML = "";
  for (int i = 0; i < sensores_maximos; i++) {
    selectHTML += "<option value='" + String(i) + "'>" + String(i + 1) + "</option>";
  }

  String html = R"rawliteral(
  <!DOCTYPE html>
  <html lang="es">
  <head>
      <meta charset="UTF-8">
      <title>TYT - Monitor de Temperatura</title>
      <style>
        body {
          font-family: monospace;
          background: #f0f0f0;
          text-align: center;
          padding-top: 20px;
        }
        select {
          font-size: 16px;
          margin: 10px;
        }
        .barra-container {
          display: flex;
          justify-content: center;
          gap: 40px;
          margin-top: 20px;
        }
        .barra {
          width: 40px;
          height: 200px;
          background: #e0e0e0;
          border: 1px solid #999;
          position: relative;
        }
        .relleno {
          width: 100%;
          position: absolute;
          bottom: 0;
          transition: height 0.5s ease;
        }
        .etiqueta {
          margin-top: 8px;
          font-size: 16px;
        }
      </style>
    </head>
    <body>

    <h2>Visualización en tiempo real</h2>

    <div class="barra-container">
      <div>
        <div class="barra">
          <div id="barraFija1" class="relleno" style="background: #8bc34a; height: 0%;"></div>
        </div>
        <div id="barratexto1" class="etiqueta">0.0 °C</div>
      </div>

      <div>
        <div class="barra">
          <div id="barraFija2" class="relleno" style="background: #03a9f4; height: 0%;"></div>
        </div>
        <div id="barratexto2" class="etiqueta">0.0 °C</div>
      </div>

      <div>
        <div class="barra">
          <div id="barraFija3" class="relleno" style="background: #ff9800; height: 0%;"></div>
        </div>
        <div id="barratexto3" class="etiqueta">0.0 °C</div>
      </div>

    <div>
        <div class="barra">
          <div id="barraFija4" class="relleno" style="background: #ff9800; height: 0%;"></div>
        </div>
        <div id="barratexto4" class="etiqueta">0.0 °C</div>
      </div>
    </div>

    <h3>Selecciona los sensores</h3>
    Sensor 1:
    <select id="sensor1">)rawliteral";

      html += selectHTML;
      html += R"rawliteral(</select>
    Sensor 2:
    <select id="sensor2">)rawliteral";

      html += selectHTML;
      html += R"rawliteral(</select>

  <h3 style="margin-top: 20px;">Diferencia de temperatura</h3>
  <div id="diferenciaTemp" style="
    display: inline-block;
    padding: 10px 20px;
    background-color: #e0e0e0;
    border-radius: 8px;
    font-size: 24px;
    font-weight: bold;
    color: #333;
    margin-bottom: 10px;
  ">
    0.0 °C
  </div>

  <h3 style="margin-top:30px;">Gráfico de temperatura</h3>
  <canvas id="graficoTemp" width="400" height="200"></canvas>

  <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
  <script>

    function actualizarBarraTemp(valor, idBarra, idTexto) {
      const barra = document.getElementById(idBarra);
      const texto = document.getElementById(idTexto);
      if (!barra || !texto) return;

        if (valor == -127){
        barra.style.height = '100%';
        barra.style.backgroundColor = '#000000';
        texto.textContent = 'Error';
        return;}

      const temp = Math.max(-30, Math.min(100, valor));
      const porcentaje = ((temp + 30) / 130) * 100;

      barra.style.height = `${porcentaje}%`;

      // Color dinámico

      if (temp < 0) {
        barra.style.backgroundColor = '#2196f3';
      } else if (temp < 35) {
        barra.style.backgroundColor = '#4caf50';
      } else if (temp < 70) {
        barra.style.backgroundColor = '#ff9800';
      } else {
        barra.style.backgroundColor = '#f44336';
      }

      texto.textContent = `${temp.toFixed(1)} °C`;
    }

    let sensor1Index = 0;
    let sensor2Index = 1;

    const ctx = document.getElementById('graficoTemp').getContext('2d');

    const datos = {
      labels: [],
      datasets: [
        {
          label: 'Sensor 1 (°C)',
          data: [],
          borderColor: '#00bcd4',
          tension: 0.3,
          fill: false
        },
        {
          label: 'Sensor 2 (°C)',
          data: [],
          borderColor: '#ff5722',
          tension: 0.3,
          fill: false
        },
        {
          label: 'ΔT (°C)',
          data: [],
          borderColor: '#4caf50',
          borderDash: [5, 5],
          tension: 0.3,
          fill: false
        }
      ]
    };

    const grafico = new Chart(ctx, {
      type: 'line',
      data: datos,
      options: {
        scales: {
          x: { title: { display: true, text: 'Hora' } },
          y: {
            suggestedMin: 0,
            suggestedMax: 100,
            title: { display: true, text: '°C' }
          }
        }
      }
    });

    function ajustarEscalaY() {
      const todosValores = datos.datasets.flatMap(ds => ds.data);
      if (todosValores.length === 0) return;

      const min = Math.min(...todosValores);
      const max = Math.max(...todosValores);
      const margen = 10;

      grafico.options.scales.y.min = Math.floor(min - margen);
      grafico.options.scales.y.max = Math.ceil(max + margen);
    }

    function limpiarGrafico() {
      datos.labels = [];
      datos.datasets.forEach(ds => ds.data = []);
      grafico.update();
    }

    function actualizarDesdeJSON() {
      fetch('/datos')
        .then(res => res.json())
        .then(data => {
          const hora = data.hora || "00:00:00";
          const temps = Array.isArray(data.temp) ? data.temp : [];

          const val1 = temps[sensor1Index] !== undefined ? temps[sensor1Index] : 0;
          const val2 = temps[sensor2Index] !== undefined ? temps[sensor2Index] : 0;
          
          const delta = val1 - val2;

          if (datos.labels.length > 19) {
            datos.labels.shift();
            datos.datasets.forEach(ds => ds.data.shift());
          }

          datos.labels.push(hora);
          datos.datasets[0].data.push(val1);
          datos.datasets[1].data.push(val2);
          datos.datasets[2].data.push(delta);

          ajustarEscalaY();
          grafico.update();

          const diferencia = Math.abs(val1 - val2);
          document.getElementById("diferenciaTemp").textContent = diferencia.toFixed(1);

          actualizarBarraTemp(temps[0], "barraFija1","barratexto1");
          actualizarBarraTemp(temps[1], "barraFija2","barratexto2");
          actualizarBarraTemp(temps[2], "barraFija3","barratexto3");
          actualizarBarraTemp(temps[3], "barraFija4","barratexto4");
        });
    }

    setInterval(actualizarDesdeJSON, 2000);
    actualizarDesdeJSON();

    document.getElementById("sensor1").addEventListener("change", () => {
      sensor1Index = parseInt(document.getElementById("sensor1").value);
      limpiarGrafico();
    });

    document.getElementById("sensor2").addEventListener("change", () => {
      sensor2Index = parseInt(document.getElementById("sensor2").value);
      limpiarGrafico();
    });

    </script>

  </body>
  </html>
  )rawliteral";

  server.send(200, "text/html", html);
}

String generarJSON_MQTT(){
  String json = "{";
  json += "\"f\":\"" + String(fechaHoraRFC) + "\",";
  json += "\"t\":[";

  for (int i = 0; i < sensores_maximos; i++) {
    json += String(temperaturaC[i], 1);  // 1 decimal
    if (i < sensores_maximos - 1) {
      json += ",";
    }
  }

  json += "]}";

  // Calcular el tamaño del json
  //int tamanoBytes = json.length();

  //Serial.print("El tamaño del JSON es: ");
  //Serial.print(tamanoBytes);
  //Serial.println(" bytes.");

  return json;
}

String generarJSON() {
  String json = "{";
  json += "\"fecha\":\"" + String(fecha) + "\",";
  json += "\"hora\":\"" + String(horaF) + "\",";
  json += "\"temp\":[";

  for (int i = 0; i < sensores_maximos; i++) {
    json += String(temperaturaC[i], 1);  // 1 decimal
    if (i < sensores_maximos - 1) {
      json += ",";
    }
  }

  json += "]}";

  // Calcular el tamaño del json
  //int tamanoBytes = json.length();

  //Serial.print("El tamaño del JSON es: ");
  //Serial.print(tamanoBytes);
  //Serial.println(" bytes.")

  return json;
}

void handleDatos() {
  String json = generarJSON();
  server.send(200, "application/json", json);
}

void sincronizarNTP(){
  Serial.print("Resincronizando con el servidor NTP\n");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  LastSync = millis();  // Reiniciar conteo
}

void escanearSensores() {
  byte address[8];
  int count = 0;

  Serial.println("Escaneando sensores DS18B20...");

  oneWire.reset_search();

  while (oneWire.search(address)) {
    Serial.print("Sensor ");
    Serial.print(++count);
    Serial.print(" - Dirección: ");

    for (int i = 0; i < 8; i++) {
      if (address[i] < 16) Serial.print("0");
      Serial.print(address[i], HEX);
    }

    Serial.println();
  }

  if (count == 0) {
    Serial.println("No se detectaron sensores.");
  }

  Serial.println("-----------------------------");
}

void pubTelegram(String mensaje) {
  HTTPClient http;
  String url = "https://api.telegram.org/bot" + botToken + "/sendMessage";

  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  String payload = "{\"chat_id\":\"" + chatID + "\",\"text\":\"" + mensaje + "\"}";
  int httpResponseCode = http.POST(payload);

  if (httpResponseCode > 0) {
    Serial.println("Mensaje enviado: " + mensaje);
  } else {
    Serial.println("Error al enviar: " + String(httpResponseCode));
  }

  http.end();
}

String payloadString(const char* fecha, const char* horaF, float* temperaturaC) {
  String resultado = String(fecha) + " " + String(horaF) + " | ";

  for (int i = 0; i < sensores_maximos; i++) {
    resultado += String(temperaturaC[i], 1) + " °C";
    if (i < 3) {
      resultado += " ";
    }
  }

  return resultado;
}

void setup() {
  Serial.begin(9600);
  sensors.begin();
  
  sensors.setResolution(sensor1, 12);
  sensors.setResolution(sensor2, 12);
  sensors.setResolution(sensor3, 12); 
  sensors.setResolution(sensor4, 12); // Este sesor no agarra la resolución, se queda en 12 siempre

  //Conexión a un SSID principal

  int timeconnect = 20; //El numero de veces que tratará de conectarse
  
  // Conexion WiFi
  WiFi.begin(ssid, password);
  Serial.print("Conectando a Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");

    timeconnect = timeconnect-1;
    if(timeconnect == 0){
      break;
    }
  }

  // Conexión al SSID backup 

  if(timeconnect==0){
    WiFi.begin("tyt_tempmonitor", "tytusb1");
    Serial.print("Conectando a Wi-Fi");
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
  }

  Serial.println("\n Conectado a Wi-Fi");
  Serial.print("IP asignada: ");
  Serial.println(WiFi.localIP());

  // Pub IP al Bot de telegram
  mensaje = "IP del Web Server: " + WiFi.localIP().toString();
  pubTelegram(mensaje);

  // Servidor Web
  server.on("/", handleRoot);
  server.on("/datos", handleDatos);
  server.begin();

  // Setup MQTT
  client.setServer(mqtt_server, mqtt_port);

  // Setup NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

void loop() {
server.handleClient();

  // Resincronizar NTP (Cada 30 minutos)
  if (millis() - LastSync >= 180000) { 
    sincronizarNTP();
  }

  // Reconexión WIFI
  if(WiFi.status() != WL_CONNECTED){
    Serial.print("Reconectando a Wi-Fi");
    WiFi.disconnect();
    WiFi.begin(ssid, password);

    if(WiFi.status() == WL_CONNECTED){

      // Publicación UART
      Serial.println("\n Conexión Wi-Fi reestablecida\n");
      Serial.print("IP asignada: ");
      Serial.println(WiFi.localIP());

      // Publicar en Bot Telegram
      mensaje = "Reconectado al Web Web Server: - IP " + WiFi.localIP().toString();
      pubTelegram(mensaje);

    } else{

      // Publicación UART
      Serial.println("\n No se pudo restablecer la conexión Wi-Fi");

      // Publicación Bot Telegram
      mensaje = "No se pudo restablecer la conexión Wi-Fi";
      pubTelegram(mensaje);
    }
  }

  // Reconexión MQTT
  if (!client.connected()) {

    Serial.print("Reconectando al broker MQTT... ");
    if (client.connect("TYT_tempmonitor")) {
      Serial.println("Conectado");
    } else {
      Serial.print("Error: ");
      Serial.println(client.state());
      //delay(5000);
      //return;
      mensaje = "Error MQTT. Código: " + String(client.state());
    }
    pubTelegram(mensaje);
  }

  client.loop();

  // Medición de temperatura
  sensors.requestTemperatures();
  temperaturaC[0] = sensors.getTempC(sensor1);
  temperaturaC[1] = sensors.getTempC(sensor2);
  temperaturaC[2] = sensors.getTempC(sensor3);
  temperaturaC[3] = sensors.getTempC(sensor4);

  
  // Obtener fecha y hora en formato RFC3339
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {

    // Hora para el servidor web
      snprintf(horaF, sizeof(horaF), "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
      snprintf(fecha, sizeof(fecha), "%04d-%02d-%02d", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
      
    // Hora para telegraf
      snprintf(fechaHoraRFC, sizeof(fechaHoraRFC),
            "%04d-%02d-%02dT%02d:%02d:%02d-04:00",
            timeinfo.tm_year + 1900,
            timeinfo.tm_mon + 1,
            timeinfo.tm_mday,
            timeinfo.tm_hour,
            timeinfo.tm_min,
            timeinfo.tm_sec);

    //Publicar
    String json = generarJSON_MQTT();
    bool enviado = client.publish("esp32/pxbl", json.c_str(), true); //QoS 0
    Serial.println(enviado ? "Publicado: " + json : "Fallo al publicar");

  } else {
    strcpy(horaF, "00:00:00");
    strcpy(fecha, "0000-00-00");
    sincronizarNTP();

    // No se publica por MQTT
  }

  // Imprimir por puerto serial
  Serial.printf("%s %s | %.1f °C %.1f °C %.1f °C %.1f °C\n\n", fecha, horaF, temperaturaC[0], temperaturaC[1], temperaturaC[2], temperaturaC[3]);

  //String mensaje = payloadString(fecha, horaF, temperaturaC);

  // Imprimir por serial
  //Serial.println(mensaje);

  // Publicar al bot de telegram
  //pubTelegram(mensaje);

  delay(5000);
}