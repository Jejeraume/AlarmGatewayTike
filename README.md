# AlarmGatewayTike

**Passerelle ESP32 entre une centrale d'alarme, le réseau Ethernet, le modem GSM et Home Assistant.**

AlarmGatewayTike est un projet basé sur **ESP32** permettant d'interfacer une centrale d'alarme avec différents services et équipements externes.

La passerelle communique directement avec la centrale par **Ethernet**, surveille son état périodiquement et permet de transmettre les événements vers :

- 📱 des téléphones par **SMS**
- ☎️ des téléphones par **appel vocal**
- 🏠 **Home Assistant**
- 🌐 une **interface Web** de configuration et de supervision
- 📩 la centrale d'alarme via des **commandes SMS**

Le projet est conçu pour fonctionner sur une carte **WT32-ETH01**, équipée d'un contrôleur Ethernet LAN8720.

---

## Fonctionnalités

### Surveillance de la centrale

La passerelle interroge régulièrement la centrale d'alarme.

Par défaut :

```text
Intervalle de polling : 10 secondes
```

L'intervalle peut être configuré depuis l'interface Web.

Lorsqu'un nouvel événement est détecté, la passerelle compare l'événement avec le précédent afin d'éviter les notifications répétitives.

---

### Détection de perte de communication

La communication avec la centrale est surveillée à chaque interrogation.

En cas de perte de communication :

```text
Perte de communication avec la centrale
```

un SMS est envoyé aux numéros configurés.

Un seul SMS est envoyé pendant la période de perte afin d'éviter de recevoir une alerte toutes les 10 secondes.

Lorsque la communication est rétablie :

```text
Communication avec la centrale rétablie
```

un nouveau SMS est envoyé.

Une nouvelle perte de communication pourra alors générer une nouvelle alerte.

---

### Notifications SMS

Les événements de la centrale peuvent déclencher l'envoi de SMS.

Les numéros de téléphone sont configurables depuis l'interface Web.

Jusqu'à **4 numéros** peuvent être configurés.

Le nombre de tentatives d'appel peut également être configuré.

---

### Appels téléphoniques

Certains événements peuvent déclencher automatiquement un appel vocal.

La passerelle utilise un modem **SIMCom A7670E**.

Le nombre de tentatives d'appel est configurable.

---

### Commandes SMS

La passerelle peut recevoir des SMS contenant des commandes destinées à la centrale.

Les commandes sont protégées par un code.

Le format général est :

```text
#PWD<code>#<commande>
```

Les commandes disponibles sont configurables dans le firmware.

Elles permettent notamment de :

- armer totalement la centrale
- armer en mode partiel
- désarmer
- annuler une alarme
- demander l'état de la centrale

---

### Home Assistant

AlarmGatewayTike peut transmettre les événements de la centrale à **Home Assistant**.

La communication utilise l'API HTTP de Home Assistant.

Les paramètres configurables sont :

```text
Adresse IP Home Assistant
Port
Token d'authentification
```

Les événements sont transmis sous forme de requête HTTP.

Exemple :

```text
POST /api/events/alarmgateway_event
```

avec les informations principales de l'événement :

```json
{
  "code": "...",
  "event": "...",
  "date": "..."
}
```

---

## Architecture

Le programme est organisé en plusieurs modules afin de séparer les différentes fonctions du système.

```text
AlarmGatewayTike
│
├── AlarmGatewayTike.ino
│
├── AlarmClient
│   ├── AlarmClient.h
│   └── AlarmClient.cpp
│
├── ConfigStore
│   ├── ConfigStore.h
│   └── ConfigStore.cpp
│
├── ModemAT
│   ├── ModemAT.h
│   └── ModemAT.cpp
│
├── SmsCommands
│   ├── SmsCommands.h
│   └── SmsCommands.cpp
│
├── WebInterface
│   ├── WebInterface.h
│   └── WebInterface.cpp
│
├── AlarmTypes
│   └── AlarmTypes.h
│
└── BuildConfig
    └── BuildConfig.h
```

### Rôle des principaux modules

| Module | Fonction |
|---|---|
| `AlarmGatewayTike.ino` | Programme principal et orchestration du système |
| `AlarmClient` | Communication HTTP avec la centrale |
| `ConfigStore` | Gestion de la configuration |
| `ModemAT` | Communication avec le modem A7670E |
| `SmsCommands` | Analyse et exécution des commandes SMS |
| `WebInterface` | Interface Web |
| `AlarmTypes` | Structures de données liées aux événements |
| `BuildConfig` | Paramètres matériels et paramètres de compilation |

---

## Fonctionnement général

Le fonctionnement principal peut être résumé ainsi :

```text
                 ┌──────────────────────┐
                 │      Centrale        │
                 │      d'alarme        │
                 └──────────┬───────────┘
                            │
                         Ethernet
                            │
                            ▼
                 ┌──────────────────────┐
                 │   AlarmGatewayTike   │
                 │        ESP32         │
                 └──────┬─────┬─────┬──┘
                        │     │     │
             ┌──────────┘     │     └──────────┐
             │                │                │
             ▼                ▼                ▼
          A7670E         Home Assistant     Interface
             │                                  Web
        SMS / appels
```

---

## Matériel

### Contrôleur principal

**WT32-ETH01**

- ESP32
- Ethernet LAN8720
- interface RJ45
- alimentation et GPIO accessibles

### Modem GSM

**SIMCom A7670E**

Communication par UART2 :

```text
Baudrate : 115200
RX       : GPIO 5
TX       : GPIO 17
```

Connexion :

