#include <stdio.h>
#include <string.h>
#include <Adafruit_NeoPixel.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <DFRobotDFPlayerMini.h>
#include <Arduino.h>


#define LENGTH_BOARD (9) //setting of LED_grid
#define DIN_PIN   (27)  // digital Pin for serial LED
#define LED_COUNT (100) // the amout of LED

#define TRUN_BLACK_PIN (26)
#define TRUN_WHITE_PIN (25)





// serial leds
Adafruit_NeoPixel pixels(LED_COUNT, DIN_PIN, NEO_GRB + NEO_KHZ800);
uint32_t white = pixels.Color(0, 0, 255);

// mp3
HardwareSerial mySerial(2);
DFRobotDFPlayerMini myDFPlayer;

// ble
// Constants for the device name, service UUID and characteristic UUID
const char* deviceName = "将棋盤";
const char* serviceUuid = "3bf59480-82a4-4d1c-9325-7e6f3deccc77";
const char* charUuid = "784a8fdb-d235-4275-8c45-97059b7c906a";

// Variables to store pointers to the server, service and characteristic objects
BLEServer* pServer = NULL;
BLEService* pService = NULL;
BLECharacteristic* pCharacteristic = NULL;

// global state for drid led
int led_array[(LENGTH_BOARD + 1) * (LENGTH_BOARD + 1)];
int size_led;


int status_cell[LENGTH_BOARD][LENGTH_BOARD] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
};


int status_grid[10][10] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

int led_flag = 0;

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

int isInside(int x, int y);
void turnOnCell(int status_cell[][LENGTH_BOARD], int x, int y);
void turnOffCell(int status_cell[][LENGTH_BOARD], int x, int y);
void turnOnGrids(int status_grid[][LENGTH_BOARD + 1], int x, int y);
void updateGrids(int status_grid[][LENGTH_BOARD + 1], int status_cell[][LENGTH_BOARD], int x_index, int y_index);
int gridSwitchJudgeByCellSts(int status_cell[][LENGTH_BOARD], int x_index, int y_index);
static void perse_led_command(String led_command);
void ledNumbersForNeoPixel(int status_grid[][LENGTH_BOARD + 1], int buffer[(LENGTH_BOARD + 1) * (LENGTH_BOARD + 1)], int * return_size);
//turn on leds base on led_array
void turnOnLEDs(int *led_array, int size_array);
void reset_whole_status();
void managePieceSelector(String command, String kind);
void reset_whole_selector();

//------------------------------LED_grid.start

int isInside(int x, int y) 
{
    // This function checks if the given x and y coordinates are within the range of the board (1 <= x <= LENGTH_BOARD and 1 <=  y <= LENGTH_BOARD)
    return (0 <= x && x < LENGTH_BOARD && 0 <=  y && y < LENGTH_BOARD);
} 

void turnOnCell(int status_cell[][LENGTH_BOARD], int x_index, int y_index) {
  status_cell[y_index][x_index] = 1;
}

void turnOffCell(int status_cell[][LENGTH_BOARD], int x_index, int y_index) {
  status_cell[y_index][x_index] = 0;
}

void turnOnGrids(int status_grid[][LENGTH_BOARD + 1], int x_index, int y_index) {
    // This function turns on the 4 grids that form a square at the given x, y index (0 index) coordinates.
    status_grid[y_index][x_index] = 1;
    status_grid[y_index][x_index+1] = 1;
    status_grid[y_index+1][x_index] = 1;
    status_grid[y_index+1][x_index+1] = 1;
}

// search ON cells which are placed around the grid [x_index, y_index]
int gridSwitchJudgeByCellSts(int status_cell[][LENGTH_BOARD], int x_index, int y_index) {
  int result = 0;

  for(int i = y_index-1; i <= y_index; i++){
    for(int j = x_index-1; j <= x_index; j++) {
      if (isInside(j + 1, i + 1) && status_cell[i][j] == 1) {
        result = 1;
      }
    }
  }

  return result;
}

// O(1)
/*
  manage grid status which is reffered by the index_pos that from web-app via ble with using search function .
*/
void updateGrids(int status_grid[][LENGTH_BOARD + 1], int status_cell[][LENGTH_BOARD], int x_index, int y_index)
{
  for(int i = y_index; i <= y_index + 1; i++){
    for(int j = x_index; j <= x_index + 1; j++) {
      if(gridSwitchJudgeByCellSts(status_cell, j,i) == 1) {
        status_grid[i][j] = 1;
      }
      else {
        status_grid[i][j] = 0;
      }
    }
  }
}

void reset_whole_status() {
  // reset status_cell with all 0
  for(int i = 0; i < LENGTH_BOARD; i++) {
    for(int j = 0; j < LENGTH_BOARD; j++) {
      status_cell[i][j] = 0;
    }
  }

  // reset status_grid with all 0
  for(int i = 0; i < LENGTH_BOARD + 1; i++) {
    for(int j = 0; j < LENGTH_BOARD + 1; j++) {
      status_grid[i][j] = 0;
    }
  }

  // also reset lcd
  //lcd.clear()
}

void handleBLECommand(String ble_command) {
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
  if (command_parts[0] == "move") {
    Serial.println("CHECK POINT 2");
    MoveInfo move_st = preprocess(command_parts[2], command_parts[1]);
    executeIndication(move_st);

    // set trun light
    digitalWrite(TRUN_BLACK_PIN, LOW);
    digitalWrite(TRUN_WHITE_PIN, HIGH);
  } else if (command_parts[0] == "clear") {
    reset_whole_status();

    // set trun light
    digitalWrite(TRUN_BLACK_PIN, HIGH);
    digitalWrite(TRUN_WHITE_PIN, LOW);
    
    // turn ON led_flag
    led_flag = 1;
  } else {
    // Handle invalid commands
    Serial.println("Invalid command received.");
  }
}


