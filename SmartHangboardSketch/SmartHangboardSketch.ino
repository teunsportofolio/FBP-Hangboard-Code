#include "HX711.h"  //library for load cell chip
// #include <FastLED.h> //fastled library for addressable LEDS, not using due to timing issues with the library.
#include <BLEDevice.h>  //libraries for BLE communication
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

/*#define FASTLED_ESP32_I2S //not using due to compatibility/timing issues
#define NUM_LEDS 20
#define DATA_PIN 19
CRGB leds[NUM_LEDS]; */

#include <NeoPixelBus.h>  //addressable LED library using I2S for good timing
#include "esp_system.h"   //esp32 function, for random number generation


#define NUM_LEDS 20  //defining led strip
#define DATA_PIN 19
NeoPixelBus<NeoGrbFeature, NeoEsp32I2s1X8Ws2812xMethod> strip(NUM_LEDS, DATA_PIN);
int led = 9;  //defining half of the led strip


BLEServer* pServer = NULL;  //creating BLE characteristics
BLECharacteristic* pSensorCharacteristic = NULL;
BLECharacteristic* pLedCharacteristic = NULL;

#define SERVICE_UUID "19b10000-e8f2-537e-4f6c-d104768a1214"  //UUID's gotten from randomnerdtutorials.
#define SENSOR_CHARACTERISTIC_UUID "19b10001-e8f2-537e-4f6c-d104768a1214"
#define LED_CHARACTERISTIC_UUID "19b10002-e8f2-537e-4f6c-d104768a1214"

bool deviceConnected = false;
bool oldDeviceConnected = false;
uint32_t value = 0;


HX711 ScaleR;  //defining the two load cell objects
HX711 ScaleL;

uint8_t dataPinL = 23;  //data pin for left HX711 chip
uint8_t dataPin = 17;   //data pin for right HX711 chip
uint8_t clockPin = 13;  //clockpin shared by both HX711

float WeightR = 0;  //all read in getweight void
float WeightL = 0;
float WeightCombined;

float OldWeightCombined;  //all used in loop
float maxWeight = 0;
float redThreshold;
float maxWeightR = 0;
float redThresholdR;
float maxWeightL = 0;
float redThresholdL;

unsigned long previousMillis = 0;   //for getweight
unsigned long previousMillis2 = 0;  //for Timers

bool workoutStart = false;  //a bunch of bools for activating hierarchy of program
bool hanging = false;
bool hangStart = false;
bool programstart = false;
bool timerStarted = false;

int setsDone = 0;  //tracking the amount of sets

int hangTime = 0;  //all sent over BLE
int restTime = 0;
int sets = 0;
int goalWeight = 0;
String workoutType;


float RunningMaxL = 0;  //all used to calculate data to send back to BLE
float RunningMaxR = 0;
int RunningCount = 0;
float RunningSumR = 0;
float RunningSumL = 0;
int previousR = 0;
int previousL = 0;

bool StartedRandomHang = false;  //for randomhangs
bool RandomHanghand;
int RandomHangTime;
bool green = true;

unsigned long StartTime;  //calculating time
unsigned long EndTime;
const int RandomTimeMax = 6000;  //Maximum timer time, adds to TimeMin
const int TimeMin = 4000;


class MyServerCallbacks : public BLEServerCallbacks {  //simple class defining when device is connected
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
  }
  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
  }
};

class MyCharacteristicCallbacks : public BLECharacteristicCallbacks {  //if a callback is recieved from the other BLE device,
  void onWrite(BLECharacteristic* pLedCharacteristic) override {
    String value = pLedCharacteristic->getValue();  //write written characteristic to a string
    // Serial.println(value);

    workoutStart = false;  //writing all bools important for workout to false/0, so a workout can be overridden by a new one.
    hanging = false;
    hangStart = false;
    setsDone = 0;

    if (value.length() == 0) return;  //if string is empty, stop

    char buffer[50];
    if (value.length() < sizeof(buffer)) {
      value.toCharArray(buffer, sizeof(buffer));  //making the string into a chararray, so it can be broken up.
      char* type = strtok(buffer, ",");           //breaking the string up with the ',', using string to token. writing null if empty
      char* hangStr = strtok(NULL, ",");
      char* restStr = strtok(NULL, ",");
      char* setsStr = strtok(NULL, ",");
      char* goalStr = strtok(NULL, ",");

      if (type && hangStr && restStr && setsStr) {  //if all necessary data is present, convert to int & string
        workoutType = String(type);
        hangTime = atoi(hangStr);  //atoi is string to number
        restTime = atoi(restStr);
        sets = atoi(setsStr);
        goalWeight = (goalStr != NULL) ? atoi(goalStr) : 0;  //shorthand for if not null, then it is the goalstring, otherwise 0.

        /*  Serial.println("Workout Type: " + workoutType);
        Serial.println("Hang Time: " + String(hangTime));
        Serial.println("Rest Time: " + String(restTime));
        Serial.println("Sets: " + String(sets)); */
      } else {
        Serial.println("Invalid CSV format");
      }
    } else {
      Serial.println("Input too long");
    }
  }
};

