#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRutils.h>
#include <IRsend.h>
#include <BluetoothSerial.h>
//#include <memory>
#include <array>
#include <Adafruit_PN532.h>
#include <Wire.h>
#include <cc1101.h>
#include <inttypes.h>
#include <ctype.h>

#define PN532_IRQ   (2)
#define PN532_RESET (3)
#define ASSERT_BOUNDS(INDEX,SIZE) if (INDEX<0 || INDEX>=SIZE) return false;
#define skip() goto beginning
#define TESTING 1

enum class LogCode : uint8_t{
  IR_CAP_START,
  IR_CAP_FAIL,
  IR_CAP_SUCCESS,
  IR_SEND_FAIL,
  IR_SEND_SUCCESS,
  NFC_CAP_START,
  NFC_CAP_FAIL,
  NFC_INV_UID,
  NFC_AUTH_FAIL,
  NFC_READ_FAIL,
  NFC_WRITE_FAIL,
  SUBGHZ_CAP_START,
  SUBGHZ_CAP_FAIL,
  SUBGHZ_CAP_SUCCESS,
  SUBGHZ_SEND_FAIL,
  SUBGHZ_SEND_SUCCESS
};


enum class Mode: uint8_t {
  IR,
  NFC,
  SUB_GHZ,
  INVALID
};
enum class State: uint8_t {
  UNREC,
  CAPTURE,
  REPLAY,
};

enum class TokenNumCap: uint8_t {
  IR=2,
  NFC=2,
  SUB_GHZ=3
};

enum class TokenNumRep: uint8_t {
  IR=5,
  NFC=3,
  SUB_GHZ=3
};

typedef byte Uid[4];
typedef byte nfcKey[6];

void __attribute__((noreturn)) halt(void)
{
  while(1);
}



/*enum class Signal{
  IR,
  NFC,
  SUB_GHZ
};*/


constexpr nfcKey commonKeys[] = {
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
    {0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5},
    {0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5},
    {0x4D, 0x3A, 0x99, 0xC3, 0x51, 0xDD},
    {0x1A, 0x98, 0x2C, 0x7E, 0x45, 0x9A},
    {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF},
    {0x71, 0x4C, 0x5C, 0x88, 0x6E, 0x97},
    {0x58, 0x7E, 0xE5, 0xF9, 0x35, 0x0F},
    {0xA0, 0x47, 0x8C, 0xC3, 0x90, 0x91},
    {0x53, 0x3C, 0xB6, 0xC7, 0x23, 0xF6},
    {0x8F, 0xD0, 0xA4, 0xF2, 0x56, 0xE9},
    {0xA6, 0x45, 0x98, 0xA7, 0x74, 0x78},
    {0x26, 0x94, 0x0B, 0x21, 0xFF, 0x5D},
    {0xFC, 0x00, 0x01, 0x87, 0x78, 0xF7},
    {0x00, 0x00, 0x0F, 0xFE, 0x24, 0x88},
    {0x5C, 0x59, 0x8C, 0x9C, 0x58, 0xB5},
    {0xE4, 0xD2, 0x77, 0x0A, 0x89, 0xBE},
    {0x00, 0x11, 0x22, 0x33, 0x44, 0x55},
    {0x43, 0x4F, 0x4D, 0x4D, 0x4F, 0x41},
    {0x43, 0x4F, 0x4D, 0x4D, 0x4F, 0x42},
    {0x47, 0x52, 0x4F, 0x55, 0x50, 0x41},
    {0x47, 0x52, 0x4F, 0x55, 0x50, 0x42},
    {0x50, 0x52, 0x49, 0x56, 0x41, 0x41},
    {0x50, 0x52, 0x49, 0x56, 0x41, 0x42},
    {0x02, 0x97, 0x92, 0x7C, 0x0F, 0x77},
    {0xEE, 0x00, 0x42, 0xF8, 0x88, 0x40},
    {0x72, 0x2B, 0xFC, 0xC5, 0x37, 0x5F},
    {0xF1, 0xD8, 0x3F, 0x96, 0x43, 0x14},
    {0x54, 0x72, 0x61, 0x76, 0x65, 0x6C},
    {0x77, 0x69, 0x74, 0x68, 0x75, 0x73},
    {0x4A, 0xF9, 0xD7, 0xAD, 0xEB, 0xE4},
    {0x2B, 0xA9, 0x62, 0x1E, 0x0A, 0x36},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x01},
    {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC},
    {0xB1, 0x27, 0xC6, 0xF4, 0x14, 0x36},
    {0x12, 0xF2, 0xEE, 0x34, 0x78, 0xC1},
    {0x34, 0xD1, 0xDF, 0x99, 0x34, 0xC5},
    {0x55, 0xF5, 0xA5, 0xDD, 0x38, 0xC9},
    {0xF1, 0xA9, 0x73, 0x41, 0xA9, 0xFC},
    {0x33, 0xF9, 0x74, 0xB4, 0x27, 0x69},
    {0x14, 0xD4, 0x46, 0xE3, 0x33, 0x63},
    {0xC9, 0x34, 0xFE, 0x34, 0xD9, 0x34},
    {0x19, 0x99, 0xA3, 0x55, 0x4A, 0x55},
    {0x27, 0xDD, 0x91, 0xF1, 0xFC, 0xF1},
    {0xA9, 0x41, 0x33, 0x01, 0x34, 0x01},
    {0x99, 0xC6, 0x36, 0x33, 0x44, 0x33},
    {0x43, 0xAB, 0x19, 0xEF, 0x5C, 0x31},
    {0xA0, 0x53, 0xA2, 0x92, 0xA4, 0xAF},
};

