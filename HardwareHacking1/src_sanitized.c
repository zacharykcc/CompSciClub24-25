/**************************************************************************
 Secret HW Vault Challenge
 
 Has a serial shell available that is always running and accepts custom
 commands.  Can use the up and down buttons to change the mode.  Challenges
 should get more difficult based on what "version" is running.

 This code started with the example code for the SSD1306 screen provided by
 Adafruit, and still heavily depends on their Arduino libraries:

 **************************************************************************/

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define RTC_I2C_ADDR 0x68
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// DS1307 I2C + RTC has RAM from bytes 0x08 - 0x3F (56 bytes)
// | addr | +0 | +1 | +2 | +3 | +4 | +5 | +6 | +7 |
// | 0x08 |mode|pin code (uint32_t)|    flag0     |
// | 0x10 |  <-             flag0             ->  |
// | 0x18 |flg0|  pin1 (uint32_t)  |    flag1     |
// | 0x20 |  <-             flag1             ->  |
// | 0x28 |flg1|  pin2 (uint32_t)  |    flag2     |
// | 0x30 |  <-             flag2             ->  |
// | 0x38 |flg2|  pin3 (uint32_t)  |highscore|    |

#define CHAL_MODE_LEN 1
#define FLAG_LEN 12
#define PIN_CODE_LEN (sizeof(unsigned long))
#define PIN_CODE_DIGITS 5
#define HIGH_SCORE_LEN 2

#define CHAL_MODE_ADDR 8
#define PIN_CODE_0_ADDR (CHAL_MODE_ADDR + CHAL_MODE_LEN)
#define FLAG_0_ADDR (PIN_CODE_0_ADDR + PIN_CODE_LEN)
#define PIN_CODE_1_ADDR (FLAG_0_ADDR + FLAG_LEN)
#define FLAG_1_ADDR (PIN_CODE_1_ADDR + PIN_CODE_LEN)
#define PIN_CODE_2_ADDR (FLAG_1_ADDR + FLAG_LEN)
#define FLAG_2_ADDR (PIN_CODE_2_ADDR + PIN_CODE_LEN)
#define PIN_CODE_3_ADDR (FLAG_2_ADDR + FLAG_LEN)
#define HIGH_SCORE_ADDR (PIN_CODE_3_ADDR + PIN_CODE_LEN)
#define HIGH_SCORE_LEN 2

// Challenge modes
// 0 = See pin via serial port
// 1 = Brute force via serial port
// 2 = Brute force via buttons
// 3 = No brute forcing

#define COMMAND_BUFFER_LEN 6
char gCommandBuffer[COMMAND_BUFFER_LEN];
char gCommandBufferPos = 0;

char gBgMode = 0;
char gIsLocked = 1;
uint8_t gChallengeMode = 0;
uint8_t gLedTimer = 0;
uint8_t gOldButtonStates = 0;

#define OLD_BUTTON_STATE_UP 1
#define OLD_BUTTON_STATE_DOWN 2
#define OLD_BUTTON_STATE_LEFT 4
#define OLD_BUTTON_STATE_RIGHT 8
#define OLD_BUTTON_STATE_A 16
#define OLD_BUTTON_STATE_B 32

#define UP_BUTTON 3
#define DOWN_BUTTON 4
#define LEFT_BUTTON 5
#define RIGHT_BUTTON 2
#define A_BUTTON 10
#define B_BUTTON 11
#define RED_LED 9
#define GREEN_LED 8

struct commandEntryStruct
{
  char const * const commandStr;
  void (*handler)();
};

void commandHelp();
void commandSecs();
void commandStart();
void commandMins();
void allRegHandler();
void commandSetTime();
void commandLock();
void commandUnlock();
void commandSetFlags();
void commandGetFlags();
void commandGetFlagsDebug();
void commandNextChallenge();
void commandSetPins();
void readDigitalButtons();
void commandGetVersion();
void commandGetHighScore();
void commandSetHighScore();
void snakeInit();

//#define DEBUG_MODE
//#define DEMO_MODE



const struct commandEntryStruct CMD_LIST[] = {
  {"help", commandHelp },
  {"secs", commandSecs },
  {"start", commandStart },
  {"mins", commandMins },
// { "allreg", allRegHandler },
  {"settim", commandSetTime},
  #ifdef DEBUG_MODE
  {"wrflgs", commandSetFlags },
  {"wrpins", commandSetPins },
  {"getflg", commandGetFlagsDebug },
  {"geths", commandGetHighScore },
  {"seths", commandSetHighScore },
  #else
  {"nxtchl", commandNextChallenge },
  {"lock", commandLock },
  {"unlock", commandUnlock },
  {"getflg", commandGetFlags },
  #endif
  {"ver", commandGetVersion }
};

#define NUM_CMDS (sizeof(CMD_LIST) / sizeof(struct commandEntryStruct))

void unlockUpHandler();
void unlockDownHandler();
void unlockLeftHandler();
void unlockRightHandler();
void unlockAHandler();
void unlockBHandler();

void defaultUpHandler();
void defaultDownHandler();
void defaultLeftHandler();
void defaultRightHandler();
void defaultAButtonHandler();
void defaultBButtonHandler();

void snakeUpHandler();
void snakeDownHandler();
void snakeLeftHandler();
void snakeRightHandler();
void snakeAButtonHandler();
void snakeBButtonHandler();
void snakeBgMode();

struct ButtonHandler {
  void (*upFunc)();
  void (*downFunc)();
  void (*leftFunc)();
  void (*rightFunc)();
  void (*aButtonFunc)();
  void (*bButtonFunc)();
};

const struct ButtonHandler gUnlockHandlers = {
  unlockUpHandler,
  unlockDownHandler,
  unlockLeftHandler,
  unlockRightHandler,
  unlockAHandler,
  unlockBHandler
};

const struct ButtonHandler gDefaultHandlers= {
  defaultUpHandler,
  defaultDownHandler,
  defaultLeftHandler,
  defaultRightHandler,
  defaultAButtonHandler,
  defaultBButtonHandler
};

const struct ButtonHandler gSnakeHandlers = {
  snakeUpHandler,
  snakeDownHandler,
  snakeLeftHandler,
  snakeRightHandler,
  snakeAButtonHandler,
  snakeBButtonHandler
};

