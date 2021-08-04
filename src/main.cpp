#include <Arduino.h>
#include <analogWrite.h>
#include <WiFi.h>
#include "DHT.h"

// Wifi
const char *ssid = "MiFibra-8A95";
const char *password = "7KL2HDpj";
WiFiServer server(80);
String header;

// Constants
#define led_red 33
#define led_green 25
#define led_blue 26
#define fan_pin 19
#define heat_pin 21
#define dht_pin 32
DHT dht(dht_pin, DHT11);

// Variables
float temp_current;
float humi_current;
float temp_desired = 30;
float temp_margin = 1.5;
int heat_max = 250;
int fan_max = 140;
int fan_mid = 75;
String incubator_state = "On";
int color_current = 0;
int color_new;
int color_delay = 10;
int color_max = 150;

// Strings
String str_incubator_status;
String str_incubator_on = "The incubator is turned on.";
String str_incubator_off = "The incubator is turned off.";
String str_incubator_heating = "The incubator is heating up.";
String str_incubator_cooling = "The incubator is cooling down.";
String str_incubator_good = "The incubator is at the desired temperature (margin ± " + String(temp_margin) + " °C).";

void rgbLedClear()
{
  analogWrite(led_red, 255);
  analogWrite(led_green, 255);
  analogWrite(led_blue, 255);
}

// RGB Led (black/red/green/blue)
void rgbLed(String color)
{
  if (color == "red")
  {
    color_new = led_red;
  }
  else if (color == "green")
  {
    color_new = led_green;
  }
  else if (color == "blue")
  {
    color_new = led_blue;
  }
  else
  {
    color_new = 0;
  }

  // fade out
  if (color_current > 0)
  {
    for (int i = 0; i <= 255; i++)
    {
      analogWrite(color_current, i);
      delay(color_delay);
    }
    rgbLedClear();
  }
  if (color_new > 0)
  {
    // fade in
    for (int i = 0; i <= 255; i++)
    {
      analogWrite(color_new, map(i, 0, 255, 255, 0));
      delay(color_delay);
    }
    // animation
    for (int i = 0; i <= 255 * 2; i++)
    {
      if (i <= 255)
      {
        // fade out
        analogWrite(color_new, i);
        delay(color_delay);
      }
      else
      {
        // fade in
        analogWrite(color_new, map(i - 255, 0, 255, 255, 0));
        delay(color_delay);
      }
    }
  }
  // Update
  color_current = color_new;
}

// Wifi initialization
void initWiFi()
{
  Serial.println("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected successfully");
  Serial.print("Got IP: ");
  Serial.println(WiFi.localIP());
  server.begin();
  Serial.println("HTTP server started");
  delay(100);
}

// HTML page
void buildPage()
{
  WiFiClient client = server.available();
  if (client)
  {
    Serial.println("New Client is requesting web page");
    String current_data_line = "";
    while (client.connected())
    {
      if (client.available())
      {
        char new_byte = client.read();
        Serial.write(new_byte);
        header += new_byte;
        if (new_byte == '\n')
        {
          if (current_data_line.length() == 0)
          {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();
            String incubator_status = "The incubator is waiting for you.";
            if (header.indexOf("GET /?TEMP=PLUS") >= 0)
            {
              temp_desired = temp_desired + 1;
            }
            else if (header.indexOf("GET /?TEMP=MINUS") != -1)
            {
              temp_desired = temp_desired - 1;
            }
            else if (header.indexOf("GET /?SWITCH=ON") != -1)
            {
              incubator_state = "On";
              str_incubator_status = str_incubator_on;
            }
            else if (header.indexOf("GET /?SWITCH=OFF") != -1)
            {
              incubator_state = "Off";
              str_incubator_status = str_incubator_off;
            }
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><meta charset='UTF-8'>");
            client.println("<style>body{font-family: SFMono-Regular, Menlo, Monaco, Consolas, 'Liberation Mono', 'Courier New', monospace; padding: 0 1vw;} button{margin-bottom: 2em;}</style>");
            client.println("</head>");
            client.println("<body><h1>☼ Incubator ☼</h1>");
            client.println("<p>" + str_incubator_status + "</p>");
            client.println("<form>");
            if (incubator_state == "On")
            {
              client.println("<button name=\"SWITCH\" value=\"OFF\" type=\"submit\">Stop ◼</button>");
            }
            else
            {
              client.println("<button name=\"SWITCH\" value=\"ON\" type=\"submit\">Start ▶</button>");
            }
            client.println("</form>");
            client.println("<hr>");
            client.println("<p>Current temperature: <b>" + String(temp_current) + "°C</b><br>");
            client.println("Current humidity: <b>" + String(humi_current) + "%</b></p>");
            client.println("<form>");
            client.println("<button type=\"submit\">Update ↻</button>");
            client.println("</form>");
            client.println("<hr>");
            client.println("<p>Desired temperature: <b>" + String(temp_desired) + "°C</b>.</p>");
            client.println("<form>");
            client.println("<button name=\"TEMP\" value=\"PLUS\" type=\"submit\">↑↑↑</button>");
            client.println("<button name=\"TEMP\" value=\"MINUS\" type=\"submit\">↓↓↓</button>");
            client.println("</form>");
            client.println("</body></html>");
            client.println();
            break;
          }
          else
          {
            current_data_line = "";
          }
        }
        else if (new_byte != '\r')
        {
          current_data_line += new_byte;
        }
      }
    }
    header = "";
    client.stop();
    Serial.println("Client disconnected.");
  }
}

void setup()
{
  Serial.begin(115200);
  delay(1000);
  dht.begin();
  pinMode(fan_pin, OUTPUT);
  pinMode(heat_pin, OUTPUT);
  pinMode(led_red, OUTPUT);
  pinMode(led_green, OUTPUT);
  pinMode(led_blue, OUTPUT);
  rgbLedClear();
  initWiFi();
}

void loop()
{
  delay(1000);
  humi_current = dht.readHumidity();
  temp_current = dht.readTemperature();
  buildPage();
  if (isnan(humi_current) || isnan(temp_current))
  {
    Serial.println("Failed to read from DHT sensor :(");
    return;
  }
  Serial.print("Humidity: ");
  Serial.print(humi_current);
  Serial.print("%\t");
  Serial.print("Temperature: ");
  Serial.print(temp_current);
  Serial.println("°C ");

  if (incubator_state == "On")
  {
    if (temp_current < temp_desired - temp_margin)
    {
      analogWrite(heat_pin, heat_max);
      analogWrite(fan_pin, fan_mid);
      rgbLed("red");
      str_incubator_status = str_incubator_heating;
    }
    else if (temp_current > temp_desired + temp_margin)
    {
      analogWrite(heat_pin, 0);
      analogWrite(fan_pin, fan_max);
      rgbLed("blue");
      str_incubator_status = str_incubator_cooling;
    }
    else
    {
      analogWrite(heat_pin, 0);
      analogWrite(fan_pin, 0);
      rgbLed("green");
      str_incubator_status = str_incubator_good;
    }
  }
  else if (incubator_state == "Off")
  {
    analogWrite(heat_pin, 0);
    analogWrite(fan_pin, 0);
    rgbLed("black");
  }
  Serial.println(str_incubator_status);
}