constexpr uint8_t sendPin = 13;  // ESP8266 GPIO pin to use. Recommended: 4 (D2).
constexpr uint8_t recvPin = 14;
constexpr uint8_t batPin = 36;
constexpr uint8_t LOOP_TASK_PRIORITY = 1;
constexpr uint16_t BAT_LVL_PERIOD_MS = 30000;
constexpr uint8_t MAX_TOKENS=10;
constexpr uint8_t MAX_TOKEN_SIZE=20;
constexpr uint8_t MSG_MAX_LEN=100;
constexpr uint8_t CAP_NUM_TOKENS=2;
constexpr uint8_t REP_NUM_TOKENS=5;
constexpr uint16_t MAX_ANALOG_VAL=4095;
constexpr uint64_t IR_REPEAT_CODE=0xFFFFFFFFFFFFFFFF;
constexpr uint8_t MIFARE_CLASSIC_UID_LEN = 4;
#if TESTING == 1
Uid nfcUid={0,1,2,3};
#elif TESTING==0
Uid nfcUid={0};
#endif
uint32_t blockNum=0;
uint8_t nfcUidLen;
decode_results results;
std::array<String,10> tokens;
//String modeString;
//String command;
uint8_t batLvl;
unsigned long lastRecordedTime=0;
uint8_t tokenNum=0;
constexpr uint16_t adcMax=4095;
constexpr float fullBatVScaled = 2.1f;
constexpr float emptyBatVScaled = 1.5f;
constexpr float adcRefV=3.3f;
constexpr uint16_t batAdcMax=adcMax*(fullBatVScaled/adcRefV);
constexpr uint16_t batAdcMin=adcMax*(emptyBatVScaled/adcRefV);
constexpr uint8_t batMinPerc=0;
constexpr uint8_t batMaxPerc=100;

byte blockData[16]={0x01};
char msgToSend[MSG_MAX_LEN];

class IRSignal
{
  public:
  decode_type_t prot;
  uint64_t code;
  int nbits;
  IRSignal(){}
  void set(decode_type_t type,uint64_t val,uint16_t bits)
  {
    prot=type;
    code=val;
    nbits=bits;
  } 
};

