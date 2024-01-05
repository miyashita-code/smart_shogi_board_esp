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
    st   : undo, moveCaution

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
    "set {isPlay} {currenTurn}" ... i.e) "set turn black"

    2-1. detail 
    isPlay : true; false
    currenTurn : black, white
    
    3. explain
    After getting these order from master ESP32, Slave ESP32 will set led light for indicationg current turn
-------------------------------------------------------------

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


#include <stdio.h>
#include <string.h>

#include <Adafruit_NeoPixel.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <DFRobotDFPlayerMini.h>
#include <Ticker.h>
#include "HX711.h"
#include <DFRobotDFPlayerMini.h>

#include <math.h>



/* **********************************************************************************
 * **********************************************************************************
 *  // preprocess for BLE and serial LED and DFPlayerMini
 * **********************************************************************************
 * **********************************************************************************
*/
#define LENGTH_BOARD (9) //setting of LED_grid
#define DIN_PIN   (14)  // digital Pin for serial LED
#define LED_COUNT (100) // the amout of LED



// for DEBUG
#define IS_USE_MP3 false

// serial leds
Adafruit_NeoPixel pixels(LED_COUNT, DIN_PIN, NEO_GRB + NEO_KHZ800);
uint32_t white = pixels.Color(0, 0, 255);

// mp3
HardwareSerial SlaveESP32Serial(2);

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

uint32_t color_led_array[(LENGTH_BOARD + 1) * (LENGTH_BOARD + 1)];
int led_array[(LENGTH_BOARD + 1) * (LENGTH_BOARD + 1)];
int size_led;

int status_cell[LENGTH_BOARD][LENGTH_BOARD];
int status_grid[LENGTH_BOARD + 1][LENGTH_BOARD + 1][3];// Updated to 3D array for RGB values



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

/*

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

*/


/* **********************************************************************************
 * **********************************************************************************
 *  // preprocess for Hall sensor and move detection process
 * **********************************************************************************
 * **********************************************************************************
*/
#define QUEUE_SIZE 6
#define LINE_PIECE_NUMBER (9)
#define PIECES_NUMBER (LINE_PIECE_NUMBER * LINE_PIECE_NUMBER)
#define BOARDS_NUMBER (10)
#define BUF_SIZE ((PIECES_NUMBER + 1)*BOARDS_NUMBER)
#define DECIMAL (10)


//queue setting
#define MAX_NUM (10 + 1)


// Mux in "SIG" pin
#define SIG_PIN 15

// Define variables to hold the data for each shift register
// starting with non-zero numbers can help troubleshoot
byte sensorVal[3] = {72, 159, 237}; // 01001000, 10011111, 11101101


// Define 2D array to store last 5 input values for each bit
const int NUM_BYTES = 3 * 9;
const int NUM_SAMPLES = 5;
byte input_buffer[NUM_SAMPLES][NUM_BYTES];
int buffer_bit_index = 0;
int buffer_cir_index = 0;

// AVG sensor ral values

int board[81];


// global sfen String for sending to server
String sfen;

int is_ready_analize_move = 0;
// board_history : [0] ... last certain status, [1] ... current cretain status
int board_historys[2][PIECES_NUMBER];
int cir_arrays[2][PIECES_NUMBER];

// for debug !! *****************************************************************************************************
String init_distribution = "000000000000000000000000000000000000000000000000000000000000000000000000000000000";
int isBleConnectionOn = false;
bool isMeBlack = true;
String processSt = "preparingStart";

//**********************************************************
// for ble move confirm process
    String bleConfirmOrderSt = "waitingAfterSent";
    int confirmResult; //(-1: not yet, 1: true, 0 : false)
    String opponentMovedSfen;
//**********************************************************

//*********************************************************
// for old sub esp32 process (now marged into one)
#define QUEUE_SIZE 3
#define IS_USE_MP3 true

// GPIO for digital_in & clk for LoadCell
#define DT_PIN 17
#define SCK_PIN 12

//GPIO for turn indication led
#define TRUN_BLACK_PIN (26)
#define TRUN_WHITE_PIN (25)

String SlaveESP32Serial_imagine_str;
bool isReadyToIdentifyFlag = false;

HardwareSerial mySerial(2);
DFRobotDFPlayerMini myDFPlayer;
HX711 scale;
//************************************************************


int turn = 0;
String currentTurn = "black";




class Multiplexer {
  public:
    Multiplexer(int s0, int s1, int s2, int s3) :
      S0(s0),
      S1(s1),
      S2(s2),
      S3(s3) {}

    void setPinMode() {
      // Set up the CD74HC4067 MUX
      pinMode(S0, OUTPUT);
      pinMode(S1, OUTPUT);
      pinMode(S2, OUTPUT);
      pinMode(S3, OUTPUT);
      digitalWrite(S0, LOW);
      digitalWrite(S1, LOW);
      digitalWrite(S2, LOW);
      digitalWrite(S3, LOW);
    }

    void selectChannel(int channel) {
      const int controlPin[] = {S0, S1, S2, S3};

      const int muxChannel[9][4] = {
          {0, 0, 0, 0},
          {1, 0, 0, 0},
          {0, 1, 0, 0},
          {1, 1, 0, 0},
          {0, 0, 1, 0},
          {1, 0, 1, 0},
          {0, 1, 1, 0},
          {1, 1, 1, 0},
          {0, 0, 0, 1},
      };

      for (int i = 0; i < 4; ++i) {
        digitalWrite(controlPin[i], muxChannel[channel][i]);
      }
    }

  private:
    const int S0;
    const int S1;
    const int S2;
    const int S3;
};



class ShiftRegister {
  public:
    ShiftRegister(int latchPin, int clockPin) :
      LATCH_PIN(latchPin),
      CLOCK_PIN(clockPin) {}

    byte readData(int dataPin) {
      int i;
      int temp = 0;
      int pinState;

      byte dataIn = 0;

      pinMode(CLOCK_PIN, OUTPUT);
      pinMode(dataPin, INPUT);

      for (i = 5; i >= 0; i--) {
        digitalWrite(CLOCK_PIN, 0);
        delayMicroseconds(20);
        temp = digitalRead(dataPin);
        dataIn |= temp << i;
        digitalWrite(CLOCK_PIN, 1);
        delayMicroseconds(20);
      }

      return dataIn;
    }

    void setPinMode() {
      // Set up the pins for the CD4021 shift register
      pinMode(LATCH_PIN, OUTPUT);
      pinMode(CLOCK_PIN, OUTPUT);
    }    

    void degitalWriteLatchPin(uint8_t St) {
      digitalWrite(LATCH_PIN, St);
    }

    void degitalWriteClockPin(uint8_t St) {
      digitalWrite(CLOCK_PIN, St);
    }

    const int LATCH_PIN;
    const int CLOCK_PIN;
};

ShiftRegister shiftRegister(13, 21);
Multiplexer multiplexer(22, 15, 35, 34);


// create queue type object
QUEUE_T createQueue(); // prototype declear
QUEUE_T queue = createQueue();