void executeIndication(MoveInfo move_st) {
    if (move_st.mode == "m") { // nomal moving
        boolean isPromotion = false;
        if (move_st.additional == "p") isPromotion = true; 
        
        Serial.println("CHECK POINT 3");        
        // turn On leds to indicate simple move
        moveIndication(move_st.from_x, move_st.from_y, move_st.to_x, move_st.to_y, isPromotion);
    } 
    else if (move_st.mode == "d") { // droping 
        dropIndication(move_st.to_x, move_st.to_y, move_st.kind);
    }

    // play Sound
    playSound("white", 9 - move_st.to_y, move_st.to_x + 1, move_st.kind, move_st.additional);
}

/*
    ------------ TODO ---------------------- 
*/
void moveIndication(int from_x, int from_y, int to_x, int to_y, boolean isPromotion) {
    Serial.println("CHECK POINT 4");

    // turn on from position
    if (isInside(from_x,from_y)) {
        Serial.println("CHECK POINT 4-1");
        turnOnCell(status_cell, from_x, from_y);
        turnOnGrids(status_grid, from_x, from_y);
    }

    // turn on to position
    if (isInside(to_x,to_y)) {
        Serial.println("CHECK POINT 4-2");
        turnOnCell(status_cell, to_x, to_y);
        turnOnGrids(status_grid, to_x, to_y);
    }

    // set flag (for truning on leds) on
    led_flag = 1;

    if (isPromotion) {
    // TODO tell promotion
    }
}



void dropIndication(int to_x, int to_y, String kind) {
    // turn on to position
    if (isInside(to_x,to_y)) {
        turnOnCell(status_cell, to_x-1, to_y-1);
        turnOnGrids(status_grid, to_x-1, to_y-1);
    }

    // set flag (for truning on leds) on
    led_flag = 1;

    // TODO change color or cahnge effect
}

/* ---------------------------------------------------------*/

void reset_whole_selector() {

}

void managePieceSelector(String command, String kind) {

}

void ledNumbersForNeoPixel(int status_grid[][LENGTH_BOARD + 1], int buffer[(LENGTH_BOARD + 1) * (LENGTH_BOARD + 1)], int * return_size) {
    // This function finds the led numbers for NeoPixel based on the status grid.
    // It stores the led numbers in the buffer array and the number of leds in return_size.
    int count = 0;
    int return_index = 0;

    for (int i = 0; i < LENGTH_BOARD + 1; i++) {
        if (i % 2 == 0) {
            for (int j = 0; j < LENGTH_BOARD + 1; j++) {
                if (status_grid[i][j] == 1) {
                    buffer[return_index++] = count;
                }
                count++;
            }
        } else {
            for (int j = LENGTH_BOARD; 0 <= j; j--) {
                if (status_grid[i][j] == 1) {
                    buffer[return_index++] = count;
                }
                count++;
            }
        }
    }
    *return_size = return_index;
}

//turn on leds base on led_array
void turnOnLEDs(int *led_array, int size_array){
  pixels.clear();
  for(int i = 0; i < size_array; i++){
    pixels.setPixelColor(led_array[i], white);
  }
  pixels.show();
}

//BLE function ----------------------------------

// Callback class for handling characteristic write events
class MyCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    // Get the value written to the characteristic
    String receivedValue = (pCharacteristic->getValue()).c_str();

    // Parse the decoded string to process the command
    if (receivedValue.length() > 0) {
      Serial.println(receivedValue);
      handleBLECommand(receivedValue);
    }
  }
};

// Callback class for handling server disconnect events
class MyServerCallbacks: public BLEServerCallbacks {
  void onDisconnect(BLEServer* pServer) {
    // Start advertising again when a client disconnects
    pServer->getAdvertising()->start();
    Serial.println("Client disconnected");
  }
};


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

// --------------- part for mp3 ------------------------

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


//-----------------------------------------------------
void setup() {
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


  pinMode(TRUN_BLACK_PIN, OUTPUT); 
  pinMode(TRUN_WHITE_PIN, OUTPUT);   
  

  
  // Initialize the serial connection and BLE
  Serial.begin(115200);
  BLEDevice::init(deviceName);

  //set pixel (serial_led by Adafruit_NeoPixel)
  pixels.begin();
  pixels.clear();
  delay(100);
  pixels.show();

  // Create a server and set the callback for disconnect events
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

    // Create a service and a characteristic, and set the callbacks for the characteristic
  pService = pServer->createService(serviceUuid);
  pCharacteristic = pService->createCharacteristic(
    charUuid,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_WRITE
  );
  pCharacteristic->setCallbacks(new MyCallbacks());

  // Start the service and advertising
  pService->start();
  pServer->getAdvertising()->start();
  Serial.println("Waiting for a client connection to the device...");
}




void loop() {
  if (led_flag == 1)
  {
    Serial.println("CHECK POINT 5");
    // clac led_array from grig(array) of status 
    ledNumbersForNeoPixel(status_grid, led_array, &size_led);

    // turn on LEDs based on led_array
    turnOnLEDs(led_array, size_led);

    
    // for debug
    for (int i = 0; i < size_led; i++)
    {
      Serial.printf("%d,", led_array[i]);      
    }

    Serial.println("");
    
    // turn OFF led_flag
    led_flag = 0;
  }
}