class BTHelper:public BluetoothSerial {
  private:
    SemaphoreHandle_t mutex;
    static constexpr uint8_t LOG_MAX_LEN=10;
  public:
    BTHelper()
    {
      mutex=xSemaphoreCreateMutex();    
    }
    void write(const char* msg)
    {
      size_t msgLen = strlen(msg);
      xSemaphoreTake(mutex,portMAX_DELAY);
      BluetoothSerial::write(msgLen);
      BluetoothSerial::write((const uint8_t*)msg,msgLen);
      xSemaphoreGive(mutex); 
    }
    void log(LogCode code)
    {
      buildMsg(msgToSend,LOG_MAX_LEN,code);
      write(msgToSend);
    }
    void empty()
    {
      while(BluetoothSerial::available())
        BluetoothSerial::read();
    }
    ~BTHelper()
    {
      vSemaphoreDelete(mutex);
    }
    
};


BTHelper BT;
State state;
IRSignal irSignal;
Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET,&Wire);
IRsend irsend(sendPin);  // Set the GPIO to be used to sending the message.
IRrecv irrecv(recvPin);

Mode determineMode(const String& modeString)
{
  Mode mode=Mode::INVALID;
  if(modeString=="ir")
    mode=Mode::IR;
  else if(modeString=="nfc")
    mode=Mode::NFC;
  else if(modeString=="ghz")
    mode=Mode::SUB_GHZ;

  return mode;
}

inline void buildMsg(char* msg,const decode_results& irResults)
{
  snprintf(msg,MSG_MAX_LEN,"ir:%d:%"PRIu64":%"PRIu16,irResults.decode_type,irResults.value,irResults.bits);
}

inline void buildMsg(char* msg,const uint8_t batteryLevel)
{
  snprintf(msg,MSG_MAX_LEN,"bat:%"PRIu8,batteryLevel);
}

inline void buildMsg(char* msg,const Uid uid)
{ 
    snprintf(msg,MSG_MAX_LEN,"nfc:%02x%02x%02x%02x",uid[0],uid[1],uid[2],uid[3]);
}

inline void buildMsg(char* msg,const double frequency)
{
  snprintf(msg,MSG_MAX_LEN,"ghz:%"PRIu32":%"PRIu8,(uint32_t)(frequency*100.0));
}

inline void buildMsg(char* msg,size_t msgLen,LogCode code)
{
  snprintf(msg,msgLen,"log:%"PRIu8,code);
}

inline void changeBlockUid(Uid newUid,byte* outBlockData,uint32_t blockNum)
{
  memcpy(outBlockData,nfcUid,MIFARE_CLASSIC_UID_LEN);
  outBlockData[4]=calcBCC(nfcUid);
}

uint8_t stringSplit(String str,const String delimiter,std::array<String, 10>& tokens)
{
  int16_t index;
  uint8_t i=0;
  while((index=str.indexOf(delimiter))!=-1 && i<MAX_TOKENS)
  {
    tokens[i]=str.substring(0,index);
    str=str.substring(index+delimiter.length());
    i++;
  }
  if(i<MAX_TOKENS)
    tokens[i]=str;
  return i+1;
}

inline uint8_t getBatLvl(const uint16_t analogVal)
{
  return map(analogVal,batAdcMin,batAdcMax,batMinPerc,batMaxPerc);
}

void getAndSendBatLvl()
{
  char msgToSend[MSG_MAX_LEN];
  uint8_t batLvl=getBatLvl(analogRead(batPin));
  buildMsg(msgToSend,batLvl);
  BT.write(msgToSend);
}

void batteryTimerCallback(TimerHandle_t xTimer) {
  static bool first=true;

  if(first)
    first=false;
  else 
    getAndSendBatLvl();
}

inline byte calcBCC(Uid uid)
{
  byte BCC=uid[0];
  for(uint8_t i=1;i<MIFARE_CLASSIC_UID_LEN;i++)
    BCC^=uid[i];
  return BCC;
}

