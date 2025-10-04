\\ Autor: Pablo Garrido

# esp32_TYT_tempMonitor_v1 es el código del ESP32
- Mide la temperatura de los sensores DS18B20 conectados l bus 1-Wire y los publica por MQTT. 
- Envía mensajes mediante el protocolo HTTP hacia la API BOT de Telegram.
- Aloja un servidor Web para responder a solicitudes HTTP desde el navegador.


# mqttdb
Aloja el ecosistema de almacenamiento y visualización mediante el despliegue de contenedores con Docker

# Sistema de Alertas
Aloja los scripts mediante consultas FLUX para el Sistema de Alertas utilizando Tasks en InfluxDB