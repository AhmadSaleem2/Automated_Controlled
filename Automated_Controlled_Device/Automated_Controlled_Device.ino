//Libraries, static data and global variables
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <AsyncElegantOTA.h>
#define FASTLED_ALLOW_INTERRUPTS 0
#include <FastLED.h>
#include <ETH.h> // arduino lib
// #include <Ethernet.h>
#include "FS.h"
#include "SD_MMC.h"
#include "Audio.h"
#include <Preferences.h> // arduino
#include "AsyncJson.h"
#include "ArduinoJson.h"
//Pins 
#define ETH_PHY_POWER 5
#define ETH_CLK_MODE ETH_CLOCK_GPIO17_OUT
#define LED_PIN 16
#define I2S_DOUT 4
#define I2S_BCLK 12
#define I2S_LRC 13
#define NUM_LEDS 38
static bool eth_connected = false;
AsyncWebServer server(80); //server

//Constants:
//wifi details
// const char* ssid = "WRLaccess";
// const char* password =  "12345678";
// Set your Gateway IP address, IP configuration
// IPAddress local_IP(192, 168, 10, 52);
// IPAddress gateway(192, 168, 10, 254);
// IPAddress subnet(255, 255, 255, 0);
// IPAddress primaryDNS(8, 8, 8, 8);   //optional
// IPAddress secondaryDNS(8, 8, 4, 4); //optional
//API Key for authorization
// const char* apiKey = "c29928bf-4283-46d4-8a5c-a21b79eac40a";
//OTA Password
// const char* OTAuserName = "WhatsRunning";
// const char* OTApassword =  "123qweE#";
//Other
// const char* applicationJson = "application/json";
// const char* defaultColorCodes = "255,255,0";
// const int defaultNoOfColors = 1;
// const int defaultFadeTime = 0; 
// const int defaultFadeBrightness = 0;
//Global variables
//Led number of colors split and color code values state this should be kept for flashing
bool turnOn = false;
bool turnOff = false;
bool flashLEDs = false;
bool LEDstate = true;
int flashOnTime;
int flashOffTime;
int noOfFlashes;
int flashFadeTime;
int flashFadeBrightness;
int flashNoOfColors;
String flashColorCodes;
unsigned long startMillis = 0;
//LEDs details
CRGB leds[NUM_LEDS];
//Audio
Audio audio;

Preferences preferences;
//===================================================================
//To be deleted once we have C# code this is just an HTML code that can upload files
String listFiles(bool ishtml = false);

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html lang="en">
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta charset="UTF-8">
</head>
<body>
  <p><h1>File Upload</h1></p>
  <p>Free Storage: %FREESPIFFS% | Used Storage: %USEDSPIFFS% | Total Storage: %TOTALSPIFFS%</p>
  <form method="POST" action="/upload" enctype="multipart/form-data"><input type="file" name="data"/><input type="submit" name="upload" value="Upload" title="Upload File"></form>
  <p>After clicking upload it will take some time for the file to firstly upload and then be written to SPIFFS, there is no indicator that the upload began.  Please be patient.</p>
  <p>Once uploaded the page will refresh and the newly uploaded file will appear in the file list.</p>
  <p>If a file does not appear, it will be because the file was too big, or had unusual characters in the file name (like spaces).</p>
  <p>You can see the progress of the upload by watching the serial output.</p>
  <p>%FILELIST%</p>
</body>
</html>
)rawliteral";



//===================================================================
//Setup and init functions called in setup, this is like a start up function that is executed at each reset
void setup() {
  Serial.begin(115200);
  initSDCard();
  initLEDs();
  initEthernet();
  initWiFI();
  initAudio();
  initServer();
  turnOn = true;
  //OTA - Over the air update
  // AsyncElegantOTA.begin(&server, OTAuserName, OTApassword);
  AsyncElegantOTA.begin(&server, "WhatsRunning", "123qweE#");
}

void initAudio(){
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(21); // 0...21
}

