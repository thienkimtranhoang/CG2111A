#include <serialize.h>
#include <stdarg.h>
#include <AFMotor.h>

#include <math.h>
#include "packet.h"
#include "constants.h"
#define PI 3.141592654

#define ALEX_LENGTH 25
#define ALEX_BREADTH 16

#define S0_PIN 6
#define S1_PIN 7
#define S2_PIN 5
#define S3_PIN 3
#define SENSOR_OUT_PIN 4

float alexDiagonal = 0.0;
float alexCirc = 0.0;
volatile TDirection dir;
/*
 * Alex's configuration constants
 */

// Number of ticks per revolution from the 
// wheel encoder.

#define COUNTS_PER_REV      4

// Wheel circumference in cm.
// We will use this to calculate forward/backward distance traveled 
// by taking revs * WHEEL_CIRC

#define WHEEL_CIRC          21

/*
 *    Alex's State Variables
 */

// Store the ticks from Alex's left and
// right encoders.
volatile unsigned long leftForwardTicks; 
volatile unsigned long rightForwardTicks;
volatile unsigned long leftReverseTicks; 
volatile unsigned long rightReverseTicks;

volatile unsigned long leftForwardTicksTurns; 
volatile unsigned long leftReverseTicksTurns;
volatile unsigned long rightForwardTicksTurns; 
volatile unsigned long rightReverseTicksTurns;
// Store the revolutions on Alex's left
// and right wheels
volatile unsigned long leftRevs;
volatile unsigned long rightRevs;

// Forward and backward distance traveled
volatile unsigned long forwardDist;
volatile unsigned long reverseDist;
unsigned long deltaDist;
unsigned long newDist;
unsigned long deltaDistback;
unsigned long newDistback;
volatile unsigned long LeftforwardDist = 0; 
volatile unsigned long RightforwardDist = 0; 
unsigned long deltaTicks;
unsigned long targetTicks;
/*
 * 
 * Alex Communication Routines.
 * 
 */
 //new function to estimate number of wheel ticks
 // needed to turn an angle
 unsigned long computeDeltaTicks(float ang){
  //we will assume that angular loopdistance moved = linear distance moved in one wheel
  // revolution. This is (probably incorrect but simplifies calculation
  //of wheel revs to make one full 360 turn is alexCirc/WHEEL_CIRC
  // This is for 360 degrees. For arg degree it will be (ang*alexCirc)/(999360*WHEEL_CIRC)
  //to concert to ticks, we multiply by COUNTS_PER_REV
  unsigned long ticks =(unsigned long)((ang*alexCirc*COUNTS_PER_REV)/(360.0*WHEEL_CIRC));
  return ticks;
 }
 
 void left(float ang, float speed){
  if(ang == 0)
  deltaTicks=99999999;
  else
  deltaTicks=computeDeltaTicks(ang); 
  targetTicks = leftReverseTicksTurns + deltaTicks;
  ccw(ang,speed);
  }

void right(float ang, float speed){
  if(ang == 0)
  deltaTicks=99999999;
  else
  deltaTicks=computeDeltaTicks(ang); 
  targetTicks = rightReverseTicksTurns + deltaTicks;
  cw(ang,speed);
  }
TResult readPacket(TPacket *packet)
{
    // Reads in data from the serial port and
    // deserializes it.Returns deserialized
    // data in "packet".
    
    char buffer[PACKET_SIZE];
    int len;

    len = readSerial(buffer);

    if(len == 0)
      return PACKET_INCOMPLETE;
    else
      return deserialize(buffer, len, packet);
    
}

