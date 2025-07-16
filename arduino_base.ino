#include <WiFiNINA.h>
#include <WiFiUdp.h>
#include <limits>
#include <math.h>

#define NODE_COUNT 4

bool useHEED = true;  // true: HEED, false: LEACH

// TODO: Replace with your Wi-Fi credentials
const char* WIFI_SSID = "YOUR_SSID";
const char* WIFI_PASS = "YOUR_PASSWORD";

// TODO: Replace with your nodes' IP addresses
IPAddress NODE1_IP(192, 168, 1, 20);
IPAddress NODE2_IP(192, 168, 1, 30);
const char* NODE2_IP_STR = "192.168.1.30";
IPAddress NODE3_IP(192, 168, 1, 40);
const char* NODE3_IP_STR = "192.168.1.40";
IPAddress NODE4_IP(192, 168, 1, 50);
const char* NODE4_IP_STR = "192.168.1.50";

const int UDP_PORT = 1234;
const int SENSOR_PIN = A0;

WiFiUDP udp;
int Cluster_NODE1, Cluster_NODE2, Cluster_NODE3, Cluster_NODE4;

struct Node {
  double energy, pos_x, pos_y, distance_to_head;
  int id, is_head = 0, assigned_to_cluster = false, reports_to = -127;
  bool is_dead = false;
};

Node nodes[NODE_COUNT], base_station;

// Energy parameters
const double E_TX = 50e-9, E_RX = 50e-9, E_DA = 5e-9, E_FS = 10e-12, E_MP = 0.0013e-12;
const int L = 4000;
const double D0 = sqrt(E_FS / E_MP);
const double ENERGY_DEAD = 0.0;

int round_number = 0;
unsigned long previous_time = 0;
const unsigned long interval = 5000; // ms

// --- Helper functions ---
double calculate_distance(Node a, Node b) {
  return sqrt(sq(a.pos_x - b.pos_x) + sq(a.pos_y - b.pos_y));
}

double energyTransmit(double d) {
  return (d < D0) ? (E_TX * L + E_FS * L * d * d) : (E_TX * L + E_MP * L * pow(d, 4));
}

double energyReceive() {
  return (E_RX + E_DA) * L;
}

// --- HEED clustering ---
void heedClustering() {
  // 1. Reset flags
  for (int i = 0; i < NODE_COUNT; i++) {
    nodes[i].is_head = 0;
    nodes[i].assigned_to_cluster = false;
    nodes[i].distance_to_head = INFINITY;
  }

  // 2. Select heads
  bool unassigned_nodes_exist = true;
  while (unassigned_nodes_exist) {
    int head_node = -1;
    double max_energy = -1;

    for (int i = 0; i < NODE_COUNT; i++) {
      if (!nodes[i].assigned_to_cluster && !nodes[i].is_dead &&
          nodes[i].energy > max_energy) {
        head_node = i;
        max_energy = nodes[i].energy;
      }
    }

    if (head_node == -1) break;

    nodes[head_node].is_head = 1;
    nodes[head_node].assigned_to_cluster = true;
    nodes[head_node].reports_to = nodes[head_node].id;

    for (int i = 0; i < NODE_COUNT; i++) {
      if (!nodes[i].is_head && !nodes[i].assigned_to_cluster && !nodes[i].is_dead) {
        double dist_to_head = calculate_distance(nodes[i], nodes[head_node]);
        double dist_to_base = calculate_distance(nodes[i], base_station);
        if (dist_to_head < dist_to_base) {
          nodes[i].distance_to_head = dist_to_head;
          nodes[i].assigned_to_cluster = true;
          nodes[i].reports_to = nodes[head_node].id;
        }
      }
    }

    unassigned_nodes_exist = false;
    for (int j = 0; j < NODE_COUNT; j++) {
      if (!nodes[j].assigned_to_cluster && !nodes[j].is_dead) {
        unassigned_nodes_exist = true;
        break;
      }
    }
  }

  // 3. Reassign to closer heads if needed
  for (int i = 0; i < NODE_COUNT; i++) {
    if (!nodes[i].is_head && !nodes[i].is_dead) {
      for (int j = 0; j < NODE_COUNT; j++) {
        if (nodes[j].is_head && !nodes[j].is_dead) {
          double new_dist = calculate_distance(nodes[i], nodes[j]);
          if (new_dist < nodes[i].distance_to_head) {
            nodes[i].distance_to_head = new_dist;
            nodes[i].reports_to = nodes[j].id;
          }
        }
      }
    }
  }

  // 4. Energy consumption
  for (int i = 0; i < NODE_COUNT; i++) {
    if (nodes[i].is_dead) continue;

    if (nodes[i].is_head) {
      double dBS = calculate_distance(nodes[i], base_station);
      nodes[i].energy -= energyTransmit(dBS);
    } else {
      nodes[i].energy -= energyTransmit(nodes[i].distance_to_head);
      for (int j = 0; j < NODE_COUNT; j++) {
        if (nodes[j].id == nodes[i].reports_to && !nodes[j].is_dead) {
          nodes[j].energy -= energyReceive();
          break;
        }
      }
    }

    if (nodes[i].energy <= ENERGY_DEAD) {
      nodes[i].energy = 0.0;
      nodes[i].is_dead = true;
      nodes[i].reports_to = -127;
    }
  }

  // 5. Update cluster assignments
  Cluster_NODE1 = nodes[0].reports_to;
  Cluster_NODE2 = nodes[1].reports_to;
  Cluster_NODE3 = nodes[2].reports_to;
  Cluster_NODE4 = nodes[3].reports_to;
}

