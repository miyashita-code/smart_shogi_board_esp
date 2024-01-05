/*
********************************************************************
    BIDIRECTION_COMMUNICATION PROTCOL BETWEEN ESP32(MASTER)-ESP32(SLAVE)


    (MASTER -> SLAVE)
    < play order of DFPlayerMini>---------------------------------------
    1. format
    "playNomal ${turn} ${kind} ${sfen}" ... i.e) "playNomal black p 3d3e"
    "playError ${st}" ... i.e) "playError undo"

    2-1. detail 
    turn : black, white
    kind : p, l, n, s, g, b, r, k, +p, +l, +n, +r, +b
    sfen : like 8d7d(nomal) or 3f5g+(promotion) or p*3d(drop)
    
    2-2. detail
    st   : undo, moveCaution, putHnad, undoHand

    3. explain
    After getting these order from master ESP32, Slave ESP32 will start playing 
    Ordered Sound to tell user something (how to move, error st, ...)


    < detection order of LoadCell>--------------------------------------
    1. format
    "LoadCell order {prosessName}" ... i.e) "LoadCell order getChangeSt"

    2-1. detail 
    prosessName : waitUntilChange, getChangeSt
    
    3. explain
    After getting these order from master ESP32, Slave ESP32 will detect
    change of LoadCell status and return null or kind of piece of removed of put

    < set turn >--------------------------------------
    1. format
    "set turn {currenTurn}" ... i.e) "set turn black"

    2-1. detail 
    currenTurn : black, white
    
    3. explain
    After getting these order from master ESP32, Slave ESP32 will set led light for indicationg current turn
    ---------------------------------------------------------------------


    (SLAVE -> MASTER)
    < play order of DFPlayerMini>---------------------------------------
    1. format
    "playNomal ok"
    "playError ok"

    < detection order of LoadCell>--------------------------------------
    1. format
    "LoadCell result {detectedKind}" ... i.e) "LoadCell result p"

    2-1. detail 
    detectedKind : p, l, n, s, g, b, r, null, error
    ---------------------------------------------------------------------
*********************************************************************
*/


#include <Arduino.h>

#include <Ticker.h>
#include "HX711.h"
#include <DFRobotDFPlayerMini.h>

#include <math.h>


#define QUEUE_SIZE 3
#define IS_USE_MP3 true

// GPIO for digital_in & clk for LoadCell
#define DT_PIN 17
#define SCK_PIN 12

//GPIO for turn indication led
#define TRUN_BLACK_PIN (26)
#define TRUN_WHITE_PIN (25)


/*
*********************************************
************* SET UP INSTANCE ***************
*********************************************
*/


HardwareSerial mySerial(2);
HardwareSerial MasterESPSerial(1); // UART1 on ESP32
DFRobotDFPlayerMini myDFPlayer;
Ticker checkSerialTicker;
HX711 scale;

class Queue {
  public:
    Queue() {
        head = 0;
        tail = 0;
    }

    void initQueue() {
        head = 0;
        tail = -1;
    }

    void push(float input) {
        data[(tail + 1) % QUEUE_SIZE] = input;
        tail = (tail + 1) % QUEUE_SIZE;
    }

    float pop() {
        float value;

        if ((tail + 1) % QUEUE_SIZE == head) {
            Serial.println("ERROR: QUEUE IS NULL, CANNOT POP");
            return -1;
        }

        value = data[head];
        head = (head + 1) % QUEUE_SIZE;

        return value;
    }

    int back() {
        int size;

        if (tail < head) {
            size = tail + QUEUE_SIZE - head + 1;
        } else {
            size = tail - head + 1;
        }

        return size;
    }

    void print() {
        int size = back();

        Serial.print("size is: ");
        Serial.println(size);
        Serial.print("order of FIFO: ");

        for (int i = 0; i < size; i++) {
            Serial.print(data[(head + i) % QUEUE_SIZE]);
            Serial.print(",");
        }
        Serial.println();
    }

