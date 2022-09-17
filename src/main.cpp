// ------------------------------------------------------------------------------------------------------
//   Nom du Projet                          : Aspir-O-Matic Client
//   Auteur                                 : Patrick LABAU
//   Date de création                       : 17.09.2022
//   Version                                : 1.0
//   Description                            : Gestion de l'aspiration de l'atelier
//-------------------------------------------------------------------------------------------------------
// ToDo -> Wifi Manager pour installation simple ?
//-------------------------------------------------------------------------------------------------------

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <CSE7766.h>
#include <SoftwareSerial.h>
#include <ESPmDNS.h>

// Creation du Serveur Web
WebServer www(80);

// ------------------------------------------------------------------------------------------------------
// PARAMETRAGE DE CE SYSTEME
// ------------------------------------------------------------------------------------------------------

// Nom de La Machine recevant ce boitier (pas d'espace !) Sera l'adresse du web (ici machine.local)
String NOM_DE_LA_MACHINE_CLIENTE = "machine";

// IP AspirOMatic Serveur (facultatif mettre "" pour autodetection)
String IP_DU_SERVEUR = "";

// Activation ou non de la mise à jour a distance.
#define UPDATE_FIRMWARE_A_DISTANCE 1

// Utilisation du bouton physique pour couper ou pas l'alim de la machine
#define BOUTON_ACTIF 1

// Parametre du Wifi
const char *SSID = "NOM_DU_WIFI";
const char *PASSWORD = "MOT_DE_PASSE_WIFI";

// ------------------------------------------------------------------------------------------------------
// NE RIEN TOUCHER ENSUITE
// ------------------------------------------------------------------------------------------------------
// Définition des entrées/sorties
#define PIN_LED_STATUS 18
#define PIN_LED_WIFI 5
#define PIN_RELAIS 13
#define PIN_BOUTON 0

// variables
String ReturnData = "";
int ReturnStatus;
bool Button_LastState = true;
bool Button_CurrentState;
bool ServeurOK_Via_MDNS = false;

// Capteur
CSE7766 Capteur;
#define PIN_CAPTEUR 16
#define READ_INTERVAL 500
float Mesure_Tension = 0;
float Mesure_Courant = 0;
float Mesure_Puissance = 0;

// Client HTTP
WiFiClient WifiClient;
HTTPClient http;

// Informations de connexion + OTA
#if UPDATE_FIRMWARE_A_DISTANCE == 1
#include <ArduinoOTA.h>
#endif

// Page html
const char www_root[] PROGMEM = R"=====(
<!doctype html>
<html lang="fr">

<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">

    <title>Aspir-O-Matic - %MACHINE% </title>
    <style>
        header {
            height: 205px;
        }

        #IMG_Logo {
            position: absolute;
            top: 75px;
            left: 10px;
        }

        #Stats {
            position: absolute;
            top: 75px;
            right: 10px;
            text-align: right;
        }

        #StatsTitle {
            text-align: right;
            font-family: Arial, Helvetica, sans-serif;
            font-size: 18px;
            font-weight: bold;
            color: darkcyan;
        }

        #StatsValServeur,
        #StatsValTension,
        #StatsValCourant,
        #StatsValPuissance {
            text-align: left;
            font-family: Arial, Helvetica, sans-serif;
            font-size: 18px;
            font-weight: bold;
            color: darkcyan;
        }

        h1,
        h2 {
            font: 3em Arial;
            text-align: center;
            font-weight: bold;
        }

        hr {
            padding: 0px;
            margin: 0px;
        }

        #Titre {
            font: 3em Arial;
            font-weight: bold;
            color: darkcyan;
            text-align: center;
            display: inline-block;
            width: 100%;
        }

        #Etat {
            font: 3em Arial;
            font-weight: bold;
            color: darkcyan;
            text-align: center;
            display: inline-block;
            margin: 10px 0px;
            width: 100%;
        }

        .bouton {
            font: 3em Arial;
            color: white;
            text-align: center;
            text-decoration: none;
            background-color: darkcyan;
            border: 1px;
            border-radius: 10px;
            padding: 5px 0px;
            display: inline-block;
            margin: 10px 0px;
            cursor: pointer;
            width: 100%;
        }
    </style>
</head>