const struct ButtonHandler* const gButtonHandlersForMode [] = {
  &gDefaultHandlers, // clock
  &gUnlockHandlers, // unlock
  &gDefaultHandlers, // version
  &gDefaultHandlers, // flag
  &gDefaultHandlers, // lock
  &gSnakeHandlers, // idle
};

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// The pins for I2C are defined by the Wire-library. 
// On an arduino UNO:       A4(SDA), A5(SCL)
// On an arduino MEGA 2560: 20(SDA), 21(SCL)DEBUG_MODE
// On an arduino LEONARDO:   2(SDA),  3(SCL), ...
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3c ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup() {
  Serial.begin(9600);

  pinMode(UP_BUTTON, INPUT_PULLUP);
  pinMode(DOWN_BUTTON, INPUT_PULLUP);
  pinMode(LEFT_BUTTON, INPUT_PULLUP);
  pinMode(RIGHT_BUTTON, INPUT_PULLUP);
  pinMode(A_BUTTON, INPUT_PULLUP);
  pinMode(B_BUTTON, INPUT_PULLUP);
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    gBgMode = -1;
    for(;;) // Don't proceed, loop forever
    {
        runShell(5000);
    }
  }

  // Board has the screen installed upside down, so rotate 180 deg
  display.setRotation(2);

  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  
  display.display();

  delay(50); // Pause for a bit

  snakeInit();
  
  // Read the challenge mode

  clockRead(CHAL_MODE_ADDR, CHAL_MODE_LEN, &gChallengeMode);
  gChallengeMode &= 0xff;
  if ( (gChallengeMode > 3 ) )
  {
    Serial.println(F("Error reading version at boot"));
    gChallengeMode = 0;
  }

  // Clear the buffer
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);

  while(1)
  {
    readDigitalButtons();
    doBGTask();
    runShell(10);
  }
 
}


#define MAX_BG_MODES 6

// Show the mode name for a little while after it changes mode
uint8_t gFreshModeChange = 0;

void doBGTask()
{
  if (gFreshModeChange)
  {
    //Serial.print(F("freshmode = "));
    //Serial.println(gFreshModeChange);

    // If the mode has just been changed, display the mode name for a second
    displayChangeModes();
    gFreshModeChange--;
    return;
  }

  switch (gBgMode)
  {
    case 0:
      displayClock();
      break;

    case 1:
      displayUnlock();
      break;

    case 2:
      displayVersion();
      break;

    case 3:
      displayFlag();
      break;

    case 4:
      displayLock();
      break;
    
    case 5:
      snakeBgMode();
      break;

    //default:
      // Do nothing
  }
}

char const * const serialPrintMode(char modeVal)
{
  switch(modeVal)
  {
    case 0:
      Serial.print(F("clock"));
      break;
    case 1:
      Serial.print(F( "unlock"));
      break;
    case 2:
      Serial.print(F( "version"));
      break;
    case 3:
      Serial.print(F( "get flag"));
      break;
    case 4:
      Serial.print(F( "lock"));
      break;
    default:
      Serial.print(F( "snake"));
      break;
  }
}

const char mode_string_0[] PROGMEM = "clock";
const char mode_string_1[] PROGMEM = "unlock";
const char mode_string_2[] PROGMEM = "version";
const char mode_string_3[] PROGMEM = "flag";
const char mode_string_4[] PROGMEM = "lock";
const char mode_string_5[] PROGMEM = "snake";
const char* const mode_string_array[] PROGMEM = { mode_string_0, mode_string_1, mode_string_2,
                                                  mode_string_3, mode_string_4, mode_string_5};


char const * const displayMode(char modeVal, int x, int y)
{
  //char const * strmem;
  char buf[8];

  if ( (modeVal >= 0) && (modeVal <= 5) )
  {
    strcpy_P(buf, (char*) pgm_read_ptr(&mode_string_array[modeVal]));
  }
  else
  {
      strcpy_P(buf, (char*) pgm_read_ptr(&mode_string_array[5]));
  }

  writeString(buf, x, y);

}

void validateCurrentMode()
{
  if (gBgMode < 0)
  {
    if (gIsLocked)
    {
      gBgMode = 2;
    }
    else
    {
      gBgMode = MAX_BG_MODES - 1;
    }
  }

  if (gIsLocked)
  {
    gBgMode %= 3;
  }
  else
  {
    gBgMode %= MAX_BG_MODES;
  }  
}

void modeUp()
{
  Serial.print(F("Old mode = "));
  serialPrintMode(gBgMode);
  Serial.println(F(""));

  gBgMode++;

  validateCurrentMode();

  Serial.print(F("New mode = "));
  serialPrintMode(gBgMode);
  Serial.println(F(""));
  gFreshModeChange = 20;
}

void modeDown()
{
  Serial.print(F("Old mode = "));
  serialPrintMode(gBgMode);
  Serial.println(F(""));

  gBgMode--;

  validateCurrentMode();

  Serial.print(F("New mode = "));
  serialPrintMode(gBgMode);
  Serial.println(F(""));
  gFreshModeChange = 20;
}


void runShell(int msForShell)
{
  unsigned long timeoutVal = millis();
  timeoutVal += msForShell;
  while(millis() < timeoutVal)
  {
    if (Serial.available())
    {
      unsigned char nb = Serial.read();
      Serial.print( (char) nb);
      if ( (nb == '\n') || (nb == '\r') )
      {
        interpretCommand();
      }
      else
      {
        if (gCommandBufferPos < COMMAND_BUFFER_LEN)
        {
          gCommandBuffer[gCommandBufferPos++] = nb;
        }
      }
    }
    delay(1);
  }
}

void interpretCommand()
{
  // Echo the command
  Serial.print(F("Command Receive: "));
  for(int i = 0; i < gCommandBufferPos; i++)
  {
    Serial.write(gCommandBuffer[i]);
  }
  Serial.println(F(""));

  unsigned char cmdMatchFound = 0;
  for(int i = 0; i < NUM_CMDS; i++)
  {
    int curListCmdLen = strlen(CMD_LIST[i].commandStr);
    if (curListCmdLen != gCommandBufferPos)
    {
      // Commands isn't the same length
      // Serial.print(F("Command "));
      // Serial.print(CMD_LIST[i].commandStr);
      // Serial.println(F(" not match due to lenght mismatch"));
      continue;
    }

    if (memcmp(CMD_LIST[i].commandStr, gCommandBuffer, curListCmdLen) == 0)
    {
      //Serial.println(F("Found mathcing command!"));
      CMD_LIST[i].handler();
      cmdMatchFound = 1;
      break;
    }
    else
    {
      // Commands isn't the same length
      // Serial.print(F("Command "));
      // Serial.print(CMD_LIST[i].commandStr);
      // Serial.println(F(" not match due to compare fail"));
    }

  }

  memset(gCommandBuffer, 0, COMMAND_BUFFER_LEN);
  gCommandBufferPos = 0;

  if (!cmdMatchFound)
  {
    Serial.println(F("No matching handler found for command"));
  }
}

