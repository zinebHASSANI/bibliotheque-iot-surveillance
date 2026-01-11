#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

// ========== CONFIGURATION WiFi ==========
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// ========== CONFIGURATION MQTT ==========
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;

// Topics MQTT
const char* topic_temp = "bibliotheque/zineb/temperature";
const char* topic_hum = "bibliotheque/zineb/humidite";
const char* topic_alert = "bibliotheque/zineb/alerte";
const char* topic_status = "bibliotheque/zineb/status";
const char* topic_ventilateur = "bibliotheque/zineb/ventilateur";
const char* topic_deshumidificateur = "bibliotheque/zineb/deshumidificateur";
const char* topic_compteur = "bibliotheque/zineb/compteur";

// Topics de commande
const char* topic_cmd_ventilateur = "bibliotheque/zineb/cmd/ventilateur";
const char* topic_cmd_deshumidificateur = "bibliotheque/zineb/cmd/deshumidificateur";

// ========== CONFIGURATION DHT22 ==========
#define DHTPIN 15
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// ========== CONFIGURATION PINS ==========
#define LED_ROUGE 2          // Alerte critique
#define LED_JAUNE 22         // Avertissement
#define LED_VERTE 23         // Normal
#define SERVO_PIN 4          // Servo Motor (ventilateur)
#define RELAY_PIN 5          // Relay Module (déshumidificateur)

// ========== CONFIGURATION SERVO ==========
// Utilisation de PWM pour contrôler le servo (natif ESP32)
const int SERVO_CHANNEL = 0;      // Canal PWM
const int SERVO_FREQ = 50;        // Fréquence 50Hz pour servo
const int SERVO_RESOLUTION = 16;  // Résolution 16 bits
int servoPosition = 0;            // Position actuelle du servo

// ========== SEUILS ==========
const float SEUIL_TEMP_CRITIQUE = 28.0;
const float SEUIL_TEMP_ELEVE = 25.0;
const float SEUIL_HUM_CRITIQUE = 70.0;
const float SEUIL_HUM_ELEVE = 65.0;

// ========== VARIABLES GLOBALES ==========
WiFiClient espClient;
PubSubClient client(espClient);

int compteurAlertesTemp = 0;
int compteurAlertesHum = 0;
int compteurAlertesCritiques = 0;

bool ventilateurActif = false;
bool deshumidificateurActif = false;
bool modeManuel = false;

unsigned long dernierEnvoi = 0;
const long intervalEnvoi = 2000; // 2 secondes

unsigned long dernierMouvementServo = 0;
const long intervalServo = 50; // Mouvement du servo toutes les 50ms

// ========== FONCTION : Écrire angle servo ==========
void servoWrite(int angle) {
  // Convertir l'angle (0-180°) en duty cycle
  // Pour un servo standard: 1ms=0°, 1.5ms=90°, 2ms=180°
  // Avec 50Hz (20ms période) et résolution 16 bits (65535)
  int dutyCycle = map(angle, 0, 180, 1638, 8192); // ~1ms à 2ms
  ledcWrite(SERVO_PIN, dutyCycle);
}

// ========== FONCTION : Connexion WiFi ==========
void setup_wifi() {
  Serial.println("========================================");
  Serial.println("🌐 Connexion au WiFi...");
  WiFi.begin(ssid, password);
  
  int tentatives = 0;
  while (WiFi.status() != WL_CONNECTED && tentatives < 20) {
    delay(500);
    Serial.print(".");
    tentatives++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi connecté !");
    Serial.print("📍 Adresse IP : ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n❌ Échec de connexion WiFi");
  }
  Serial.println("========================================");
}

// ========== FONCTION : Callback MQTT ==========
void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  Serial.print("📥 Message reçu [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(message);
  
  // Contrôle manuel du ventilateur (Servo)
  if (String(topic) == topic_cmd_ventilateur) {
    if (message == "ON") {
      ventilateurActif = true;
      modeManuel = true;
      Serial.println("🌀 Ventilateur (Servo) activé manuellement");
    } else if (message == "OFF") {
      ventilateurActif = false;
      servoWrite(90); // Position neutre
      Serial.println("🌀 Ventilateur (Servo) désactivé manuellement");
    }
  }
  
  // Contrôle manuel du déshumidificateur (Relay)
  if (String(topic) == topic_cmd_deshumidificateur) {
    if (message == "ON") {
      deshumidificateurActif = true;
      modeManuel = true;
      digitalWrite(RELAY_PIN, HIGH);
      Serial.println("💧 Déshumidificateur (Relay) activé manuellement");
    } else if (message == "OFF") {
      deshumidificateurActif = false;
      digitalWrite(RELAY_PIN, LOW);
      Serial.println("💧 Déshumidificateur (Relay) désactivé manuellement");
    }
  }
}

