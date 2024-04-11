/*   Pasarela ESP-NOW <-> MQTT con auto emparejamiento
     El dispositivo envía mensajes en JSON por ESPNOW (más un primer byte de control)
     Los mensajes se publican en infind/espnow/<MAC>[/<TOPIC>]
     Siendo MAC la dirección mac del dispositivo emisor y TOPIC opcionamente puede establecerse con la clave "topic" en el JSON

     Se pueden enviar mensajes a los dispositivos a través del topic infind/espnowdevice
     El mensaje de debe contener una cadena JSON, con las claves: mac, topic, payload
     Siendo mac la dirección mac del dispositivo destino, topic una cadena que identifique el tipo de mensaje y payload el mensaje.

     Para arquitectura ESP32, y así poder comunicarse simultaneamente por ESPNOW y WiFi (mqtt).

   Distribuido bajo licencia: CC BY-NC-SA 4.0 http://creativecommons.org/licenses/by-nc-sa/4.0/deed.es
   Autor: Andrés Rodríguez (andres@uma.es)

   Basado en el trabajo de:
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/?s=esp-now
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
  Based on JC Servaye example: https://https://github.com/Servayejc/esp8266_espnow
*/


#include <esp_now.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <queue>
#include <list>
#include <string>
#include <algorithm>

#include "AUTOpairing_common.h"


//-------------------------------------------------------------
// clase para manejar un mensaje esp-now
class TmensajeESP
{
  public:
    uint8_t mac[6];
    uint8_t data[500];
    uint16_t len;
    TmensajeESP ( uint8_t *_mac, uint8_t *_data, uint16_t _len)
    {
      memcpy(mac, _mac, 6);
      if (_len > 500) _len = 500;
      memcpy(data, _data, _len);
      len = _len;
    }
};

//-------------------------------------------------------------
// clase para manejar un mensaje MQTT
class TmensajeMQTT
{
  public:
    uint8_t mac[6];
    uint8_t data[500];
    uint16_t len;
    TmensajeMQTT ( uint8_t *_mac, uint8_t *_data, uint16_t _len)
    {
      memcpy(mac, _mac, 6);
      if (_len > 500) _len = 500;
      memcpy(data, _data, _len);
      len = _len;
    }
};

//-------------------------------------------------------------
// clase para manejar un mensaje PAN
class TmensajePAN
{
  public:
    uint8_t mac[6];
    uint8_t pan;
    unsigned long time;
    std::list<String> sent_macs;
    uint8_t data[500];
    uint16_t len;
    TmensajePAN ( uint8_t *_mac, uint8_t *_data, uint16_t _len,  uint8_t _pan)
    {
      memcpy(mac, _mac, 6);
      if (_len > 500) _len = 500;
      memcpy(data, _data, _len);
      sent_macs.push_back(byte2HEX(_mac)); // para no enviar a si mismo
      len = _len;
      pan = _pan;
      time = millis();
      //Serial.print("PAN len: "); Serial.println(len);
    }
};

//-------------------------------------------------------------
// clase para manejar un dispositivo que espera sus mensajes PAN
class TmacPAN
{
  public:
    String mac;
    uint8_t pan;
    TmacPAN ( String _mac, uint8_t _pan)
    {
      mac = _mac;
      pan = _pan;
    }
};


// cola de mensajes esp-now recibidos
std::queue<TmensajeESP> cola_mensajes;
// cola de mensajes esp-now recibidos
std::list<TmensajeMQTT> mensajes_MQTT;

std::list<TmensajePAN> mensajes_PAN;

// cola de dispositivos (mac) esperando por sus mensajes
std::queue<String> readyMACs;
std::queue<TmacPAN> readyMACsPAN;

std::queue<String> pairMACs;
std::list<String> pairedMACs;

WiFiClient wClient;
PubSubClient mqtt_client(wClient);

// cadenas para topics e ID
char ID_PLACA[16];
char topic_PUB[256];
char mensaje_mqtt[512];
char JSON_serie[500];  // 6 bytes de la MAC + 1 byte del tamaño del mensaje esp-now + 250 bytes para el mensaje = 257
char JSON_serie_bak[500];
String deviceMac;