void commandHelp()
{
  Serial.println(F("Command List:"));

  for(int i = 0; i < NUM_CMDS; i++)
  {
    Serial.print(F(" "));
    Serial.println(CMD_LIST[i].commandStr);
  }

  if (gBgMode == -1)
  {
    Serial.println(F("Initialization failed for display!"));
  }
}

void commandSecs(void)
{
  Serial.println(F("Secs Handler"));
  unsigned char numSecs;
  unsigned char br = clockRead(0, 1, &numSecs);
}

void commandMins(void)
{
  Serial.println(F("Mins Handler"));

  Wire.beginTransmission(0x68);
  Wire.write(0x01);
  Wire.endTransmission();

  int br = Wire.requestFrom(0x68, 1);

  for(int i = 0; i < br; i++)
  {
    if (Wire.available() == 0)
    {
      Serial.print(F("Error. read "));
      Serial.print(br);
      Serial.print(F(" bytes, but only "));
      Serial.print(i);
      Serial.println(F(" available"));
      break;
    }

    unsigned char curChar = Wire.read();
    Serial.print(F("Read: "));
    hexPrint(curChar);
    Serial.println(F(""));
  }

}

void commandStart()
{
  Serial.println(F("Start Handler"));

  Wire.beginTransmission(0x68);
  Wire.write(0x00);
  Wire.write(0x44);
  Wire.endTransmission();
}

void allRegHandler()
{
  Serial.println(F("All Regs Handler"));
  unsigned char regVals[8];
  unsigned char br = clockRead(0, 8, regVals);
}

void commandSetTime()
{
  Serial.println(F("Set Time Handler"));

  Serial.println(F("Enter the time as HHMMSS, HHMMSSa, or HHMMSSp"));

  unsigned char timeBuf[8];
  int br = readString(8, timeBuf, 60);  

  Serial.print(F("Bytes read = "));
  Serial.println(br);

  if ( (br != 6) && (br != 7) )
  {
      Serial.println(F("Time val must be 6/7 chars long"));
      return;
  }

  // Validate HH
  if ( (timeBuf[0] < '0') || (timeBuf[0] > '2') || (timeBuf[1] < '0') || (timeBuf[1] > '9') )
  {
    Serial.println(F("Invalid HH value"));
    return;
  }

  // Validate MM
  if ( (timeBuf[2] < '0') || (timeBuf[2] > '5') || (timeBuf[3] < '0') || (timeBuf[3] > '9') )
  {
    Serial.println(F("Invalid MM value"));
    return;
  }

  // Validate SS
  if ( (timeBuf[4] < '0') || (timeBuf[4] > '2') || (timeBuf[5] < '0') || (timeBuf[5] > '9') )
  {
    Serial.println(F("Invalid SS value"));
    return;
  }

  if (br == 7)
  {
    if ( (timeBuf[6] != 'a') && (timeBuf[6] != 'p') )
    {
      Serial.println(F("Invalid a/p value"));
      return;
    }
  }

  unsigned regVal;
  regVal = ( (timeBuf[0] - '0') << 4 );
  regVal &= 0x30;
  regVal |= ( (timeBuf[1] - '0') & 0x0f );

  // special hour bits
  if (br == 6)
  {
    // Set 24 hour mode
    regVal |= 0x40;
  }
  else
  {
    if (timeBuf[6] == 'p')
    {
      regVal |= 0x20;
    }
  }

  if (clockWrite(2, 1, (unsigned char*) &regVal))
  {
    Serial.println(F("Error writing hours for clock"));
    return;
  }
  
  regVal = ( (timeBuf[2] - '0') << 4 );
  regVal &= 0x70;
  regVal |= ( (timeBuf[3] - '0') & 0x0f );

  if (clockWrite(1, 1, (unsigned char*) &regVal))
  {
    Serial.println(F("Error writing minutes for clock"));
    return;
  }

  regVal = ( (timeBuf[4] - '0') << 4 );
  regVal &= 0x70;
  regVal |= ( (timeBuf[5] - '0') & 0x0f );

  if (clockWrite(0, 1, (unsigned char*) &regVal))
  {
    Serial.println(F("Error writing minutes for clock"));
    return;
  }

  Serial.println(F("Set Time handler complete"));
}



void writeFlag(int flagNum)
{
  Serial.println(F("Give me a flag to write (don't include wildcat or curly braces)"));
  char flag[FLAG_LEN+1];
  memset(flag, 0, FLAG_LEN+1);

  int bytesRead = readString(FLAG_LEN, flag, 30);
  if (bytesRead == -1)
  {
    Serial.println(F("Timeout reading flag from user"));
    return;
  }

  Serial.print(F("Writing flag "));
  Serial.print(flagNum);
  Serial.print(F(": wildcat{"));
  Serial.print(flag);
  Serial.println(F("}"));

  flagNum %= 3;
  int addr = FLAG_0_ADDR;
  addr += (FLAG_LEN + PIN_CODE_LEN) * flagNum;
  clockWrite(addr, FLAG_LEN, flag);

  Serial.println(F("Done"));
}

void commandSetFlags()
{
  for(int i = 0; i < 3; i++)
  {
    writeFlag(i);
  }
}

// Hide this in the shared source
#ifdef DEMO_MODE
const char flag_4[] PROGMEM = "s4mpl3-flg4";
#else
const char flag_4[] PROGMEM = "********";
#endif
void getFlag(int flagNum, char* flagBuf)
{
  if (flagNum >= 3)
  {
    strcpy_P(flagBuf, flag_4);
    return;
  }

  flagNum %= 3;
  memset(flagBuf, 0, FLAG_LEN+1);

  int addr = FLAG_0_ADDR;
  addr += (FLAG_LEN + PIN_CODE_LEN) * flagNum;

  clockRead(addr, FLAG_LEN, flagBuf);
}

void getFlagMyChalMode(char* flagBuf)
{
  getFlag(gChallengeMode, flagBuf);
}