// --- LEACH clustering ---
void leachClustering(int round) {
  const double p = 0.5;

  auto thresholdT = [&](int round, int nodeID) -> double {
    int r_mod = round % static_cast<int>(1.0 / p);
    double denom = (1 - p * r_mod);
    if (denom <= 0.0) denom = 1e-9;
    return p / denom;
  };

  for (int i = 0; i < NODE_COUNT; i++) {
    nodes[i].is_head = 0;
    nodes[i].assigned_to_cluster = false;
    nodes[i].distance_to_head = INFINITY;
    nodes[i].reports_to = -127;
  }

  for (int i = 0; i < NODE_COUNT; i++) {
    if (nodes[i].is_dead) continue;
    double randVal = static_cast<double>(random(10000)) / 10000.0;
    if (randVal < thresholdT(round, nodes[i].id)) {
      nodes[i].is_head = 1;
      nodes[i].assigned_to_cluster = true;
      nodes[i].reports_to = nodes[i].id;
    }
  }

  for (int i = 0; i < NODE_COUNT; i++) {
    if (nodes[i].is_dead || nodes[i].is_head) continue;
    double min_dist = INFINITY;
    int closest_head = -1;
    for (int j = 0; j < NODE_COUNT; j++) {
      if (!nodes[j].is_dead && nodes[j].is_head) {
        double dist = calculate_distance(nodes[i], nodes[j]);
        if (dist < min_dist) {
          min_dist = dist;
          closest_head = j;
        }
      }
    }

    if (closest_head != -1) {
      nodes[i].distance_to_head = min_dist;
      nodes[i].reports_to = nodes[closest_head].id;
      nodes[i].assigned_to_cluster = true;
    }
  }

  for (int i = 0; i < NODE_COUNT; i++) {
    if (!nodes[i].is_dead && nodes[i].reports_to == -127) {
      nodes[i].is_head = 1;
      nodes[i].reports_to = nodes[i].id;
      nodes[i].assigned_to_cluster = true;
      nodes[i].distance_to_head = 0;
    }
  }

  for (int i = 0; i < NODE_COUNT; i++) {
    if (nodes[i].is_dead) continue;

    if (nodes[i].is_head) {
      double dBS = calculate_distance(nodes[i], base_station);
      nodes[i].energy -= energyTransmit(dBS);
    } else {
      if (isfinite(nodes[i].distance_to_head))
        nodes[i].energy -= energyTransmit(nodes[i].distance_to_head);

      for (int j = 0; j < NODE_COUNT; j++) {
        if (nodes[j].id == nodes[i].reports_to && !nodes[j].is_dead) {
          nodes[j].energy -= energyReceive();
          break;
        }
      }
    }

    if (nodes[i].energy <= ENERGY_DEAD || !isfinite(nodes[i].energy)) {
      nodes[i].energy = 0.0;
      nodes[i].is_dead = true;
      nodes[i].reports_to = -127;
    }
  }

  Cluster_NODE1 = nodes[0].reports_to;
  Cluster_NODE2 = nodes[1].reports_to;
  Cluster_NODE3 = nodes[2].reports_to;
  Cluster_NODE4 = nodes[3].reports_to;
}