void initSDCard(){
  if(!SD_MMC.begin("/sdcard", true)){
      return;
  }
  uint8_t cardType = SD_MMC.cardType();

  if(cardType == CARD_NONE){
      return;
  }

  // Serial.print("SD_MMC Card Type: ");

  uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
  // Serial.printf("SD_MMC Card Size: %lluMB\n", cardSize);
}

//Ethernet config
void initEthernet() {
  WiFi.onEvent(wiFiEvent);
  ETH.begin(ETH_PHY_ADDR, ETH_PHY_POWER, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_LAN8720, ETH_CLK_MODE);
  // ETH.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS);
  ETH.config(IPAddress(192, 168, 10, 52), IPAddress(192, 168, 10, 254), IPAddress(255, 255, 255, 0), IPAddress(8, 8, 8, 8), IPAddress(8, 8, 4, 4));
}

void wiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case SYSTEM_EVENT_ETH_START:
      //set eth hostname here
      ETH.setHostname("esp32-ethernet");
      break;
    case SYSTEM_EVENT_ETH_CONNECTED:
      break;
    case SYSTEM_EVENT_ETH_GOT_IP:
      // Serial.print("ETH MAC: ");
      // Serial.print(ETH.macAddress());
      // Serial.print(", IPv4: ");
      // Serial.print(ETH.localIP());

      // if (ETH.fullDuplex()) {
      //   Serial.print(", FULL_DUPLEX");
      // }
      // Serial.print(", ");
      // Serial.print(ETH.linkSpeed());
      eth_connected = true;
      break;
    case SYSTEM_EVENT_ETH_DISCONNECTED:
      eth_connected = false;
      break;
    case SYSTEM_EVENT_ETH_STOP:
      eth_connected = false;
      break;
    default:
      break;
  }
}

//init functions used in Setup
void initLEDs(){
  FastLED.addLeds<WS2811, LED_PIN, RGB>(leds, NUM_LEDS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 500);
  FastLED.clear();
  FastLED.show();
}

void initWiFI(){
  // if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
  // }

  WiFi.config(IPAddress(192, 168, 10, 52), IPAddress(192, 168, 10, 254), IPAddress(255, 255, 255, 0), IPAddress(8, 8, 8, 8), IPAddress(8, 8, 4, 4));

  // WiFi.begin(ssid, password);  //Connect to the WiFi network
  WiFi.begin("WRLaccess", "12345678");  //Connect to the WiFi network
  int i=0;
  while (WiFi.status() != WL_CONNECTED || (i>10 && eth_connected)) {  //Wait for connection
    delay(500);
    i++;
  }
  // Serial.print("IP address: ");
}

//To note handlers are used with body, they will work to be called with any verb post, put, this is the recommened way inside the library to use this
//other option to use is to use the handle body function this will mean that we get the raw body handler and we have to do the logic to deserialize it ourselves 
//if no body is used the function server.on is used where you can also define clearly the verb
void initServer(){
  initLightAPIs();
  initSoundAPIs();
  initFileAPIs();
  server.onNotFound([](AsyncWebServerRequest *request) {handleNotFoundAPI(request);});
  server.begin(); //Start the server
}