void sendStatus() {
    TPacket statusPacket; // Create a new packet named statusPacket
    statusPacket.packetType = PACKET_TYPE_RESPONSE; // Set packet type to PACKET_TYPE_RESPONSE
    statusPacket.command = RESP_STATUS; // Set the command field to RESP_STATUS

    // Set the params array
    statusPacket.params[0] = leftForwardTicks;
    statusPacket.params[1] = rightForwardTicks;
    statusPacket.params[2] = leftReverseTicks;
    statusPacket.params[3] = rightReverseTicks;
    statusPacket.params[4] = leftForwardTicksTurns;
    statusPacket.params[5] = rightForwardTicksTurns;
    statusPacket.params[6] = leftReverseTicksTurns;
    statusPacket.params[7] = rightReverseTicksTurns;
    statusPacket.params[8] = forwardDist;
    statusPacket.params[9] = reverseDist;
    // Assuming the length of the params array here. Ensure it matches your actual TPacket structure.
//    statusPacket.paramCount = 10; // Set the paramCount to the number of parameters you've set.

    // Send out the packet using sendResponse
    sendResponse(&statusPacket);
}

void sendMessage(const char *message)
{
  // Sends text messages back to the Pi. Useful
  // for debugging.
  
  TPacket messagePacket;
  messagePacket.packetType=PACKET_TYPE_MESSAGE;
  strncpy(messagePacket.data, message, MAX_STR_LEN);
  sendResponse(&messagePacket);
}
void dbprintf(char *format,...){
  va_list args;
  char buffer[128];
  va_start(args, format);
  vsprintf(buffer, format, args);
  sendMessage(buffer);
  }
void sendBadPacket()
{
  // Tell the Pi that it sent us a packet with a bad
  // magic number.
  
  TPacket badPacket;
  badPacket.packetType = PACKET_TYPE_ERROR;
  badPacket.command = RESP_BAD_PACKET;
  sendResponse(&badPacket);
  
}

void sendBadChecksum()
{
  // Tell the Pi that it sent us a packet with a bad
  // checksum.
  
  TPacket badChecksum;
  badChecksum.packetType = PACKET_TYPE_ERROR;
  badChecksum.command = RESP_BAD_CHECKSUM;
  sendResponse(&badChecksum);  
}

void sendBadCommand()
{
  // Tell the Pi that we don't understand its
  // command sent to us.
  
  TPacket badCommand;
  badCommand.packetType=PACKET_TYPE_ERROR;
  badCommand.command=RESP_BAD_COMMAND;
  sendResponse(&badCommand);
}

void sendBadResponse()
{
  TPacket badResponse;
  badResponse.packetType = PACKET_TYPE_ERROR;
  badResponse.command = RESP_BAD_RESPONSE;
  sendResponse(&badResponse);
}

void sendOK()
{
  TPacket okPacket;
  okPacket.packetType = PACKET_TYPE_RESPONSE;
  okPacket.command = RESP_OK;
  sendResponse(&okPacket);  
}

void sendResponse(TPacket *packet)
{
  // Takes a packet, serializes it then sends it out
  // over the serial port.
  char buffer[PACKET_SIZE];
  int len;

  len = serialize(buffer, packet, sizeof(TPacket));
  writeSerial(buffer, len);
}


/*
 * Setup and start codes for external interrupts and 
 * pullup resistors.
 * 
 */
// Enable pull up resistors on pins 18 and 1
  void enablePullups(){
  // Use bare-metal to enable the pull-up resistors on pins
  // 19 and 18. These are pins PD2 and PD3 respectively.
  // We set bits 2 and 3 in DDRD to 0 to make them inputs. 
      // Configure pins PD2 and PD3 as inputs by clearing bits 2 and 3 in DDRD
    DDRD &= ~((1 << PD2) | (1 << PD3));
  
    // Enable pull-up resistors on PD2 and PD3 by setting bits 2 and 3 in PORTD
    PORTD |= (1 << PD2) | (1 << PD3);
}