    int getValues(float *returnVlues, int max_size) {
        const int size = 3;

        if (size > max_size) 
          return 0;  // error status


        for(int i = 0; i < size; i++) {
          returnVlues[i] =  data[i];         
        }

        return size;
    }

  private:
    float data[QUEUE_SIZE];
    int head;
    int tail;
};

Queue q;

// Declare the MoveInfo struct
struct MoveInfo {
  int from_x;
  int from_y;
  int to_x;
  int to_y;
  String kind;
  String additional;
  String mode;
};


/*
*********************************************
************* GOLOBAL VARIABLE **************
*********************************************
*/


float weight_history[2] = {0,0};
bool isReadyToIdentifyFlag = false;
String current_turn = "BLACK";


void initializeLoadCell() {

  Serial.print("read:");
  Serial.println(scale.read());
  scale.set_scale();

  Serial.println("Tare... remove any weights from the scale.");  
  delay(500);
  scale.tare();
  Serial.println("Tare done...");

  Serial.print("Place a known weight on the scale...");
  Serial.print("calibrating...");  
  delay(500);
  Serial.println(scale.get_units(10));
  
  Serial.print("read (calibrated):");
  Serial.println(scale.get_units(10));
}

// function to detect and return kind of piece which is removed or put (String)
String identifyObject(Queue *pQueue) {
  float weight_comp = 0.0; 
  float mean, standardDeviation;

  String detectedKind;

  while (true) {
    // show index and load cell value
    pQueue->push(scale.get_units(10)); 
    
    // caluculate the 
    calculateStats(pQueue, &mean, &standardDeviation);
    Serial.print("standardDeviation : ");
    Serial.println(standardDeviation);
    if (standardDeviation < 0.5){
      weight_history[1] = mean;
      break;
    }
  }

  weight_comp = abs(weight_history[0] - weight_history[1])/ 10; // calc the difference

  Serial.print("weight_comp : ");
  Serial.println(weight_comp);
  if (weight_comp <= 5) {
    Serial.println("null");
    detectedKind = "null";
  }
  else if (5 < weight_comp && weight_comp <= 7.1){
    Serial.println("l");
    detectedKind = "l";
  }
  else if (7.1 < weight_comp && weight_comp <= 7.45){
    Serial.println("n");
    detectedKind = "n";
  }
  else if (7.45 < weight_comp && weight_comp <= 8.3){
    Serial.println("g");
    detectedKind = "g";
  }
  else if (8.3 < weight_comp && weight_comp <= 9.1){
    Serial.println("s");
    detectedKind = "s";
  }
  else if (9.1 < weight_comp && weight_comp <= 9.85){
    Serial.println("b");
    detectedKind = "b";
  }
  else if (9.85 < weight_comp && weight_comp <= 11){
    Serial.println("r");
    detectedKind = "r";
  }
  else if (11 < weight_comp && weight_comp <= 13){
    Serial.println("p");
    detectedKind = "p";
  }
  else{
    Serial.println("error");
    detectedKind = "error";
  }

  return detectedKind;
}


// function for calc statistacal values for judging if convergenced
void calculateStats(Queue *pQueue, float *pmean, float *pstandardDeviation) {
  float total = 0;
  float sumSquaredErrors = 0;
  float buf[3];  
  int queue_size = pQueue->getValues(buf, 3);

    // return ealry if unknone error st
  if (queue_size == 0) {*pstandardDeviation = 1; return;}
  
  // calc sum of buffers to know statical value
  for (int i = 0; i < queue_size; i++) {
      total += buf[i];
  }

  // return mena
  *pmean = total / queue_size;
  
  for (int i = 0; i < queue_size; i++) {
      sumSquaredErrors += pow(buf[i] - *pmean, 2);
  }
  
  // return standardDeviation
  *pstandardDeviation = sqrt(sumSquaredErrors / queue_size);
}