void printFlagToSerial(int flagNum)
{
  char flag[FLAG_LEN+1];
  getFlag(flagNum, flag);

  // Need to make sure we have a null terminator before printing
  for(int i = 0; i < FLAG_LEN; i++)
  {
    if (flag[i] == 0)
    {
      // Found null terminator, print flag
      Serial.print(F("Flag "));
      Serial.print(flagNum);
      Serial.print(F(": wildcat{"));
      Serial.print(flag);
      Serial.println(F("}"));
      return;
    }
}

Serial.println(F("Error. Flag data lacks null terminator"));
}

void commandGetFlagsDebug()
{
  printFlagToSerial(0);
  printFlagToSerial(1);
  printFlagToSerial(2);
  printFlagToSerial(3);
}

void commandGetFlags()
{
  if (gIsLocked == 0)
  {
    printFlagToSerial(gChallengeMode);
  }
  else
  {
    Serial.println(F("Must unlock device first!"));
  }
}


uint32_t readPinFromRam(int pinStoreNum)
{
  uint32_t retVal;
  pinStoreNum %= 4;
  int addr = PIN_CODE_0_ADDR + (PIN_CODE_LEN + FLAG_LEN) * pinStoreNum;

  if (clockRead(addr, sizeof(uint32_t), (unsigned char*) &retVal) != sizeof(uint32_t))
  {
    Serial.println(F("Error reading the pin code"));
    retVal = -1;
  }
  else
  {
    // If somehow large pin goes in, unlock is only uint16_t
    retVal &= 0xffff;

    // Can't make this challenge that easy!
    //Serial.print(F("Read pin "));
    //Serial.print(retVal);
    //Serial.println(F(" from external RAM"));
  }

  return retVal;
}

void writePinToRam(int pinStoreNum, uint32_t pinVal)
{
  pinStoreNum %= 4;
  int addr = PIN_CODE_0_ADDR + (PIN_CODE_LEN + FLAG_LEN) * pinStoreNum;

  if (clockWrite(addr, sizeof(uint32_t), (unsigned char*) &pinVal) != 0)
  {
    Serial.println(F("Error save the pin code"));
  }
  else
  {
    Serial.print(F("Wrote pin "));
    Serial.print(pinVal);
    Serial.println(F(" to external RAM"));
  }
}

void setPin(int pinNum)
{
  char pinCode[PIN_CODE_DIGITS + 1];
  memset(pinCode, 0, PIN_CODE_DIGITS+1);

  Serial.println(F("Give me a pin to write (no mor than 5 digits)"));

  int br = readString(PIN_CODE_DIGITS + 1, pinCode, 30);
  if (br == -1)
  {
    Serial.println(F("Timeout waiting for pin code"));
    return;
  }

  if ( (br < 3) || (br > 5) )
  {
    Serial.print(F("Invalid pin length of "));
    Serial.println(br);
    return;
  }

  // Validate the pin code
  for(int i = 0; i < PIN_CODE_LEN; i++)
  {
    if ( (pinCode[i] < '0') || (pinCode[i] > '9') )
    {
      Serial.println(F("Pin code is invalid"));
      return;
    }
  }

  unsigned long pinRaw = strtoul(pinCode, 0, 10);

  Serial.print(F("Writing pin #"));
  Serial.print(pinNum);
  Serial.print(F(" as "));
  Serial.println(pinRaw);

  writePinToRam(pinNum, pinRaw);
}

void commandSetPins()
{
  for(int i = 0; i < 4; i++)
  {
    setPin(i);
  }
}

const char ver_string_0[] PROGMEM = "Flag via serial CLI";
const char ver_string_1[] PROGMEM = "Flag via serial pin brute force";
const char ver_string_2[] PROGMEM = "Flag via button brute force";
const char ver_string_3[] PROGMEM = "Flag via hardware monitoring";

const char* const ver_string_array[] PROGMEM = {ver_string_0, ver_string_1, ver_string_2, ver_string_3};

char const g_ver_buffer[32];

char const * const getVersionString(int verNum)
{
  verNum %= 4;
  strcpy_P(g_ver_buffer, (char*) pgm_read_ptr(&ver_string_array[verNum]));
  return g_ver_buffer;
}

void commandGetVersion()
{
#ifdef DEBUG_MODE
  Serial.print(F("DEBUG! "));
#else
  Serial.print(F("Version: "));
#endif

  Serial.println(gChallengeMode);
  Serial.println(getVersionString(gChallengeMode));

}

void commandNextChallenge()
{
  if (gBgMode == 3)
  {
    Serial.println(F("Can't be on flag screen!"));
    return;
  }

  Serial.println(F("You really want to goto next challenge?"));
  Serial.println(F("Type yes to confirm"));

  char buf[4];
  int num_chars = readString(4, buf, 30);
  Serial.println(F(""));

  if (num_chars != 3)
    return;

  if ( (buf[0] != 'y') || (buf[1] != 'e' ) || (buf[2] != 's') )
    return;

  gChallengeMode += 1;
  if (gChallengeMode == 4)
    gChallengeMode = 0;

  Serial.println(F("Mode changed to "));

  Serial.println(gChallengeMode);
  Serial.println(getVersionString(gChallengeMode));

  clockWrite(CHAL_MODE_ADDR, CHAL_MODE_LEN, &gChallengeMode);
  gIsLocked = 1;
}

void commandLock()
{
  Serial.println(F("Locking!"));
  gIsLocked = 1;
}

void commandUnlock()
{
  char pinCode[PIN_CODE_DIGITS + 1];
  memset(pinCode, 0, PIN_CODE_DIGITS+1);

  Serial.println(F("Enter the pin (no mor than 5 digits)"));

  int br = readString(PIN_CODE_DIGITS + 1, pinCode, 30);
  if (br == -1)
  {
    Serial.println(F("Timeout waiting for pin code"));
    return;
  }

  unsigned long pinRaw = strtoul(pinCode, 0, 10);

  uint32_t expectedPin = readPinFromRam(gChallengeMode);
  Serial.println(F(""));

  if (pinRaw == expectedPin)
  {
    Serial.println(F("PIN ACCEPTED!"));
    gIsLocked = 0;
  }
  else
  {
    Serial.print(F("PIN "));
    Serial.print(pinRaw);
    Serial.println(F(" INVALID"));

    if (gChallengeMode >= 2)
    {
      Serial.println(F("Brute force guard!  Wait 5 seconds"));
      delay(5000);
      Serial.println(F("You can try again now!"));
    }
  }

}

void commandGetHighScore()
{
  uint16_t hsVal;
  clockRead(HIGH_SCORE_ADDR, HIGH_SCORE_LEN, (unsigned char*) &hsVal);
  Serial.print(F("Read high score of "));
  Serial.print(hsVal);
  Serial.println(F(" from backup RAM"));
}

