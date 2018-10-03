/*********
  Rui Santos
  Complete project details at http://randomnerdtutorials.com  
*********/
//Source: https://randomnerdtutorials.com/esp8266-web-server/

// Load Wi-Fi library
#include <ESP8266WiFi.h>

// Replace with your network credentials
const char* ssid     = "BigPond 12";
const char* password = "Why not Whales-4-Wales!!!";
//const char* ssid = "TP-Link-AkinaSpeedStars-2.4GHz";
//const char* password = "Cool VIBRATIONS!?! Nan1? :0";

// Set web server port number to 80
WiFiServer server(80);

// Variable to store the HTTP request
String header;

// Auxiliar variables to store the current output state
//String output5State = "off";
//String output4State = "off";
String current_operations = "None from the Real-time Network";
String log_operations = "";
String status_RTN = "Not acquired";

// Tokenisation preparations
int ind1;
int ind2;
int counter;
int string_length;
int number_of_packets = 0;
String token = "";

//// Assign output variables to GPIO pins
//const int output5 = 5;
//const int output4 = 4;

String incoming_bytes = "";

void read_serial() {
    if(Serial.available() > 0) {
    incoming_bytes = "";
    incoming_bytes = Serial.readString();  // Read incoming data as a String
    current_operations = incoming_bytes;
    log_operations += current_operations;

    string_length = log_operations.length();
    Serial.println("string_length");
    Serial.println(string_length);
    number_of_packets = string_length/7;
    if (string_length == 0) {
      string_length = 1;
    } else {
      ind1 = log_operations.lastIndexOf(':',(string_length-2));
      Serial.println("ind1");
      Serial.println(ind1);
      current_operations = log_operations.substring(ind1+1, (string_length-1));
    }
  }
}

void clear_variables() {
  current_operations = "";
  log_operations = "";
  status_RTN = "";
  token = "";
  ind1 = 0;
  ind2 = 0;
  string_length = 0;
}

void setup() {
  Serial.begin(115200);
//  // Initialize the output variables as outputs
//  pinMode(output5, OUTPUT);
//  pinMode(output4, OUTPUT);
//  // Set outputs to LOW
//  digitalWrite(output5, LOW);
//  digitalWrite(output4, LOW);

  // Connect to Wi-Fi network with SSID and password
//  Serial.print("Connecting to ");
//  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
//    Serial.print(".");
  }
//  // Print local IP address and start web server
//  Serial.println("");
//  Serial.println("WiFi connected.");
//  Serial.println("IP address: ");
//  Serial.println(WiFi.localIP());
  server.begin();
}

void loop(){
  WiFiClient client = server.available();   // Listen for incoming clients

  if (client) {                             // If a new client connects,
//    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        // Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();
            
            // turns the GPIOs on and off
            if (header.indexOf("GET /5/on") >= 0) {
              // output5State = "on";
              // digitalWrite(output5, HIGH);
              Serial.println("Data?");
              read_serial();
              
//            } else if (header.indexOf("GET /5/off") >= 0) {
//              output5State = "off";
//              // digitalWrite(output5, LOW);
//              Serial.println("First response");
//              read_serial();
//            } 

            } else if (header.indexOf("GET /4/on") >= 0) {
              // output4State = "on";
              // GPIO_4_state = "ON";
              // Serial.println("ON");
              Serial.println("First response");
              clear_variables();
              
            }
            
//            } else if (header.indexOf("GET /4/off") >= 0) {
//              Serial.println("GPIO 4 off");
//              output4State = "off";
//              // GPIO_4_state = "OFF";
//              //Serial.println("OFF");
//            }
            
            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            
            // CSS to style the on/off buttons 
            // Feel free to change the background-color and font-size attributes to fit your preferences
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println("table, td {border: 1px solid black;}");
            client.println(".button { background-color: #195B6A; border: none; color: white; padding: 16px 40px;");
            client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
            client.println(".button2 {background-color: #77878A;}</style></head>");
            
            // Web Page Heading
            client.println("<body><h1>s3483160 - Gateway</h1>");
            client.println("<body><h2>Current Operations : " + current_operations + " </h2>");
            client.println("<body><h2>Status : " + log_operations + " </h2>");
                    
            // Display current state, and ON/OFF buttons for GPIO 5  
//            client.println("<p>GPIO 5 - State " + output5State + "</p>");
            // If the output5State is off, it displays the ON button
            client.println("<p><a href=\"/5/on\"><button class=\"button\">Request</button></a></p>");       
//            if (output5State=="off") {
//              client.println("<p><a href=\"/5/on\"><button class=\"button\">ON</button></a></p>");
//            } else {
//              client.println("<p><a href=\"/5/off\"><button class=\"button button2\">OFF</button></a></p>");
//            } 
               
            // Display current state, and ON/OFF buttons for GPIO 4  
//            client.println("<p>GPIO 4 - State " + output4State + "</p>");
            // If the output4State is off, it displays the ON button
            client.println("<p><a href=\"/4/on\"><button class=\"button\">Clear</button></a></p>");       
//            if (output4State=="off") {
//              client.println("<p><a href=\"/4/on\"><button class=\"button\">ON</button></a></p>");
//            } else {
//              client.println("<p><a href=\"/4/off\"><button class=\"button button2\">OFF</button></a></p>");
//            }

            //Table for displaying values and buttons
            client.println("<table align='center'>");
            client.println("<tr>");
            client.println("<td>Time/Date</td>");
            client.println("<td>Content</td>");
            client.println("</tr>");
            counter = 0;
            ind2 = log_operations.indexOf(':', 0);
            Serial.println("Ind2");
            Serial.println(ind2);
            for (int i = 0; i < number_of_packets; i++) {
              if (i == 0) {
                Serial.println("number_of_packets");
                Serial.println(number_of_packets);
                token = log_operations.substring(0, ind2);
                Serial.println("token");
                Serial.println(token);
              } else {
                token = log_operations.substring(ind2+counter, (ind2+counter+7));
                 Serial.println("token");
                Serial.println(token);
                counter = counter + 7;
                Serial.println("counter");
                Serial.println(counter);
              }
              client.println("<tr>");
              client.println("<td>" + String(i) + "</td>");
              client.println("<td>" + token  + "</td>");
              client.println("</tr>");
            }
            client.println("</table>");

            client.println("</body></html>");
            
            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
//    Serial.println("Client disconnected.");
//    Serial.println("");
  }
}