<header>
    <div id="Titre">Aspir-O-Matic</div>
    <hr>
    <img id="IMG_Logo"
        src="data:image/png;base64,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">
    <table id="Stats">
        <tr>
            <td id="StatsTitle">Serveur :</td>
            <td id="StatsValServeur">Init...</td>
        </tr>
        <tr>
            <td id="StatsTitle">Tension :</td>
            <td id="StatsValTension">Init...</td>
        </tr>
        <tr>
            <td id="StatsTitle">Courant :</td>
            <td id="StatsValCourant">Init...</td>
        </tr>
        <tr>
            <td id="StatsTitle">Puissance :</td>
            <td id="StatsValPuissance">Init...</td>
        </tr>
    </table>
</header>

<body>
    <hr>
    <h1 id="Etat">Init...</h1>
    <hr>

    <div>
        <button class="bouton" onclick="appelServeur('/relais', traiteReponse)">Alim ON/OFF</button>
    </div>

    <div>
        <button class="bouton" onclick="appelServeur('/reset', traiteReponse)">Redémarrer</button>
    </div>

    <script>
        // Rafraichissement apres clic sur bouton
        function appelServeur(url, cFonction) {
            var xhttp = new XMLHttpRequest();
            xhttp.onreadystatechange = function () {
                if (this.readyState == 4 && this.status == 200) {
                    cFonction(this);
                }
            };
            xhttp.open("GET", url, true);
            xhttp.send();
        }
        // Rafraichissement apres clic sur bouton
        function traiteReponse(xhttp) {
            document.getElementById("Etat").innerHTML = xhttp.responseText;
        }

        //Rafraichissement Auto toute les Secondes
        setInterval(function () { requestData(); }, 1000);

        function requestData() {

            var xhttp = new XMLHttpRequest();
            xhttp.open("GET", "ReadStatus", true);

            xhttp.onload = function () {
                if (xhttp.status === 200) {

                    if (xhttp.responseText) { // if the returned data is not null, update the values

                        var data = JSON.parse(xhttp.responseText);

                        document.getElementById("StatsValServeur").innerText = data.Serveur;
                        document.getElementById("StatsValTension").innerText = data.Tension;
                        document.getElementById("StatsValCourant").innerText = data.Courant;
                        document.getElementById("StatsValPuissance").innerText = data.Puissance;
                        document.getElementById("Etat").innerText = data.Etat;


                    } else { // a problem occurred

                        document.getElementById("StatsValServeur").innerText = "?";
                        document.getElementById("StatsValTension").innerText = "?";
                        document.getElementById("StatsValCourant").innerText = "?";
                        document.getElementById("StatsValPuissance").innerText = "?";
                        document.getElementById("Etat").innerText = "?";
                    }
                } else {
                    console.log('Request failed. Returned status of ' + xhttp.status);

                    document.getElementById("StatsValServeur").innerText = "?";
                    document.getElementById("StatsValTension").innerText = "?";
                    document.getElementById("StatsValCourant").innerText = "?";
                    document.getElementById("StatsValPuissance").innerText = "?";
                    document.getElementById("Etat").innerText = "?";
                }
            };
            xhttp.send();
        }
    </script>
</body>

</html>
)=====";

void Web_root()
{
  String FinalPage(reinterpret_cast<const __FlashStringHelper *>(www_root));

  FinalPage.replace("%MACHINE%", NOM_DE_LA_MACHINE_CLIENTE);

  www.send(200, "text/html", FinalPage);
}

void Change_Relais_State()
{
  if (digitalRead(PIN_RELAIS) == HIGH)
  {
    digitalWrite(PIN_RELAIS, LOW);
    digitalWrite(PIN_LED_STATUS, HIGH);
    www.send(200, "text/html", "Alim OFF");
    Serial.println("Relais : OFF");
  }
  else
  {
    digitalWrite(PIN_RELAIS, HIGH);
    digitalWrite(PIN_LED_STATUS, LOW);
    www.send(200, "text/html", "Prêt");
    Serial.println("Relais : ON");
  }
}

void Change_Remote_Relais_ON()
{
  //  On va appeler quelle URL ?
  http.begin(WifiClient, "http://" + IP_DU_SERVEUR + "/on");
  ReturnStatus = http.GET();

  if (ReturnStatus > 0)
  {
    ReturnData = http.getString();
    Serial.print("Retour Serveur : ");
    Serial.println(ReturnData);
  }
  else
  {
    Serial.print("Error code: ");
    Serial.println(ReturnStatus);
  }

  // Free resources
  http.end();
}