const char* mqtt_server = "iot.ac.uma.es";
const char* mqtt_user = "infind";
const char* mqtt_pass = "zancudo";

// Replace with your network credentials (STATION)
//const char* ssid = "HortSost";
//const char* password = "9b11c2671e5b";
const char* ssid = "IoTLab";
const char* password = "4cc3s0IoT@";
//const char* ssid = "tfgshuerta";
//const char* password = "946CXQ2d*WHm";

esp_now_peer_info_t slave;
int myChannel;


int msg_length = 0;

//int num_paq;
String serialmsg;
String msgbuff;
int long_previa = 0;

char msg_buff[500] = {0};

size_t buffer_size;
char *buffer;
char *next;
char* ptr;
//-----------------------------------------------------
void conecta_wifi() {
  Serial.printf("\nConnecting to %s:\n", ssid);

  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    Serial.print(".");
  }
  Serial.printf("\nWiFi connected, IP address: %s\n", WiFi.localIP().toString().c_str());
}

//-----------------------------------------------------
void conecta_mqtt() {
  // Loop until we're reconnected
  while (!mqtt_client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqtt_client.connect(ID_PLACA, mqtt_user, mqtt_pass)) {
      Serial.printf(" conectado a broker: %s\n", mqtt_server);
      mqtt_client.subscribe("infind/espnowdevice");
    } else {
      Serial.printf("failed, rc=%d  try again in 5s\n", mqtt_client.state());
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}


// ---------------------------- esp_ now -------------------------
void printMAC(const uint8_t * mac_addr) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.print(macStr);
}

//------------------------------------------------------------
bool addPeer(const uint8_t *peer_addr) {      // add pairing
  memset(&slave, 0, sizeof(slave));
  const esp_now_peer_info_t *peer = &slave;
  memcpy(slave.peer_addr, peer_addr, 6);

  slave.channel = myChannel; // pick a channel
  slave.encrypt = 0; // no encryption
  // check if the peer exists
  bool exists = esp_now_is_peer_exist(slave.peer_addr);
  if (exists) {
    // Slave already paired.
    Serial.println("Already Paired");
    return false;
  }
  else {
    esp_err_t addStatus = esp_now_add_peer(peer);
    if (addStatus == ESP_OK) {
      // Pair success
      Serial.println("Pair success");
      return true;
    }
    else
    {
      Serial.println("Pair failed");
      return false;
    }
  }
}

//------------------------------------------------------------
// callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Last Packet Send Status: ");
  Serial.print(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success to " : "Delivery Fail to ");
  printMAC(mac_addr);
  Serial.println();
}

bool long_packet = false;
//------------------------------------------------------------
String _message;

void OnDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len) {
  uint8_t type = incomingData[0];       // first message byte is the type of message


  String num_paq_str;
  Serial.println();
  Serial.print("received message type = ");
  Serial.print(type, BIN);
  Serial.println(messType2String(type)); Serial.print("ESPNOW: ");
  Serial.print(len);
  Serial.print(" bytes of data received from : ");
  printMAC(mac_addr);
  Serial.println();
  //  int num_paq = (int) incomingData[1];       // first message byte is the type of message
  //  Serial.print("Paquete numero: ");
  //  Serial.println(num_paq);
  int pan  = (type & MASK_PAN) >> PAN_OFFSET;

  uint8_t macAddr[6];

  memcpy(macAddr, incomingData + PAN_MAC_offset, PAN_MAC_size);
  printMAC(macAddr);
  Serial.println();
  //char payload[300];
  char *payload = (char *)malloc(len - PAN_payload_offset + 1);
  snprintf(payload, len - PAN_payload_offset + 1, "%s", (char *)incomingData + 7);
  Serial.printf("Mensaje len: %d \n payload: %s\n", len, payload);


  serialmsg = String(payload);
  free(payload);
  //Obtenemos el mensaje formado como topic|mensaje
  String topic = serialmsg.substring(0, serialmsg.indexOf("|")) + '\0';
  String message = serialmsg.substring(serialmsg.indexOf("|") + 1, len); // + '\0' no funciona??
  Serial.print("Topic received: ");
  Serial.println(topic);
  Serial.print("Message received: ");
  Serial.println(message);
  int num_paq = message.substring(0, 1).toInt();
  

  Serial.print("Paquete numero: ");
  Serial.println(num_paq);
  if (num_paq != 0) {
    message.remove(0, 1);
    long_packet = true;
  }
  // Fin comprobación mensaje largo

  //******************* OTRO METODO ***********//
  // MIRAR URL:: https://stackoverflow.com/questions/3038957/how-to-concatenate-2-c-strings-without-overwriting-any-terminating-null-charact
  //*****************************************************//

  //******************* OTRO METODO ***********//
  // MIRAR URL:: https://www.ibm.com/docs/es/i/7.5?topic=functions-strncat-concatenate-strings
  //*****************************************************//

  if (type & CHECK)
  {
    Serial.println("Device wants to get messages (CHECK==1)...");
    readyMACs.push(byte2HEX((uint8_t*)mac_addr));

    if (pan > 1)
    {
      Serial.println("Device needs to check PAN messages (PAN>1)...");
      readyMACsPAN.push(TmacPAN(byte2HEX((uint8_t*)mac_addr), pan));
    }
  }

  switch (type & MASK_MSG_TYPE) {  // dos bits menos significativos

    case DATA :                           // the message is data type
      if (long_packet == true) {
        // Inicio procesamiento mensaje largo
        //************* Metodo principal ******************************//


        if (num_paq > 0) {
          Serial.println("ESTOY EN NUM_PAQ>0");
          msgbuff += message;
          msg_length += len;
          Serial.print("Length message builded: ");
          Serial.println(msgbuff.length());
        }
        else {
          Serial.println("ESTOY EN ELSE");
          msgbuff.concat(message);
          Serial.print("Length message builded: ");
          Serial.println(msgbuff.length());
          Serial.print("Message builded: ");
          Serial.println(msgbuff);
          cola_mensajes.push(TmensajeESP((uint8_t *)mac_addr, (uint8_t *)msgbuff.c_str(), msgbuff.length() + 1));
          if (pan > 1)
          {
            mensajes_PAN.push_back(TmensajePAN((uint8_t *)mac_addr, (uint8_t *)msgbuff.c_str(),  msgbuff.length() + 1, pan));
          }
          msgbuff = "";
          long_packet = false;
        }

        // Fin procesamiento mensaje largo
      }
      else {
        cola_mensajes.push(TmensajeESP((uint8_t *)mac_addr, (uint8_t *)message.c_str(), message.length() + 1));
        if (pan > 1)
        {
          mensajes_PAN.push_back(TmensajePAN((uint8_t *)mac_addr, (uint8_t *)message.c_str(),  message.length() + 1, pan));
        }
      }

      // encola el mensaje para su envío por serie en loop()
      //      cola_mensajes.push(TmensajeESP((uint8_t *)mac_addr, (uint8_t *)incomingData + 1, len - 1));
      //      if (pan > 1)
      //      {
      //        mensajes_PAN.push_back(TmensajePAN((uint8_t *)mac_addr, (uint8_t *)incomingData + 1,  len - 1, pan));
      //      }

      if ( addPeer(mac_addr))
      {
        Serial.println("New device paired");
        pairMACs.push(byte2HEX((uint8_t*)mac_addr));
        pairedMACs.push_back(byte2HEX((uint8_t*)mac_addr));

      }
      // TEST respuesta
      /*    struct_espnow mensaje_esp;
        mensaje_esp.msgType=DATA;
        memcpy(mensaje_esp.payload, "topic/dos|{\"otro\":12}",21);
        esp_now_send(mac_addr, (uint8_t *) &mensaje_esp, 22);
      */
      break;

    case PAIRING:                            // the message is a pairing request
      struct_pairing pairingData;
      memcpy(&pairingData, incomingData, sizeof(pairingData));
      Serial.print("Pairing request from: ");
      printMAC(mac_addr);
      Serial.println();
      Serial.print("id: ");
      Serial.println(pairingData.id);
      Serial.print("msgType: ");
      Serial.println(pairingData.msgType);
      if (pairingData.id != GATEWAY) {     // do not replay to server itself
        if (pairingData.msgType == PAIRING) {
          pairingData.id = GATEWAY;
          // Server is in AP_STA mode: peers need to send data to server soft AP MAC address
          WiFi.softAPmacAddress(pairingData.macAddr);   // send my mac Address to pairing device
          pairingData.channel = myChannel;
          Serial.println("send response");
          if ( addPeer(mac_addr))
          {
            Serial.println("New device paired");
            pairMACs.push(byte2HEX((uint8_t*)mac_addr));
            pairedMACs.push_back(byte2HEX((uint8_t*)mac_addr));
          }

          esp_err_t result = esp_now_send(mac_addr, (uint8_t *) &pairingData, sizeof(pairingData));

        }
      }
      break;
  }
}