void setup() {
  Serial.begin(19600);
  randomSeed(analogRead(SENSOR_PIN));
  base_station.id = -1; base_station.pos_x = 0; base_station.pos_y = 0;
  nodes[0].id = -6; nodes[1].id = -8; nodes[2].id = -9; nodes[3].id = -10;

  for (int i = 0; i < NODE_COUNT; i++) {
    nodes[i].energy = random(101) / 20000.0; // 0.5 J - 0.55 J
    nodes[i].pos_x = random(101);
    nodes[i].pos_y = random(101);
    nodes[i].distance_to_head = INFINITY;
  }

  WiFi.config(NODE1_IP);
  while (WiFi.begin(WIFI_SSID, WIFI_PASS) != WL_CONNECTED) delay(1000);
  Serial.println("Connected to Wi-Fi.");
  udp.begin(UDP_PORT);
}

void loop() {
  unsigned long current_time = millis();
  if (current_time - previous_time >= interval) {
    previous_time = current_time;

    if (useHEED) heedClustering();
    else leachClustering(round_number++);

    Serial.print("Energy: ");
    for (int i = 0; i < NODE_COUNT; i++) {
      Serial.print(nodes[i].energy, 5);
      Serial.print(" ");
    }
    Serial.println();

    udp.beginPacket(NODE2_IP_STR, UDP_PORT);
    udp.write((uint8_t *)&Cluster_NODE2, sizeof(Cluster_NODE2));
    udp.endPacket();

    udp.beginPacket(NODE3_IP_STR, UDP_PORT);
    udp.write((uint8_t *)&Cluster_NODE3, sizeof(Cluster_NODE3));
    udp.endPacket();

    udp.beginPacket(NODE4_IP_STR, UDP_PORT);
    udp.write((uint8_t *)&Cluster_NODE4, sizeof(Cluster_NODE4));
    udp.endPacket();

    for (int i = 0; i < NODE_COUNT; i++)
      if (nodes[i].reports_to == -127) Serial.print("(-) ");
      else Serial.print(String(nodes[i].reports_to) + " ");
    Serial.println();
  }

  int measurement = analogRead(SENSOR_PIN);
  int incomingData;
  int packetSize = udp.parsePacket();

  switch (Cluster_NODE1) {
    case -6:
      Serial.println("NODE1: " + String(measurement));
      break;
    case -8:
      udp.beginPacket(NODE2_IP_STR, UDP_PORT);
      udp.write((uint8_t *)&measurement, sizeof(measurement));
      udp.endPacket();
      break;
    case -9:
      udp.beginPacket(NODE3_IP_STR, UDP_PORT);
      udp.write((uint8_t *)&measurement, sizeof(measurement));
      udp.endPacket();
      break;
    case -10:
      udp.beginPacket(NODE4_IP_STR, UDP_PORT);
      udp.write((uint8_t *)&measurement, sizeof(measurement));
      udp.endPacket();
      break;
  }

  if (packetSize == sizeof(incomingData)) {
    udp.read((char *)&incomingData, sizeof(incomingData));
    IPAddress remoteIP = udp.remoteIP();
    if (remoteIP == NODE2_IP) Serial.print("NODE2: ");
    else if (remoteIP == NODE3_IP) Serial.print("NODE3: ");
    else if (remoteIP == NODE4_IP) Serial.print("NODE4: ");
    Serial.println(String(incomingData));
  }

  delay(1000);
}