void Change_Remote_Relais_OFF()
{
  //  On va appeler quel URL ?
  http.begin(WifiClient, "http://" + IP_DU_SERVEUR + "/off");
  ReturnStatus = http.GET();

  if (ReturnStatus > 0)
  {
    ReturnData = http.getString();
    Serial.print("Retour Serveur : ");
    Serial.println(ReturnData);
  }
  else
  {
    Serial.print("Error code: ");
    Serial.println(ReturnStatus);
  }
  // Free resources
  http.end();
}

void Systeme_WiFi_LED()
{
  static unsigned long previousMillis = 0;
  unsigned long currentMillis = millis();
  const long interval = 250;

  // Si le wifi est pas Connecté on clignote toutes les 250 ms sinon on est fixe.
  if (WiFi.isConnected())
  {
    digitalWrite(PIN_LED_WIFI, LOW);
  }
  else
  {
    if (currentMillis - previousMillis >= interval)
    {
      previousMillis = currentMillis;
      if (digitalRead(PIN_LED_WIFI) == LOW)
        digitalWrite(PIN_LED_WIFI, HIGH);
      else
        digitalWrite(PIN_LED_WIFI, LOW);
    }
  }
}

void Systeme_Reset()
{
  www.send(200, "text/plain", "Redémarrage ...");
  Serial.println("Redémarrage ...");
  delay(2000);
  ESP.restart();
}

void ReadStatus()
{
  String Status_V;
  String Status_A;
  String Status_W;
  String Status_SYS;
  String Status_ToWeb;

  if (int(Mesure_Tension < 50))
    Status_V = "0 V";
  else
    Status_V = String(int(Mesure_Tension)) + " V";

  Status_A = String(Mesure_Courant) + " A";
  Status_W = String(int(Mesure_Puissance)) + " W";

  if (digitalRead(PIN_RELAIS) == LOW)
    Status_SYS = "Alim OFF";
  else if (ReturnData == "")
    Status_SYS = "Prêt";
  if (ReturnData == "ON")
    Status_SYS = "ON";
  if (ReturnData == "OFF")
    Status_SYS = "OFF";
  if (ReturnData == "Désactivé")
    Status_SYS = "Désactivé";
  if (ReturnData == "Désactivé - UnLock SVP")
    Status_SYS = "Désactivé - UnLock SVP";

  // un json du format : {"Serveur":"Init...","Tension":"0 V","Courant":"0.00 A","Puissance":"0 W","Etat":"Prêt"}
  Status_ToWeb = "{\"Serveur\":\"" + IP_DU_SERVEUR + "\",\"Tension\":\"" + Status_V + "\",\"Courant\":\"" + Status_A + "\",\"Puissance\":\"" + Status_W + "\",\"Etat\":\"" + Status_SYS + "\"}";
  www.send(200, "application/json", Status_ToWeb);
}

void Syteme_ReadPower()
{
  static unsigned long ReadLastTime = 0;

  if ((millis() - ReadLastTime) >= READ_INTERVAL)
  {
    Capteur.handle();
    ReadLastTime = millis();

    Mesure_Tension = Capteur.getVoltage();
    Mesure_Courant = Capteur.getCurrent();
    Mesure_Puissance = Capteur.getActivePower();
  }
}

void Systeme_DoAction()
{
  static String Current_Status = "";
  static String Last_Status = "OFF";

  // dans quel etat doit etre l'aspi distant ?
  if (Mesure_Tension > 200 && Mesure_Puissance > 15)
    Current_Status = "ON";
  else
    Current_Status = "OFF";

  // Envoi de la commande si changement d'etat seulement.
  if (Current_Status != Last_Status)
  {
    if (Current_Status == "ON")
    {
      Serial.println("Envoi Commande : ON");
      Change_Remote_Relais_ON();
      // Si erreur dans la page on change rien et on retente dansla bouce d'apres (status < 0)
      if (ReturnStatus > 0)
        Last_Status = Current_Status;
      else
        Serial.println("HTTP Error - Retrying . . .");
    }
    if (Current_Status == "OFF")
    {
      Serial.println("Envoi Commande : OFF");
      Change_Remote_Relais_OFF();
      // Si erreur dans la page on change rien et on retente dansla bouce d'apres (status < 0)
      if (ReturnStatus > 0)
        Last_Status = Current_Status;
      else
        Serial.println("HTTP Error - Retrying . . .");
    }
  }
}