// Functions to be called by INT2 and INT3 ISRs.
/*void leftISR()
{
  leftTicks++;
  LeftforwardDist = (leftTicks * WHEEL_CIRC)/COUNTS_PER_REV;
  Serial.print("LEFT: ");
  Serial.println(leftTicks);
  Serial.print("LeftDistance: ");
  Serial.println(LeftforwardDist);
}

void rightISR()
{
  rightTicks++;
  RightforwardDist = (rightTicks * WHEEL_CIRC)/COUNTS_PER_REV;
  Serial.print("RIGHT: ");
  Serial.println(rightTicks);
  Serial.print("RightDistance: ");
  Serial.println(RightforwardDist);
}
*/
void leftISR() {
    // Check the current direction and update the appropriate counters
    if (dir == FORWARD1) {
        leftForwardTicks++;
        forwardDist = (unsigned long) ((float) leftForwardTicks / COUNTS_PER_REV * WHEEL_CIRC);
    } else if (dir == BACKWARD1) {
        leftReverseTicks++;
         reverseDist = (unsigned long) ((float) leftReverseTicks / COUNTS_PER_REV * WHEEL_CIRC);
    } else if (dir == LEFT) {
        leftReverseTicksTurns++;
         //leftForwardTurnsDist = (unsigned long) ((float) leftForwardTicksTurns / COUNTS_PER_REV * WHEEL_CIRC);
    } else if (dir == RIGHT) {
        leftForwardTicksTurns++;
       // leftReverseTurnsDist = (unsigned long) ((float) leftReverseTicksTurns / COUNTS_PER_REV * WHEEL_CIRC);
    }
}

void rightISR() {
    // Check the current direction and update the appropriate counters
    if (dir == FORWARD1) {
        rightForwardTicks++;
    } else if (dir == BACKWARD1) {
        rightReverseTicks++;
    } else if (dir == LEFT) {
        rightForwardTicksTurns++;
    } else if (dir == RIGHT) {
        rightReverseTicksTurns++;
    }
}
// Set up the external interrupt pins INT2 and INT3
// for falling edge triggered. Use bare-metal.
void setupEINT()
{
  // Use bare-metal to configure pins 18 and 19 to be
  // falling edge triggered. Remember to enable
  // the INT2 and INT3 interrupts.
  // Hint: Check pages 110 and 111 in the ATmega2560 Datasheet.
EICRA =0b10100000;
EIMSK =0b00001100;
}

// Implement the external interrupt ISRs below.
// INT3 ISR should call leftISR while INT2 ISR
// should call rightISR.

ISR(INT2_vect){
  rightISR();
  
  }
ISR(INT3_vect){
  leftISR();
  }
// Implement INT2 and INT3 ISRs above.

/*
 * Setup and start codes for serial communications
 * 
 */
// Set up the serial connection. For now we are using 
// Arduino Wiring, you will replace this later
// with bare-metal code.
void setupSerial()
{
  // To replace later with bare-metal.
  Serial.begin(9600);
  // Change Serial to Serial2/Serial3/Serial4 in later labs when using the other UARTs
}

// Start the serial connection. For now we are using
// Arduino wiring and this function is empty. We will
// replace this later with bare-metal code.

void startSerial()
{
  // Empty for now. To be replaced with bare-metal code
  // later on.
  
}

// Read the serial port. Returns the read character in
// ch if available. Also returns TRUE if ch is valid. 
// This will be replaced later with bare-metal code.

int readSerial(char *buffer)
{

  int count=0;

  // Change Serial to Serial2/Serial3/Serial4 in later labs when using other UARTs

  while(Serial.available())
    buffer[count++] = Serial.read();

  return count;
}

// Write to the serial port. Replaced later with
// bare-metal code

void writeSerial(const char *buffer, int len)
{
  Serial.write(buffer, len);
  // Change Serial to Serial2/Serial3/Serial4 in later labs when using other UARTs
}

/*
 * Alex's setup and run codes
 * 
 */

// Clears all our counters
void clearCounters()
{
 /* leftTicks=0;
  rightTicks=0;
  leftRevs=0;
  rightRevs=0;
  forwardDist=0;
  reverseDist=0; */
  leftForwardTicks = 0;
  rightForwardTicks = 0;
  leftForwardTicksTurns = 0;
  rightForwardTicksTurns = 0;
  forwardDist = 0;
}