/*void FillSolid(int g, int r, int b, int ledStart, int ledEnd) {
  fill_solid(&(leds[ledStart]), ledEnd - ledStart, CRGB(r, g, b));
  FastLED.show();    

} */
//used previously, not used anymore due to library timing issues

void FillSolid(int g, int r, int b, int ledStart, int ledEnd) {
  RgbColor color(g, r, b);                   // Creating variable for color of strip, using additives in Fillsolid
  for (int i = ledStart; i < ledEnd; i++) {  //write color from defined starting led to defined ending led.
    strip.SetPixelColor(i, color);
  }
  strip.Show();  //show the changes.
}

void setup() {
  Serial.begin(115200);  //high, since we have an esp32
  delay(20);             //give some time for startup


  ScaleR.begin(dataPin, clockPin);  //start scale objects
  ScaleL.begin(dataPinL, clockPin);

  ScaleL.set_offset(-413935);  //These offsets and set_scale make the scales display the correct weigh
  ScaleL.set_scale(-41.620079);
  ScaleR.set_offset(-273972);
  ScaleR.set_scale(-41.620079);
  ScaleR.tare();
  ScaleL.tare();

  // FastLED.addLeds<WS2812B, DATA_PIN, RGB>(leds, NUM_LEDS);//used previously, not used anymore due to library timing issues

  strip.Begin();  //start led strip
  strip.Show();   // clearing leds

  BLEDevice::init("ESP32");  //initialize BLE

  pServer = BLEDevice::createServer();  //creating server and attributes to read and write.
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService* pService = pServer->createService(SERVICE_UUID);

  pSensorCharacteristic = pService->createCharacteristic(
    SENSOR_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_INDICATE);

  pLedCharacteristic = pService->createCharacteristic(
    LED_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_WRITE);

  pLedCharacteristic->setCallbacks(new MyCharacteristicCallbacks());

  pSensorCharacteristic->addDescriptor(new BLE2902());
  pLedCharacteristic->addDescriptor(new BLE2902());

  pService->start();

  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();  //show BLE device ot other devices
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);
  BLEDevice::startAdvertising();

  Serial.println("setup succesfull");  //for debugging
}

void Timer(const long interval, void (*callback)() = nullptr) {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis2 >= interval) {  //simple defined interval timer, not using delay
    previousMillis2 = currentMillis;
    timerStarted = false;

    if (callback) {  // if callback exists/is not null
      callback();    // callback to another void
    }
    yield();  //to make sure BLE isnt blocked by other functions, it yields to it.
  }
}

void PrintBoolStates() {  //debug void, easy to activate/deactive when not necessary
  Serial.print("workoutStart: ");
  Serial.println(workoutStart);
  Serial.print("hanging: ");
  Serial.println(hanging);
  Serial.print("hangStart: ");
  Serial.println(hangStart);
  Serial.print("timerStarted: ");
  Serial.println(timerStarted);
  Serial.print("green: ");
  Serial.println(green);
  Serial.print("StartedRandomHang: ");
  Serial.println(StartedRandomHang);
  Serial.print("WeightL: ");
  Serial.println(WeightL);
  Serial.print("WeightR: ");
  Serial.println(WeightR);
}