// Function to preprocess a given SFEN string into a MoveInfo struct
MoveInfo preprocess(String sfen, String kind) {
  // Initialize the MoveInfo struct with default values
  MoveInfo move = {-1, -1, -1, -1, "", "", ""};

  // Get the length of the input string
  int len = sfen.length();
  // Initialize index variable for iterating through the string
  int index = 0;

  // Check for drop mode
  move.mode = (sfen.indexOf("*") != -1) ? "d" : "m";


  
  // Check for promotion
  move.additional = (sfen.indexOf("+") != -1) ? "p" : "";

  // Parse start and end positions 
  /*
    Right Top -> Left Top
  */
  if (sfen[index] >= '0' && sfen[index] <= '9') {
    // moving
    move.from_y =  9 - (sfen[index] - '0');
    index++;
    move.from_x = sfen[index] - 'a';
    index++;
    move.to_y = 9 - (sfen[index] - '0');
    index++;
    move.to_x = sfen[index] - 'a';

    // set kind
    move.kind = kind;
  } else {
    // droping
    move.kind = toLowerCase(sfen[index]);
    index++;
    move.mode = "d";
    index++;
    move.to_y = 9 - (sfen[index] - '0');
    index++;
    move.to_x = sfen[index] - 'a';
    index++;
  }

  // Return the MoveInfo struct
  return move;
}


/* **********************************************************************************
 * **********************************************************************************
 *  FUNCTIONS FOR DFPLAYER MINI (MP3 SOUND MANAGE)
 * **********************************************************************************
 * **********************************************************************************
*/

int getPieceMp3Number(String kind) {
  int number = 0;
  char kind_buf;

  if (kind.length() >= 2 && kind[0] == '+') {
    number += 10;
    kind_buf = kind[1];
  }
  else {
    kind_buf = kind[0];
  }

  switch (kind_buf) {
    case 'k': number += 10; break;
    case 'p': number += 11; break;
    case 'r': number += 12; break;
    case 'b': number += 13; break;
    case 'g': number += 14; break;
    case 's': number += 15; break;
    case 'n': number += 16; break;
    case 'l': number += 17; break;
  }

  return number;
}

void playSound(String turn, int to_x, int to_y, String kind, String additional) {
  int sound_array[5]; // [turn, x, y, kind, (additional or none)]
  int size_sound_array = 5; // 4 or 5

  if (turn == "black") {
    sound_array[0] = 40;
  } else if (turn == "white") {
    sound_array[0] = 41;
  }

  sound_array[1] = to_x;
  sound_array[2] = to_y;

  sound_array[3] = getPieceMp3Number(kind);

  if (additional == "p"){
     sound_array[4] += 30; // "promotion"
  }
  else if (additional == "n") {
    sound_array[4] += 31; // "notpromotion"
  }
  else if (additional == "d") {
    sound_array[4] += 32; // "drop"
  }
  else if (additional == "s") {
    sound_array[4] += 33; // "same"
  }
  else{
    size_sound_array--;
  } 

  for (int i = 0; i < size_sound_array; i++) {
    myDFPlayer.playMp3Folder(sound_array[i]);
    Serial.print("----"); Serial.print(i); Serial.print("----: sound num");Serial.println(sound_array[i]);
    delay(1000);
    //Wait(DFPlayerPlayFinished);    
  }
}

void Wait(uint8_t expectedType) {
  while (!myDFPlayer.available()) {
    delay(10);
    Serial.println("Waiting for module to become available...");
  }

  uint8_t type = 0;
  uint8_t value = 0;
  int count = 0;
  while (type != expectedType) {
    count++;
    type = myDFPlayer.readType();
    value = myDFPlayer.read();
    printDetail(type, value);
    delay(100);
    Serial.println("Waiting for playback to finish...");
    if (count == 5) break;
  }
  Serial.println("Playback finished.");
}