// Clears one particular counter
void clearOneCounter(int which)
{
  clearCounters();
/*  switch(which)
  {
    case 0:
      clearCounters();
      break;

    case 1:
      leftTicks=0;
      break;

    case 2:
      rightTicks=0;
      break;

    case 3:
      leftRevs=0;
      break;

    case 4:
      rightRevs=0;
      break;

    case 5:
      forwardDist=0;
      break;

    case 6:
      reverseDist=0;
      break;
  }*/
}
// Intialize Alex's internal states

void initializeState()
{
  clearCounters();
}

void handleCommand(TPacket *command)
{
  switch(command->command)
  {
    case COMMAND_GET_STATS:
    sendStatus();
    break;
    case COMMAND_CLEAR_STATS:
      // Call clearOneCounter, though it clears all counters in this implementation.
      clearOneCounter(command->params[0]);
      // Send back an OK packet to acknowledge the command.
      sendOK();
      break;
    break;
    // For movement commands, param[0] = distance, param[1] = speed.
    case COMMAND_FORWARD:
        sendOK();
        forward((double) command->params[0], (float) command->params[1]);
      break;
    case COMMAND_REVERSE:
        sendOK();
        backward((double) command->params[0], (float) command->params[1]);
      break;
       case COMMAND_TURN_LEFT:
        sendOK();
        left((double) command->params[0], (float) command->params[1]);
      break;
       case COMMAND_TURN_RIGHT:
        sendOK();
        right((double) command->params[0], (float) command->params[1]);
      break;
        case COMMAND_STOP:
        sendOK();
        stop();
      break;
    /*
     * Implement code for other commands here.
     * 
     */
        
    default:
      sendBadCommand();
  }
}

void waitForHello()
{
  int exit=0;

  while(!exit)
  {
    TPacket hello;
    TResult result;
    
    do
    {
      result = readPacket(&hello);
    } while (result == PACKET_INCOMPLETE);

    if(result == PACKET_OK)
    {
      if(hello.packetType == PACKET_TYPE_HELLO)
      {
     

        sendOK();
        exit=1;
      }
      else
        sendBadResponse();
    }
    else
      if(result == PACKET_BAD)
      {
        sendBadPacket();
      }
      else
        if(result == PACKET_CHECKSUM_BAD)
          sendBadChecksum();
  } // !exit
}

void setup() {
  // put your setup code here, to run once:
  alexDiagonal = sqrt((ALEX_LENGTH * ALEX_LENGTH) + (ALEX_BREADTH *ALEX_BREADTH));
  alexCirc = PI * alexDiagonal;
  cli();
  setupEINT();
  setupSerial();
  startSerial();
  enablePullups();
  initializeState();
  sei();
}

void handlePacket(TPacket *packet)
{
  switch(packet->packetType)
  {
    case PACKET_TYPE_COMMAND:
      handleCommand(packet);
      break;

    case PACKET_TYPE_RESPONSE:
      break;

    case PACKET_TYPE_ERROR:
      break;

    case PACKET_TYPE_MESSAGE:
      break;

    case PACKET_TYPE_HELLO:
      break;
  }
}

void loop() {
// Uncomment the code below for Step 2 of Activity 3 in Week 8 Studio 2

// forward(0, 100);
//Serial.println("q");
// Uncomment the code below for Week 9 Studio 2


 // put your main code here, to run repeatedly:
  TPacket recvPacket; // This holds commands from the Pi

  TResult result = readPacket(&recvPacket);
  
  if(result == PACKET_OK)
    handlePacket(&recvPacket);
  else
    if(result == PACKET_BAD)
    {
      sendBadPacket();
    }
    else
      if(result == PACKET_CHECKSUM_BAD)
      {
        sendBadChecksum();
      } 
    if(deltaDist > 0)
{
if(dir==FORWARD)
{
if(forwardDist > newDist)
{
deltaDist=0;
newDist=0;
stop();
}
}
else
if(dir == BACKWARD)
{
if(reverseDist > newDistback)
{
deltaDist=0;
newDistback=0;
stop();
}
}
else
if(dir == STOP)
{
deltaDist=0;
newDist=0;
stop();
}
}  
  if(deltaTicks > 0)
{
if(dir==LEFT)
{
if(leftReverseTicksTurns >= targetTicks)
{
deltaTicks=0;
targetTicks=0;
stop();
}
}
else
if(dir == RIGHT)
{
if(rightReverseTicksTurns >= targetTicks)
{
deltaTicks=0;
targetTicks=0;
stop();
}
}
else
if(dir == STOP)
{
deltaTicks=0;
targetTicks=0;
stop();
}
}
}
// return 0 if Red, return 1 if Green, return 2 if neither detected
long currentArray[3] = {0,0,0}; 
void setup() {
// Setting pin modes
DDRA |= (1 << S0_PIN) | (1 << S1_PIN) | (1 << S2_PIN) | (1 << S3_PIN); //S0 S1 S2 S3 as output
DDRA &= ~(1 << SENSOR_OUT_PIN); // sensorOut pin as input

// Setting frequency scaling to 20%
PORTA |= (1 << S0_PIN);
PORTA &= ~(1 << S1_PIN);
Serial.begin(9600);
}