/* **********************************************************************************
 * **********************************************************************************
 *  FUNCTIONS FOR LED MANAGE
 * **********************************************************************************
 * **********************************************************************************
*/

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

void turnOnGrids(int status_grid[][LENGTH_BOARD + 1][3], int x_index, int y_index, int r, int g, int b) {
  // This function turns on the 4 grids that form a square at the given x, y index (0 index) coordinates.
  status_grid[y_index][x_index][0] = r;
  status_grid[y_index][x_index][1] = g;
  status_grid[y_index][x_index][2] = b;

  status_grid[y_index][x_index + 1][0] = r;
  status_grid[y_index][x_index + 1][1] = g;
  status_grid[y_index][x_index + 1][2] = b;

  status_grid[y_index + 1][x_index][0] = r;
  status_grid[y_index + 1][x_index][1] = g;
  status_grid[y_index + 1][x_index][2] = b;

  status_grid[y_index + 1][x_index + 1][0] = r;
  status_grid[y_index + 1][x_index + 1][1] = g;
  status_grid[y_index + 1][x_index + 1][2] = b;
}

int gridSwitchJudgeByCellSts(int status_cell[][LENGTH_BOARD], int x_index, int y_index) {
  int result = 0;

  for (int i = y_index - 1; i <= y_index; i++) {
    for (int j = x_index - 1; j <= x_index; j++) {
      if (isInside(j + 1, i + 1) && status_cell[i][j] == 1) {
        result = 1;
      }
    }
  }

  return result;
}

void updateGrids(int status_grid[][LENGTH_BOARD + 1][3], int status_cell[][LENGTH_BOARD], int x_index, int y_index, int r, int g, int b) {
  for (int i = y_index; i <= y_index + 1; i++) {
    for (int j = x_index; j <= x_index + 1; j++) {
      if (gridSwitchJudgeByCellSts(status_cell, j, i) == 1) {
        status_grid[i][j][0] = r;
        status_grid[i][j][1] = g;
        status_grid[i][j][2] = b;
      } else {
        status_grid[i][j][0] = 0;
        status_grid[i][j][1] = 0;
        status_grid[i][j][2] = 0;
      }
    }
  }
}

void reset_whole_status() {
  // reset status_cell with all 0
  for (int i = 0; i < LENGTH_BOARD; i++) {
    for (int j = 0; j < LENGTH_BOARD; j++) {
      status_cell[i][j] = 0;
    }
  }

  // reset status_grid with all 0
  for (int i = 0; i < LENGTH_BOARD + 1; i++) {
    for (int j = 0; j < LENGTH_BOARD + 1; j++) {
      status_grid[i][j][0] = 0;
      status_grid[i][j][1] = 0;
      status_grid[i][j][2] = 0;
    }
  }

  // also reset lcd
  //lcd.clear()
}



uint32_t calculateColor(int r, int g, int b) {
  // Ensure that r, g, b values are either 0 or 1
  r = r > 0 ? 1 : 0;
  g = g > 0 ? 1 : 0;
  b = b > 0 ? 1 : 0;

  int sum = r + g + b;

  if (sum == 0) {
    return pixels.Color(0, 0, 0);
  }

  int color_value = 255 / sum;
  return pixels.Color(r * color_value, g * color_value, b * color_value);
}


void colorLedNumbersForNeoPixel(int status_grid[][LENGTH_BOARD + 1][3], uint32_t  *color_buffer, int *led_buffer, int * return_size) {
  int count = 0;
  int return_index = 0;

  for (int i = 0; i < LENGTH_BOARD + 1; i++) {
    if (i % 2 == 0) {
      for (int j = LENGTH_BOARD; 0 <= j; j--) {
        if (status_grid[j][i][0] != 0 || status_grid[j][i][1] != 0 || status_grid[j][i][2] != 0) {
          color_buffer[return_index] = calculateColor(status_grid[j][i][0], status_grid[j][i][1], status_grid[j][i][2]);
          led_buffer[return_index++] = count;
        }
        count++;
      }
    } else {
      for (int j = 0; j < LENGTH_BOARD + 1; j++) {
        if (status_grid[j][i][0] != 0 || status_grid[j][i][1] != 0 || status_grid[j][i][2] != 0) {
          color_buffer[return_index] = calculateColor(status_grid[j][i][0], status_grid[j][i][1], status_grid[j][i][2]);
          led_buffer[return_index++] = count;
        }
        count++;
      }
    }
  }
  *return_size = return_index;
}


void settingLEDs() {
  if (led_flag == 1)
  {
    // Calculate led_array and color_led_array from the status_grid
    colorLedNumbersForNeoPixel(status_grid, color_led_array, led_array, &size_led);

    // Turn on LEDs based on led_array and color_led_array
    for (int i = 0; i < size_led; i++) {
      pixels.setPixelColor(led_array[i], color_led_array[i]);
    }
    pixels.show();

    // Debug
    for (int i = 0; i < size_led; i++)
    {
      Serial.printf("LED: %d, Color: %08X\n", led_array[i], color_led_array[i]);
    }

    // Turn OFF led_flag
    led_flag = 0;
  }
}

/* **********************************************************************************
 * **********************************************************************************
 *  FUNCTIONS FOR CONNECTOIN (MANAGEMANE) OF BETWEEN BLE AND INDICATION SYSTEM 
 * **********************************************************************************
 * **********************************************************************************
*/

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
    playSfenOrder(currentTurn ,command_parts[1], command_parts[2]);
    MoveInfo move_st = preprocess(command_parts[2], command_parts[1]);
    executeIndication(move_st);


    // goto next processSt  from "waitOpponetTurn" to "detectionOpponent"
    processSt = "detectionOpponent";
    opponentMovedSfen = command_parts[2]; // store into global opponent sfen variable

  } else if (command_parts[0] == "clear") {
    reset_whole_status();

    // turn ON led_flag
    led_flag = 1;
  } else if (command_parts[0] == "confirm" && bleConfirmOrderSt == "waitingAfterSent") {
    // example) message : "confirm result true"
    reset_whole_status();
    confirmResult = command_parts[2] == "true" ? true : false;

    Serial.println("************************************************************************************************************");

    //update status
    bleConfirmOrderSt = "gotConfirmResult";
  } 
  else {
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
}


void moveIndication(int from_x, int from_y, int to_x, int to_y, boolean isPromotion) {
    Serial.println("CHECK POINT 4");

    // turn on from position
    if (isInside(from_x,from_y)) {
        turnOnCell(status_cell, from_x, from_y);
        turnOnGrids(status_grid, from_x, from_y, 0, 0, 1);
    }

    // turn on to position
    if (!isInside(to_x,to_y)) {
      // pass
    } else if (!isPromotion) {
        turnOnCell(status_cell, to_x, to_y);
        turnOnGrids(status_grid, to_x, to_y, 0, 0, 1); // nomal blue
    } else {
        turnOnCell(status_cell, to_x, to_y);
        turnOnGrids(status_grid, to_x, to_y, 1, 0, 0); // promotion red
    }

    // set flag (for truning on leds) on
    led_flag = 1;

}