void commandSetHighScore()
{
  char highscore[6];
  memset(highscore, 0, 6);

  Serial.println(F("Give me a high score to write"));

  int br = readString(6, highscore, 30);
  if (br == -1)
  {
    Serial.println(F("Timeout waiting for high score"));
    return;
  }

  uint16_t hsVal = strtoul(highscore, 0, 10);
  clockWrite(HIGH_SCORE_ADDR, HIGH_SCORE_LEN, (unsigned char*) &hsVal);
  Serial.print(F("Wrote high score of "));
  Serial.print(hsVal);
  Serial.println(F(" to backup RAM"));
}

void displayClock()
{
  unsigned char curTime[3];
  if (clockRead(0, 3, curTime) != 3)
  {
    Serial.println(F("Error reading the time"));
    return;
  }

  char timeStr[12];
  memset(timeStr, 0, 12);
  timeStr[0] = '0' + ( (curTime[2] & 0x30) >> 4);
  timeStr[1] = '0' + (curTime[2] & 0x0f);
  timeStr[2] = ':';
  timeStr[3] = '0' + ( (curTime[1] & 0xf0) >> 4);
  timeStr[4] = '0' + (curTime[1] & 0x0f);
  timeStr[5] = ':';
  timeStr[6] = '0' + ( (curTime[0] & 0x70) >> 4);
  timeStr[7] = '0' + (curTime[0] & 0x0f);

  if ( (curTime[2] & 0x40) )
  {
    // Display 24 hr clock
    display.clearDisplay();
    writeString(timeStr, 14, 25);
    display.display();
  }
  else
  {
    if (curTime[2] & 0x20)
    {
      // PM
      timeStr[0] = '0' + ( (curTime[2] & 0x10) >> 4);
      //timeStr[8] = ' ';
      timeStr[9] = 'P';
      timeStr[10] = 'M';
    }
    else
    {
      //timeStr[8] = ' ';
      timeStr[9] = 'A';
      timeStr[10] = 'M';
    }

    // Display 12 hr clock
    display.clearDisplay();
    writeString(timeStr, 14, 12);
    writeString(&timeStr[9], 50, 38);
    display.display();
  }
}

uint16_t gCurrentPinGuess = 0;
uint8_t gCurrentPinGuessPos = 0;

void  displayUnlock()
{
  display.clearDisplay();
  //writeString("Unlock Display", 0, 10);
  //display.display();

  writeString("v", gCurrentPinGuessPos * 16, 0);
  writeString("^", gCurrentPinGuessPos * 16, 40);

  uint8_t singleDigit;
  uint16_t curModPin = gCurrentPinGuess;
  char strBuf[2];
  strBuf[1] = 0;
  for(int i = 0; i < 5; i++)
  {
    singleDigit = curModPin % 10;
    curModPin /= 10;
    strBuf[0] = '0' + singleDigit;
    writeString(strBuf, 64 - i * 16, 20);
  }
  display.display();

  //gIsLocked = 0;
  //runShell(1000);
}

void unlockUpHandler()
{
  uint16_t curPosVal = 1;
  for(uint8_t i = 0; i < 4 - gCurrentPinGuessPos; i++)
  {
    curPosVal *= 10;
  }

  gCurrentPinGuess += curPosVal;

  Serial.print(F("gCurrentPinGuess: "));
  Serial.println(gCurrentPinGuess);
}

void unlockDownHandler()
{
  uint16_t curPosVal = 1;
  for(uint8_t i = 0; i < 4 - gCurrentPinGuessPos; i++)
  {
    curPosVal *= 10;
  }

  gCurrentPinGuess -= curPosVal;

  Serial.print(F("gCurrentPinGuess: "));
  Serial.println(gCurrentPinGuess);
}

void unlockLeftHandler()
{
  if (gCurrentPinGuessPos == 0)
  {
    return;
  }

  gCurrentPinGuessPos -= 1;

  Serial.print(F("gCurrentPinGuessPos: "));
  Serial.println(gCurrentPinGuessPos);
}

void unlockRightHandler()
{
  if (gCurrentPinGuessPos >= 4)
  {
    return;
  }

  gCurrentPinGuessPos += 1;
}

void unlockAHandler()
{
  gFreshModeChange = 20;
}

const char WAIT_MSG[] PROGMEM = "WRONG";

void unlockBHandler()
{
  uint32_t expectedPin = readPinFromRam(gChallengeMode);

  if (gCurrentPinGuess == expectedPin)
  {
    Serial.println(F("Valid Pin"));
    digitalWrite(GREEN_LED, 1);
    digitalWrite(RED_LED, 0);
    gLedTimer = 20;
    gIsLocked = 0;
  }
  else
  {
    Serial.println(F("Invalid pin"));
    digitalWrite(GREEN_LED, 0);
    digitalWrite(RED_LED, 1);
    gLedTimer = 20;

    if (gChallengeMode == 3)
    {
      char buf[8];
      for(int i = 20; i >= 0; i--)
      {
        display.clearDisplay();
        strcpy_P(buf, WAIT_MSG);
        writeString(buf, 30 ,10);

        sprintf(buf,"%d", i);
        writeString(buf, 60, 40);

        display.display();
        delay(1000);
      }
    }
  }
}


void displayVersion()
{
  display.clearDisplay();

  char versionNum[10];

#ifdef DEBUG_MODE
  sprintf(versionNum, "DBG 1.%d", gChallengeMode);
#else
  sprintf(versionNum, "Ver 1.%d", gChallengeMode);
#endif

  writeString(versionNum, 0, 10);

  writeString(getVersionString(gChallengeMode), 0, 25);
  display.display();
}

void displayFlag()
{
  char flagStr[FLAG_LEN+0x10];
  strcpy(flagStr, "wildcat{");
  getFlagMyChalMode(flagStr + strlen(flagStr));
  flagStr[strlen(flagStr)] = '}';
  flagStr[strlen(flagStr)] = 0;

  display.clearDisplay();
  writeString(flagStr, 0, 0);
  display.display();
}

const char SECURE_MSG[] PROGMEM = "Vault\nSecured";

void displayLock()
{
  char buf[14];
  display.clearDisplay();
  strcpy_P(buf, SECURE_MSG);
  writeString(buf, 0 ,10);
  display.display();
  gIsLocked = 1;
}

