# bibliotheque-iot-surveillance
Système IoT intelligent de surveillance pour bibliothèque avec ESP32, MQTT et Node-RED
Système complet de surveillance et contrôle automatique pour la préservation des livres et documents dans une bibliothèque ou salle d'archives.
Ce projet IoT permet de :

Surveiller la température et l'humidité en temps réel
Activer automatiquement les systèmes de ventilation et déshumidification
Visualiser les données sur un dashboard web interactif
Recevoir des alertes visuelles selon l'état du système
Contrôler les actionneurs à distance via Node-RED


Démonstration:
https://wokwi.com/projects/452156716409151489
http://localhost:1880/ui/#!/0?socketid=ZXSYrSgTpy_WUUbUAAAl
http://127.0.0.1:1880/#flow/750026b40a2d664f
Schéma de connexion
DHT22 (Capteur)

VCC → 3.3V
GND → GND
DATA → GPIO 15

LEDs
LED Rouge (Alerte critique):

Cathode (-) → GND
Anode (+) → Résistance 220Ω → GPIO 2

LED Jaune (Avertissement):

Cathode (-) → GND
Anode (+) → Résistance 220Ω → GPIO 22

LED Verte (Normal):

Cathode (-) → GND
Anode (+) → Résistance 220Ω → GPIO 23

Servo Motor (Ventilateur)

Signal (Orange/Jaune) → GPIO 4
VCC (Rouge) → 5V (ou VIN)
GND (Marron/Noir) → GND

Module Relay (Déshumidificateur)

VCC → 5V
GND → GND
IN (Signal) → GPIO 5
Seuils de fonctionnement
Température
 NORMAL : < 25°C 
 AVERTISSEMENT : 25°C - 27.9°C
 CRITIQUE: ≥ 28°C
Humidité
 NORMAL : < 65%
 AVERTISSEMENT : 65% - 69.9%
 CRITIQUE: ≥ 70%
 
 Installation et Configuration
1. Configuration Wokwi (Simulation)

Ouvrez le projet sur Wokwi : https://wokwi.com/projects/452156716409151489
Les bibliothèques nécessaires :DHT sensor library,PubSubClient
Lancez la simulation

2. Installation Node-RED
Installer Node-RED (si pas déjà installé)
npm install -g node-red

# Installer le module dashboard
npm install node-red-dashboard

# Lancer Node-RED
node-red


3. Importer le flow Node-RED

Accédez à Node-RED : http://localhost:1880
Menu (☰) → Import → Clipboard
Collez le contenu de node-red/flows.json
Cliquez sur "Import"

4. Accéder au Dashboard
Ouvrez votre navigateur : http://localhost:1880/ui/#!/0?socketid=ZXSYrSgTpy_WUUbUAAAl