void printDetail(uint8_t type, int value) {
  switch (type) {
    case TimeOut:
      Serial.println(F("Time Out!"));
      break;
    case WrongStack:
      Serial.println(F("Stack Wrong!"));
      break;
    case DFPlayerCardInserted:
      Serial.println(F("Card Inserted!"));
      break;
    case DFPlayerCardRemoved:
      Serial.println(F("Card Removed!"));
      break;
    case DFPlayerCardOnline:
      Serial.println(F("Card Online!"));
      break;
    case DFPlayerUSBInserted:
      Serial.println("USB Inserted!");
      break;
    case DFPlayerUSBRemoved:
      Serial.println("USB Removed!");
      break;
    case DFPlayerPlayFinished:
      Serial.print(F("Number:"));
      Serial.print(value);
      Serial.println(F(" Play Finished!"));
      break;
    case DFPlayerError:
      Serial.print(F("DFPlayerError:"));
      switch (value) {
        case Busy:
          Serial.println(F("Card not found"));
          break;
        case Sleeping:
          Serial.println(F("Sleeping"));
          break;
        case SerialWrongStack:
          Serial.println(F("Get Wrong Stack"));
          break;
        case CheckSumNotMatch:
          Serial.println(F("Check Sum Not Match"));
          break;
        case FileIndexOut:
          Serial.println(F("File Index Out of Bound"));
          break;
        case FileMismatch:
          Serial.println(F("Cannot Find File"));
          break;
        case Advertise:
          Serial.println(F("In Advertise"));
          break;
        default:
          break;
      }
      break;
    default:
      break;
  }
}


// function to play sound of indication of error st
void playErrorSound(String errorSt) {
  int stIndexOfmp3Foloder;

  // select folder
  if (errorSt == "undo") {
    stIndexOfmp3Foloder = 50;
  } 
  else if (errorSt == "moveCaution") {
    stIndexOfmp3Foloder = 51;
  } 
  else if (errorSt == "putHand") {
    stIndexOfmp3Foloder = 52;
  } 
  else if (errorSt == "undoHand") {
    stIndexOfmp3Foloder = 53;
  }
  
  // play sound
  myDFPlayer.playMp3Folder(stIndexOfmp3Foloder);
  delay(500);
  //Wait(DFPlayerPlayFinished);  
}

void playTurn(String trun) {
  int stIndexOfmp3Foloder = trun == "black" ? 40 : 41;

  // play sound
  myDFPlayer.playMp3Folder(stIndexOfmp3Foloder);
  delay(500);
  //Wait(DFPlayerPlayFinished);  

}


/* **********************************************************************************
 * **********************************************************************************
 *  FUNCTIONS FOR UART COMMUNICATION WITH MASTER
 * **********************************************************************************
 * **********************************************************************************
*/

// just send text
void sendToMasterSerial(String text) {
    MasterESPSerial.print(text.c_str());
}