void displayChangeModes()
{
  display.clearDisplay();
  displayMode(gBgMode, 10, 20);
  display.display();
}

/**
 * Reads a string from serial port.  Caller supplies buffer.  If successful,
 * a string returned without /n on end, null-terminated, and num chars read.
 * @param timeoutVal How long to wait for user in seconds
 */
int readString(int len, char* strBuf, unsigned long timeoutVal)
{
  memset(strBuf, 0, len);
  unsigned char pos = 0;
  timeoutVal = millis() * (timeoutVal * 1000);
  while(millis() < timeoutVal)
  {
    if (Serial.available())
    {
      strBuf[pos] = Serial.read();
      Serial.print( (char) strBuf[pos] ); // echo
    }
    else
    {
      // Nothing available yet
      delay(10);
      continue;
    }

    if ( (strBuf[pos] == '\n') || (strBuf[pos] == '\r') )
    {
      strBuf[pos] = 0;
      return pos;
    }

    pos++;
    if (pos == len)
    {
      // Can't overwrite the null at the end
      return (pos - 1);
    }  
  }

  Serial.println(F("\nTIMEOUT"));
  return -1;
}

void hexPrint(unsigned char val)
{
  unsigned char nibble = val / 16;
  if (nibble >= 10)
  {
    Serial.write('a' + nibble - 10);
  }
  else
  {
    Serial.write('0' + nibble);
  }

  nibble = val % 16;
  if (nibble >= 10)
  {
    Serial.write('a' + nibble - 10);
  }
  else
  {
    Serial.write('0' + nibble);
  }
}

int clockWrite(unsigned char clockAddr,
                unsigned char numBytes,
                unsigned char* buf)
{
  Serial.println(F("clockWrite"));

  Wire.beginTransmission(RTC_I2C_ADDR);
  Wire.write(clockAddr);

  int br = Wire.write(buf, numBytes);
  if (br != numBytes)
  {
    Serial.print(F("Wire.write wrote "));
    Serial.print(br);
    Serial.print(F(" bytes of expected "));
    Serial.println(numBytes);
  }
  
  int retVal = Wire.endTransmission();

  Serial.print(F("Wrote "));
  Serial.print(numBytes);
  Serial.print(F(" bytes to "));
  hexPrint(clockAddr);
  Serial.println(F(""));

  return retVal;
}

unsigned char clockRead(unsigned char clockAddr,
                        unsigned char numBytes,
                        unsigned char* buf)
{
  //Serial.println(F("clockRead"));

  Wire.beginTransmission(RTC_I2C_ADDR);
  Wire.write(clockAddr);
  int err = Wire.endTransmission();

  if (err)
  {
    switch(err)
    {
      case 0:
        Serial.println(F("Success"));
        break;
      case 1:
        Serial.println(F("Data too long!"));
        break;
      case 2:
        Serial.println(F("Received NAK on address"));
        break;
      case 3:
        Serial.println(F("Received NAK on data"));
        break;
      case 4:
        Serial.println(F("Other error"));
        break;
      case 5:
        Serial.println(F("Timeout"));
        break;
      default:
        Serial.println(F("Invalid error code"));
        Serial.print(F("Err code ="));
        Serial.println(err);
    }
  }

  int br = Wire.requestFrom( (uint8_t) 0x68, (uint8_t) numBytes);

  if (gChallengeMode == 0)
  {
    // Print out all the I2C traffic for challenge 1 onlyg
    Serial.print(F("Read "));
    Serial.print( (int) clockAddr );
    Serial.print(F(": "));
    for(int i = 0; i < br; i++)
    {
      if (Wire.available() == 0)
      {
        Serial.print(F("Error. read "));
        Serial.print(br);
        Serial.print(F(" bytes, but only "));
        Serial.print(i);
        Serial.println(F(" available"));
        return i;
      }

      buf[i] = Wire.read();
      hexPrint(buf[i]);
    }
    Serial.println(F(""));
  }
  else
  {
      for(int i = 0; i < br; i++)
      {
        if (Wire.available() == 0)
        {
          return i;
        }

        buf[i] = Wire.read();
      }
  }

  return br;
}

void loop()
{
  // Never called
}

void writeString(char* msg, int x, int y)
{
  //display.clearDisplay();

  //display.setTextSize(1);      // Normal 1:1 pixel scale
  //display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(x, y);     // Start at top-left corner
  display.cp437(true);  

  display.write(msg);
}

void defaultUpHandler()
{
  Serial.println(F("Up"));
  modeUp();
}

void defaultDownHandler()
{
  Serial.println(F("Down"));
  modeDown();
}

void defaultLeftHandler()
{
  Serial.println(F("Left"));
}

void defaultRightHandler()
{
  Serial.println(F("Right"));
}

void defaultAButtonHandler()
{
  Serial.println(F("A Button"));
}

void defaultBButtonHandler()
{
  Serial.println(F("B Button"));
}