void loop() {

  if (!programstart) {  //when the software first start up, make the board light up white.
    FillSolid(40, 40, 40, 0, 19);
    programstart = true;
  }

  // PrintBoolStates(); //debugging
  GetWeight(100);  //Gets the weight values from the HX711's, they can sample 10 times per second, thus an interval of 100ms

  if (workoutType == "ReactionHangs") {  //if this is recieved from the BLE

    if (!workoutStart) {  //make blue to know somethign was recieved
      FillSolid(0, 0, 128, 0, 19);
    }
    if (WeightR > 1000 && WeightL > 1000 && !hanging) {  // if both hands are excerting a bit of force, the workout starts.
      workoutStart = true;
      hanging = true;
      hangStart = false;
    }

    if (workoutStart) {
      if (!hangStart) {//when the workoutstarts, it first does random timer, since these are reaction based hangs.

        if (!timerStarted) {
          uint32_t raw = esp_random(); //uses ESP wifi to create true random numbers
          RandomHangTime = raw % RandomTimeMax + TimeMin; //uses the remainder of the random number (0-6000)+ minimum time of 4000 to create an interval between 4 and 10 seconds.

          previousMillis2 = millis();  // Reset timer at start of hang
          timerStarted = true;
        }

        if (!green) {//when a hang has been succesfull, device is green for 1 second. 
          FillSolid(0, 255, 0, 0, 19);
          Timer(1000, greenAddition);
        }
        if (green) {//after turning green or during the first set, it is blue, but more faded than the start up blue, to know the workout has officially started.
          FillSolid(0, 0, 70, 0, 19);
          Timer(RandomHangTime, RandomHangtimerAddition); //creates timer with random interval, adds the RandomHangtimerAddition void 
        }
      }
    }
    if (hangStart) {
      if (!StartedRandomHang) {//processes once which hand should be hung on
        uint32_t raw = esp_random();
        RandomHanghand = raw % 2; //remainder of 2 creates a 1 or 0/ true or false. if true, right hand, if false, left hand.
        FillSolid(0, 0, 0, 0, 19); //makes all leds black
        if (!RandomHanghand) { FillSolid(255, 0, 0, 0, 9); }//half of the leds red, on the side of the hand to which force should be applied
        if (RandomHanghand) { FillSolid(255, 0, 0, 10, 19); }
        StartTime = millis();

        StartedRandomHang = true;
      }
      // startTimer

      if ((goalWeight * 1000 < WeightL && RandomHanghand) || (goalWeight * 1000 < WeightR && !RandomHanghand)) {//if the weight of the hand which should hang is high enough
        //send time to program
        EndTime = millis();
        float totalTime = EndTime - StartTime;
        SendSingleData(totalTime); //sending the time it took to hang the needed weight
        setsDone++; //one set completed
        hangStart = false; //send back to the timer. 
        StartedRandomHang = false;
        green = false;
      } 
      if (setsDone >= sets) {//stops script if workout is completed. 
        hanging = false;
        workoutType = "stop";
        setsDone = 0;
      }
    }
  }

  if (workoutType == "MaxHangs") {//if the workout type sent over BLE is maxhangs. 
    if (!workoutStart) {//write blue when message recieved. 
      FillSolid(0, 0, 128, 0, 19);
    }
    if (WeightR > 1000 && WeightL > 1000 && !hanging) {//start workout when load applied to both hands
      workoutStart = true;
      hanging = true;
      hangStart = true;
    }

    // Serial.println("sets done: " + String(setsDone));

    if (workoutStart) {
      if (hangStart) {
        if (!timerStarted) {
          previousMillis2 = millis();  // reset timer at start of hang
          timerStarted = true;
        }
        Timer(hangTime * 1000, MaxHangtimerAddition); //timer for the amount of seconds send over BLE
        if (WeightCombined != OldWeightCombined) {//if the data is not the same as the last data sent, send weightcombined over BLE again (prevents BLE from getting flooded)
          SendSingleData(WeightCombined); 
        }

        OldWeightCombined = WeightCombined;

        // processing max weight per set
        if (WeightR > RunningMaxR) {
          RunningMaxR = WeightR;
        }
        if (WeightL > RunningMaxL) {
          RunningMaxL = WeightL;
        }

        //calculations for avarage, avarage itself calculated each set.
        if (WeightR != previousR) {
          RunningSumR += WeightR;
          previousR = WeightR;
          RunningCount++;
        }

        if (WeightL != previousL) {
          RunningSumL += WeightL;
          previousL = WeightL;
        }

        //if the goalweight is filled it, each hand should do 1/2 of the goal weight.
        if (goalWeight != 0) {
          maxWeightL = goalWeight / 2;
          maxWeightR = goalWeight / 2;
        }

        //otherwise, the highest load of each hand is the goal weight for the hands. 
        if (goalWeight == 0) {
          if (maxWeightL < WeightL) maxWeightL = WeightL;
          if (maxWeightR < WeightR) maxWeightR = WeightR;
        }

        redThresholdR = -(maxWeightR * .9) + maxWeightR; //leds turn red when <85% of max load is applied to a hand
        redThresholdL = -(maxWeightL * .9) + maxWeightL;

        if (WeightR < redThresholdR) {
          FillSolid(255, 0, 0, 0, 9);  // This was for the left, now for right
        } else {
          int ledFade = map(WeightR, redThresholdR, maxWeightR * .99, -255, 255); //maps current weight between the redthreshold and 99% of the maxweight from 255 to -255
          if (ledFade < 0) FillSolid(255, 255 - abs(ledFade), 0, 0, 9); //if under 0, it is fully red, but becoming more green the closer to 0 the ledfade is.  
          else FillSolid(255 - ledFade, 255, 0, 0, 9); //green, becoming more yellow the closer to 0 ledfade is
        }

        // Same but for the left side
        if (WeightL < redThresholdL) {
          FillSolid(255, 0, 0, 9, 19);  // This was for the right, now for left
        } else {
          int ledFade = map(WeightL, redThresholdL, maxWeightL * .99, -255, 255);
          if (ledFade < 0) FillSolid(255, 255 - abs(ledFade), 0, 9, 19);
          else FillSolid(255 - ledFade, 255, 0, 9, 19);
        }

      } else {
        if (!timerStarted) {
          previousMillis2 = millis();  // Reset timer at start of hang
          timerStarted = true;
        }
        RestTimer(restTime * 1000); //start resttimer, which is a little special as explained below. 
      }

      if (setsDone >= sets) { //stop workout if all sets are done. 
        hanging = false;
        workoutType = "stop";
        setsDone = 0;
      }
    }
  }

  if (workoutType == "stop") {
    FillSolid(0, 0, 0, 0, 19);
  } //if a workout is done, turn off the leds. 

  if (!deviceConnected && oldDeviceConnected) {
    delay(500);                   // Short delay for the stack to clear
    pServer->startAdvertising();  // Restart advertising
    Serial.println("Start advertising again");
    oldDeviceConnected = deviceConnected;
  }

  // Connect handling
  if (deviceConnected && !oldDeviceConnected) {
    // Just connected
    oldDeviceConnected = deviceConnected;
  }
}