// ========== FONCTION : Reconnexion MQTT ==========
void reconnect() {
  while (!client.connected()) {
    Serial.print("🔄 Connexion au broker MQTT...");
    
    String clientId = "ESP32_Bibliotheque_Zineb_";
    clientId += String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str())) {
      Serial.println(" ✅ Connecté !");
      
      // Souscription aux topics de commande
      client.subscribe(topic_cmd_ventilateur);
      client.subscribe(topic_cmd_deshumidificateur);
      
      Serial.println("📡 Souscription aux topics de commande OK");
      
    } else {
      Serial.print(" ❌ Échec, rc=");
      Serial.print(client.state());
      Serial.println(" ⏳ Nouvelle tentative dans 5s...");
      delay(5000);
    }
  }
}

// ========== FONCTION : Contrôle des LEDs d'état ==========
void controlerLEDs(String etat) {
  if (etat == "CRITIQUE") {
    digitalWrite(LED_ROUGE, HIGH);
    digitalWrite(LED_JAUNE, LOW);
    digitalWrite(LED_VERTE, LOW);
  } else if (etat == "AVERTISSEMENT") {
    digitalWrite(LED_ROUGE, LOW);
    digitalWrite(LED_JAUNE, HIGH);
    digitalWrite(LED_VERTE, LOW);
  } else { // NORMAL
    digitalWrite(LED_ROUGE, LOW);
    digitalWrite(LED_JAUNE, LOW);
    digitalWrite(LED_VERTE, HIGH);
  }
}

// ========== FONCTION : Faire tourner le servo (ventilateur) ==========
void animerVentilateur() {
  if (ventilateurActif) {
    unsigned long maintenant = millis();
    if (maintenant - dernierMouvementServo >= intervalServo) {
      dernierMouvementServo = maintenant;
      
      // Mouvement de va-et-vient pour simuler rotation
      servoPosition += 10;
      if (servoPosition > 180) {
        servoPosition = 0;
      }
      servoWrite(servoPosition);
    }
  }
}

// ========== FONCTION : Gestion des actionneurs ==========
void gererActionneurs(float temperature, float humidite) {
  if (modeManuel) {
    Serial.println("🔧 Mode manuel actif - Pas de contrôle automatique");
    return;
  }
  
  // Gestion du ventilateur (Servo pour température)
  if (temperature >= SEUIL_TEMP_CRITIQUE) {
    if (!ventilateurActif) {
      ventilateurActif = true;
      client.publish(topic_ventilateur, "ON");
      Serial.println("🌀 Ventilateur activé (Servo tourne - temp critique)");
    }
  } else if (temperature < SEUIL_TEMP_ELEVE) {
    if (ventilateurActif) {
      ventilateurActif = false;
      servoWrite(90); // Arrêt en position neutre
      client.publish(topic_ventilateur, "OFF");
      Serial.println("🌀 Ventilateur désactivé (Servo arrêté)");
    }
  }
  
  // Gestion du déshumidificateur (Relay Module pour humidité)
  if (humidite >= SEUIL_HUM_CRITIQUE) {
    if (!deshumidificateurActif) {
      digitalWrite(RELAY_PIN, HIGH);
      deshumidificateurActif = true;
      client.publish(topic_deshumidificateur, "ON");
      Serial.println("💧 Déshumidificateur activé (Relay ON - hum critique)");
    }
  } else if (humidite < SEUIL_HUM_ELEVE) {
    if (deshumidificateurActif) {
      digitalWrite(RELAY_PIN, LOW);
      deshumidificateurActif = false;
      client.publish(topic_deshumidificateur, "OFF");
      Serial.println("💧 Déshumidificateur désactivé (Relay OFF)");
    }
  }
}

// ========== FONCTION : Déterminer état du système ==========
String determinerEtat(float temperature, float humidite) {
  if (temperature >= SEUIL_TEMP_CRITIQUE || humidite >= SEUIL_HUM_CRITIQUE) {
    return "CRITIQUE";
  } else if (temperature >= SEUIL_TEMP_ELEVE || humidite >= SEUIL_HUM_ELEVE) {
    return "AVERTISSEMENT";
  } else {
    return "NORMAL";
  }
}