int coloridentify()
{
  //setting red filtered photodiodes to be read
  PORTA &= ~((1 << S2_PIN) | (1 << S3_PIN));
  currentArray[0] = pulseIn(26, LOW);
  delay(200);

  //setting green filtered photodiodes to be read
  PORTA |= (1 << S2_PIN) | (1 << S3_PIN);
  currentArray[1] = pulseIn(26, LOW);
  delay(200);

  //setting blue filtered photodiodes to be read
  PORTA &= ~(1 << S2_PIN);
  PORTA |= (1 << S3_PIN);
  currentArray[2] = pulseIn(26, LOW);
  delay(200);
  //return the value of the color identified
 return colornumber(currentArray);
}
// floats to hold colour array to be callibrated
long red1Array[3] = {273,461,363};
long green1Array[3] = {423,389,350};
long blue1Array[3] = {472,376,260};
long orange1Array[3] = {287,516,442};
long purple1Array[3] = {399,443,297};
long black1Array[3] = {600,627,516};
long white1Array[3] = {183,187,152};

long red2Array[3] = {144,352,270};
long green2Array[3] = {364,293,299};
long blue2Array[3] = {277,158,108};
long orange2Array[3] = {172,388,328};
long purple2Array[3] = {250,271,172};
long black2Array[3] = {576,560,445};
long white2Array[3] = {107,109,89};

long red3Array[3] = {372,482,388};
long green3Array[3] = {456,442,382};
long g3Array[3] = {468,355, 327};
long r3Array[3] = {305,511,358};
long blue3Array[3] = {558,422,314};
long orange3Array[3] = {411,616,510};
long purple3Array[3] = {489,522,379};
long black3Array[3] = {692,733,590};
long white3Array[3] = {310, 330, 263} ;

long *referenceColours[21] = {red1Array, red2Array, red3Array, green1Array,  green2Array,  green3Array, white1Array, black1Array, blue1Array, orange1Array, purple1Array, white2Array, black2Array, blue2Array, orange2Array, purple2Array, black3Array, white3Array, blue3Array, orange3Array, purple3Array};
int colornumber(long colourArray[]) {
  double min_distance = 100000;
  short min_color = -1;
  
  // finds minimum Euclidean distance between measured color and reference colors
  for (int i = 0; i < 15; i += 1) {
    double redDifference = referenceColours[i][0] - currentArray[0];
    double greenDifference = referenceColours[i][1] - currentArray[1];
    double blueDifference = referenceColours[i][2] - currentArray[2];
    double distance = sqrtf((double) redDifference * redDifference
      + (double)greenDifference * greenDifference + (double)blueDifference * blueDifference);

    if (distance < min_distance) {
      min_distance = distance;
      if (i < 3) //still in the field of redArray
      {
        min_color = 0;
      }
      else if (i < 6) // still in the range of greenArray
      {
        min_color = 1;
      }
      else{ //out of range of red and green arrays
        min_color = 2;
      }
    }
   }
return min_color;
}