void dropIndication(int to_x, int to_y, String kind) {
    // turn on to position
    if (isInside(to_x,to_y)) {
        turnOnCell(status_cell, to_x, to_y);
        turnOnGrids(status_grid, to_x, to_y, 0, 1, 0); // drop green
    }

    // set flag (for truning on leds) on
    led_flag = 1;
}

/* ---------------------------------------------------------*/

void reset_whole_selector() {

}

void managePieceSelector(String command, String kind) {

}


/*
class Blink {
  private:
    int blink_delay = 300;

  public:
    Blink(int delay) {
      blink_delay = delay;
    }

    void blink(int *array, int size_array) {
      pixcel.clear();
      delay(10);
      pixcel.show();

      delay(blink_delay);

      pixcel.clear();
      delay(10);
      pixcel.show();
      
      turnOnLEDs(array, array);
      delay(blink_delay);

      pixcel.clear();
      delaY(10);
      pixcel.show();

      delay(blink_delay);

      led_flag = 1;
      settingLEDs();
    }

    void manageBlink(int col, int low) {
      int buf_array[4];
      int buf_status_grid[10][10];

      // Initialize buf_status_grid with 0
      for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
          buf_status_grid[i][j] = 0;
        }
      }

      // Set grids on
      setGridsOn(col, low);

      // Calculate led_array from grid(array) of status
      ledNumbersForNeoPixel(buf_status_grid, buf_array, &size_led);

      // Blink LEDs based on led_array
      blink(buf_array, size_led);
    }



        //turn on leds base on led_array
    void turnOnLEDs(int *array, int size){
      pixels.clear();
      for(int i = 0; i < size; i++){
        pixels.setPixelColor(array[i], pixels.Color(0, 0, 255););
      }
      pixels.show();
    }
};
*/


/* **********************************************************************************
 * **********************************************************************************
 *  FUNCTIONS OF BLE
 * **********************************************************************************
 * **********************************************************************************
*/


// Function to send notifications to the connected BLE device
void sendNotification(const String message) {
    Serial.println("SENDING NOTIFICATION");
    pCharacteristic->setValue(message.c_str());
    pCharacteristic->notify();
}

// Callback class for handling characteristic write events
class MyCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    // Get the value written to the characteristic
    String receivedValue = (pCharacteristic->getValue()).c_str();

    // Parse the decoded string to process the command
    if (receivedValue.length() > 0) {
      Serial.println(receivedValue);
      if (receivedValue == "enable_notifications") {
        sendNotification("hello from ESP32");

        // Update the ble connection flag
        isBleConnectionOn = true;
      }else {
        handleBLECommand(receivedValue);
        Serial.println("Let's parse the receved message from react");
        Serial.print("St : ");Serial.println(bleConfirmOrderSt);
      }
    }
  }

  // Override onStatus to check if notifications were sent successfully
  void onStatus(BLECharacteristic* pCharacteristic, Status s, uint32_t code) {
        // debug
    Serial.print("Status: ");
    Serial.print(s);
    Serial.print(" Error code: ");
    Serial.println(code);
    
    if (s == SUCCESS_NOTIFY) {
      Serial.println("Notification sent successfully");
    } else {
      Serial.println("Failed to send notification");
    }
  }  
};