void initLightAPIs() {
  //Turn on leds on the scanner
  //Parameters:     
  //fadeTime - represents the value 
  //noOfColors - the number of section and colors you want to show
  //colorCodes - the RGB codes for them
  //fadeBrightness - represent an int from 0 to 255 that will be used to define how much should the brightness of the LEDs be dimed
  AsyncCallbackJsonWebHandler* turnOnColorsHandler = new AsyncCallbackJsonWebHandler("/turnOnColors", [](AsyncWebServerRequest *request, JsonVariant &json) {authorizeWithBody(request, json,&turnOnColorsAPI);});
  server.addHandler(turnOnColorsHandler); 

  //Turn off leds on the scanner
  //NoParameters
  server.on("/turnOffLeds",  HTTP_POST, [](AsyncWebServerRequest *request) {authorize(request,&turnOffColorsAPI);}); 

  //Flash leds in a specific manner
  //Parameters:     
  //onTime - the time in miliseconds you want the LEDs to stay on
  //offTime - the time in miliseconds you want the LEDs to stay off 
  //fadeTime - the time we want the LEDs to turn on this means that between turning on each led we will hafe fadeTime/numOfLeds
  //noOfFlashes - how many times you want the sequence to repeat - if empty it will flash with the current colors
  //noOfColors - the number of section and colors you want to show - if empty it will flash with the current colors
  //colorCodes - the RGB codes for them
  //fadeBrightness - represent an int from 0 to 255 that will be used to define how much should the brightness of the LEDs be dimed
  //Note: after flash is triggered the leds will always remain on the last know state
  AsyncCallbackJsonWebHandler* flashHandler = new AsyncCallbackJsonWebHandler("/flash",  [](AsyncWebServerRequest *request, JsonVariant &json) {authorizeWithBody(request, json,&flashLedsAPI);});
  server.addHandler(flashHandler);

  //reset to a default initial state, currently red
  //NoParameters
  server.on("/resetToDefault",  HTTP_POST, [](AsyncWebServerRequest *request) {authorize(request,&resetToDefaultAPI);});
}

void initSoundAPIs() {
  //Play a specific sound
  //Parameters:
  //sound - the sound number you want to play
  server.on("/playSound",  HTTP_POST, [](AsyncWebServerRequest *request) {authorize(request,&playSoundAPI);});

  //Change volume for speaker
  //Parameters:
  //soundVolume - a value from 0-30 which represents the volume you would like, if parameter is not send it will turn volume to default value which is the maximum value 30
  server.on("/soundVolume",  HTTP_POST, [](AsyncWebServerRequest *request) {authorize(request,&changeVolumeAPI);});
}

void initFileAPIs() {
  //Handle file
  //"/" api should be deleted or commented 
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    String logmessage = "Client:" + request->client()->remoteIP().toString() + + " " + request->url();
    request->send_P(200, "text/html", index_html, processor);
  });

  //get list of files in root folder
  server.on("/getFilesInRootFolder", HTTP_GET, [](AsyncWebServerRequest *request){ authorize(request,&getFilesInRootFolderAPI); });

  //download file
  //Parameters:
  //filename - represents a string with the file name
  server.on("/downloadFile", HTTP_GET, [](AsyncWebServerRequest *request){ authorize(request,&downloadFile); });

  //delete file
  //Parameters:
  //filename - represents a string with the file name
  server.on("/deleteFile", HTTP_DELETE, [](AsyncWebServerRequest *request){ authorize(request,&deleteFile); });

  // run handleUpload function when any file is uploaded
  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request) { }, handleUpload);
}

//===================================================================
//Loop actions, this code is executed like it is in a while true statement
void loop() {
  //due to interupt #define FASTLED_ALLOW_INTERRUPTS 0 we will need to do all operations for LEDs inside the loop function
  //this is because in some cases the watch dog will be triggered if the code is executed outside the loop function
  //also we should not use delay because audio.loop() needs to be called every about 150 ms (based on my tests) so that the sound does not get interupted 
  if (flashLEDs)
  {
    unsigned long currentMillis = millis(); 
    if (((currentMillis - startMillis >= flashOnTime && noOfFlashes > 0 ) || startMillis == 0)&& LEDstate)
    {
      startMillis = millis();
      turnOffLeds();
      LEDstate = false;
    }

    if (currentMillis - startMillis >= flashOffTime && noOfFlashes > 0 && !LEDstate)
    {
      startMillis = millis();
      noOfFlashes--;
      turnOnLedsBasedOnComand(flashFadeTime, flashNoOfColors, flashColorCodes, flashFadeBrightness);  
      LEDstate = true;
    }

    if (currentMillis - startMillis >= flashOnTime && noOfFlashes <=0)
    {
      turnOn = !turnOff;
      if (turnOff)
      {
        turnOffLeds();
      }
      flashLEDs =false;
    }
  }

  if (turnOn)
  {
    turnOnLedsFromPreferenceSettings();
    turnOn = false;
  }

  // loopWithoutLeds();
  audio.loop();
}

