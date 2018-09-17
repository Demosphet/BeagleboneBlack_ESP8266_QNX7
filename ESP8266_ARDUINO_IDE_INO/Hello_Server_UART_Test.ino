#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

//const char* ssid = "BigPond 12";
//const char* password = "Why not Whales-4-Wales!!!";
const char* ssid = "TP-Link-AkinaSpeedStars-2.4GHz";
const char* password = "Cool VIBRATIONS!?! Nan1? :0";


String incoming_bytes = "Welcome";
String hello_message = "Hello there\n";
String response = "NULL";

ESP8266WebServer server(80);

const int led = 13;

void handleRoot() {
  digitalWrite(led, 1);
  server.send(200, "text/plain", hello_message);
  digitalWrite(led, 0);
}

void handleNotFound() {
  digitalWrite(led, 1);
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  digitalWrite(led, 0);
}

void read_serial() {
    if(Serial.available() > 0) {
    incoming_bytes = "";
    incoming_bytes = Serial.readString();  // Read incoming data as a String
    hello_message = incoming_bytes;
    if (incoming_bytes == "Hello\r\n") {
      response = "\nHereCN";
      Serial.print(response);
    } else if (incoming_bytes == "Hello\n") {
      response = "\nHereN";
      Serial.print(response);
    } else if (incoming_bytes == "Hello") {
      response = "\nHere"; 
      Serial.print(response);
    } else if (incoming_bytes == "Hello\r") {
      response = "\nHereC";
      Serial.print(response);
    } else {
      response = "\n400";
      Serial.print(response);
    }
  }
}

void setup(void) {
  pinMode(led, OUTPUT);
  digitalWrite(led, 0);
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("esp8266")) {
    Serial.println("MDNS responder started");
  }

  server.on("/", handleRoot);

  server.on("/inline", []() {
    server.send(200, "text/plain", "this works as well");
  });

  server.on("/serial", []() {
    server.send(200, "text/plain", response);
  });

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");
}

void loop(void) {
  server.handleClient();
  read_serial();
}