// Callback class for handling server disconnect events
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
    // Print a message when a client connects
    Serial.println("BLE CONNECTED!");

    delay(1000);

  }
  void onDisconnect(BLEServer* pServer) {
    // Start advertising again when a client disconnects
    pServer->getAdvertising()->start();
    Serial.println("Client disconnected");

    
    // update ble connection flag
    isBleConnectionOn = false;
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
 *  FUNCTIONS FOR MOVE DETECTION BASED ON SENSOR VALS
 * **********************************************************************************
 * **********************************************************************************
*/

//LED function (ARDUINO)
void blink(int low, int col)
{


    //TODO...blink Point(low, col) to tell change detected successfull



}

/////////////////////////////////////////////////////////////////////////////
/*
*  MOVING DETECTION FUNCIONS
*/
/////////////////////////////////////////////////////////////////////////////
// Function to initialize the Queue structure
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


// function to tell user error status 
void errorBlink() {
  pixels.clear();

  for(int i = 0; i < 2; i++) {
    for(int j = 0; j < LENGTH_BOARD + 1; j++) {
      if(i == 0) { status_grid[i][j][0] = 1; }
      else { status_grid[LENGTH_BOARD + 1 - i][j][0] = 1;}
    }
  }


  for(int i = 0; i < 2; i++) {
    for(int j = 0; j < LENGTH_BOARD + 1; j++) {
      if(i == 0) { status_grid[j][i][0] = 1; }
      else { status_grid[j][LENGTH_BOARD + 1- i][0] = 1;}
    }
  }

  
  led_flag = 1;
  settingLEDs();

  delay(20);
  pixels.show();

  delay(500);

  pixels.clear();
  delay(20);
  pixels.show();
};

// function to return beform status of error blinking
void turnOffErrorBlink() {
  pixels.clear();

  for(int i = 0; i < 2; i++) {
    for(int j = 0; j < LENGTH_BOARD + 1; j++) {
      if(i == 0) { status_grid[i][j][0] = 0; }
      else { status_grid[LENGTH_BOARD + 1 - i][j][0] = 0;}
    }
  }

  for(int i = 0; i < 2; i++) {
    for(int j = 0; j < LENGTH_BOARD + 1; j++) {
      if(i == 0) { status_grid[j][i][0] = 0; }
      else { status_grid[j][LENGTH_BOARD + 1- i][0] = 0;}
    }
  }
  
  led_flag = 1;
  settingLEDs();

  delay(20);
  pixels.show();
};

// sound move indication : format "playNomal ${turn} ${kind} ${sfen}"
void playSfenOrder(String turnStr, String kindStr, String sfenStr) {
  delay(100);
  // tell order play kifu sound to slave esp32 to play dfplayer mini
  SlaveESP32Serial_imagine_str = ("******** playNomal " + turnStr + " " + kindStr + " " + sfenStr).c_str();

  if (kindStr == "undifined") {
      delay(300);
      // tell order play kifu sound to slave esp32 to play dfplayer mini
      SlaveESP32Serial_imagine_str = ("******** playNomal " + turnStr + " " + kindStr + " " + sfenStr).c_str();
  }
  isReadyToIdentifyFlag = true;
}

// sound undoError st indication 
void playUndoOrderingSound() {
  // tell error st of undo to slave esp32 to play dfplayer mini
  SlaveESP32Serial_imagine_str = "******** playError undo";
  isReadyToIdentifyFlag = true;
};

// wait for pawn that captured is correctry put on hand desk
void waitPutHand() {
  SlaveESP32Serial_imagine_str = "******** playError putHand";
  isReadyToIdentifyFlag = true;
  delay(10);
};


void waitUndoHand() {
  SlaveESP32Serial_imagine_str = "******** playError undoHand";
  isReadyToIdentifyFlag = true;
  delay(10);
}

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

///******************************************************
// this part was another esp slave code, we margemaster and slave into one, so here instaed of utrx but flag communication
// parse the UTRX command from master and handle them
void handleUTRXCommand_imagine(String command) {
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
void checkSerial_imagine() {
  // set the flag
  isReadyToIdentifyFlag = true;
}


// tell user to undo becouse of error status of board (wrong place of cells)
void waitUndo() {
    Serial.println("Waiting Undo");

    // play sound of telling undo st
    playUndoOrderingSound();

    // waiting untill undoed
    while (!isSameWithFirstStatus()) {
      Serial.println("...");
      // tell error to make user undo
      errorBlink();
      delay(400);
    }

    turnOffErrorBlink();

    Serial.println("Aleady been Undo!");

    // set detecting st
    if (isMeBlack && currentTurn == "black" || !isMeBlack && currentTurn == "white") {
      processSt = "detectionMine";
    }
    else {
      processSt = "detectionOpponent";
    }
}


// function that return 1/0 (undoed or not) : get board st and , check current board st is same to the first one at the first of the turn ()
int isSameWithFirstStatus() {
  int isSameFlag = 1;
  int buffer[81];

  // get current sensor valuse status 
  getBoardSensors(buffer);  
  
  // check (trun off the same flag) if the current st is not same with last certain board status
  for(int i = 0; i < PIECES_NUMBER; i++) {
    Serial.print("DEBUG: buffer, board_historys"); Serial.print(buffer[i]);Serial.print(","), Serial.println(board_historys[0][i]);
    if (buffer[i] != board_historys[0][i]) isSameFlag = 0;
  }

  if (isSameFlag == 1) {
    Serial.println("CHECK OK * BLEACK UNDO LOOP");
  } else {
    Serial.println("CHECK NO * KEEP UNDO LOOP");
  }

  return isSameFlag;
}

//search for each grid of board and detect change
void detectChange(int *now, int *last, QUEUE_T *queue, int * is_ready_analize_move, int * returnCertainState) 
{
    int buf;
    for(int i=0; i<PIECES_NUMBER; i++) {
        buf = last[i] - now[i];

        //if sub of last and now is 0 (no change is detected), just pass
        if(buf == 0) continue;

        //tell user that here change detected!
        blink(i/LINE_PIECE_NUMBER, i%LINE_PIECE_NUMBER);

        //push index and value into queue 
        QueuePush(queue, i);
        QueuePush(queue, buf);  // (index, differntVal)

        if(QueueBack(queue) == MAX_NUM) {
            printf("ERROR: DETECT TOO MUCH CHANGE FROM SENSORS\n");
            waitUndo();
            initQueue(queue);
        }

        if (buf < 0) {
            *is_ready_analize_move = 1; //set FLAG (for the sequence of moving action) 1(...HIGH)

            //hold this state for if the sequence of move is accepted
            memcpy(returnCertainState ,now, sizeof(int)*PIECES_NUMBER);
        }
    }
}

/*
 *
 *        TODO MARGE WITH LADCELL SYSTEM
 * 
*/

// parse function that like "LoadCell result p" -> "LoadCell","result","p" and size is 3 (return : int)
int parseCommand(String command, String * returnStrs)
{
  int last_space = -1;
  int size = 0;
  for (int i = 0; i < 5; i++) {
    int next_space = command.indexOf(' ', last_space + 1);
    size++;
    if (next_space == -1) {
      returnStrs[i] = command.substring(last_space + 1);
      break;
    } else {
      returnStrs[i] = command.substring(last_space + 1, next_space);
      last_space = next_space;
    }
  }

  return size;
}


//detection of what kind of captured piece 
String detectCapturedChange() 
{
  String buf[3];
  String kind;
  bool isFinished = false;
  int count = 0;

  delay(300);

  Serial.println("loadcell order");
  SlaveESP32Serial.print("******** LoadCell order getChangeSt");

  delay(100);

  // loop while
  while (true && count < 40) {
    // when available
    if (SlaveESP32Serial.available() > 0) {
        // format is like "LoadCell result {detectedKind}"
        if(parseCommand(SlaveESP32Serial.readString(), buf) != 3) {
          // pass (size is not 3 , menaing not above format)
        }
        else {
          kind = buf[2];
          break;
        } 
    }

    count++;

    delay(300);
  }

  if (count >= 40) {
    kind = detectCapturedChange();
  }

  Serial.println("****************************************************");
Serial.print("detected kind : "); Serial.println(kind);
Serial.println("****************************************************");

  Serial.println("loadcell order returned");

  if (kind == "error") { // when more than 2 piece is changed still
    waitPutHand();

    // recursion
    kind = detectCapturedChange();
  }

Serial.println("****************************************************");
Serial.print("detected kind : "); Serial.println(kind);
Serial.println("****************************************************");
  return kind;
}


// return just hand desk has changed or not
bool detectCapturedChangeOpponent() 
{
  bool st = detectCapturedChange() == "null" ? false : true;

  Serial.println("return detectCapturedChangeOpponent");

  return st;
}


//analize of move using captured piece, RETURN STRING SFEN
String useCapturedMine(QUEUE_T * queue)
{
    int low, col;
    int qdata[2];

    String sfenStr;

    for(int i=0; i<2; i++) {
        qdata[i] = QueuePop(queue);
    }

    //promotion directly ... return ERROR
    if(qdata[1] == -2) {
        printf("ERROR: ILLIGAL PROMOTION (use_move)\n");
        sfenStr = "error";// remain


        return sfenStr;
    }

    //add the kind of piece and '*'
    sfenStr = detectCapturedChange();

    //error cheak of nodetectionError at captured
    if(sfenStr == "null") {
        printf("ERROR: NO CAPTURED CHANGE DETECTED\n");
        sfenStr = "error";
        return sfenStr;
    }
    else {
        sfenStr += '*';
    }

    //add position 

    //cal (low, col)
    low = qdata[0]/LINE_PIECE_NUMBER;
    col = qdata[0]%LINE_PIECE_NUMBER;

    //like ... str[2*i] = (char)(col+1)
    sfenStr += (String)(low+1);

    switch (col) {
        case 0: sfenStr += 'a'; break;
        case 1: sfenStr += 'b'; break;
        case 2: sfenStr += 'c'; break;
        case 3: sfenStr += 'd'; break;
        case 4: sfenStr += 'e'; break;
        case 5: sfenStr += 'f'; break;
        case 6: sfenStr += 'g'; break;
        case 7: sfenStr += 'h'; break;
        case 8: sfenStr += 'i'; break;
        default: printf("ERROR: LOW MUST BE IN 0 TO 1 (MOVE)\n");
    }
  
  return sfenStr;
}


//analize of move using captured piece, RETURN STRING SFEN
String useCapturedOpponent(QUEUE_T * queue)
{
    int low, col;
    int qdata[2];

    String sfenStr;

    for(int i=0; i<2; i++) {
        qdata[i] = QueuePop(queue);
    }

    //promotion directly ... return ERROR
    if(qdata[1] == -2) {
        printf("ERROR: ILLIGAL PROMOTION (use_move)\n");
        sfenStr = "error";// remain

        return sfenStr;
    }

    sfenStr = "**";

    /*
    //add the '**'
    if (!detectCapturedChangeOpponent()) {
      sfenStr = "**";
    }
    else {
        printf("ERROR: ILLIGAL USING MINE CAPTURE  m (use_move)\n");
        sfenStr = "error";// remain

        return sfenStr;
    }
    */
    



    //add position 

    //cal (low, col)
    low = qdata[0]/LINE_PIECE_NUMBER;
    col = qdata[0]%LINE_PIECE_NUMBER;

    //like ... str[2*i] = (char)(col+1)
    sfenStr += (String)(low+1);

    switch (col) {
        case 0: sfenStr += 'a'; break;
        case 1: sfenStr += 'b'; break;
        case 2: sfenStr += 'c'; break;
        case 3: sfenStr += 'd'; break;
        case 4: sfenStr += 'e'; break;
        case 5: sfenStr += 'f'; break;
        case 6: sfenStr += 'g'; break;
        case 7: sfenStr += 'h'; break;
        case 8: sfenStr += 'i'; break;
        default: printf("ERROR: LOW MUST BE IN 0 TO 1 (MOVE)\n");
    }
  
  return sfenStr;
}


//analize of most simplest move, RETURN STRING SFEN
String justMove(QUEUE_T * queue)
{
    int buf, low, col;
    int qdata[4];

    String sfenStr;

    //take data out of queue
    for(int i=0; i<4; i++) {
        qdata[i] = QueuePop(queue);
    }
    
    //if there is no change, return remain and clear queue data
    if(qdata[0] == qdata[2]) {
        if(qdata[1] + qdata[3] == 0) {
            sfenStr = "rem";// remain
        } else {
            // illigal promotion
            sfenStr = "error";          
        }

        return sfenStr;
    }
    else if (qdata[1] + qdata[3] > 0) {
      // illigal unpromotion
      return sfenStr = "error";     
    }

    //add position 
    for(int i=0; i<2; i++) {
        //cal (low, col)
        low = qdata[2*i]/LINE_PIECE_NUMBER;
        col = qdata[2*i]%LINE_PIECE_NUMBER;

        //like ... 3 -> a -> 6 -> b
        sfenStr += String(low+1);

        switch (col) {
            case 0: sfenStr += 'a'; break;
            case 1: sfenStr += 'b'; break;
            case 2: sfenStr += 'c'; break;
            case 3: sfenStr += 'd'; break;
            case 4: sfenStr += 'e'; break;
            case 5: sfenStr += 'f'; break;
            case 6: sfenStr += 'g'; break;
            case 7: sfenStr += 'h'; break;
            case 8: sfenStr += 'i'; break;
            default: printf("ERROR: LOW MUST BE IN 0 TO 1 (MOVE)\n");
        }

    }

    //poromotion when (-2 = 0 - 2)
    if(qdata[3] == -2 && qdata[1] != 2) {
        sfenStr += '+'; 
    }
    
    return sfenStr;
}

//analize of moving with capturing, RETURN SFEN STRING
String captureMoveMine(QUEUE_T *queue) 
{
    //index[from, to]
    int index[2];

    int low, col;
    int qdata[6];

    String sfenStr;
    String kind;

    for(int i=0; i<6; i++) {
        qdata[i] = QueuePop(queue);
    }

    /* ??? */
    //cheak if it is legal or not and set from/to index of data in queue
    if(qdata[0] == qdata[2]) {
        //illegal promotion
        printf("ERROR: ILLEGAL PROMOTION\n"); 
        sfenStr = "error"; 
        return sfenStr;
    }
    else if(qdata[0] == qdata[4]) {
        //legal move possibility
        index[0] = 2;
        index[1]   = 4;
    }
    else if(qdata[2] == qdata[4]) {
        //legal move possibility
        index[0] = 0;
        index[1]   = 4;
    }
    else {
        //perfect illegal:
        printf("ERROR: COMPLITELY ILLIGAL MOVE\n"); 
        sfenStr = "error"; 
        
        return sfenStr;
    }
    
    //here legal cheak by ovsering captured
    kind = detectCapturedChange();

    //return only when illegal
    if(kind == "null") {
        //no change is detected at captured ... error
        printf("ERROR: no change detected ... still not pun on pawn on hand desk");

        while (kind == "null") {
          waitPutHand();

          // update kind of captured piece
          kind = detectCapturedChange();
        }
    }
    
    //add position 
    for(int i=0; i<2; i++) {
        //cal (low, col)
        low = qdata[index[i]]/LINE_PIECE_NUMBER;
        col = qdata[index[i]]%LINE_PIECE_NUMBER;

        //like ... 3 -> a -> 6 -> b
        sfenStr += String(low+1);

        switch (col) {
            case 0: sfenStr += 'a'; break;
            case 1: sfenStr += 'b'; break;
            case 2: sfenStr += 'c'; break;
            case 3: sfenStr += 'd'; break;
            case 4: sfenStr += 'e'; break;
            case 5: sfenStr += 'f'; break;
            case 6: sfenStr += 'g'; break;
            case 7: sfenStr += 'h'; break;
            case 8: sfenStr += 'i'; break;
            default: printf("ERROR: LOW MUST BE IN 0 TO 1 (MOVE)\n");
        }
    }

    //poromotion when position_to value is -2 (-2 = 0 - 2)
    if(qdata[index[1]+1] == -2) {
        sfenStr += '+'; 
    }


    return sfenStr;
}

String captureMoveOpponent(QUEUE_T *queue) 
{
    //index[from, to]
    int index[2];

    int low, col;
    int qdata[6];

    String sfenStr;
    String kind;

    for(int i=0; i<6; i++) {
        qdata[i] = QueuePop(queue);
    }

    /* ??? */
    //cheak if it is legal or not and set from/to index of data in queue
    if(qdata[0] == qdata[2]) {
        //illegal promotion
        printf("ERROR: ILLEGAL PROMOTION\n"); 
        sfenStr = "error"; 
        return sfenStr;
    }
    else if(qdata[0] == qdata[4]) {
        //legal move possibility
        index[0] = 2;
        index[1]   = 4;
    }
    else if(qdata[2] == qdata[4]) {
        //legal move possibility
        index[0] = 0;
        index[1]   = 4;
    }
    else {
        //perfect illegal:
        printf("ERROR: COMPLITELY ILLIGAL MOVE\n"); 
        sfenStr = "error"; 
        
        return sfenStr;
    }
    
    //here legal cheak by ovsering captured
    delay(1000);
    kind = detectCapturedChange();

    //return only when illegal
    if(kind != "null") {
        //no change is detected at captured ... error
        printf("ERROR: illigal change detected ");
        while (kind != "null") {
          waitUndoHand();

          // update kind of captured piece
          kind = detectCapturedChange();
        }
    }
    
    //add position 
    for(int i=0; i<2; i++) {
        //cal (low, col)
        low = qdata[index[i]]/LINE_PIECE_NUMBER;
        col = qdata[index[i]]%LINE_PIECE_NUMBER;

        //like ... 3 -> a -> 6 -> b
        sfenStr += String(low+1);

        switch (col) {
            case 0: sfenStr += 'a'; break;
            case 1: sfenStr += 'b'; break;
            case 2: sfenStr += 'c'; break;
            case 3: sfenStr += 'd'; break;
            case 4: sfenStr += 'e'; break;
            case 5: sfenStr += 'f'; break;
            case 6: sfenStr += 'g'; break;
            case 7: sfenStr += 'h'; break;
            case 8: sfenStr += 'i'; break;
            default: printf("ERROR: LOW MUST BE IN 0 TO 1 (MOVE)\n");
        }
    }

    //poromotion when position_to value is -2 (-2 = 0 - 2)
    if(qdata[index[1]+1] == -2) {
        sfenStr += '+'; 
    }


    return sfenStr;
}

// Analize move and return the SFEN data
String analizeMoveMine(QUEUE_T *queue)
{
    String sfenStr;
    // Error
    if(QueueBack(queue) <= 0) {
        Serial.println("ERROR: QUEUE IS NULL,SO CANNOT ANALIZE");
        return "error";
    }
    
    switch(QueueBack(queue)) {
        case 2: sfenStr = useCapturedMine(queue); Serial.print("(use) "); break;
        case 4: sfenStr = justMove(queue); Serial.print("(just) "); break;
        case 6: sfenStr = captureMoveMine(queue); Serial.print("(cap) "); break;
        default: sfenStr = "error";
    }

    return sfenStr;
}

String analizeMoveOpponent(QUEUE_T *queue)
{
    String sfenStr;
    // Error
    if(QueueBack(queue) <= 0) {
        Serial.println("ERROR: QUEUE IS NULL,SO CANNOT ANALIZE");
        return "error";
    }
    
    switch(QueueBack(queue)) {
        case 2: sfenStr = useCapturedOpponent(queue); Serial.print("(use) "); break;
        case 4: sfenStr = justMove(queue); Serial.print("(just) "); break;
        case 6: sfenStr = captureMoveOpponent(queue); Serial.print("(cap) "); break;
        default: sfenStr = "error";
    }

    Serial.println("HERE 1");
    Serial.print("sfenStr : ");Serial.println(sfenStr);

    return sfenStr;
}

// Switching array buffer and do main process of moving detect
// esspecially for my turn !
void mainMineMoveDetection(int *pturn, int cir_arrays[][PIECES_NUMBER], QUEUE_T *queue)
{
    // Use this pointer as main array in this turn
    int *pnow, *plast;
    String sfenStr = "";

    switch(*pturn){
        case 0: pnow = cir_arrays[0]; plast = cir_arrays[1]; break;
        case 1: pnow = cir_arrays[1]; plast = cir_arrays[0]; break;
        default: Serial.println("ERROR: TURN MUST BE 0 OR 1"); return;
    }

    // Read board sensors status
    getBoardSensors(pnow);

/*    
    Serial.println("pnow :");
    for (int i = 0; i < 81; i++) {
      Serial.print(pnow[i]);
      if(i % 9 == 0) {
        Serial.println("|");
      }
    }
    Serial.println("---------------------------");
    Serial.println("plast :");
    for (int i = 0; i < 81; i++) {
      Serial.print(plast[i]);
      if(i % 9 == 0) {
        Serial.println("|");
      }
    }
    Serial.println("---------------------------"); 

    */
    // Calculate difference, if find endpoint set is_ready_analize_move HIGH
    detectChange(pnow, plast, queue, &is_ready_analize_move, board_historys[1]);

    // If is_ready_analize_move is HIGH, do analize process
    if(is_ready_analize_move == 1) {
        sfenStr = analizeMoveMine(queue);

        if(sfenStr == "error") {
            Serial.println("detected ERROR");
            waitUndo();

            // reset detection status
            is_ready_analize_move = 0;
            memcpy(cir_arrays[0], board_historys[0], sizeof(int)*PIECES_NUMBER);
            memcpy(cir_arrays[1], board_historys[0], sizeof(int)*PIECES_NUMBER);
            initQueue(queue);
        }
        else if(sfenStr == "rem") {
          // pass
        }
        else {
          
          is_ready_analize_move = 0;
          sfen = sfenStr;
          processSt = "waitConfirmMine";
          bleConfirmOrderSt = "notSendYet";
          Serial.println("GO TO waitConfirm Mine");          
          Serial.println(sfen);
          initQueue(queue);
        }
    }
    else {
        Serial.println("...");
    }


    *pturn = 1 - *pturn;
}

bool isLastThreeCharsSameWithStar(String str1, String str2) {
  int len1 = str1.length();
  int len2 = str2.length();

  if (len1 < 3 || len2 < 3) {
    // Return false if either string is less than three characters long
    return false;
  }

  if (str1.charAt(len1-1) != str2.charAt(len2-1)) {
    // Return false if the last character of each string does not match
    return false;
  }

  if (str1.charAt(len1-2) != str2.charAt(len2-2)) {
    // Return false if the second-to-last character of each string does not match
    return false;
  }

  if (str1.charAt(len1-3) != str2.charAt(len2-3)) {
    // Return false if the third-to-last character of each string does not match
    return false;
  }

  if (str1.indexOf('*') == -1 || str2.indexOf('*') == -1) {
    // Return false if either string does not contain the character '*'
    return false;
  }

  // Return true if all conditions are met
  return true;
}


void mainOpponentMoveDetection(int *pturn, int cir_arrays[][PIECES_NUMBER], QUEUE_T *queue) {
// Use this pointer as main array in this turn
    int *pnow, *plast;
    String sfenStr = "";
    bool isMineHnadDeskChanged;

    switch(*pturn){
        case 0: pnow = cir_arrays[0]; plast = cir_arrays[1]; break;
        case 1: pnow = cir_arrays[1]; plast = cir_arrays[0]; break;
        default: Serial.println("ERROR: TURN MUST BE 0 OR 1"); return;
    }

    // Read board sensors status
    getBoardSensors(pnow);


    /*
    Serial.println("pnow :");
    for (int i = 0; i < 81; i++) {
      Serial.print(pnow[i]);
      if(i % 9 == 0) {
        Serial.println("|");
      }
    }
    Serial.println("---------------------------");
    Serial.println("plast :");
    for (int i = 0; i < 81; i++) {
      Serial.print(plast[i]);
      if(i % 9 == 0) {
        Serial.println("|");
      }
    }
    Serial.println("---------------------------"); 
  */

    // Calculate difference, if find endpoint set is_ready_analize_move HIGH
    detectChange(pnow, plast, queue, &is_ready_analize_move, board_historys[1]);

    // If is_ready_analize_move is HIGH, do analize process
    if(is_ready_analize_move == 1) {
        Serial.println("HERE 0");      
        sfenStr = analizeMoveOpponent(queue);

        Serial.println("HERE 3");
        isMineHnadDeskChanged = detectCapturedChangeOpponent();


        Serial.println("sfenStr and opponentMovedSfen  :");Serial.print(sfenStr);Serial.print(",");Serial.println(opponentMovedSfen);
        if(sfenStr == "error" || isMineHnadDeskChanged) {
            Serial.println("detected ERROR");
            waitUndo();

            // reset detection status
            is_ready_analize_move = 0;
            memcpy(cir_arrays[0], board_historys[0], sizeof(int)*PIECES_NUMBER);
            memcpy(cir_arrays[1], board_historys[0], sizeof(int)*PIECES_NUMBER);
            initQueue(queue);
        }
        else if(sfenStr == "rem") {
          // pass
        }
        // 1. same like 2b3b and 2b3b, 2. usecapture & same distination like p*3d and **3d
        else if (sfenStr == opponentMovedSfen || isLastThreeCharsSameWithStar(sfenStr,opponentMovedSfen)){
          is_ready_analize_move = 0;
          processSt = "waitConfirmOpponent";
          bleConfirmOrderSt = "notSendYet";
          Serial.println("GO TO waitConfirm Opponent");      
          //memcpy(board_historys[0], board_historys[1], sizeof(int)*PIECES_NUMBER);    
          initQueue(queue);
        }
    }
    else {
        Serial.println("...");
    }


    *pturn = 1 - *pturn;
}

// Hold new state of board (copy newCertainState into preCertainState :: update pre st)
void holdCertainState(int *newCertainState, int *preCertainState) {
    //
    memcpy(preCertainState, newCertainState, sizeof(int)*PIECES_NUMBER);

    Serial.print("CONFIRMED STATE IS : ");
    for(int i=0; i<PIECES_NUMBER; i++) {
        Serial.print(preCertainState[i]);
        Serial.print(",");
    }
    Serial.println();
}


//get sensors status
void getBoardSensors(int  * sensors_status)
{
    byte sensorStVals[27];

    // get raw sensor status
    readSensorValues(sensorStVals);

    /*
    Serial.println("sensorStVals");

    for(int i = 0; i < 27; i++ ){
      if(i % 3 == 0) Serial.println("|");
      Serial.print(sensorStVals[i]);
    }

    Serial.println("------------------------");
    */
    // tranform into piece status of current board
    processSensorData(sensorStVals, sensors_status);
}


/* **********************************************************************************
 * **********************************************************************************
 *  FUNCTIONS FOR SENSOR OVSERBATION FUNCTION (connect via getBoardSensors function)
 * **********************************************************************************
 * **********************************************************************************
*/

byte get_output_byte(int byte_index) {
  int bitSums[6] = {0, 0, 0, 0, 0, 0};
  for (int row = 0; row < 5; row++) {
    byte currentByte = input_buffer[row][byte_index];
    for (int bit = 0; bit < 6; bit++) {
      bitSums[bit] += (currentByte >> bit) & 1;
    }
  }

  byte resultByte = 0;
  for (int bit = 0; bit < 6; bit++) {
    int bitValue = (bitSums[bit] > 2) ? 1 : 0;
    resultByte |= bitValue << bit;
  }

  return resultByte;
}

void readSensorColByMux(int channel) {
  multiplexer.selectChannel(channel);
  delayMicroseconds(20);

  byte Buf_sensorVal[3];
  int count = 0;
  byte mask = 3;

  do {
    for (int i = 0; i < 3; i++) {
      Buf_sensorVal[i] = shiftRegister.readData(SIG_PIN);
      delayMicroseconds(20);
    }
    count++;
  } while (Buf_sensorVal[0] * Buf_sensorVal[1] * Buf_sensorVal[2] == 0 && count < 4);

  for (int j = 0; j < 3; j++) {
    for (int i = 0; i < 5; i += 2) {
      if (((Buf_sensorVal[j] >> i) & mask) == 0) {
        Buf_sensorVal[j] |= (mask << i);
      }
    }
  }

  for (int i = 0; i < 3; i++) {
    input_buffer[buffer_cir_index][buffer_bit_index++] = Buf_sensorVal[i];
  }
}


// Read the sensor value for all channels
void readSensorValues(byte *sensorStVals){


  for (int i = 0; i < NUM_SAMPLES; i++) {
  // Read the sensor values for each channel
  for (int j = 0; j < 9; j++) {
      //set it to 1 to collect parallel data, wait
      digitalWrite(shiftRegister.LATCH_PIN,1);
      delayMicroseconds(20);

      //set it to 0 to transmit data serially
      digitalWrite(shiftRegister.LATCH_PIN,0);
      delayMicroseconds(20);
      
      readSensorColByMux(j);
    }
    // Increment the buffer index (and wrap around if necessary)
    buffer_cir_index++;
    buffer_cir_index %= NUM_SAMPLES;
    buffer_bit_index = 0;

      delayMicroseconds(20);
  }

  
  //Serial.println("get_output_byte");
  for (int i = 0; i < 9; i++) {
    for (int j = 0; j < 3; j++) {
      sensorStVals[i * 3 + j] = get_output_byte(i * 3 + j);
      //Serial.print(sensorStVals[i * 3 + j]) ;    
    }
    //Serial.println("|");
  }
  //Serial.println("---------------------------------");
  
}


/*
The code snippet requires the location of the data and clock pins and returns a byte. 
Each bit in the byte corresponds to a pin on the shift register, 
with the leftmost bit (bit 6) representing pin 7 and the rightmost bit (bit 0) representing pin 0.
*/
byte myShiftIn(int myDataPin, int myClockPin) {

  int i;
  int temp = 0;
  int pinState;

  byte myDataIn = 0;

  pinMode(myClockPin, OUTPUT);
  pinMode(myDataPin, INPUT);

  /*
  The code snippet controls a shift register by setting the clock pin high 8 times in a loop. 
  This triggers the register to change the state of its data pin according to the next bit in the sequence. 
  The register transmits information from pin 7 to pin 0, which is why the loop counts down.
  */

  for (i=5; i>=0; i--)

  {
    digitalWrite(myClockPin, 0);
    delayMicroseconds(20);
    temp = digitalRead(myDataPin);

    //Serial.print(temp);

    if (temp) {
      pinState = 1;

      //set the bit to 0 no matter what
      myDataIn = myDataIn | (1 << i);
    }

    else {
      //turn it off -- only necessary for debugging: print statement since myDataIn starts as 0
      pinState = 0;
    }
    digitalWrite(myClockPin, 1);
    delayMicroseconds(20);
  }

  // there is no possibilty of  0000000 excep error
  //if (myDataIn == 0) myDataIn = 63;
  return myDataIn;
}


void processSensorData(byte *data, int *board) {
  int dataIndex = 0;
  for (int i = 0; i < 27; i++) {
    byte currentByte = data[i];
    for (int j = 0; j < 3; j++) {
      byte segment = (currentByte >> (4 - j * 2)) & 0b11;
      int status = 0;
      if (segment == 0b10) {
        status = 1;
      } else if (segment == 0b01) {
        status = 2;
      }
      board[dataIndex] = status;
      dataIndex++;
    }
  }
}


/* **********************************************************************************
 * **********************************************************************************
 *  FUNCTIONS FOR BLE CONNECTION FUNCTION WHILE DETECTION
 * **********************************************************************************
 * **********************************************************************************
*/

int orderConfirmViaBle(String sfenStr) {
  int result = -1;

  if (bleConfirmOrderSt == "notSendYet") {
    sendNotification("sfen " + sfenStr);
    bleConfirmOrderSt = "waitingAfterSent";
  }
  else if (bleConfirmOrderSt == "waitingAfterSent") {
    // wait status change (changed by callback function of BLE in)
    Serial.println("...");
  }
  else if (bleConfirmOrderSt == "gotConfirmResult") {
    result = confirmResult; //(true or false)
    bleConfirmOrderSt = "notSendYet";
  }

  Serial.print("result :"); Serial.println(result); 
  return result;
}


/* **********************************************************************************
 * **********************************************************************************
 *  FUNCTIONS FOR MIAN LOOP
 * **********************************************************************************
 * **********************************************************************************
*/


// init matrix
void initLedMatrix(int status_cell[][LENGTH_BOARD], int status_grid[][LENGTH_BOARD+1][3]) {
  for(int i = 0; i < LENGTH_BOARD; i++) {
    for(int j = 0; j < LENGTH_BOARD; j++) {
      status_cell[i][j] = 0;
    }
  }

  for(int k = 0; k < 3; k++) {
    for(int i = 0; i < LENGTH_BOARD + 1; i++) {
      for(int j = 0; j < LENGTH_BOARD + 1; j++) {
        status_grid[i][j][k] = 0;
      }
    }
  }
}

// initalize seting function , like get inti info from web client and set propery as hardware
void initSettingSome() {
      /*
      *@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
      *@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
      *   TODO --- ADD INIT SETTING WITH WEB CLIENT
      *@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
      *@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
      */
      // ble connected action as init
      for(int i=0; i<LED_COUNT; i++) {
        
        pixels.setPixelColor(i, pixels.Color(0, 0, 64)); // blue
        pixels.show();
        delay(10);
      }


      Serial.println("1. INIT NOW");
      getBoardSensors(cir_arrays[0]);
      getBoardSensors(cir_arrays[1]);
      holdCertainState(cir_arrays[0], board_historys[0]);

      // send Slave to change turn
      SlaveESP32Serial.print(("******** set turn " + currentTurn).c_str());

      delay(1000);
      pixels.clear();
      pixels.show();

}

// function for change hardware turn value (not neccesary to async with web client)
void changeCurrentTurn(bool isPlay) {
  String isPlayStr = isPlay ? "true ": "false ";
  if(currentTurn == "black") {
    currentTurn = "white";
  } else {
    currentTurn = "black";
  }

  // send Slave to change turn
  SlaveESP32Serial.print(("******** set " + isPlayStr+ currentTurn).c_str());
}

void setup() {
    // ---------start Serial--------------
    Serial.begin(115200);
    delay(100);
    Serial.println("Serial Start!");

    
    delay(100);
    SlaveESP32Serial_imagine_str = "******** Hello from master!";
    isReadyToIdentifyFlag = true;
  

  //---------set pixel (serial_led by Adafruit_NeoPixel)-----------------
  initLedMatrix(status_cell, status_grid);
  pixels.begin();
  pixels.clear();
  delay(100);
  pixels.show();


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

    //------------set BLE-------------------------
    // Initialize the serial connection and BLE
  BLEDevice::init(deviceName);


  // Create a server and set the callback for disconnect events
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

    // Create a service and a characteristic, and set the callbacks for the characteristic
  pService = pServer->createService(serviceUuid);
  pCharacteristic = pService->createCharacteristic(
    charUuid,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_WRITE|
    BLECharacteristic::PROPERTY_NOTIFY
  );

    // Set a default descriptor for the client configuration
  BLE2902 *p2902Descriptor = new BLE2902();
  pCharacteristic->addDescriptor(p2902Descriptor);

  pCharacteristic->setCallbacks(new MyCallbacks());

  // Start the service and advertising
  pService->start();
  pServer->getAdvertising()->start();
  Serial.println("Waiting for a client connection to the device...");

  // ----------------- set init sensor Sys --------------------
    shiftRegister.setPinMode();
    multiplexer.setPinMode();

    initQueue(&queue);

    Serial.println("FIN SETUP");
}


  int waitCount = 0;

void loop() {
  int result = -1;

  settingLEDs();
    
    // when preparetion and get to connect ble, init cir_array based on piece distribution from BLE
    if (processSt == "preparingStart" && isBleConnectionOn) {

      initSettingSome();

      processSt = "detectionMine";
    }
    if (processSt == "detectionMine") {
      Serial.println("2. MINE DETECT");
      mainMineMoveDetection(&turn, cir_arrays, &queue);
      
    }
    else if (processSt == "waitConfirmMine"){
      Serial.println("NOW ON, WAIT CONFIRM !");
      result = orderConfirmViaBle(sfen);

      Serial.print("RESULT IS :");  Serial.println(result);

      if (result == -1) {
        // wait ... pass
        waitCount++;

        if(waitCount == 300) {
          bleConfirmOrderSt == "notSendYet";
          waitCount = 0;
        }
      }
      else if (result == 0) {
        // illigal status
        waitUndo();
        memcpy(cir_arrays[0], board_historys[0], sizeof(int)*PIECES_NUMBER);
        memcpy(cir_arrays[1], board_historys[0], sizeof(int)*PIECES_NUMBER);
        initQueue(&queue);

        waitCount = 0;
      }
      else if (result == 1) {   // result == 1         
        holdCertainState(board_historys[1], board_historys[0]);
        Serial.println("GOTO waitOpponetTurn !");
        processSt = "waitOpponentTurn";

        //change turn
        changeCurrentTurn(false);

        waitCount = 0;
      }
    }
    else if (processSt == "waitOpponetTurn") {
      // waiting ble 
    }
    else if (processSt == "detectionOpponent") {
        Serial.println("3. OPPONENT DETECT");
        mainOpponentMoveDetection(&turn, cir_arrays, &queue);
    }
    else if (processSt == "waitConfirmOpponent"){
        Serial.println("Waiting Confirm!");
        Serial.println("...");

        // change turn an dprocess status
        processSt = "detectionMine";
        changeCurrentTurn(true);

        pixels.clear();
        delay(100);
        pixels.show();

        holdCertainState(board_historys[1], board_historys[0]);

    };
    
      // push new observed value
  q.push(scale.get_units(10));

  scale.power_down();
  delay(40);
  scale.power_up();



  if (isReadyToIdentifyFlag) {
    if (MasterESPSerial.available()) { // Check if there's data available from ESP_1
      String receivedMessage = SlaveESP32Serial_imagine_str; // Read the incoming message
      Serial.println("Received from ESP_1: " + receivedMessage); // Print the received message on Serial Monitor
      handleUTRXCommand_imagine(receivedMessage);
    }

    isReadyToIdentifyFlag = false;
  }

}