// void loopWithoutLeds()
// {
//   audio.loop();
// }

//===================================================================
//APIs

//===================================================
//Generic
//preference is the library that saves persistent data so we use it to save the colors called in the turn on leds API
void saveColorConfiguration (int noOfColorsState, String &colorCodesState, int fadeTimeState, int fadeBrightness) {
  preferences.begin("color", false);
  preferences.putUInt("noOfColorsState", noOfColorsState);
  preferences.putString("colorCodesState", colorCodesState);
  preferences.putUInt("fadeTimeState", fadeTimeState);
  preferences.putUInt("fadeBrightness", fadeBrightness);
  preferences.end();
}

void handleNotFoundAPI(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "API Not Found");
}

void authorize (AsyncWebServerRequest *request, void (*func)(AsyncWebServerRequest *request)){
  // if(request->hasHeader("api-key") and String(request->header("api-key").c_str())==apiKey)
  if(request->hasHeader("api-key") and String(request->header("api-key").c_str())=="c29928bf-4283-46d4-8a5c-a21b79eac40a")
  {
    func(request);
  }
  else {
    request->send(401, "text/plain", "Api-Key incorect or not provided!");
  }
}

void authorizeWithBody (AsyncWebServerRequest *request, JsonVariant &json, void (*func)(AsyncWebServerRequest *request, JsonVariant &json)){
  // if(request->hasHeader("api-key") and String(request->header("api-key").c_str())==apiKey)
  if(request->hasHeader("api-key") and String(request->header("api-key").c_str())=="c29928bf-4283-46d4-8a5c-a21b79eac40a")
  {
    func(request, json);
  }
  else {
    request->send(401, "text/plain", "Api-Key incorect or not provided!");
  }
}

//===================================================
//LEDs
void resetToDefaultAPI(AsyncWebServerRequest *request) {
  //reset to default means turn on the lights in default values currently, this could do many other operations is needed
  turnOff = false;
  turnOn = true;
  String message = "Reset to default";
  sendMessageResponse(request, message);
}

void flashLedsAPI(AsyncWebServerRequest *request, JsonVariant &json) {
//the api call just sets the parameters so that flashing is done
  const JsonObject& jsonObj = json.as<JsonObject>();

  if (jsonObj.containsKey("OnTime") == true and jsonObj["OnTime"].is<int>() == true)
  {
    flashOnTime = jsonObj["OnTime"].as<int>();
  }
  else {
    flashOnTime = 500; //set default value of 500
  }

  if (jsonObj.containsKey("OffTime") == true and jsonObj["OffTime"].is<int>() == true)
  {
    flashOffTime = jsonObj["OffTime"].as<int>();
  }
  else {
    flashOffTime = 500; //set default value of 500
  }

  if (jsonObj.containsKey("NoOfFlashes") == false or jsonObj["NoOfFlashes"].is<int>() == false) 
  {
    noOfFlashes = 1;
  }
  else {
    noOfFlashes = jsonObj["NoOfFlashes"].as<int>();
  }

  if (jsonObj.containsKey("NoOfColors") == false or jsonObj.containsKey("ColorCodes") == false or jsonObj["NoOfColors"].is<int>() == false) { 
    preferences.begin("color", false);
    // flashNoOfColors = preferences.getUInt("noOfColorsState", defaultNoOfColors);
    flashNoOfColors = preferences.getUInt("noOfColorsState", 1);
    flashColorCodes = preferences.getString("colorCodesState", "255,255,0");
    // flashColorCodes = preferences.getString("colorCodesState", defaultColorCodes);
    preferences.end();
  }
  else {
  flashNoOfColors = jsonObj["NoOfColors"].as<int>();
  flashColorCodes = jsonObj["ColorCodes"].as<String>();
  }

  if (flashNoOfColors < 1)
  {
    flashNoOfColors = 1;
  }

  flashFadeTime = 500;

  if (jsonObj.containsKey("FadeTime") == true and jsonObj["FadeTime"].is<int>() == true)
  {
    flashFadeTime = jsonObj["FadeTime"].as<int>();
  } 

  flashFadeBrightness = 0;

  if (jsonObj.containsKey("FadeBrightness") == true and jsonObj["FadeBrightness"].is<int>() == true)
  {
    flashFadeBrightness = jsonObj["FadeBrightness"].as<int>();
  }

  if (flashFadeBrightness > 255){
    flashFadeBrightness = 255;
  }

  if (flashFadeBrightness < 0 ){
    flashFadeBrightness = 0;
  } 

  flashLEDs = true;
  String message = "Flashing!";
  sendMessageResponse(request, message);
}

