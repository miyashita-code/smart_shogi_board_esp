#include <Arduino.h>

#include <stdio.h>
#include <string.h>
#include <Adafruit_NeoPixel.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <DFRobotDFPlayerMini.h>
#include <Arduino.h>

/* **********************************************************************************
 * **********************************************************************************
 *  // preprocess for BLE and serial LED and DFPlayerMini
 * **********************************************************************************
 * **********************************************************************************
*/
#define LENGTH_BOARD (9) //setting of LED_grid
#define DIN_PIN   (21)  // digital Pin for serial LED
#define LED_COUNT (100) // the amout of LED

#define TRUN_BLACK_PIN (26)
#define TRUN_WHITE_PIN (25)

// for DEBUG
#define IS_USE_MP3 false

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

// init with "detectionMine"
//String processSt = "detectionMine";

// for debug !! *****************************************************************************************************
String init_distribution = "000000000000000000000000000000000000000000000000000000000000000000000000000000000";
int isBleConnectionOn = false;
String processSt = "preparingStart";
// ******************************************************************************************************************

//**********************************************************
// for ble move confirm process
    String bleConfirmOrderSt = "waitingAfterSent";
    int confirmResult; //(-1: not yet, 1: true, 0 : false)
    String opponentMovedSfen;
//**********************************************************

int turn = 0;



typedef struct Queue {
    int data[QUEUE_SIZE];
    int head;
    int tail;
} QUEUE_T;



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

ShiftRegister shiftRegister(23, 22);
Multiplexer multiplexer(18, 5, 4, 2);


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
    MoveInfo move_st = preprocess(command_parts[2], command_parts[1]);
    executeIndication(move_st);

    // set trun light
    digitalWrite(TRUN_BLACK_PIN, LOW);
    digitalWrite(TRUN_WHITE_PIN, HIGH);

    // goto next processSt  from "waitOpponetTurn" to "detectionOpponent"
    processSt = "detectionOpponent";
    opponentMovedSfen = command_parts[2]; // store into global opponent sfen variable

  } else if (command_parts[0] == "clear") {
    reset_whole_status();

    // set trun light
    digitalWrite(TRUN_BLACK_PIN, HIGH);
    digitalWrite(TRUN_WHITE_PIN, LOW);
    
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
        turnOnCell(status_cell, to_x-1, to_y-1);
        turnOnGrids(status_grid, to_x-1, to_y-1, 0, 1, 0); // drop green
    }

    // set flag (for truning on leds) on
    led_flag = 1;
}

/* ---------------------------------------------------------*/

void reset_whole_selector() {

}

void managePieceSelector(String command, String kind) {

}


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
QUEUE_T createQueue() {
  QUEUE_T q;
  q.head = 0;
  q.tail = 0;
  return q;
}


//function initialize queue
void initQueue(QUEUE_T *queue)
{
    //make queue null
    queue->head = 0;
    queue->tail = -1;
}


//utility of queue
void QueuePush(QUEUE_T *queue, int input)
{

  Serial.print("QUEUE INPUT :");
  Serial.println(input);

    //if queue is full do, just pass
    if((queue->tail + 2) % MAX_NUM == queue->head){
        printf("EROOR QUEUE IS FULL, CANNOT PUSH\n");
        return;
    }

    //add input to last of queue
    queue->data[(queue->tail + 1) % MAX_NUM] = input;

    //slide tail next
    queue->tail = (queue->tail + 1) % MAX_NUM;
}


//utility of queue (when null return -1)
int QueuePop(QUEUE_T *queue)
{
    int value;

    //queue is null, just pass
    if((queue->tail + 1) % MAX_NUM == queue->head){
        printf("EROOR: QUEUE IS NULL, CANNOT POP\n");
        return -1;
    }

    //get value from top of queue
    value = queue->data[queue->head];

    //slide top next
    queue->head = (queue->head + 1) % MAX_NUM;

    return value;
}

//utility of queue, return size of active data of queue
int QueueBack(QUEUE_T *queue)
{
    int size;

    if (queue -> tail < queue->head) {
        size = queue->tail + MAX_NUM - queue->head + 1;
    }
    else {
        size = queue->tail - queue->head + 1;
    }

    return (size);
}


//utility of queue, showing queue
void printQueue(QUEUE_T *queue)
{
    int size;

    size = QueueBack(queue);

    printf("size is : %d\n", size);
    printf("order of FIFO: ");

    for(int i = 0; i < size; i++) {
        printf("%d,", queue->data[(queue->head + i) % MAX_NUM]);
    }
    printf("\n");
}

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

void playUndoOrderingSound() {
  // -- TODO --
};

// wait for pawn that captured is correctry put on hand desk
void waitPutHand() {
  // --TODO --
};



