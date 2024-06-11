#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <chrono>
#include "time.h"

//Provide the token generation process info.
#include "addons/TokenHelper.h"

//Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

/* 1. Define the WiFi credentials */
#define WIFI_SSID "UCInet Mobile Access"

#define NTP_SERVER "north-america.pool.ntp.org"

// For the following credentials, see examples/Authentications/SignInAsUser/EmailPassword/EmailPassword.ino

/* 2. Define the API Key */
#define API_KEY "AIzaSyD3lmzl4K15azVc7AUFx1FLAHLzTiHHbUo"

/* 3. Define the RTDB URL */
#define DATABASE_URL "https://cs147-99c39-default-rtdb.firebaseio.com/" //<databaseName>.firebaseio.com or <databaseName>.<region>.firebasedatabase.app

/* 4. Define the user Email and password that alreadey registerd or added in your project */
#define USER_EMAIL ""
#define USER_PASSWORD ""

#define PIN_GATE_IN 39
#define IRQ_GATE_IN  0
#define PIN_LED_OUT 32
#define PIN_ANALOG_IN 33
#define MOTION_IN 25


// Define Firebase Data object
FirebaseData fbdo;

FirebaseAuth auth;
FirebaseConfig config;
unsigned long sendDataPrevMillis = 0;

//For getting time with ntp
const long  gmtOffset_sec = -28800;
const int   daylightOffset_sec = 3600;

bool signupOK = false; 

unsigned long start_time = 0; // Traffic light timer.
unsigned long motion_timer = 0;

int clap_count = 0;
bool light_on = false;
bool activity = false;
int motion = 0;
unsigned int motion_count = 0;
int milliwatt_min = 0;
int moving = 0;

void printLocalTime()
{
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

std::string date(){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return "Failed to obtain time";
  }
  static char timeString[12]; // Buffer to hold the formatted time string
  snprintf(timeString, sizeof(timeString), "%04d-%02d-%02d", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
  return std::string(timeString);
}

std::string getLocalTimeAsString()
{
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return "Failed to obtain time";
  }
  static char timeString[9]; // Buffer to hold the formatted time string
  strftime(timeString, sizeof(timeString), "%H:%M:%S", &timeinfo);
  return std::string(timeString);
}


void setup()
{
  Serial.begin(115200);

  // configure input to interrupt
  pinMode(PIN_GATE_IN, INPUT);

  //  Configure LED pin as output
  pinMode(PIN_LED_OUT, OUTPUT);
  pinMode(MOTION_IN, INPUT);

  // Display status
  Serial.println("Initialized");

  WiFi.begin(WIFI_SSID);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  configTime(gmtOffset_sec, daylightOffset_sec, NTP_SERVER);
  printLocalTime();

  Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);

  /* Assign the api key (required) */
  config.api_key = API_KEY;

  /* Assign the RTDB URL (required) */
  config.database_url = DATABASE_URL;

  if (Firebase.signUp(&config, &auth, "", "")){
    Serial.println("ok");
    signupOK = true;
  }
  else{
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }
  
  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; // see addons/TokenHelper.h

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h
  
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void loop()
{
  int value;
  value = digitalRead(PIN_GATE_IN);
  
  Serial.print("Sound Level: ");
  Serial.println(value);
  if (value == 1){
    clap_count++;
    Serial.println("Clap Count");
    Serial.println(clap_count);
  }
  

  if (millis() - start_time > 2000){
    start_time = millis();
    if (clap_count >= 2 && clap_count <= 4){
      if (light_on == false){
        digitalWrite(PIN_LED_OUT, HIGH);
        Serial.println("LIGHT ON");
        clap_count = 0;
        light_on = true;
      }
      else{
        digitalWrite(PIN_LED_OUT,LOW);
        Serial.println("LIGHT OFF");
        clap_count = 0;
        light_on = false;

      }
    }
    else{
      clap_count = 0;
    }
  }

  if(millis() - motion_timer > 1000){
    motion_timer = millis();

    if (light_on){
      milliwatt_min += 14;
    }

    if (digitalRead(MOTION_IN) == 0){
      motion_count++;
    }
    else{
      Serial.println("MOVEMENT");
      moving++;
      motion_count = 0;
      activity = true;
    }
  }

  //if no motion detected after 5 counts then turn off led
  if (motion_count >= 10){
    motion_count = 0;
    activity = false;
    digitalWrite(PIN_LED_OUT,LOW);
    light_on = false;
  }

  //FIREBASE UPLOADINg
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 60000)){
    sendDataPrevMillis = millis();
    // Write an Int number on the database path test/int

    //MAKING PATH FOR DATE and TIME
    std::string dateTime = date();
    dateTime = dateTime + "/";
    dateTime = dateTime + getLocalTimeAsString();
    
    // int power_usage = 0;
    // if (light_on){
    //   power_usage = 50;
    // }

    motion = digitalRead(MOTION_IN);

    //PATH FOR POWER USAGE
    std::string power_str = dateTime + "/power_usage";

    //PATH FOR MOTION ACTIVITY
    std::string motion_str = dateTime + "/activity_detected";
    

    //UPLOADING POWER USAGE
    if (Firebase.RTDB.setInt(&fbdo, power_str.c_str(), milliwatt_min)){
      Serial.println("POWER PASSED");
      milliwatt_min = 0;
    }
    else {
      Serial.println("POWER FAILED");
      Serial.println(fbdo.errorReason());
    }

    //UPLOADING MOTION ACTIVITY
    if (Firebase.RTDB.setInt(&fbdo, motion_str.c_str(), moving)){
      Serial.println("ACTIVITY PASSED");
      moving = 0;
    }
    else {
      Serial.println("ACTIVITY FAILED");
      Serial.println(fbdo.errorReason());
    }
  }
  printLocalTime();
  // pause for 1 second
  delay(250);
}