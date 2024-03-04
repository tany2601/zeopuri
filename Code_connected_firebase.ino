#include <Arduino.h>
#include <Wire.h>  // Include the Wire library for I2C communication
#include <LiquidCrystal_I2C.h>  // Include the LiquidCrystal_I2C library
#if defined(ESP32)
  #include <WiFi.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
#endif
#include <Firebase_ESP_Client.h>
#include <time.h>
#include "sntp.h"
#include "DHT.h"

//Provide the token generation process info.
#include "addons/TokenHelper.h"
//Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

// Insert your network credentials
#define WIFI_SSID "Tany Wifi-tp-link"
#define WIFI_PASSWORD "Tany@2608MS"

// Insert Firebase project API Key
#define API_KEY "AIzaSyAceHTAU_hhYO_efcGa8vTXN9UcDl5gV60"

// Insert RTDB URLefine the RTDB URL */
#define DATABASE_URL "https://zeopuri-default-rtdb.firebaseio.com/" 


#define DHTPIN 4           // Pin connected to DHT11 sensor
#define DHTTYPE DHT11      // DHT 11
#define MQ7PIN A0          // Pin connected to MQ-7 sensor
#define BAD_THRESHOLD 50  // Threshold for bad air quality
#define TEMP_THRESHOLD 30  // Temperature threshold for alert
#define DISPLAY_DELAY 5000 // Delay for displaying air quality in milliseconds
#define ARDUINO_SERIAL_TX 17 // TX pin of ESP32 connected to RX pin of Arduino
#define ARDUINO_SERIAL_RX 16 // RX pin of ESP32 connected to TX pin of Arduino
#define R0_VALUE 10.0 



const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";

//Define Firebase Data object
FirebaseData fbdo;

FirebaseAuth auth;
FirebaseConfig config;

FirebaseJson json;
FirebaseJson json2;

bool signupOK = false;

// LCD configuration
#define LCD_ADDRESS 0x3F  // I2C address of your LCD display
#define LCD_COLUMNS 16    // Number of columns in your LCD display
#define LCD_ROWS 2        // Number of rows in your LCD display

LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLUMNS, LCD_ROWS);  // Initialize the LCD object
DHT dht(DHTPIN, DHTTYPE);


void setup(){
  Serial.begin(115200);
  Serial2.begin(9600); // Initialize Serial2 for communication with Arduino
  lcd.init();  // Initialize the LCD display
  lcd.backlight();  // Turn on the backlight

  pinMode(MQ7PIN, INPUT);
  dht.begin();

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  /* Assign the api key (required) */
  config.api_key = API_KEY;

  /* Assign the RTDB URL (required) */
  config.database_url = DATABASE_URL;

  /* Sign up */
  if (Firebase.signUp(&config, &auth, "", "")){
    Serial.println("ok");
    updateLCD("ok");
    delay(1000);
    // lcd.clear();
    signupOK = true;

  }
  else{
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  sntp_set_time_sync_notification_cb(NULL); // Disable time adjustment callback
  sntp_servermode_dhcp(1); // Enable SNTP server mode
  configTime(19800, 0, ntpServer1, ntpServer2); // Start NTP time synchronization

  // Display welcome message
  // lcd.clear();
  // lcd.setCursor(0, 0);
  // lcd.print("Welcome to");
  // lcd.setCursor(0, 1);
  // lcd.print("Agriculture App");
  // delay(2000); // 2 seconds delay
  // lcd.clear();

}

String getFormattedLocalTime()
{
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("No time available (yet)");
    return "000000000000"; // Return a default value if time is not available
  }

  char buffer[15]; 
  sprintf(buffer, "%04d%02d%02d%02d%02d%02d", 
          timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
          timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  return String(buffer);
}



void pushDataToFirebase() {
 
  String username;
  username = "ZeoData";

  
  String formattedTime = getFormattedLocalTime();

  String path =  "/" + username + "/temperature_value";

  float temperature = dht.readTemperature();
  
  updateLCD("Temperature: ");
  lcd.print(temperature);
  lcd.print("C");
  delay(1000);
  if (temperature > TEMP_THRESHOLD) {
    Serial.println("High Temperature Alert");
    delay(2000);
  }
  json.clear();
  json.add("id", formattedTime);
  json.add("value", temperature);
  if (Firebase.RTDB.pushJSON(&fbdo, path, &json)) {
    Serial.println("Temperature data pushed to Firebase");
  } else {
    Serial.println("Error pushing temperature data to Firebase");
    Serial.println(fbdo.errorReason());
  }

  String path1 = "/" + username +  "/Humidity_value";
  float humidity = dht.readHumidity();
  
  updateLCD("Humidity: ");
  lcd.print(humidity);
  lcd.print("%  ");
  delay(1000);

  json.clear();
  json.add("id", formattedTime);
  json.add("value", humidity);
  if (Firebase.RTDB.pushJSON(&fbdo, path1, &json)) {
    Serial.println("Humidity data pushed to Firebase");
    
  } else {
    Serial.println("Error pushing Humidity data to Firebase");
    Serial.println(fbdo.errorReason());
  }
  
  String path2 = "/" + username +  "/Air_quality";
  float airQuality = analogRead(MQ7PIN);

  updateLCD("Air Quality: ");
  lcd.print(airQuality);
  delay(1000);
    
  if (airQuality > BAD_THRESHOLD) {
    lcd.setCursor(0, 1);
    lcd.print("Air: Bad");
      // Send signal to turn on fan
    Serial2.println("FAN_ON");
    delay(30000); // fan or for 30 secs
  } else {
    lcd.setCursor(0, 1);
    lcd.print("Air: Good");
      // Send signal to turn off fan
    Serial2.println("FAN_OFF");
  }

    Serial.print("Air Quality: ");
    Serial.println(airQuality);
    
  
  json.clear();
  json.add("id", formattedTime);
  json.add("value", airQuality);
  if (Firebase.RTDB.pushJSON(&fbdo, path2, &json)) {
    Serial.println("Air Quality data pushed to Firebase");
    
  } else {
    Serial.println("Error pushing Air Quality data to Firebase");
    Serial.println(fbdo.errorReason());
  }

  
}

void updateLCD(const String& message) {
  lcd.clear();               
  lcd.setCursor(0, 0);      
  lcd.print(message);        
}



void loop() {
  static unsigned long lastPushTime = 0;
  const unsigned long pushInterval = 5000;

  unsigned long currentMillis = millis();
  if (currentMillis - lastPushTime >= pushInterval) {
    pushDataToFirebase();
    lastPushTime = currentMillis;
  }



  delay(1000); 
}