```text
A7670E TX → ESP32 GPIO5
A7670E RX → ESP32 GPIO17
GND       → GND
```

### Ethernet

Le WT32-ETH01 utilise le contrôleur Ethernet LAN8720 intégré.

Configuration utilisée :

```cpp
ETH.begin(
    ETH_PHY_LAN8720,
    1,
    23,
    18,
    16,
    ETH_CLOCK_GPIO0_IN
);
```

---

## Gestion du réseau

Au démarrage, la passerelle tente d'obtenir une configuration réseau par DHCP.

Si DHCP est disponible, la passerelle utilise la configuration obtenue.

En cas d'absence de DHCP, un mode de connexion directe est utilisé.

### Mode DHCP

```text
ESP32
  │
  └── Ethernet ──► Réseau local
```

### Mode connexion directe

La passerelle utilise :

```text
ESP32
IP       : 192.168.1.1
Masque   : 255.255.255.0
Gateway  : 0.0.0.0
DNS      : 0.0.0.0
```

La centrale utilise alors son adresse :

```text
192.168.1.81
```

Ce mode permet notamment une connexion directe entre la passerelle et la centrale sans routeur DHCP.

---

## Centrale d'alarme

La communication avec la centrale utilise le protocole HTTP.

Les principales ressources utilisées sont notamment :

```text
/SystemLog.htm
/RemoteCtr.htm
/AlarmEvent.htm
```

La communication HTTP utilise une authentification **Basic Authentication**.

`AlarmClient` encapsule cette communication afin que le reste du programme n'ait pas à gérer directement les requêtes HTTP.

---

## Gestion des événements

Les événements sont représentés par la structure `AlarmEntry`.

Un événement contient notamment :

- date
- état
- code
- signature permettant d'identifier un changement

Le programme conserve le dernier événement connu.

Le principe est :

```text
Polling
   │
   ▼
Lecture centrale
   │
   ├── Erreur ──► Gestion perte communication
   │
   └── OK
        │
        ▼
   Comparaison avec événement précédent
        │
        ├── Identique ──► aucune notification
        │
        └── Différent
               │
               ▼
         Notification événement
```

---

## Configuration

La configuration locale contient notamment :

### Centrale

```text
Adresse IP
Utilisateur
Mot de passe
```

### Réseau Wi-Fi

```text
SSID
Mot de passe
```

### Téléphonie

```text
4 numéros de téléphone
Nombre de tentatives d'appel
Mot de passe des commandes SMS
```

### Polling

```text
Intervalle d'interrogation
```

Valeurs :

```text
Défaut : 10 secondes
Minimum : 1 seconde
Maximum : 3600 secondes
```

### Home Assistant

```text
Adresse IP
Port
Token
```

### Événements

Le système possède une table locale de **40 événements** permettant de définir, pour chaque événement :

```text
Nom
SMS
Appel vocal
Home Assistant
```

---

## Interface Web

Une interface Web permet de consulter et configurer la passerelle.

Elle permet notamment de gérer :

- configuration réseau
- paramètres de la centrale
- numéros de téléphone
- paramètres du modem
- événements
- intervalle de polling
- Home Assistant
- état de communication avec la centrale
- dernier événement reçu

La configuration peut également être exportée et importée sous forme de fichier `.cfg`.

---

## Sécurité

Les communications avec la centrale utilisent une authentification HTTP Basic.

Les commandes SMS nécessitent également un code d'authentification.

Les fichiers de configuration contenant des informations sensibles ne doivent **pas être ajoutés au dépôt Git**.

Il est recommandé d'utiliser uniquement des fichiers de configuration d'exemple dans le dépôt.

---

## Installation

### 1. Matériel

Connecter :

```text
WT32-ETH01
     │
     ├── Ethernet ── Centrale
     │
     └── UART ────── A7670E
```

### 2. Arduino IDE

Sélectionner une carte ESP32 compatible.

Le firmware actuel nécessite un **ESP32**.

Le projet n'est plus destiné aux ESP8266.

### 3. Configuration

Configurer les paramètres nécessaires depuis l'interface Web.

En particulier :

```text
Adresse IP de la centrale
Identifiants de la centrale
Numéros de téléphone
Paramètres A7670E
Paramètres Home Assistant
```

### 4. Première connexion

En mode réseau local, la passerelle utilise DHCP lorsqu'il est disponible.

En mode connexion directe :

```text
ESP32 : 192.168.1.1
Centrale : 192.168.1.81
```

---

## État du projet

**Version actuelle : `6.0.0-dev1`**

Le projet est actuellement en développement.

Les fonctionnalités principales déjà intégrées sont :

- [x] ESP32 / WT32-ETH01
- [x] Ethernet LAN8720
- [x] DHCP
- [x] Mode connexion directe
- [x] Communication HTTP avec la centrale
- [x] Authentification centrale
- [x] Polling périodique
- [x] Détection de changement d'état
- [x] SMS
- [x] Appels téléphoniques
- [x] Commandes SMS
- [x] Détection de perte de communication
- [x] Notification de rétablissement
- [x] Interface Web
- [x] Configuration locale
- [x] Export / import de configuration
- [x] Intégration Home Assistant
- [x] Gestion de 40 événements

---

## Avertissement

Ce projet interagit avec un système d'alarme réel.

Il est recommandé de tester les fonctions de commande et de notification dans un environnement contrôlé avant toute utilisation opérationnelle.

L'auteur ne peut être tenu responsable d'une mauvaise configuration, d'une perte de communication ou d'un fonctionnement incorrect de la centrale d'alarme.

---

## Licence

Projet personnel en cours de développement.

La licence du projet sera précisée ultérieurement.