void readDigitalButtons()
{
  if (gLedTimer)
  {
    gLedTimer--;
    if (gLedTimer == 0)
    {
      digitalWrite(RED_LED, 0);
      digitalWrite(GREEN_LED, 0);
    }
  }

  if (digitalRead(UP_BUTTON) == 0)
  {
    if (!(gOldButtonStates & OLD_BUTTON_STATE_UP))
    {
      if (gFreshModeChange)
      {
        defaultUpHandler();
      }
      else
      {
        gButtonHandlersForMode[gBgMode]->upFunc();
      }

      gOldButtonStates |= OLD_BUTTON_STATE_UP;
    }
  }
  else
  {
    gOldButtonStates &= ~OLD_BUTTON_STATE_UP;
  }

  if (digitalRead(DOWN_BUTTON) == 0)
  {
    if (!(gOldButtonStates & OLD_BUTTON_STATE_DOWN))
    {
      if (gFreshModeChange)
      {
        defaultDownHandler();
      }
      else
      {
        gButtonHandlersForMode[gBgMode]->downFunc();
      }
      gOldButtonStates |= OLD_BUTTON_STATE_DOWN;
    }
  }
  else
  {
    gOldButtonStates &= ~ OLD_BUTTON_STATE_DOWN;
  }

  if (digitalRead(LEFT_BUTTON) == 0)
  {
    if (!(gOldButtonStates & OLD_BUTTON_STATE_LEFT))
    {
      if (gFreshModeChange)
      {
        defaultLeftHandler();
      }
      else
      {
        gButtonHandlersForMode[gBgMode]->leftFunc();
      }
      gOldButtonStates |= OLD_BUTTON_STATE_LEFT;
    }
  }
  else
  {
    gOldButtonStates &= ~OLD_BUTTON_STATE_LEFT;
  }

  if (digitalRead(RIGHT_BUTTON) == 0)
  {
    if (!(gOldButtonStates & OLD_BUTTON_STATE_RIGHT))
    {
      if (gFreshModeChange)
      {
        defaultRightHandler();
      }
      else
      {
        gButtonHandlersForMode[gBgMode]->rightFunc();
      }
      gOldButtonStates |= OLD_BUTTON_STATE_RIGHT;
    }
  }
  else
  {
    gOldButtonStates &= ~OLD_BUTTON_STATE_RIGHT;
  }


  if (digitalRead(A_BUTTON) == 0)
  {
    if (!(gOldButtonStates & OLD_BUTTON_STATE_A))
    {
      if (gFreshModeChange)
      {
        defaultAButtonHandler();
      }
      else
      {
        gButtonHandlersForMode[gBgMode]->aButtonFunc();
      }
      gOldButtonStates |= OLD_BUTTON_STATE_A;
    }
  }
  else
  {
    gOldButtonStates &= ~OLD_BUTTON_STATE_A;
  }


  if (digitalRead(B_BUTTON) == 0)
  {
    if (!(gOldButtonStates & OLD_BUTTON_STATE_B))
    {
      if (gFreshModeChange)
      {
        defaultBButtonHandler();
      }
      else
      {
        gButtonHandlersForMode[gBgMode]->bButtonFunc();
      }
      gOldButtonStates |= OLD_BUTTON_STATE_B;
    }
  }
  else
  {
    gOldButtonStates &= ~OLD_BUTTON_STATE_B;
  }


}
struct Point
{
  int8_t x;
  int8_t y;
};

bool operator==(struct Point const & lhs, struct Point const & rhs)
{
  return (lhs.x == rhs.x) && (lhs.y == rhs.y);
}

#define MAX_APPLES 8
#define MAX_SNAKE_LEN 16

uint8_t gSnakeDir = 0;
uint8_t gSnakeTime = 0;
uint8_t gSnakeBufferPos = 0;
uint8_t gSnakeLen = 0;
uint8_t gSnakeSpeed = 0x5;
uint16_t gSnakeScore = 0;

Point gSnake[MAX_SNAKE_LEN];
Point gApples[MAX_APPLES];

#define SNAKE_LEN_MAX 16
#define SNAKE_LEN_MIN 3

#define SNAKE_UP 0
#define SNAKE_DOWN 1
#define SNAKE_LEFT 2
#define SNAKE_RIGHT 3
#define SNAKE_DIR_MASK 3

#define SNAKE_SCREEN_WIDTH 64
#define SNAKE_SCREEN_HEIGHT 32

void snakeDrawPixel(struct Point const & p)
{
  display.drawRect(p.x * 2, p.y * 2, 2, 2, 1);
}

void snakeClearPixel(struct Point const & p)
{
  display.drawRect(p.x * 2, p.y * 2, 2, 2, 0);
}

void snakeDrawApples()
{
  for(int i = 0; i < MAX_APPLES; i++)
  {
    // Draw apples
    if (gApples[i].x != -1)
    {
      snakeDrawPixel(gApples[i]);
    }
  }

}

void snakeDrawSnake()
{
  // Did the snake hit the snake?
  //Serial.print(F("SnakeLen= "));
  //Serial.print(gSnakeLen);
  //Serial.print(F(", bufPos="));
  //Serial.println(gSnakeBufferPos);

  int snakeIndex = gSnakeBufferPos;

  // skip index 0, cause we can't hit the current head
  for(int i = 0; i < gSnakeLen; i++)
  {
    if (snakeIndex == -1)
    {
      // wrap around
      //Serial.println(F("Wrap"));
      snakeIndex = MAX_SNAKE_LEN - 1;
    }

    snakeDrawPixel(gSnake[snakeIndex]);
    //Serial.print(F("S "));
    //Serial.print(gSnake[snakeIndex].x);
    //Serial.print(F(","));
    //Serial.println(gSnake[snakeIndex].y);
  
    snakeIndex--;
  }
}

void snakeRedrawDisplay()
{
  display.clearDisplay();
  display.drawFastHLine(0, 0, SCREEN_WIDTH, 1);
  display.drawFastHLine(0, 1, SCREEN_WIDTH, 1);
  display.drawFastHLine(0, SCREEN_HEIGHT - 1, SCREEN_WIDTH, 1);
  display.drawFastHLine(0, SCREEN_HEIGHT - 2, SCREEN_WIDTH, 1);
  
  display.drawFastVLine(0,0,SCREEN_HEIGHT, 1);
  display.drawFastVLine(1,0,SCREEN_HEIGHT, 1);
  display.drawFastVLine(SCREEN_WIDTH - 2,0,SCREEN_HEIGHT, 1);
  display.drawFastVLine(SCREEN_WIDTH - 1,0,SCREEN_HEIGHT, 1);

  snakeDrawApples();
  snakeDrawSnake();
  display.display();
}

const char GAME_OVER_MSG[] PROGMEM  = "Game Over";
const char HIGH_SCORE_MSG[] PROGMEM = "HighScore";
void snakeReset(uint8_t draw_apples)
{
  // Snake game reset score / died

  char buf[10];


  for(int i = 0; i < 5; i++)
  {

    display.clearDisplay();

    if ( (i < 3) && (draw_apples) )
    {
      snakeDrawApples();
    }

    strcpy_P(buf, GAME_OVER_MSG);
    writeString(buf, 5, 5);

    sprintf(buf, "%d", gSnakeScore);
    writeString(buf, 5, 20);

    uint16_t hs;
    clockRead(HIGH_SCORE_ADDR, HIGH_SCORE_LEN, (unsigned char*) &hs);
    if (gSnakeScore > hs)
    {
      Serial.println(F("New High Score"));
      Serial.println(F("wildcat{**************}"));
      hs = gSnakeScore;
      clockWrite(HIGH_SCORE_ADDR, HIGH_SCORE_LEN, (unsigned char*) &hs);
    }

    strcpy_P(buf, HIGH_SCORE_MSG);
    writeString(buf, 5, 35);

    sprintf(buf, "%d", hs);
    writeString(buf, 5, 50);

    display.display();

    delay(1000);
  }

  snakeInit();
}