#if BOUTON_ACTIF == 1
void Button_Check()
{
  // A Chaque Appui le bouton on change l'etat du relais.
  Button_LastState = Button_CurrentState;        // save the last state
  Button_CurrentState = digitalRead(PIN_BOUTON); // read new state

  if (Button_LastState == HIGH && Button_CurrentState == LOW)
  {
    if (digitalRead(PIN_RELAIS) == LOW)
    {
      digitalWrite(PIN_RELAIS, HIGH);
      digitalWrite(PIN_LED_STATUS, LOW);
      Serial.println("Relay ON");
    }
    else
    {
      digitalWrite(PIN_RELAIS, LOW);
      digitalWrite(PIN_LED_STATUS, HIGH);
      Serial.println("Relay OFF");
    }
  }
}
#endif

void FindServer()
{
  static unsigned long SearchLastTime = 0;

  // On cherche le serveur toute les 2 secondes au debut et on refresh toutes les 30 ensuite.
  if ((((millis() - SearchLastTime) >= 2000) && (!ServeurOK_Via_MDNS)) || (millis() - SearchLastTime) >= 30000)
  {
    int nrOfServices = MDNS.queryService("http", "tcp");

    if (nrOfServices != 0)
    {
      for (int i = 0; i < nrOfServices; i = i + 1)
      {
        if (MDNS.hostname(i) == "aspiromatic")
        {
          Serial.print("Serveur Trouvé via MDNS : ");
          IP_DU_SERVEUR = MDNS.IP(i).toString();
          Serial.println(IP_DU_SERVEUR);
          ServeurOK_Via_MDNS = true;
        }
      }
    }
    SearchLastTime = millis();
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.println("");

  // Configuration des entrées/sorties
  pinMode(PIN_LED_STATUS, OUTPUT);
  pinMode(PIN_LED_WIFI, OUTPUT);
  pinMode(PIN_RELAIS, OUTPUT);

  // ACtivation du relais  et de la lED
    digitalWrite(PIN_RELAIS, HIGH);
    digitalWrite(PIN_LED_STATUS, LOW);

  // Connecion du WIFI
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);

  // Wait for wifi to be connected
  Serial.println("Connexion au WiFi . . .");
  uint32_t notConnectedCounter = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(250);
    if (digitalRead(PIN_LED_WIFI) == LOW)
      digitalWrite(PIN_LED_WIFI, HIGH);
    else
      digitalWrite(PIN_LED_WIFI, LOW);
    Serial.print(".");
    notConnectedCounter++;
    if (notConnectedCounter > 120)
    { // Reset board if not connected after 30s
      Serial.println("Pas de Connexion WIFI -> Reboot . . . ");
      ESP.restart();
    }
  }
  // WiFi Connecté (Led ON a Low)
  digitalWrite(PIN_LED_WIFI, LOW);

  Serial.print("Wifi connecté, IP : ");
  Serial.println(WiFi.localIP());

  // On defini une valeur si le champs serveur est vide pour l'interface web :)
  if (IP_DU_SERVEUR == "")
    IP_DU_SERVEUR = "Init...";

  // Gestion MDNS
  if (!MDNS.begin(NOM_DE_LA_MACHINE_CLIENTE.c_str()))
  {
    Serial.println("Erreur Demarrage mDNS !");
    return;
  }
  MDNS.addService("http", "tcp", 80);

  // Mise en place du serveur Web
  www.on("/reset", Systeme_Reset);
  www.on("/relais", Change_Relais_State);
  www.on("/ReadStatus", ReadStatus);

  www.on("/", Web_root);
  www.begin();

  // Capteur de tension
  Capteur.setRX(PIN_CAPTEUR);
  Capteur.begin(); // will initialize serial to 4800 bps

#if UPDATE_FIRMWARE_A_DISTANCE == 1
  // Nom Pour Flash OTA  + Demarrage.
  ArduinoOTA.setHostname(NOM_DE_LA_MACHINE_CLIENTE.c_str());
  ArduinoOTA.begin();
#endif
}

void loop()
{
  FindServer();       // Recherche du serveur via MDNS
  Systeme_WiFi_LED(); // Statut Wifi
  Syteme_ReadPower(); // Lectuer du capteur de courant CSE7766
  Systeme_DoAction(); // GEstion des ACtions selon detection
  www.handleClient(); // Web (facultatif)

#if BOUTON_ACTIF == 1
  Button_Check(); // relais via bouton (facultatif)
#endif

#if UPDATE_FIRMWARE_A_DISTANCE == 1
  ArduinoOTA.handle();
#endif
}