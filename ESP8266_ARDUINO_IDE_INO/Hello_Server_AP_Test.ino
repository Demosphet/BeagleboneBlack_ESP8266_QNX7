/* Create a WiFi access point and provide a web server on it. */

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>

/* Set these to your desired credentials. */
const char *ssid = "ESP-Test-AP";
const char *password = "thereisnospoon";

String incoming_bytes = "Welcome";
String hello_message = "<h1>You are connected</h1>";
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
      response = "HereCN";
      Serial.print(response);
    } else if (incoming_bytes == "Hello\n") {
      response = "HereN";
      Serial.print(response);
    } else if (incoming_bytes == "Hello") {
      response = "Here"; 
      Serial.print(response);
    } else if (incoming_bytes == "Hello\r") {
      response = "HereC";
      Serial.print(response);
    } else {
      response = "400";
      Serial.print(response);
    }
  }
}

void setup() {
  pinMode(led, OUTPUT);
  digitalWrite(led, 0);
  delay(1000);
  Serial.begin(115200);
  Serial.println();
  Serial.print("Configuring access point...");
  /* You can remove the password parameter if you want the AP to be open. */
  WiFi.softAP(ssid, password);

  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
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

void loop() {
  server.handleClient();
  read_serial();
}