void snakeInit()
{
  Serial.println(F("SnakeInit"));
  for(int i = 0; i < 8; i++)
  {
    gApples[i].x = -1;
    gApples[i].y = -1;
  }

  gSnake[0].x = SNAKE_SCREEN_WIDTH >> 1;
  gSnake[0].y = SNAKE_SCREEN_HEIGHT >> 1;

  gSnake[1].x = (SNAKE_SCREEN_WIDTH >> 1) + 1;
  gSnake[1].y = SNAKE_SCREEN_HEIGHT >> 1;
  
  gSnake[2].x = (SNAKE_SCREEN_WIDTH >> 1) + 2;
  gSnake[2].y = SNAKE_SCREEN_HEIGHT >> 1;

  //gSnakeBufferPos = 0x32;
  gSnakeLen = 3;
  gSnakeBufferPos = 2;
  gSnakeDir = SNAKE_RIGHT;
  gSnakeTime = 0;
  gSnakeScore = 0;
  gSnakeSpeed = 0x5;

}

void snakeUpHandler()
{
  Serial.println(F("S Up"));
  gSnakeDir = SNAKE_UP;
}

void snakeDownHandler()
{
  Serial.println(F("S Down"));
  gSnakeDir = SNAKE_DOWN;
}

void snakeLeftHandler()
{
  Serial.println(F("S Left"));
  gSnakeDir = SNAKE_LEFT;
}

void snakeRightHandler()
{
  Serial.println(F("S Right"));
  gSnakeDir = SNAKE_RIGHT;
}

void snakeAButtonHandler()
{
  Serial.println(F("SA"));
  gFreshModeChange = 20;
}

void snakeBButtonHandler()
{
  Serial.println(F("SB"));
  snakeInit();
}

void snakeBgMode()
{
  //uint8_t curTime = (gSnakeDir >> 2) & 0x3F;
  uint8_t curTime = ++gSnakeTime;
  //Serial.println(curTime);
  
  // At some large time interval, add an apple on the map
  if ( (curTime & 0x3f) == 0x3f)
  {
    Serial.println(F("Add an apple"));
    // Add another apple

    digitalWrite(GREEN_LED, 1);
    gLedTimer = 2;

    uint8_t too_many_apples = 1;
    for(int i = 0; i < 8; i++)
    {
      if (gApples[i].x == -1)
      {
        gApples[i].x = random(SNAKE_SCREEN_WIDTH);
        gApples[i].y = random(SNAKE_SCREEN_HEIGHT);

        Serial.print(F("Added an apple "));
        Serial.print(gApples[i].x);
        Serial.print(F(" x "));
        Serial.print(gApples[i].y);
        Serial.print(F(" , i ="));
        Serial.println(i);

        too_many_apples = 0;
        i = 8;

        // This could add an apple where there is already an apple, but I snake code will probably only just eat one and then have to come back and eat it again

        delay(100);
      }
    }

    if (too_many_apples)
    {
      Serial.println(F("Too many apples!"));
      digitalWrite(RED_LED, 1);
      snakeReset(1);
      return;
    }
   
  } // adding apples done

  // At a shorter time interval, lets move the snake
  if ( (curTime & gSnakeSpeed) == 0x0 )
  {
    // Discard the lowest 3 bits of the timer
    Serial.println(F("Move the snake"));

    Point* curPos = gSnake + gSnakeBufferPos;

    int nextBufferPos = gSnakeBufferPos + 1;
    if (nextBufferPos == MAX_SNAKE_LEN)
    {
      nextBufferPos = 0;
    }
    Point* nextPos = gSnake + nextBufferPos;

    nextPos->x = curPos->x;
    nextPos->y = curPos->y;

    switch (gSnakeDir) // & SNAKE_DIR_MASK)
    {
      case SNAKE_UP:
      Serial.print(F(" [UP] "));
        nextPos->y -= 1;
        if (nextPos->y < 0)
        {
          snakeReset(0);
          Serial.println(F("Top hit!"));
          return;
        }
        break;
      case SNAKE_DOWN:
      Serial.print(F(" [DOWN] "));
        nextPos->y += 1;
        if (nextPos->y >= SNAKE_SCREEN_HEIGHT)
        {
          Serial.println(F("Bottom hit!"));
          snakeReset(0);
          return;
        }
        break;
      case SNAKE_LEFT:
        Serial.print(F(" [LEFT] "));
        nextPos->x -= 1;
        if (nextPos->x <= 0)
        {
          Serial.println(F("Left wall hit!"));
          snakeReset(0);
          return;
        }
        break;
      case SNAKE_RIGHT:
      Serial.print(F(" [RIGHT] "));
        nextPos->x += 1;
        if (nextPos->x >= SNAKE_SCREEN_WIDTH - 1)
        {
          Serial.println(F("Right wall hit!"));
          snakeReset(0);
          return;
        }
        break;
      default:
        Serial.print(F(" [ERROR] "));
        return;
    } // end switch

    // Did the snake hit the snake?
    //uint8_t snakeLen = (gSnakeBufferPos >> 4) & 0xf + 1;
    int snakeIndexToCheck = gSnakeBufferPos - 1;
    // skip index 0, cause we can't hit the current head
    for(int i = 1; i < gSnakeLen; i++)
    {
      if (snakeIndexToCheck == -1)
      {
        // wrap around
        snakeIndexToCheck = MAX_SNAKE_LEN - 1;
      }

      if (*nextPos == gSnake[snakeIndexToCheck])
      {
        Serial.println(F("Snake hit"));
        snakeReset(0);
        return;
      }

        snakeIndexToCheck--;
    }

    // If we got here, we didn't hit anything
    snakeDrawPixel(*nextPos);
    snakeClearPixel(gSnake[snakeIndexToCheck]);

    gSnakeBufferPos = nextBufferPos;

    // Did the snake eat an apple?
    for(int i = 0; i < 8; i++)
    {
      if (gApples[i].x != -1)
      {
        if (*nextPos == gApples[i])
        {
          Serial.println(F("Yummy!!"));

          gApples[i].x = -1;
          gSnakeLen += 1;
          if (gSnakeLen == SNAKE_LEN_MAX)
          {
            Serial.println(F("ANACONDA!!"));
            gSnakeLen -= 1;
          }

          gSnakeScore += 1;
          if (gSnakeScore > 10)
          {
            gSnakeSpeed = 0x3;
          }

          if (gSnakeScore > 25)
          {
            gSnakeSpeed = 0x1;
          }
        }

      }
    } // end of apple eating

  } // end of snake moved

  
  // Draw everything
  snakeRedrawDisplay();

}

