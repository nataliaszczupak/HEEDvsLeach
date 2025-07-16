#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

// --- User configuration ---
// TODO: Replace with your Wi-Fi credentials
const char* WIFI_SSID = "YOUR_SSID";
const char* WIFI_PASS = "YOUR_PASSWORD";

// TODO: Replace with this node's IP and your network settings
IPAddress THIS_NODE_IP(192, 168, 1, 50);
IPAddress GATEWAY(192, 168, 1, 1);
IPAddress SUBNET(255, 255, 255, 0);

// TODO: Replace with other nodes' IPs
IPAddress NODE1_IP(192, 168, 1, 20);
IPAddress NODE2_IP(192, 168, 1, 30);
IPAddress NODE3_IP(192, 168, 1, 40);

const char* NODE1_IP_STR = "192.168.1.20";
const char* NODE2_IP_STR = "192.168.1.30";
const char* NODE3_IP_STR = "192.168.1.40";

// --- Other constants ---
const int UDP_PORT = 1234;
const int SENSOR_PIN = A0;

// --- Global ---
WiFiUDP udp;
extern int Cluster = -127;

void setup() {
  Serial.begin(115200);

  // Configure static IP
  WiFi.config(THIS_NODE_IP, GATEWAY, SUBNET);

  // Connect to Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
  }
  Serial.println("Connected to Wi-Fi.");

  udp.begin(UDP_PORT);
}

void loop() {
  int measurement = analogRead(SENSOR_PIN);
  int receivedValue;
  int packetSize = udp.parsePacket();

  // Send sensor measurement to the assigned cluster-head
  switch (Cluster) {
    case -6:
      udp.beginPacket(NODE1_IP_STR, UDP_PORT);
      udp.write((uint8_t*)&measurement, sizeof(measurement));
      udp.endPacket();
      break;
    case -8:
      udp.beginPacket(NODE2_IP_STR, UDP_PORT);
      udp.write((uint8_t*)&measurement, sizeof(measurement));
      udp.endPacket();
      break;
    case -9:
      udp.beginPacket(NODE3_IP_STR, UDP_PORT);
      udp.write((uint8_t*)&measurement, sizeof(measurement));
      udp.endPacket();
      break;
    case -10:
      Serial.println("This Node (COM10): " + String(measurement));
      break;
  }

  // Check for incoming packets
  if (packetSize == sizeof(receivedValue)) {
    udp.read((char*)&receivedValue, sizeof(receivedValue));

    if (receivedValue < 0) {
      Cluster = receivedValue;
      if (receivedValue == -127) {
        Serial.println("(-) No cluster-head assigned.");
      } else {
        Serial.println("COM" + String(-Cluster) + " becomes the cluster-head.");
      }
    } else {
      IPAddress senderIP = udp.remoteIP();
      if (senderIP == NODE1_IP) Serial.print("COM6: ");
      else if (senderIP == NODE2_IP) Serial.print("COM8: ");
      else if (senderIP == NODE3_IP) Serial.print("COM9: ");
      Serial.println(String(receivedValue));
    }
  }

  delay(1000);
}