bool nfcBruteForceAuth(Uid uid, uint32_t blockNum)
{
  bool success=false;
  bool isKeyB=false;
  constexpr uint8_t commonKeysSize=sizeof(commonKeys)/sizeof(commonKeys[0]);
  for(uint8_t i=0;i<2;i++)
  {
    for(uint8_t j=0;j<commonKeysSize;j++)
    {
      success=nfc.mifareclassic_AuthenticateBlock(uid,MIFARE_CLASSIC_UID_LEN,blockNum,isKeyB,(byte*)commonKeys[j]);
      if(success)
        return true;
    }
    isKeyB=true;
  }
  return success;
}

int8_t hexCharToDec(char hexChar)
{
  constexpr uint8_t decDigitAsciiOffset=48;
  constexpr uint8_t hexDigitAsciiOffset=55;
  int8_t ret=-1; // indicates that hex char wasnt an appropriate value
  hexChar=toupper(hexChar);
  if(hexChar <= '9' && hexChar >= '0')
    ret=hexChar-decDigitAsciiOffset;
  else if(hexChar<='F' && hexChar >= 'A')
    ret=hexChar-hexDigitAsciiOffset;
  return ret;
}


bool hexStrToUid(const String& str, Uid uid)
{
  uint8_t j=0;
  uint32_t strLen=str.length();
  if(strLen!=8)
    return false;
  for(uint8_t i=0;i<strLen;i+=2)
  {
    int8_t firstChar=hexCharToDec(str[i]);
    int8_t secondChar=hexCharToDec(str[i+1]);
    if(firstChar==-1 || secondChar==-1)
      return false;
    uid[j++]=(uint8_t)((firstChar<<4)|secondChar);
  }
  return true;
}


void setup() {
  // put your setup code here, to run once:
  BT.begin("Pwny");
  BT.enableSSP();
  BT.setTimeout(1500);
  irrecv.enableIRIn();  // Start the receiver
  irsend.begin();
  irrecv.pause();
  Serial.begin(115200);
  //nfc.begin();
  TimerHandle_t batteryTimer=xTimerCreate("Battery", pdMS_TO_TICKS(BAT_LVL_PERIOD_MS), pdTRUE, 0, batteryTimerCallback);
  xTimerStart(batteryTimer,0);
}

