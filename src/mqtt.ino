/*
  mqtt.ino - FreeDs mqtt functions
  Derivador de excedentes para ESP32 DEV Kit // Wifi Kit 32
  
  Copyright (C) 2020 Pablo Zerón (https://github.com/pablozg/freeds)

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


void connectToWifi()
{
  Serial.println("Connecting to Wi-Fi...");
  #ifdef OLED
    showLogo(_CONNECTING_, false);
  #endif

  wifiMulti.addAP(config.ssid1, config.pass1);
  wifiMulti.addAP(config.ssid2, config.pass2);

  while (wifiMulti.run() != WL_CONNECTED)
  {
    delay(500);
    INFO(".");
  }
  
  if (config.dhcp == false)
  {
    IPAddress local_IP, gateway, subnet, primaryDNS, secondaryDNS;
    local_IP.fromString((String)config.ip);
    gateway.fromString((String)config.gw);
    subnet.fromString((String)config.mask);
    primaryDNS.fromString((String)config.dns1);
    secondaryDNS.fromString((String)config.dns2);

    if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS))
    {
      Serial.println("Fixed IP -  Failed to configure");
    }
  }

  WiFi.setSleep(false);
  WiFi.persistent(false);
  scanDoneCounter = 0;
}

void errorConnectToWifi(void)
{
  Error.ConexionWifi = true;
#ifdef OLED
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 0, "WIFI");
  display.drawString(64, 16, "ERROR");
  display.drawString(64, 30, _PRGRESTORE_);
  display.drawString(64, 40, _WAIT_);
  display.display();
#endif
  timers.ErrorConexionWifi = millis();
  INFOLN(PSTR("AP Not valid, Waiting 30 seconds before restart, press 'prg' button to defaults settings or 'rst' button to restart now"));
  
  while ((millis() - timers.ErrorConexionWifi) < 30000)
  {
    if (digitalRead(0) == LOW)
    {
      ButtonLongPress = true;
      break;
    }
    yield();
  }
  
  if (ButtonLongPress)
  {
    INFOLN(PSTR("Prg pressed"));
    defaultValues();
    saveEEPROM();
    delay(1000);
    ESP.restart();
  }
  else
  {
    INFOLN(PSTR("No PRG pressed"));
    ESP.restart();
  }
}

void connectToMqtt()
{
  if (config.wversion != 0 && config.mqtt)
  {
    INFOLN(PSTR("Connecting to MQTT..."));
    mqttClient.connect();
  }
}

void WiFiEvent(WiFiEvent_t event)
{
  INFO(PSTR("[WiFi-event] event: "));
  INFOLN(event);

  switch (event)
  {
  case SYSTEM_EVENT_STA_GOT_IP:
    Error.ConexionWifi = false;
    INFOLN(PSTR("WiFi connected"));
    INFO(PSTR("IP address: "));
    INFOLN(WiFi.localIP());
    if (config.mqtt && config.wversion != 0)
    {
      connectToMqtt();
    }
    else
    {
      xTimerStop(mqttReconnectTimer, 0);
    }
    break;
  case SYSTEM_EVENT_STA_DISCONNECTED:
    Error.ConexionWifi = true;
    INFOLN(PSTR("WiFi lost connection"));
    xTimerStop(mqttReconnectTimer, 0); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
    xTimerStart(wifiReconnectTimer, 0);
    break;
  case SYSTEM_EVENT_WIFI_READY:
  case SYSTEM_EVENT_SCAN_DONE:
    scanDoneCounter++;
    if (scanDoneCounter >= 5)
    {
      errorConnectToWifi();
    }
    break;
  case SYSTEM_EVENT_STA_START:
  case SYSTEM_EVENT_STA_STOP:
  case SYSTEM_EVENT_STA_CONNECTED:
  case SYSTEM_EVENT_STA_AUTHMODE_CHANGE:
  case SYSTEM_EVENT_STA_LOST_IP:
  case SYSTEM_EVENT_STA_WPS_ER_SUCCESS:
  case SYSTEM_EVENT_STA_WPS_ER_FAILED:
  case SYSTEM_EVENT_STA_WPS_ER_TIMEOUT:
  case SYSTEM_EVENT_STA_WPS_ER_PIN:
  case SYSTEM_EVENT_STA_WPS_ER_PBC_OVERLAP:
  case SYSTEM_EVENT_AP_START:
  case SYSTEM_EVENT_AP_STOP:
  case SYSTEM_EVENT_AP_STACONNECTED:
  case SYSTEM_EVENT_AP_STADISCONNECTED:
  case SYSTEM_EVENT_AP_STAIPASSIGNED:
  case SYSTEM_EVENT_AP_PROBEREQRECVED:
  case SYSTEM_EVENT_GOT_IP6:
  case SYSTEM_EVENT_ETH_START:
  case SYSTEM_EVENT_ETH_STOP:
  case SYSTEM_EVENT_ETH_CONNECTED:
  case SYSTEM_EVENT_ETH_DISCONNECTED:
  case SYSTEM_EVENT_ETH_GOT_IP:
  case SYSTEM_EVENT_MAX:
    break;
  }
}

void onMqttConnect(bool sessionPresent)
{
  INFOLN("Connected to MQTT");
  Error.ConexionMqtt = false;

  mqttClient.subscribe("freeds/cmnd/pwm", 0);
  mqttClient.subscribe("freeds/cmnd/pwmman", 0);
  mqttClient.subscribe("freeds/cmnd/pwmmanvalue", 0);
  mqttClient.subscribe("freeds/cmnd/screen", 0);
  mqttClient.subscribe("freeds/cmnd/pwmfrec", 0);
  mqttClient.subscribe("freeds/cmnd/brightness", 0);

  if (config.wversion == 3) { suscribeMqttMeter(); }
}

void suscribeMqttMeter(void)
{
  mqttClient.subscribe(config.Solax_mqtt, 0);
  mqttClient.subscribe(config.Meter_mqtt, 0);
}

void publisher(const char *topic, const char *topublish)
{
  mqttClient.publish(topic, 0, true, topublish);
}

void publishMqtt()
{
  //  MQTT TOPICS:
  // freeds/pwm -> PWM Value
  // freeds/pv1c -> corriente string 1
  // freeds/pv2c -> corriente string 2
  // freeds/pv1v -> tension string 1
  // freeds/pv2v -> tension string 2
  // freeds/pw1 -> potencia string 1
  // freeds/pw2 -> potencia string 2
  // freeds/gridv ->  tension de red
  // freeds/wsolar ->  Potencia solar
  // freeds/wtoday ->  Potencia solar diaria
  // freeds/wgrid ->  Potencia de red (Negativo: de red - Positivo: a red)
  // freeds/wtogrid -> Potencia enviada a red

  DEBUGLN("\r\nPUBLISHMQTT()");

  if (config.mqtt && !Error.ConexionMqtt && config.wversion != 0)
  {
    static char tmpString[33];

    dtostrfd(inverter.pv1c, 2, tmpString);
    publisher("freeds/pv1c", tmpString);
    dtostrfd(inverter.pv2c, 2, tmpString);
    publisher("freeds/pv2c", tmpString);
    dtostrfd(inverter.pw1, 2, tmpString);
    publisher("freeds/pw1", tmpString);
    dtostrfd(inverter.pw2, 2, tmpString);
    publisher("freeds/pw2", tmpString);
    dtostrfd(inverter.pv1v, 2, tmpString);
    publisher("freeds/pv1v", tmpString);
    dtostrfd(inverter.pv2v, 2, tmpString);
    publisher("freeds/pv2v", tmpString);
    dtostrfd(inverter.gridv, 2, tmpString);
    publisher("freeds/gridv", tmpString);
    dtostrfd(inverter.wsolar, 2, tmpString);
    publisher("freeds/wsolar", tmpString);
    dtostrfd(inverter.wtoday, 2, tmpString);
    publisher("freeds/wtoday", tmpString);
    dtostrfd(inverter.wgrid, 2, tmpString);
    publisher("freeds/wgrid", tmpString);
    dtostrfd(inverter.wtogrid, 2, tmpString);
    publisher("freeds/wtogrid", tmpString);
    publisher("freeds/pwm", pro.c_str());
    publisher(config.R01_mqtt, digitalRead(PIN_RL1) ? "ON" : "OFF");
    publisher(config.R02_mqtt, digitalRead(PIN_RL2) ? "ON" : "OFF");
    publisher(config.R03_mqtt, digitalRead(PIN_RL3) ? "ON" : "OFF");
    publisher(config.R04_mqtt, digitalRead(PIN_RL4) ? "ON" : "OFF");

    if (config.wversion >= 4 && config.wversion <= 6)
    {

      DynamicJsonDocument jsonValues(512);
      static char buffer[512];

      jsonValues["Power"] = meter.activePower;
      jsonValues["Voltage"] = meter.voltage;
      jsonValues["Current"] = meter.current;
      jsonValues["Frequency"] = meter.frequency;
      jsonValues["Factor"] = meter.powerFactor;
      jsonValues["EnergyTotal"] = meter.energyTotal;
      jsonValues["AparentPower"] = meter.aparentPower;
      jsonValues["ReactivePower"] = meter.reactivePower;
      jsonValues["ImportActive"] = meter.importActive;
      jsonValues["ExportActive"] = meter.exportActive;
      jsonValues["ImportReactive"] = meter.importReactive;
      jsonValues["ExportReactive"] = meter.exportReactive;
      jsonValues["PhaseAngle"] = meter.phaseAngle;
      size_t n = serializeJson(jsonValues, buffer);
      mqttClient.publish("freeds/METER", 0, true, buffer, n);
    }
  }
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
  INFOLN("Disconnected from MQTT");

  Error.ConexionMqtt = true;

  if (WiFi.isConnected() && config.mqtt && config.wversion != 0)
  {
    xTimerStart(mqttReconnectTimer, 0);
  }
}

void onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total)
{
  DEBUGLN("Publish received.");
  DEBUG("  topic: ");
  DEBUGLN(topic);
  DEBUG("  total: ");
  DEBUGLN(total);

  if (config.mqtt && config.wversion != 0)
  {
    if (config.wversion == 3)
    {
      if (strcmp(topic, config.Meter_mqtt) == 0)
      {
        DeserializationError error = deserializeJson(root, payload, len);

        if (error)
        {
          INFO("deserializeJson() Meter MQTT failed: ");
          INFOLN(error.c_str());
        }
        else
        {
          DEBUGLN("deserializeJson() OK");

          inverter.wgrid = (float)root["ENERGY"]["Power"] * -1; // Potencia de red (Negativo: de red - Positivo: a red) para usar con los datos de Tasmota
                    
          Error.ConexionInversor = false;
          Error.ConexionMqtt = false;
          timers.ErrorConexionRed = millis();
        }
      }

      if (strcmp(topic, config.Solax_mqtt) == 0)
      {
        DeserializationError error = deserializeJson(root, payload, len);

        if (error)
        {
          INFO("deserializeJson() Solax MQTT failed: ");
          INFOLN(error.c_str());
        }
        else
        {
          DEBUGLN("deserializeJson() OK");

          inverter.pv1c = (float)root["ENERGY"]["Pv1Current"]; // Corriente string 1
          inverter.pv2c = (float)root["ENERGY"]["Pv2Current"]; // Corriente string 2
          inverter.pv1v = (float)root["ENERGY"]["Pv1Voltage"]; // Tension string 1
          inverter.pv2v = (float)root["ENERGY"]["Pv2Voltage"]; // Tension string 2
          inverter.pw1 = (float)root["ENERGY"]["Pv1Power"];    // Potencia string 1
          inverter.pw2 = (float)root["ENERGY"]["Pv2Power"];    // Potencia string 2
          inverter.gridv = (float)root["ENERGY"]["Voltage"];   // Tension de red
          inverter.wtoday = (float)root["ENERGY"]["Today"];    // Potencia solar diaria
          inverter.wsolar = (float)root["ENERGY"]["Power"];    // Potencia solar actual
        }
      }
    }

    if (strcmp(topic, "freeds/cmnd/pwm") == 0)
    { // pwm control ON-OFF
      INFOLN("Mqtt _ PWM control");
      if ((char)payload[0] == '1')
      {
        INFOLN("PWM ACTIVATED");
        config.P01_on = true;
      }
      else
      {
        config.P01_on = false;
        down_pwm(true);
      }
      saveEEPROM();
    }

    if (strcmp(topic, "freeds/cmnd/pwmman") == 0)
    { // pwm control manual ON-OFF
      String Mode = (char)payload[0] == '1' ? "MANUAL" : "AUTO";
      INFOLN("Mqtt _ Manual PWM control set to :");
      INFOLN(Mode);
      config.pwm_man = (char)payload[0] == '1' ? true : false;
      saveEEPROM();
    }

    if (strcmp(topic, "freeds/cmnd/pwmmanvalue") == 0)
    { // Manual pwm control value
      INFOLN("Mqtt _ Manual PWM control value");
      String strData;
      for (int i = 0; i < len; i++)
      {
        strData += (char)payload[i];
      }
      INFO("MQTT PAYLOAD: ");
      INFOLN(strData);
      config.manualControlPWM = constrain(strData.toInt(), 0, 100);
      saveEEPROM();
    }

    if (strcmp(topic, "freeds/cmnd/pwmfrec") == 0)
    { // Pwm frequency
      INFOLN("Mqtt _ Manual PWM Frequency control value");
      String strData;
      for (int i = 0; i < len; i++)
      {
        strData += (char)payload[i];
      }
      INFO("MQTT PAYLOAD: ");
      INFOLN(strData);
      config.pwmFrequency = constrain(strData.toInt(), 500, 3000);
      saveEEPROM();
    }

    if (strcmp(topic, "freeds/cmnd/screen") == 0)
    { // Info Screen
      INFO("Mqtt _ Change to Screen: ");
      INFOLN(int(payload[0] - '0'));
      screen = constrain(int(payload[0] - '0'), 0, MAX_SCREENS);
      if ((config.wversion < 4 || config.wversion > 6) && screen == 2) { screen = 1; }
      if ((config.wversion >= 4 && config.wversion <= 6)  && screen == 1) { screen = 2; }
    }

    if (strcmp(topic, "freeds/cmnd/brightness") == 0)
    { // Oled brightness value
      String strData;
      for (int i = 0; i < len; i++)
      {
        strData += (char)payload[i];
      }
      INFO("Mqtt _ Change screen brightness: ");
      INFOLN(strData);
      config.oledBrightness = ((constrain(strData.toInt(), 0, 100) * 255) / 100);
      Flags.setBrightness = true;
    }
  }
}