void turnOffColorsAPI(AsyncWebServerRequest *request) {
//this api turns off leds and sets the turnOff flag to true so in case of flashing at the end it will stay off
  turnOff = true;

  if (!flashLEDs)
  {
    turnOffLeds();
  }

  String message = "Leds turned off!";
  sendMessageResponse(request, message);
}

void turnOnColorsAPI(AsyncWebServerRequest *request, JsonVariant &json) {
  //this api saves the configuration in preferences and gives command to the loop to turn on in the new colors
  const JsonObject& jsonObj = json.as<JsonObject>();
  
  if (jsonObj.containsKey("NoOfColors") == false or jsonObj.containsKey("ColorCodes") == false) { 
    request->send(400, "text/plain", "Arguments not received");
    return;
  }

  int noOfColorsState = jsonObj["NoOfColors"].as<int>();

  if (noOfColorsState < 1)
  {
    noOfColorsState = 1;
  }

  String colorCodesState = jsonObj["ColorCodes"].as<String>();
  int fadeTimeState = 0;

  if (jsonObj.containsKey("FadeTime") == true and jsonObj["FadeTime"].is<int>() == true)
  {
    fadeTimeState = jsonObj["FadeTime"].as<int>();
  } 

  int fadeBrightness = 0;

  if (jsonObj.containsKey("FadeBrightness") == true and jsonObj["FadeBrightness"].is<int>() == true)
  {
    fadeBrightness = jsonObj["FadeBrightness"].as<int>();
  }

  String message = "Lights were turned on!";
  saveColorConfiguration(noOfColorsState, colorCodesState, fadeTimeState, fadeBrightness);
  turnOn = true;
  sendMessageResponse(request, message);
}

//LEDs Commands to hardware and logic
void turnOnLedsFromPreferenceSettings() {
  preferences.begin("color", false);
  // int noOfColorsState = preferences.getUInt("noOfColorsState", defaultNoOfColors);
  int noOfColorsState = preferences.getUInt("noOfColorsState", 1);
  // String colorCodesState = preferences.getString("colorCodesState", defaultColorCodes);
  String colorCodesState = preferences.getString("colorCodesState", "255,255,0");
  // int fadeTimeState = preferences.getUInt("fadeTimeState", defaultFadeTime);
  int fadeTimeState = preferences.getUInt("fadeTimeState", 0);
  // int fadeBrightness = preferences.getUInt("fadeBrightness", defaultFadeBrightness);
  int fadeBrightness = preferences.getUInt("fadeBrightness", 0);
  preferences.end();
  turnOffLeds();
  turnOnLedsBasedOnComand(fadeTimeState, noOfColorsState, colorCodesState, fadeBrightness);
}