//------------------------------------------------------------
void initESP_NOW() {
  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);
}

//-----------------------------------------------------
//RECIBIR MQTT
void procesa_mensaje(char* topic, byte* payload, unsigned int length) {
  String mensaje = String(std::string((char*) payload, length).c_str());
  Serial.println("Mensaje recibido [" + String(topic) + "] " + mensaje); //LEVEL CON LO DE PROCESS
  // compruebo el topic
  if (String(topic) == "infind/espnowdevice")
  {
    StaticJsonDocument<512> json; // el tamaño tiene que ser adecuado para el mensaje
    // Deserialize the JSON document
    DeserializationError error = deserializeJson(json, mensaje.c_str());

    // Compruebo si no hubo error
    if (error) {
      Serial.print("Error funcion deserializeJson(): ");
      Serial.println(error.c_str());
    }
    else if (!json.containsKey("mac") && !json.containsKey("topic") && !json.containsKey("payload")) // comprobar si existe el campo/clave que estamos buscando
    {
      Serial.print("Error : ");
      Serial.println("campos mac, topic o payload no encontrados");
    }
    else
    {
      String mac = json["mac"];
      String topic = json["topic"];
      String output;
      serializeJson(json["payload"], output);

      String compacto = topic + "|" + output;

      Serial.print("Mensaje OK, mac = ");
      Serial.println(mac);//es lo que hay que sacar

      uint8_t mac_addr[6];
      HEX2byte(mac_addr, (char*) mac.c_str() );
      mensajes_MQTT.push_back(TmensajeMQTT(mac_addr, (uint8_t *)compacto.c_str(), compacto.length()));
      Serial.print("compacto = ");
      Serial.println(compacto);//es lo que hay que sacar

    }
  } // if topic
  else
  {
    Serial.println("Error: Topic desconocido");
  }

}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);

  Serial.println();

  sprintf(ID_PLACA, "ESP_%d", ESP.getEfuseMac());
  conecta_wifi();

  Serial.print("Server MAC Address:  ");
  Serial.println(WiFi.macAddress());

  // Set the device as a Station and Soft Access Point simultaneously

  Serial.print("Server SOFT AP MAC Address:  ");
  Serial.println(WiFi.softAPmacAddress());

  mqtt_client.setServer(mqtt_server, 1883);
  mqtt_client.setBufferSize(512); // para poder enviar mensajes de hasta X bytes
  mqtt_client.setCallback(procesa_mensaje);

  myChannel = WiFi.channel();
  Serial.print("Station IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Wi-Fi Channel: ");
  Serial.println(WiFi.channel());
  Serial.printf("Identificador placa: %s\n", ID_PLACA );

  initESP_NOW();

}

