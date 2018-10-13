/*********
  Rui Santos
  Complete project details at http://randomnerdtutorials.com  
*********/
//Source: https://randomnerdtutorials.com/esp8266-web-server/

// Load Wi-Fi library
#include <ESP8266WiFi.h>

//// Replace with your network credentials
//const char* ssid     = "BigPond 12";
//const char* password = "Why not Whales-4-Wales!!!";
////const char* ssid = "TP-Link-AkinaSpeedStars-2.4GHz";
////const char* password = "Cool VIBRATIONS!?! Nan1? :0";

 // WIFI Access point credentials
 const char* ssid     = "ESP-AP - s3483160";
 const char* password = "Test Password";

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
int ind0;
int ind1;
int ind2;
int ind3;
int ind4;
int ind5;
int counter;
int string_length;
int number_of_packets = 0;
int packet_length = 21;
String data_token = "";
String time_date_token = "";

//// Assign output variables to GPIO pins
//const int output5 = 5;
//const int output4 = 4;

// String incoming_bytes = "";
char incoming_bytes;

void read_serial() {
    while (Serial.available() > 0) {
      // incoming_bytes = "";
      
      // Serial.println("Serial.readString()");
      // incoming_bytes = Serial.readString();  // Read incoming data as a String
      incoming_bytes = Serial.read();
      Serial.println(incoming_bytes);  

      current_operations = incoming_bytes;
      log_operations += current_operations;
      
      Serial.println("current_operations");
      Serial.println(current_operations); 

      Serial.println("string_length - current_operations");
      Serial.println(current_operations.length());    

      string_length = log_operations.length();
      Serial.println("string_length");
      Serial.println(string_length);
    number_of_packets = string_length/packet_length;
    } if (string_length == 0) {
      string_length = 1;
    } else {
      ind0 = log_operations.lastIndexOf('|',(string_length-2));
      ind1 = log_operations.lastIndexOf('|',(ind0-1));
      Serial.println("ind0");
      Serial.println(ind0);
      Serial.println("ind1");
      Serial.println(ind1);
      current_operations = log_operations.substring(ind1+1, (string_length-1));
    }
}

void clear_variables() {
  current_operations = "";
  log_operations = "";
  status_RTN = "";
  data_token = "";
  time_date_token = "";
  ind0 = 0;
  ind1 = 0;
  ind2 = 0;
  ind3 = 0;
  ind4 = 0;
  string_length = 0;
}

void setup() {
  Serial.begin(115200);

   // WIFI Access Point Code
   Serial.print("Configuring access point...");
   WiFi.softAP(ssid, password);
   IPAddress myIP = WiFi.softAPIP();
   Serial.print("AP IP address: ");
   Serial.println(myIP);
   server.begin();
   Serial.println("HTTP server started");

//  // WIFI Client Code
//  Serial.print("Connecting to ");
//  Serial.println(ssid);
//  WiFi.begin(ssid, password);
//  while (WiFi.status() != WL_CONNECTED) {
//    delay(500);
//    Serial.print(".");
//  }
  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  server.begin();
}

void loop(){
  WiFiClient client = server.available();   // Listen for incoming clients

  if (client) {                             // If a new client connects,
    Serial.println("New Client.");          // print a message out in the serial port
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
              // Serial.println("Data?");
              read_serial();

            } else if (header.indexOf("GET /4/on") >= 0) {
              // Serial.println("First response");
              clear_variables();
              
            }
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
            client.println("<p><a href=\"/5/on\"><button class=\"button\">Request</button></a></p>");
            client.println("<p><a href=\"/4/on\"><button class=\"button\">Clear</button></a></p>");       

            //Table for displaying values and buttons
            client.println("<table align='center'>");
            client.println("<tr>");
            client.println("<td>Time/Date</td>");
            client.println("<td>Key Presses</td>");
            client.println("</tr>");
            counter = 0;
            ind3 = log_operations.indexOf('|', 0);
            ind4 = log_operations.indexOf('|',(ind3+1));
            Serial.println("Ind3");
            Serial.println(ind3);
            Serial.println("Ind4");
            Serial.println(ind4);
            for (int i = 0; i <= number_of_packets; i++) {
              if (i == 0) {
                Serial.println("number_of_packets");
                Serial.println(number_of_packets);

                data_token = log_operations.substring(0, ind3);
                time_date_token = log_operations.substring((ind3+1), ind4);
                Serial.println("data_token");
                Serial.println(data_token);
                Serial.println("time_date_token");
                Serial.println(time_date_token);

                ind2 = ind4 + 1;
                ind3 = log_operations.indexOf('|',(ind2));
                ind4 = log_operations.indexOf('|',(ind3+1));
                Serial.println("Ind2");
                Serial.println(ind2);
                Serial.println("Ind3");
                Serial.println(ind3);
                Serial.println("Ind4");
                Serial.println(ind4);
              } else {
                // data_token = log_operations.substring(ind4+1+counter, (ind4+1+counter+7));
                // time_date_token = log_operations.substring(ind4+1+7+counter, (ind4+1+7+counter+18));
                data_token = log_operations.substring(ind2, ind3);
                time_date_token = log_operations.substring((ind3+1), ind4);
                Serial.println("data_token");
                Serial.println(data_token);
                Serial.println("time_date_token");
                Serial.println(time_date_token);

                counter = counter + packet_length;
                Serial.println("counter");
                Serial.println(counter);

                ind2 = ind4 + 1;
                ind3 = log_operations.indexOf('|',(ind2));
                ind4 = log_operations.indexOf('|',(ind3+1));
                Serial.println("Ind2");
                Serial.println(ind2);
                Serial.println("Ind3");
                Serial.println(ind3);
                Serial.println("Ind4");
                Serial.println(ind4);
              }
              client.println("<tr>");
              client.println("<td>" + time_date_token + "</td>");
              client.println("<td>" + data_token  + "</td>");
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
    Serial.println("Client disconnected.");
//    Serial.println("");
  }
}