void turnOnLedsBasedOnComand (int fadeTime, int noOfColors, String &ledsColors, int fadeBrightness) {
  if (noOfColors < 1)
  {
    return;
  }

  int first = 0;
  int ledsPerColor = NUM_LEDS/noOfColors;
  int extraLeds = NUM_LEDS%noOfColors;
  int last = ledsPerColor-1;

  if (extraLeds > 0)
  {
    last++;
  }
  for (int i = 0; i < noOfColors; i++) {
    String colorCodes = getValue(ledsColors, ';', i);
    int r = getValue(colorCodes, ',', 0).toInt();
    int g = getValue(colorCodes, ',', 1).toInt();
    int b = getValue(colorCodes, ',', 2).toInt();
    TurnOnLeds(r, g, b, first, last, fadeTime, fadeBrightness);
    first = last+1;
    last = last+ledsPerColor;
    extraLeds=extraLeds-1;

    if (extraLeds > 0)
    {
      last++;
    }
  }
}

void turnOffLeds() {
  for (int i = 0; i < NUM_LEDS; i++)
    leds[i] = CRGB(0, 0, 0);  //rgb
  FastLED.show();
}

void TurnOnLeds(int r, int g, int b, int first, int last, int fadeTime, int fadeBrightness) {
  unsigned long startMillis = 0;
  for (int i = first; i <=last; i++) {
    leds[i] = CRGB(r, g, b);
    FastLED.show();

    if (fadeTime != 0) {
      unsigned long startMillis = millis();
      unsigned long currentMillis = startMillis;

      while (currentMillis - startMillis <= fadeTime/NUM_LEDS)
      {
        //this audio.loop() is introduced because if the fade time is to big the audio will be interrupted
        // loopWithoutLeds();
        audio.loop();
        currentMillis = millis();
      }
      
    }
  }
}

//===================================================
//Sound
void playSoundAPI(AsyncWebServerRequest *request){
//this api sets up the sound that needs to be played by the loop when audio.loop is called
  if (request->hasParam("sound") == false ) { 
    request->send(400, "text/plain", "Arguments not received");
    return;
  }

  String sound;
  sound = request->getParam("sound")->value();
  File file = SD_MMC.open(sound);

  if (!file)
  {
    request->send(400, "text/plain", "No file with this name found!");
    return;
  }

  audio.connecttoFS(SD_MMC, sound.c_str());

  String message = "Played sound: " + sound + ".";
  sendMessageResponse(request, message);
}

void changeVolumeAPI(AsyncWebServerRequest *request){
//this api sets a new volume
  int soundVolume;

  if (request->hasParam("soundVolume") == false)
  {
    request->send(400, "text/plain", "Arguments was not received");
    return;
  } 

  soundVolume = request->getParam("soundVolume")->value().toInt();
  
  if (soundVolume > 21){
    soundVolume = 21;
  }

  if (soundVolume < 0 ){
    soundVolume = 0;
  }

  audio.setVolume(soundVolume);
  String message = "Sound volume was changed!";
  sendMessageResponse(request, message);
}

//Upload files
// Make size of files human readable
// source: https://github.com/CelliesProjects/minimalUploadAuthESP32
String humanReadableSize(const size_t bytes) {
  if (bytes < 1024) return String(bytes) + " B";
  else if (bytes < (1024 * 1024)) return String(bytes / 1024.0) + " KB";
  else if (bytes < (1024 * 1024 * 1024)) return String(bytes / 1024.0 / 1024.0) + " MB";
  else return String(bytes / 1024.0 / 1024.0 / 1024.0) + " GB";
}

// handles uploads
void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {

  // if(request->hasHeader("api-key") == false or String(request->header("api-key").c_str())!=apiKey)
  if(request->hasHeader("api-key") == false or String(request->header("api-key").c_str())!="c29928bf-4283-46d4-8a5c-a21b79eac40a")
  {
    request->send(401, "text/plain", "Api-Key incorect or not provided!");
    return;
  }

  String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();

  if (!index) {
    logmessage = "Upload Start: " + String(filename);
    // open the file on first call and store the file handle in the request object
    request->_tempFile = SD_MMC.open("/" + filename, "w");
  }

  if (len) {
    // stream the incoming chunk to the opened file
    request->_tempFile.write(data, len);
    logmessage = "Writing file: " + String(filename) + " index=" + String(index) + " len=" + String(len);
  }

  if (final) {
    logmessage = "Upload Complete: " + String(filename) + ",size: " + String(index + len);
    // close the file handle as the upload is now done
    request->_tempFile.close();
    //request->redirect("/");
    sendMessageResponse(request, logmessage);
  }
}