void GetWeight(const long interval) {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) { //samples every interval
    previousMillis = currentMillis;
    WeightR = ScaleR.get_units(1); //gets the average of 1 reading from the scale, speed over precisions, since it is post-processed for the BLE later. 
    WeightL = ScaleL.get_units(1);
    WeightCombined = WeightR + WeightL; //calculates the combined weight immidiatly. 
    // Serial.println(WeightCombined);
  }
}




void MaxHangtimerAddition() {//addition to timer void for max hangs
  
  hangStart = false; // goes back to resttimer
  setsDone++; // 1 set done
  SendData(); //send  data -> see void
  RunningMaxL = 0; //resets all sent data
  RunningMaxR = 0;
  RunningCount = 0;
  RunningSumR = 0;
  RunningSumL = 0;
}

void RandomHangtimerAddition() {//starts hangs.
  hangStart = true;
}

void greenAddition() {//makes sure the script goes back to the randomhangtimer after showing green for a second. 
  green = true;
}

void SendData() {
  if (RunningCount == 0) return;  // if no data was collected, this void is not ran
  float avgR = RunningSumR / RunningCount; //calculates average by dividing all the readings added up by the amount of readings. 
  float avgL = RunningSumL / RunningCount;
  float max = RunningMaxL + RunningMaxR;
  float avg = avgR + avgL;

  char Data[100]; //create a chararray for the data 
  snprintf(Data, sizeof(Data), "%.2f;%.2f;%.2f;%.2f;%.2f;%.2f", RunningMaxL, avgL, RunningMaxR, avgR, max, avg); //A lot going on in one line. prints data to the chararray, as a string, %.2f prints all the data as floats with 2 decimals, separates with ;
  pSensorCharacteristic->setValue(Data); //set the value to be send to the data
  pSensorCharacteristic->notify(); //send the data
}

void SendSingleData(float value) {// smaller buffer, 1 datapoint, same principle as above but simples. 

  char Data[20]; 
  snprintf(Data, sizeof(Data), "%.2f", value);
  pSensorCharacteristic->setValue(Data);
  pSensorCharacteristic->notify();
  return;
}

void RestTimer(const long interval) {
  unsigned long currentMillis = millis();
  unsigned long elapsedTime = currentMillis - previousMillis2; //calculates the time this timer has ran for

  int ledsOn = 19 - (elapsedTime * 19 / interval); //calculates the amount of leds on based on the elapsed time asnd interval

  if (ledsOn < 0) {ledsOn = 0;} //makes sure nothing weird happens with the numbers 
  if (ledsOn > 19) {ledsOn = 19;}


  FillSolid(0, 0, 255, 0, ledsOn);  //turns on all the leds from led 0 to the amount of leds on
  FillSolid(0, 0, 0, ledsOn, 19); //turns off all other leds. (yes one led is overlapping, but + or - 1 would give weird cases in both)
  if (elapsedTime >= interval) { //if the timer ran out, return to the hangs
    hangStart = true;
    timerStarted = false;
    previousMillis2 = currentMillis;
  }
}
