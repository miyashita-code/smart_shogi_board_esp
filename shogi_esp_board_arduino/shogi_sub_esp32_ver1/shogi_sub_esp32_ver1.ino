#include <Arduino.h>
#include "HX711.h"
#include <math.h>
#include <DFRobotDFPlayerMini.h>


HardwareSerial mySerial(2);
DFRobotDFPlayerMini myDFPlayer;


#define QUEUE_SIZE 3

#define IS_USE_MP3 true

// digital_in pin & clk pin
const int DT_PIN = 8;
const int SCK_PIN = 9;


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




int t = 0;
float weight_history[2] = {0,0};
float mean, standardDeviation;
bool turn = true;
bool stab = true;

HX711 scale;




void initializeLoadCell() {

  Serial.print("read:");
  Serial.println(scale.read());
  scale.set_scale();

  Serial.println("Tare... remove any weights from the scale.");  
  delay(1000);
  scale.tare();
  Serial.println("Tare done...");

  Serial.print("Place a known weight on the scale...");
  Serial.print("calibrating...");  
  delay(1000);
  Serial.println(scale.get_units(10));
  
  Serial.print("read (calibrated):");
  Serial.println(scale.get_units(10));
}

// 物体の識別を行う関数
String identifyObject(Queue *pQueue) {
  float weight_comp = 0.0; 

  while (true) {
    // show index and load cell value
    Serial.println(pQueue->back());
    pQueue->push(scale.get_units(10)); 
    
    calculateStats(pQueue, &mean, &standardDeviation);
    Serial.print("安定するまで待機. . .");
    Serial.println(standardDeviation);
    if (standardDeviation < 0.5){
      weight_history[1] = mean;
      break;
    }
  }

  Serial.print("判別可能");
  weight_comp = abs(weight_history[0] - weight_history[1]); // 変化量の計算

  Serial.println("これは");
  Serial.println(weight_comp);
  if (weight_comp <= 1.45) {
    Serial.println("歩");
  }
  else if (1.45 < weight_comp && weight_comp <= 1.6875){
    Serial.println("香車");
  }
  else if (1.6875 < weight_comp && weight_comp <= 1.8625){
    Serial.println("桂馬");
  }
  else if (1.8625 < weight_comp && weight_comp <= 2.15){
    Serial.println("金");
  }
  else if (2.15 < weight_comp && weight_comp <= 2.375){
    Serial.println("銀");
  }
  else if (2.375 < weight_comp && weight_comp <= 2.475){
    Serial.println("角");
  }
  else if (2.475 < weight_comp && weight_comp <= 2.625){
    Serial.println("飛車");
  }
  else if (5.4 < weight_comp && weight_comp <= 5.6){
    Serial.println("飛車");
  }
  else if (5.6 < weight_comp && weight_comp <= 5.8){
    Serial.println("飛車");
  }
  else{
    Serial.println("error");
  }
}


// function for calc statistacal values for judging if convergenced
void calculateStats(Queue *pQueue, float *mean, float *standardDeviation) {
  float total = 0;
  float sumSquaredErrors = 0;
  float buf[3];  
  int queue_size = pQueue->getValues(buf, 3);
  

  if (queue_size == 0) {*standardDeviation = 1; return;}
  
  for (int i = 0; i < queue_size; i++) {
      total += buf[i];
  }

  // return 
  *mean = total / queue_size;
  
  for (int i = 0; i < queue_size; i++) {
      sumSquaredErrors += pow(buf[i] - *mean, 2);
  }
  
  // return
  *standardDeviation = sqrt(sumSquaredErrors / queue_size);
}