void loop() {
  // put your main code here, to run repeatedly:
  beginning:
  if(BT.available())
  {
  //BT.readString().toCharArray(msg,MSG_MAX_LEN);
  //if(!splitString(msg,":",tokens))
    String recdMsg=BT.readStringUntil('\n');
    #if TESTING == 1
    Serial.println(recdMsg);
    #endif
    if(recdMsg=="bat")
    {
      getAndSendBatLvl();
      //goto beginning;
    }
    else if((tokenNum=stringSplit(recdMsg,":",tokens))<2)
    {
      Serial.println(F("Wrong format, try again!"));
      BT.write("dbg:Wrong message format");
      BT.empty();
      //goto beginning;
    }
    else
    {
      const String& modeString=tokens[0];
      Mode mode=determineMode(modeString);
      const String& command=tokens[1];
      if(command=="cap" && ( (mode==Mode::IR && tokenNum==(uint8_t)TokenNumCap::IR) 
      || (mode==Mode::NFC && tokenNum==(uint8_t)TokenNumCap::NFC)|| (mode==Mode::SUB_GHZ && tokenNum==(uint8_t)TokenNumCap::SUB_GHZ) ) )
      {
        state=State::CAPTURE;
        Serial.println(F("Capture command received!"));
      }
      else if(command=="rep" && ( (mode==Mode::IR && tokenNum==(uint8_t)TokenNumRep::IR) 
      || (mode==Mode::NFC && tokenNum==(uint8_t)TokenNumRep::NFC)|| (mode==Mode::SUB_GHZ && tokenNum==(uint8_t)TokenNumRep::SUB_GHZ) ) )
      {
        state=State::REPLAY;
        Serial.println(F("Replay command received!"));
      }
      else
        state=State::UNREC;

      switch (state)
      {
        case State::CAPTURE:
          switch(mode)
          {
            case Mode::IR:
            {
              irrecv.resume();  // Receive the next value
              Serial.println(F("Capturing the ir signal..."));
              again:
              while (!irrecv.decode(&results))
              {
                if (BT.available() && BT.read()=='c')
                {
                  BT.empty();
                  BT.write("dbg:Cancelled IR capture");
                  //BT.println("Capture cancelled!");
                  goto beginning;
                }
              } 
              if (results.value==IR_REPEAT_CODE)
              {
                irrecv.resume();
                goto again;
              }
              if(results.decode_type!=UNKNOWN)
              {
                buildMsg(msgToSend,results);
                BT.write(msgToSend);
              }
              else
              {
                Serial.println("ir:unknown code");
                BT.write("dbg:Unknown ir code");
                irrecv.resume();
                goto again;
              }
              break;
            }
            case Mode::NFC:
            {
              Serial.println(F("Capturing the nfc signal..."));
              #if TESTING == 1
              buildMsg(msgToSend,nfcUid);
              BT.write(msgToSend);
              #endif
              bool reading=true;
              while(reading)
              {
                bool uidReadSuccess = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, nfcUid, &nfcUidLen,50);
                if(uidReadSuccess)
                {
                  if(nfcUidLen!=MIFARE_CLASSIC_UID_LEN)
                  {
                    Serial.println("Not a MIFARE classic tag!");
                  }
                  else
                  {
                    buildMsg(msgToSend,nfcUid);
                    BT.write(msgToSend);
                  }
                  reading=false;
                }
                if (BT.available() && BT.read()=='c')
                {
                  reading=false;
                  if(!uidReadSuccess)
                  {
                    Serial.println("UID read cancelled!");
                  }
                }
              }
              break;
            }
            case Mode::SUB_GHZ:
            {
              Serial.println(F("Capturing the sub ghz signal..."));
              #if TESTING == 1
              buildMsg(msgToSend,433.92);
              BT.write(msgToSend);
              #endif
              //logic here
              break;
            }
          }
          break;
        case State::REPLAY:
          switch(mode)
          {
            case Mode::IR:
            {          
              if(irsend.send((decode_type_t)atoi(tokens[2].c_str()),strtoull(tokens[3].c_str(),NULL,10),(uint16_t)strtoul(tokens[4].c_str(),NULL,10)))
              {
                Serial.println(F("Sent the ir signal!"));
                BT.write("dbg:Sent the ir signal");
              }
              else
              {
                Serial.println(F("Failed sending the ir signal!"));
                BT.write("dbg:Failed sending the ir signal");
              }
              break;
            }
              case Mode::NFC:
              {
                //logic here
                bool isNfcCodeValid=hexStrToUid(tokens[2],nfcUid);
                if(!isNfcCodeValid)
                {
                  Serial.println(F("Invalid NFC code"));
                  BT.write("dbg:Invalid NFC code");
                  break;
                }
                if (!nfcBruteForceAuth(nfcUid, blockNum)) {
                    Serial.println(F("NFC auth failure"));
                    BT.write("dbg:NFC auth failure");
                    break; 
                }

                if (!nfc.mifareclassic_ReadDataBlock(blockNum, blockData)) {
                    Serial.println(F("NFC read failure"));
                    BT.write("dbg:NFC read failure");
                    break;
                }

                changeBlockUid(nfcUid, blockData, blockNum);
                if (!nfc.mifareclassic_WriteDataBlock(blockNum, blockData)) {
                    Serial.println(F("NFC write failure"));
                    BT.write("dbg:NFC write failure");
                    break;
                }

                Serial.println(F("NFC write success"));
                BT.write("dbg:NFC write success");
                break;
              }
              case Mode::SUB_GHZ:
              {
                //logic here
                Serial.println(F("Sent the sub ghz signal"));
                BT.write("dbg:Sent the sub ghz signal");
                break;
              }
          }
          break;
        case State::UNREC:
          Serial.println(F("Error: Unrecognized command..."));
          BT.write("dbg:Unrecognized command");
          BT.empty();
          break;
      }
    }
    //emptyBTBuf();
  }
}