// tell user to undo becouse of error status of board (wrong place of cells)
void waitUndo() {
    Serial.println("Waiting Undo");
    while (!isSameWithFirstStatus()) {
      Serial.println("...");
      // tell error to make user undo
      errorBlink();
      playUndoOrderingSound();
      delay(400);
    }

    Serial.println("Aleady been Undo!");

    // set detecting st
    processSt = "detectionMine";
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

//detection of what kind of captured piece 
String detectCapturedChange() 
{
  String kind = "n";


    //--- TODO(pass) ---


    //when nothing detected return buf = 'X';

    //just demo
    return kind;
}

//detection of what kind of captured piece 
String detectCapturedChangeOpponentDemo() 
{
  String st = "nothing";


    //--- TODO(pass) ---


    //when nothing detected return buf = 'X';

    //just demo
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
    if(sfenStr[0] == 'X') {
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

    //add the '**'
    sfenStr = "**";



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
String captureMove(QUEUE_T *queue) 
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
    if(kind == "X") {
        //no change is detected at captured ... error
        printf("ERROR: no change detected ... still not pun on pawn on hand desk");
        waitPutHand();

        // update kind of captured piece
        kind = detectCapturedChange();
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
        case 6: sfenStr = captureMove(queue); Serial.print("(cap) "); break;
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
        case 6: sfenStr = captureMove(queue); Serial.print("(cap) "); break;
        default: sfenStr = "error";
    }

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
    String isMineHnadDeskChanged;

    switch(*pturn){
        case 0: pnow = cir_arrays[0]; plast = cir_arrays[1]; break;
        case 1: pnow = cir_arrays[1]; plast = cir_arrays[0]; break;
        default: Serial.println("ERROR: TURN MUST BE 0 OR 1"); return;
    }

    // Read board sensors status
    getBoardSensors(pnow);

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
    // Calculate difference, if find endpoint set is_ready_analize_move HIGH
    detectChange(pnow, plast, queue, &is_ready_analize_move, board_historys[1]);

    // If is_ready_analize_move is HIGH, do analize process
    if(is_ready_analize_move == 1) {
        sfenStr = analizeMoveOpponent(queue);
        isMineHnadDeskChanged = detectCapturedChangeOpponentDemo();


        Serial.println("sfenStr and opponentMovedSfen :");Serial.print(sfenStr);Serial.print(",");Serial.println(opponentMovedSfen);
        if(sfenStr == "error" || isMineHnadDeskChanged != "nothing") {
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

void setup() {
    // ---------start Serial--------------
    Serial.begin(115200);
    delay(100);
    Serial.println("Serial Start!");


    // set GPIO for trun indication led pare
  pinMode(TRUN_BLACK_PIN, OUTPUT); 
  pinMode(TRUN_WHITE_PIN, OUTPUT);   
  

  //---------set pixel (serial_led by Adafruit_NeoPixel)-----------------
  initLedMatrix(status_cell, status_grid);
  pixels.begin();
  pixels.clear();
  delay(100);
  pixels.show();


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




void loop() {
  int result = -1;
  settingLEDs();
    
    // when preparetion and get to connect ble, init cir_array based on piece distribution from BLE
    if (processSt == "preparingStart" && isBleConnectionOn) {

      // ble connected action
      for(int i=0; i<LED_COUNT; i++) {
        
        pixels.setPixelColor(i, pixels.Color(0, 0, 64)); // blue
        pixels.show();
        delay(10);
      }

      delay(1000);
      pixels.clear();
      pixels.show();

      Serial.println("1. INIT NOW");
      getBoardSensors(cir_arrays[0]);
      getBoardSensors(cir_arrays[1]);
      holdCertainState(cir_arrays[0], board_historys[0]);


      // -- TODO wait init position
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
      }
      else if (result == 0) {
        // illigal status
        waitUndo();
      }
      else if (result == 1) {   // result == 1         
        holdCertainState(board_historys[1], board_historys[0]);
        Serial.println("GOTO waitOpponetTurn !");
        processSt = "waitOpponentTurn";
      }
    }
    else if (processSt == "waitOpponetTurn") {
      // waiting
    }
    else if (processSt == "detectionOpponent") {
        Serial.println("3. OPPONENT DETECT");
        mainOpponentMoveDetection(&turn, cir_arrays, &queue);
    }
    else if (processSt == "waitConfirmOpponent"){
        Serial.println("Waiting Confirm!");
        Serial.println("...");

        // --TODO--
        processSt = "detectionMine";
        pixels.clear();
        delay(100);
        pixels.show();

        holdCertainState(board_historys[1], board_historys[0]);

    };
    delay(10);
    
    
}