void downloadFile(AsyncWebServerRequest *request) {
  if (request->hasParam("fileName") == false)
  {
    request->send(400, "text/plain", "Filename not provided!");
    return;
  } 

  String fileName = request->getParam("fileName")->value();
  // Serial.println(fileName);

  if (!SD_MMC.exists(fileName))
  {
    request->send(400, "text/plain", "File not found!");
    return;
  }

  request->send(SD_MMC, fileName, "application/octet-stream", true);
}

void getFilesInRootFolderAPI(AsyncWebServerRequest *request){
//this api returns a list of files and their sizes from the root folder
  AsyncJsonResponse * response = new AsyncJsonResponse(true);
  const JsonArray& files = response->getRoot();
  //JsonArray &files = root.createNestedArray("Files");
  File rootFolder = SD_MMC.open("/");
  File foundFile = rootFolder.openNextFile();
  while (foundFile) {
    const JsonObject& file = files.createNestedObject();
    file["FileName"] = String(foundFile.name());
    file["Size"] = foundFile.size();
    foundFile = rootFolder.openNextFile();
  }
  rootFolder.close();
  foundFile.close();
  response->setLength();
  request->send(response);
}

void deleteFile(AsyncWebServerRequest *request) {

  if (request->hasParam("fileName") == false)
  {
    request->send(400, "text/plain", "File name not provided!");
    return;
  } 

  String fileName = request->getParam("fileName")->value();

  if (!SD_MMC.exists(fileName))
  {
    String message = "File was not found! No deletion needed!";
    sendMessageResponse(request, message);
    return;
  }

  if (SD_MMC.remove(fileName))
  {
    String message = "File deleted!";
    sendMessageResponse(request, message);
  } else {
    request->send(400, "text/plain", "File deletion failed!");
  }
}

String processor(const String& var) {

  if (var == "FILELIST") {
    return listFiles(true);
  }

  if (var == "FREESPIFFS") {
    return humanReadableSize((SD_MMC.totalBytes() - SD_MMC.usedBytes()));
  }

  if (var == "USEDSPIFFS") {
    return humanReadableSize(SD_MMC.usedBytes());
  }

  if (var == "TOTALSPIFFS") {
    return humanReadableSize(SD_MMC.totalBytes());
  }

  return String();
}

// list all of the files, if ishtml=true, return html rather than simple text
String listFiles(bool ishtml) {
  String returnText = "";
  File root = SD_MMC.open("/");
  File foundfile = root.openNextFile();

  if (ishtml) {
    returnText += "<table><tr><th align='left'>Name</th><th align='left'>Size</th></tr>";
  }

  while (foundfile) {

    if (ishtml) {
      returnText += "<tr align='left'><td>" + String(foundfile.name()) + "</td><td>" + humanReadableSize(foundfile.size()) + "</td></tr>";
    } else {
      returnText += "File: " + String(foundfile.name()) + "\n";
    }

    foundfile = root.openNextFile();
  }

  if (ishtml) {
    returnText += "</table>";
  }

  root.close();
  foundfile.close();
  return returnText;
}

//===================================================================
//Other
//getValue is used to get the values in a string based on a separator, mainly used for colors decoding
String getValue(String &data, char separator, int index) {
  int found = 0;
  int strIndex[] = { 0, -1 };
  int maxIndex = data.length() - 1;
  for (int i = 0; i <= maxIndex && found <= index; i++) {

    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    
    }
  }

  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

void sendMessageResponse(AsyncWebServerRequest *request, String &message) {
  request->send(200, "text/plain", "\"" + message + "\"");
}