// parse the UTRX command from master and handle them
void handleUTRXCommand(String command) {
  Serial.println("CHECK POINT 1");
  // Split the command into individual components
  String command_parts[5];
  int last_space = -1;
  for (int i = 0; i < 5; i++) {
    int next_space = command.indexOf(' ', last_space + 1);
    if (next_space == -1) {
      command_parts[i] = command.substring(last_space + 1);
      break;
    } else {
      command_parts[i] = command.substring(last_space + 1, next_space);
      last_space = next_space;
    }
  }

  // Determine the command type and call the appropriate function
  if (command_parts[1] == "playNomal") { // like "********** playNomal black p 3d3e"
    Serial.println("CHECK POINT 2");
    MoveInfo move_st = preprocess(command_parts[4], command_parts[3]);

	// play sound of indication of moving
    playSound(command_parts[2],  9 - move_st.to_y, move_st.to_x + 1, move_st.kind, move_st.additional);
  } 

  else if (command_parts[1] == "playError") { // like "********** playError undo"
	// play sound of indication of moving
    playErrorSound(command_parts[2]);
  } 
  
  else if (command_parts[1] == "set") { // like "********** set turn black"
    
    // set trun light
    if (command_parts[3] == "black") {
      digitalWrite(TRUN_BLACK_PIN, HIGH);
      digitalWrite(TRUN_WHITE_PIN, LOW);
    }
    else {
      digitalWrite(TRUN_BLACK_PIN, LOW);
      digitalWrite(TRUN_WHITE_PIN, HIGH);
    }

    if (command_parts[2] == "true") {
      playTurn(command_parts[3]);
    }

    // update wight history for next comparing
    weight_history[0] = weight_history[1];
  }
  else if (command_parts[1] == "LoadCell") { // like "********** LoadCell order getChangeSt" 
    sendToMasterSerial("LoadCell result " + identifyObject(&q));
  } 
  else {
    // Handle invalid commands
    Serial.println("Invalid command received.");
  }
}


// boaring function to ovserve esp MasterESPSerial(command comunication)
void checkSerial() {
  // set the flag
  isReadyToIdentifyFlag = true;
}


void setup() {

  pinMode(TRUN_BLACK_PIN, OUTPUT);
  pinMode(TRUN_WHITE_PIN, OUTPUT);

  delay(100);
  digitalWrite(TRUN_BLACK_PIN, HIGH);
  digitalWrite(TRUN_WHITE_PIN, HIGH);
    /*******************************************
     * *********** SERIALS SETUP PART **********
     * *****************************************
    */
  // set Serial 0 
  Serial.begin(9600);
  delay(1000);
  Serial.println("start");

  // set Serial 1 for ESP32s didirection connection
  // TODO ----- SET CORRECT PIN NUMVER TO BEGIN FUNCTION ------
  MasterESPSerial.begin(9600, SERIAL_8N1, 18, 19); // Initialize UART1 , 8 data bits


    /*******************************************
     * *********** LOADCELL SETUP PART **********
     * *****************************************
    */
  scale.begin(DT_PIN, SCK_PIN);
  /* initializeLoadCell(); */
  scale.set_scale(853.55853);
  scale.tare();

  delay(100);

  // set init st

  for(int i = 0; i < 2; i++) {
    q.push(scale.get_units(10));

    scale.power_down();
    delay(40);
    scale.power_up();
    delay(20);
  }

  identifyObject(&q);
  weight_history[0] = weight_history[1];


    /*******************************************
     * *********** SERIALS SETUP PART **********
     * *****************************************
    */
  if (IS_USE_MP3) {
      // set up mp3 player
      mySerial.begin(9600, SERIAL_8N1); // RX, TX pins for DFPlayer mini module
      delay(100);
      Serial.println("Initializing DFPlayer...");
      if (!myDFPlayer.begin(mySerial)) {
          Serial.println("Unable to begin");
          while(true);
      }
      Serial.println("DFPlayer Mini online.");
      myDFPlayer.volume(24); //Set volume value. From 0 to 30
  }

  /*******************************************
   * *********** BOARING FOR MASTER SERIAL COMUNICATION **********
   * *****************************************
  */  

  checkSerialTicker.attach(0.1, checkSerial);
}


void loop() {
  // push new observed value
  q.push(scale.get_units(10));

  scale.power_down();
  delay(40);
  scale.power_up();


  delay(30);

  if (isReadyToIdentifyFlag) {
    if (MasterESPSerial.available()) { // Check if there's data available from ESP_1
      String receivedMessage = MasterESPSerial.readString(); // Read the incoming message
      Serial.println("Received from ESP_1: " + receivedMessage); // Print the received message on Serial Monitor
      handleUTRXCommand(receivedMessage);
    }

    isReadyToIdentifyFlag = false;
  }

  delay(30);
}