// ========== SETUP ==========
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n");
  Serial.println("╔════════════════════════════════════════╗");
  Serial.println("║   SYSTÈME IoT BIBLIOTHÈQUE - v2.0     ║");
  Serial.println("║   Surveillance Intelligente            ║");
  Serial.println("║   Servo + Relay + LEDs                 ║");
  Serial.println("╚════════════════════════════════════════╝");
  Serial.println();
  
  // Configuration des pins
  pinMode(LED_ROUGE, OUTPUT);
  pinMode(LED_JAUNE, OUTPUT);
  pinMode(LED_VERTE, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  
  // Initialisation du Servo avec PWM (API moderne ESP32)
  ledcAttach(SERVO_PIN, SERVO_FREQ, SERVO_RESOLUTION);
  servoWrite(90); // Position neutre (arrêt)
  Serial.println("✅ Servo Motor (Ventilateur) initialisé via PWM");
  
  // État initial
  digitalWrite(LED_VERTE, HIGH);
  digitalWrite(LED_ROUGE, LOW);
  digitalWrite(LED_JAUNE, LOW);
  digitalWrite(RELAY_PIN, LOW);
  
  // Initialisation DHT22
  dht.begin();
  Serial.println("✅ Capteur DHT22 initialisé");
  
  // Connexion WiFi
  setup_wifi();
  
  // Configuration MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  
  Serial.println("✅ Système prêt !");
  Serial.println("========================================\n");
}

// ========== LOOP ==========
void loop() {
  // Maintenir connexion MQTT
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
  // Animer le ventilateur si actif
  animerVentilateur();
  
  unsigned long maintenant = millis();
  
  // Envoi des données toutes les 2 secondes
  if (maintenant - dernierEnvoi >= intervalEnvoi) {
    dernierEnvoi = maintenant;
    
    // Lecture des capteurs
    float temperature = dht.readTemperature();
    float humidite = dht.readHumidity();
    
    // Vérification des données
    if (isnan(temperature) || isnan(humidite)) {
      Serial.println("❌ Erreur de lecture du capteur DHT22 !");
      return;
    }
    
    // Affichage
    Serial.println("─────────────────────────────────────");
    Serial.print("🌡️  Température : ");
    Serial.print(temperature, 1);
    Serial.println(" °C");
    Serial.print("💧 Humidité     : ");
    Serial.print(humidite, 1);
    Serial.println(" %");
    
    // Déterminer l'état
    String etat = determinerEtat(temperature, humidite);
    String messageStatus = "";
    
    if (etat == "CRITIQUE") {
      compteurAlertesCritiques++;
      if (temperature >= SEUIL_TEMP_CRITIQUE) compteurAlertesTemp++;
      if (humidite >= SEUIL_HUM_CRITIQUE) compteurAlertesHum++;
      messageStatus = "🔴 CRITIQUE";
      Serial.println("🚨 ÉTAT : CRITIQUE !");
    } else if (etat == "AVERTISSEMENT") {
      if (temperature >= SEUIL_TEMP_ELEVE) compteurAlertesTemp++;
      if (humidite >= SEUIL_HUM_ELEVE) compteurAlertesHum++;
      messageStatus = "🟡 AVERTISSEMENT";
      Serial.println("⚠️  ÉTAT : AVERTISSEMENT");
    } else {
      messageStatus = "🟢 NORMAL";
      Serial.println("✅ ÉTAT : NORMAL");
    }
    
    // Contrôle des LEDs d'état
    controlerLEDs(etat);
    
    // Gestion des actionneurs
    gererActionneurs(temperature, humidite);
    
    // Conversion en String pour MQTT
    String tempStr = String(temperature, 1);
    String humStr = String(humidite, 1);
    String compteurStr = String(compteurAlertesTemp) + "," + 
                        String(compteurAlertesHum) + "," + 
                        String(compteurAlertesCritiques);
    
    // Publication MQTT
    client.publish(topic_temp, tempStr.c_str());
    client.publish(topic_hum, humStr.c_str());
    client.publish(topic_status, messageStatus.c_str());
    client.publish(topic_compteur, compteurStr.c_str());
    
    // Message d'alerte détaillé
    if (etat == "CRITIQUE") {
      String msgAlerte = "ALERTE CRITIQUE! Temp: " + tempStr + "°C, Hum: " + humStr + "%";
      client.publish(topic_alert, msgAlerte.c_str());
    } else if (etat == "AVERTISSEMENT") {
      String msgAlerte = "AVERTISSEMENT! Temp: " + tempStr + "°C, Hum: " + humStr + "%";
      client.publish(topic_alert, msgAlerte.c_str());
    } else {
      client.publish(topic_alert, "Conditions normales");
    }
    
    // Affichage actionneurs
    Serial.print("🌀 Ventilateur (Servo)   : ");
    Serial.print(ventilateurActif ? "ON ✅ (Position: " : "OFF (Position: 90°)");
    if (ventilateurActif) {
      Serial.print(servoPosition);
      Serial.println("°)");
    } else {
      Serial.println();
    }
    Serial.print("💧 Déshumidif. (Relay)   : ");
    Serial.println(deshumidificateurActif ? "ON ✅" : "OFF");
    Serial.print("📊 Alertes: Temp=");
    Serial.print(compteurAlertesTemp);
    Serial.print(" | Hum=");
    Serial.print(compteurAlertesHum);
    Serial.print(" | Crit=");
    Serial.println(compteurAlertesCritiques);
    Serial.println("─────────────────────────────────────\n");
  }
}