void getProcessOrder() {
  if (Serial1.available() > 0) {
    Serial.println("detected");
      char input = Serial.read();
      Serial.print(input);
      if (input == 'c') {  
        Serial.println('交代します');
        weight_history[0] = weight_history[1];
        if (turn){
          turn = false;
        }
        else {
          turn = true;
        }
      }
      if (input == 'i'){
        identifyObject(&q);
      }
    }  

    if (turn){
      Serial.println("おじいちゃんの番");
      if (stab){
        Serial.println("安定");
        weight_history[1] = mean;
      }
      else{
        Serial.println("不安定");
      }
    }
    else {
      Serial.println("相手の番");
      if (stab){
        Serial.println("安定");
        weight_history[1] = mean;
      }
      else{
        Serial.print("不安定 ");
        Serial.println("相手の取った駒取っちゃったかも");
      }
    }      
}


void handleUTRXCommand(String ble_command) {
  Serial.println("CHECK POINT 1");
  // Split the command into individual components
  String command_parts[3];
  int last_space = -1;
  for (int i = 0; i < 3; i++) {
    int next_space = ble_command.indexOf(' ', last_space + 1);
    if (next_space == -1) {
      command_parts[i] = ble_command.substring(last_space + 1);
    } else {
      command_parts[i] = ble_command.substring(last_space + 1, next_space);
      last_space = next_space;
    }
  }

  // Determine the command type and call the appropriate function
  if (command_parts[0] == "play") {
    Serial.println("CHECK POINT 2");
    MoveInfo move_st = preprocess(command_parts[2], command_parts[1]);

	// play sound of indication of moving
    playSound("white", 9 - move_st.to_y, move_st.to_x + 1, move_st.kind, move_st.additional);

  } else if (command_parts[0] == "set") {
    // -- TODO--

    // set trun light
    //digitalWrite(TRUN_BLACK_PIN, HIGH);
    //digitalWrite(TRUN_WHITE_PIN, LOW);

  } else if (command_parts[0] == "detecte") {
    
  } 
  else {
    // Handle invalid commands
    Serial.println("Invalid command received.");
  }
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
  move.mode = (sfen.indexOf('*') != -1) ? "d" : "m";
  
  // Check for promotion
  move.additional = (sfen.indexOf('+') != -1) ? "p" : "";

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
    move.kind = sfen[index];
    index++;
    move.mode = "d";
    index++;
    move.to_y = sfen[index] - '0' - 1;
    index++;
    move.to_x = 8 - (sfen[index] - 'a');
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

  if (additional == "p") sound_array[4] += 30; // "promotion"
  else if (additional == "n") sound_array[4] += 31; // "notpromotion"
  else if (additional == "d") sound_array[4] += 32; // "drop"
  else if (additional == "s") sound_array[4] += 33; // "same"
  else size_sound_array--;

  for (int i = 0; i < size_sound_array; i++) {
    myDFPlayer.playMp3Folder(sound_array[i]);
    Serial.print("----"); Serial.print(i); Serial.println("----");
    delay(1000);
    Wait(DFPlayerPlayFinished);    
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




void setup() {
  Serial.begin(9600);
  Serial.println("start");
  scale.begin(DT_PIN, SCK_PIN);
  // initializeLoadCell();
  scale.set_scale(853.55853);
  scale.tare();


    // ---------set mp3---------------------
  if (IS_USE_MP3) {
      // set up mp3 player
      mySerial.begin(9600, SERIAL_8N1); // RX, TX pins for DFPlayer mini module
      delay(1000);
      Serial.println("Initializing DFPlayer...");
      if (!myDFPlayer.begin(mySerial)) {
          Serial.println("Unable to begin");
          while(true);
      }
      Serial.println("DFPlayer Mini online.");
      myDFPlayer.volume(25); //Set volume value. From 0 to 30
  }
}

void loop() {
  bool isLoadcellStayble = false;  
  float mean, standardDeviation;

  q.push(scale.get_units(10));

  calculateStats(&q, &mean, &standardDeviation);

  Serial.print("Standard deviation: ");
  Serial.println(standardDeviation);

  // update is stable
  if (standardDeviation < 0.5){
    isLoadcellStayble = true;
  }


  if (mean != 0) {
    getProcessOrder();
  }

  scale.power_down();
  delay(5);
  scale.power_up();
}