//------------------------------------------------------------
//std::list<TmensajeMQTT> mensajes_MQTT;
std::list<TmensajeMQTT>::iterator encuentra_mensaje(uint8_t *_mac)
{
  for (std::list<TmensajeMQTT>::iterator it = mensajes_MQTT.begin() ; it != mensajes_MQTT.end(); it++ ) {
    if (igualMAC(it->mac, _mac)) return it;
  }
  return mensajes_MQTT.end();
}

bool ausente(std::list<String> lista, String mac)
{
  for (std::list<String>::iterator it = lista.begin() ; it != lista.end(); it++ )
  {
    if (*it == mac) return false;
  }

  return true;
}

//------------------------------------------------------------
//std::list<TmensajePAN> mensajes_PAN;
std::list<TmensajePAN>::iterator encuentra_mensajePAN(uint8_t pan, String mac)
{
  for (std::list<TmensajePAN>::iterator it = mensajes_PAN.begin() ; it != mensajes_PAN.end(); it++ ) {
    //  if(1) return mensajes_PAN.end();
    if ((it->pan == pan) && ausente(it->sent_macs, mac))
      return it;
  }
  return mensajes_PAN.end();
}

//------------------------------------------------------------
//-----------------------------------------------------------------------------
void loop() {
  static unsigned long lastEventTime = millis();
  if (!mqtt_client.connected()) conecta_mqtt();
  mqtt_client.loop(); // esta llamada para que la librería recupere el control

  if (!readyMACs.empty())
  {
    uint8_t mac_addr[6];
    uint8_t size;
    String mac = readyMACs.front();
    struct_espnow mensaje_esp;
    //
    Serial.println("Buscando mensajes para dispositivo " + mac);
    HEX2byte(mac_addr, (char*)mac.c_str());
    std::list<TmensajeMQTT>::iterator it = encuentra_mensaje(mac_addr);
    if (it == mensajes_MQTT.end())
    {
      readyMACs.pop();
      mensaje_esp.msgType = NODATA;
      size = 1;
      Serial.println(" > Sin mensajes MQTT pendientes");
      esp_err_t result = esp_now_send(mac_addr, (uint8_t *) &mensaje_esp, size);
    }
    else
    {
      mensaje_esp.msgType = DATA;
      memcpy(mensaje_esp.payload, it->data, it->len);
      size = it->len + 1;
      mensajes_MQTT.erase(it);
      Serial.println(" > Un mensaje encontrado");
      esp_err_t result = esp_now_send(mac_addr, (uint8_t *) &mensaje_esp, size);
    }

  }

  /* en CONSTRUCCION  BEGIN */
  if (!readyMACsPAN.empty())
  {
    uint8_t mac_addr[6];
    uint8_t size;
    TmacPAN macPAN = readyMACsPAN.front();
    struct_espnow mensaje_esp;
    //
    Serial.println("Buscando mensajes PAN para dispositivo " + macPAN.mac + " con PAN " + macPAN.pan);
    HEX2byte(mac_addr, (char*)macPAN.mac.c_str());
    std::list<TmensajePAN>::iterator it = encuentra_mensajePAN(macPAN.pan, macPAN.mac);
    if (it == mensajes_PAN.end() )
    {
      readyMACsPAN.pop();
      mensaje_esp.msgType = NODATA | ((macPAN.pan << PAN_OFFSET) & MASK_PAN) ;
      size = 1;
      Serial.println(" > Sin mensajes PAN pendientes");
      esp_err_t result = esp_now_send(mac_addr, (uint8_t *) &mensaje_esp, size);
    }
    else
    {
      uint8_t mensaje_esp[500];
      mensaje_esp[0] = PAN_DATA | ((macPAN.pan << PAN_OFFSET) & MASK_PAN) ;
      Serial.println("MAC de origen PAN");
      printMAC(it->mac);
      memcpy(mensaje_esp + PAN_MAC_offset, it->mac, PAN_MAC_size); // copy MAC
      uint32_t old_ms = (millis() - it->time);
      memcpy(mensaje_esp + PAN_MSold_offset, (void*)&old_ms, PAN_MSold_size); // copy ms uint32_t

      memcpy(mensaje_esp + PAN_payload_offset, it->data, it->len);
      /*
        Serial.println(it->len);
        Serial.println("***");
        for(int k=0; k<it->len; k++) Serial.print((char)(mensaje_esp[k+11]));
        Serial.println("***");
        Serial.println(sizeof(mensaje_esp));
        Serial.println((unsigned int)&mensaje_esp,HEX);
      */
      size = it->len + 11; // un byte de control, 6 mac y los 4 del old_ms
      it->sent_macs.push_back(macPAN.mac);
      // if pairedMACs == sent_macs  ==>  mensajes_PAN.erase(it);
      // AQUI HAY QUE ELIMINAR SI LAS MACsPaired coinciden con el PAN, no como está ahora!!!!
      if (is_permutation(it->sent_macs.begin(), it->sent_macs.end(), pairedMACs.begin()))
      {
        Serial.println("*** Mensaje PAN borrado (distribuido a todas las MACs registradas)");
        mensajes_PAN.erase(it);
      }



      Serial.println(" > Un mensaje encontrado");
      esp_err_t result = esp_now_send(mac_addr, (uint8_t *) &mensaje_esp, size);
    }

  }

  /* en CONSTRUCCION  END*/


  if (!pairMACs.empty())
  {
    uint8_t mac_addr[6];
    uint8_t size;
    String mac = pairMACs.front();
    pairMACs.pop();
    Serial.println("Enviando mensaje MQTT notificación pairing " + mac);
    sprintf(mensaje_mqtt, "{\"mac\":\"%s\"}", mac.c_str());
    mqtt_client.publish("infind/espnowpairing", mensaje_mqtt);
  }

  // envía todo lo que haya en la cola de mensajes pendientes
  if (!cola_mensajes.empty())
  {
    TmensajeESP mensaje = cola_mensajes.front();
    cola_mensajes.pop();

    deviceMac = byte2HEX(mensaje.mac);

    int len =  mensaje.len;
    // recibimos el mensaje
    for (int i = 0; i < len ; i++)
    {
      JSON_serie[i] = mensaje.data[i];
    }
    JSON_serie[len] = '\0'; //fin de cadena de caracteres de C

    Serial.printf("Mensaje len: %d \n payload: %s\n", len, JSON_serie);
    // parece que la deserialización altera la cadena de caracteres original, así que la copiamos antes
    strcpy(JSON_serie_bak, JSON_serie);

    //comprobamos si hay campo topic en el mensaje, para añadirlo al topic de envío
    StaticJsonDocument<512> doc;
    // Deserialize the JSON document
    DeserializationError error = deserializeJson(doc, JSON_serie, len);

    sprintf(topic_PUB, "infind/espnow/%s", deviceMac.c_str());

    // Compruebo si no hubo error
    if (error) {
      Serial.print("Error sintaxis JSON; deserializeJson() failed: ");
      Serial.println(error.c_str());
      mqtt_client.publish(topic_PUB, "{\"error\":\"Error sintaxis JSON; deserializeJson() failed\"}");
    }
    else if (doc.containsKey("topic") && doc["topic"].is<const char*>()) // comprobar si existe el campo/clave que estamos buscando
    {
      String valor = doc["topic"];
      Serial.print("Mensaje OK, topic = ");
      Serial.println(valor);
      sprintf(topic_PUB, "infind/espnow/%s/%s", deviceMac.c_str(), valor.c_str() );
    }
    else // no existe el campo topic
    {
      Serial.println("\"topic\" key not found in JSON");
    }

    sprintf(mensaje_mqtt, "{\"mac\":\"%s\",\"payload\":%s}", deviceMac.c_str(), JSON_serie_bak);
    // publica el mensaje recibido
    mqtt_client.publish(topic_PUB, mensaje_mqtt);

    Serial.printf("Mensaje from: %s, publicado en MQTT\n", deviceMac.c_str());
    Serial.printf("       topic: %s\n", topic_PUB);
    Serial.printf("     payload: %s\n\n", mensaje_mqtt);
  }

}
