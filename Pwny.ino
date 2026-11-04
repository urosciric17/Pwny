#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRutils.h>
#include <IRsend.h>
#include <BluetoothSerial.h>
#include <Adafruit_PN532.h>
#include <Wire.h>
#include <inttypes.h>
#include <RadioLib.h>
#include "driver/rmt.h"
#include "esp_err.h"
//#include "soc/soc.h"
//#include "soc/rtc_cntl_reg.h"
//#include <ctype.h>

#define PN532_RESET (2)  //was 3
#define ASSERT_BOUNDS(INDEX, SIZE) \
  if (INDEX < 0 || INDEX >= SIZE) return false;
#define skip() goto beginning
#define CS_PIN 27
#define IRQ_PIN 18
#define IRQ2_PIN 19
#define TESTING 0

enum class LogCode : uint8_t {
  IR_UNKNOWN_CODE,
  IR_CAP_START,
  IR_CAP_SUCCESS,
  IR_CAP_CANCEL,
  IR_SEND_FAIL,
  IR_SEND_SUCCESS,
  NFC_INV_UID,
  NFC_UID_READ_START,
  NFC_UID_READ_SUCCESS,
  NFC_UID_READ_CANCEL,
  NFC_NOT_MIFARE_CLASSIC,
  NFC_TARGET_NOT_FOUND,
  NFC_AUTH_FAIL,
  NFC_READ_FAIL,
  NFC_WRITE_START,
  NFC_WRITE_FAIL,
  NFC_WRITE_SUCCESS,
  NFC_WRITE_CANCEL,
  SUBGHZ_CAP_START,
  SUBGHZ_CAP_CANCEL,
  SUBGHZ_CAP_SUCCESS,
  SUBGHZ_SEND_FAIL,
  SUBGHZ_SEND_SUCCESS,
  UNREC_COMMAND
};

enum class Mode : uint8_t {
  IR,
  NFC,
  SUB_GHZ,
  INVALID
};
enum class State : uint8_t {
  UNREC,
  CAPTURE,
  REPLAY,
};

enum class TokenNumCap : uint8_t {
  IR = 2,
  NFC = 2,
  SUB_GHZ = 3
};

enum class TokenNumRep : uint8_t {
  IR = 5,
  NFC = 3,
  SUB_GHZ = 4
};

typedef byte Uid[4];
typedef byte nfcKey[6];

void __attribute__((noreturn)) halt(void) {
  while (1)
    ;
}

/*enum class Signal{
  IR,
  NFC,
  SUB_GHZ
};*/

nfcKey commonKeys[] = {
  { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF },
  { 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5 },
  { 0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7 },
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
  { 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5 },
  { 0x4D, 0x3A, 0x99, 0xC3, 0x51, 0xDD },
  { 0x1A, 0x98, 0x2C, 0x7E, 0x45, 0x9A },
  { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF },
  { 0x71, 0x4C, 0x5C, 0x88, 0x6E, 0x97 },
  { 0x58, 0x7E, 0xE5, 0xF9, 0x35, 0x0F },
  { 0xA0, 0x47, 0x8C, 0xC3, 0x90, 0x91 },
  { 0x53, 0x3C, 0xB6, 0xC7, 0x23, 0xF6 },
  { 0x8F, 0xD0, 0xA4, 0xF2, 0x56, 0xE9 },
  { 0xA6, 0x45, 0x98, 0xA7, 0x74, 0x78 },
  { 0x26, 0x94, 0x0B, 0x21, 0xFF, 0x5D },
  { 0xFC, 0x00, 0x01, 0x87, 0x78, 0xF7 },
  { 0x00, 0x00, 0x0F, 0xFE, 0x24, 0x88 },
  { 0x5C, 0x59, 0x8C, 0x9C, 0x58, 0xB5 },
  { 0xE4, 0xD2, 0x77, 0x0A, 0x89, 0xBE },
  { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55 },
  { 0x43, 0x4F, 0x4D, 0x4D, 0x4F, 0x41 },
  { 0x43, 0x4F, 0x4D, 0x4D, 0x4F, 0x42 },
  { 0x47, 0x52, 0x4F, 0x55, 0x50, 0x41 },
  { 0x47, 0x52, 0x4F, 0x55, 0x50, 0x42 },
  { 0x50, 0x52, 0x49, 0x56, 0x41, 0x41 },
  { 0x50, 0x52, 0x49, 0x56, 0x41, 0x42 },
  { 0x02, 0x97, 0x92, 0x7C, 0x0F, 0x77 },
  { 0xEE, 0x00, 0x42, 0xF8, 0x88, 0x40 },
  { 0x72, 0x2B, 0xFC, 0xC5, 0x37, 0x5F },
  { 0xF1, 0xD8, 0x3F, 0x96, 0x43, 0x14 },
  { 0x54, 0x72, 0x61, 0x76, 0x65, 0x6C },
  { 0x77, 0x69, 0x74, 0x68, 0x75, 0x73 },
  { 0x4A, 0xF9, 0xD7, 0xAD, 0xEB, 0xE4 },
  { 0x2B, 0xA9, 0x62, 0x1E, 0x0A, 0x36 },
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 },
  { 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC },
  { 0xB1, 0x27, 0xC6, 0xF4, 0x14, 0x36 },
  { 0x12, 0xF2, 0xEE, 0x34, 0x78, 0xC1 },
  { 0x34, 0xD1, 0xDF, 0x99, 0x34, 0xC5 },
  { 0x55, 0xF5, 0xA5, 0xDD, 0x38, 0xC9 },
  { 0xF1, 0xA9, 0x73, 0x41, 0xA9, 0xFC },
  { 0x33, 0xF9, 0x74, 0xB4, 0x27, 0x69 },
  { 0x14, 0xD4, 0x46, 0xE3, 0x33, 0x63 },
  { 0xC9, 0x34, 0xFE, 0x34, 0xD9, 0x34 },
  { 0x19, 0x99, 0xA3, 0x55, 0x4A, 0x55 },
  { 0x27, 0xDD, 0x91, 0xF1, 0xFC, 0xF1 },
  { 0xA9, 0x41, 0x33, 0x01, 0x34, 0x01 },
  { 0x99, 0xC6, 0x36, 0x33, 0x44, 0x33 },
  { 0x43, 0xAB, 0x19, 0xEF, 0x5C, 0x31 },
  { 0xA0, 0x53, 0xA2, 0x92, 0xA4, 0xAF },
};

rmt_item32_t slow_blink[] = {
  // 8 items of pure HIGH (16 phases of 31250us = 500,000us)
  { { { 31250, 1, 31250, 1 } } },
  { { { 31250, 1, 31250, 1 } } },
  { { { 31250, 1, 31250, 1 } } },
  { { { 31250, 1, 31250, 1 } } },
  { { { 31250, 1, 31250, 1 } } },
  { { { 31250, 1, 31250, 1 } } },
  { { { 31250, 1, 31250, 1 } } },
  { { { 31250, 1, 31250, 1 } } },
  { { { 31250, 1, 31250, 1 } } },
  { { { 31250, 1, 31250, 1 } } },
  { { { 31250, 1, 31250, 1 } } },
  { { { 31250, 1, 31250, 1 } } },
  { { { 31250, 1, 31250, 1 } } },
  { { { 31250, 1, 31250, 1 } } },
  { { { 31250, 1, 31250, 1 } } },
  { { { 31250, 1, 31250, 1 } } },

  // 8 items of pure LOW
  { { { 31250, 0, 31250, 0 } } },
  { { { 31250, 0, 31250, 0 } } },
  { { { 31250, 0, 31250, 0 } } },
  { { { 31250, 0, 31250, 0 } } },
  { { { 31250, 0, 31250, 0 } } },
  { { { 31250, 0, 31250, 0 } } },
  { { { 31250, 0, 31250, 0 } } },
  { { { 31250, 0, 31250, 0 } } },
  { { { 31250, 0, 31250, 0 } } },
  { { { 31250, 0, 31250, 0 } } },
  { { { 31250, 0, 31250, 0 } } },
  { { { 31250, 0, 31250, 0 } } },
  { { { 31250, 0, 31250, 0 } } },
  { { { 31250, 0, 31250, 0 } } },
  { { { 31250, 0, 31250, 0 } } },
  { { { 31250, 0, 31250, 0 } } }
};

const rmt_item32_t test_signal[] = {
  { { { 750, 1, 250, 0 } } },  // Item 0: 500us High, 500us Low
  { { { 750, 1, 250, 0 } } },  // Item 1: 500us High, 500us Low
  { { { 750, 1, 250, 0 } } },  // Item 2: 500us High, 500us Low
  { { { 750, 1, 250, 0 } } },  // Item 3: 500us High, 500us Low
  { { { 750, 1, 250, 0 } } },  // Item 4: 500us High, 500us Low
  { { { 750, 1, 250, 0 } } },  // Item 5: 500us High, 500us Low
  { { { 750, 1, 250, 0 } } },  // Item 6: 500us High, 500us Low
  { { { 750, 1, 250, 0 } } },  // Item 7: 500us High, 500us Low
  { { { 750, 1, 250, 0 } } }   // Item 8: 500us High, 500us Low
};


constexpr uint8_t sendPin = 32;  // ESP8266 GPIO pin to use. Recommended: 4 (D2).
constexpr uint8_t recvPin = 34;
constexpr uint8_t batPin = 35;
constexpr uint8_t rx2_pin = 16;
constexpr uint8_t tx2_pin = 17;
constexpr uint8_t LOOP_TASK_PRIORITY = 1;
constexpr uint16_t BAT_LVL_PERIOD_MS = 30000;
constexpr uint8_t MAX_TOKENS = 5;
constexpr uint16_t MAX_LVL_TIMES = 1000;
constexpr uint16_t MAX_SUBGHZ_SIGNAL_SIZE = MAX_LVL_TIMES/2;
constexpr uint16_t MAX_SUBGHZ_SIGNAL_STR_SIZE = MAX_SUBGHZ_SIGNAL_SIZE*4;
constexpr uint8_t MAX_TOKEN_SIZE = 20;
constexpr uint8_t MSG_MAX_LEN = 100;
constexpr uint8_t CAP_NUM_TOKENS = 2;
constexpr uint8_t REP_NUM_TOKENS = 5;
constexpr uint64_t IR_REPEAT_CODE = 0xFFFFFFFFFFFFFFFF;
constexpr uint8_t MIFARE_CLASSIC_UID_LEN = 4;
constexpr uint8_t NFC_READ_TIMEOUT_MS = 50;
constexpr uint16_t MAX_SUBGHZ_DURATION = 32767;
uint32_t blockNum = 0;
Uid nfcUid;
uint8_t nfcUidLen;
decode_results results;
String tokens[MAX_TOKENS];
String lvlTimes[MAX_LVL_TIMES];
//String modeString;
//String command;
uint8_t batLvl;
unsigned long lastRecordedTime = 0;
uint8_t tokenNum = 0;
constexpr uint16_t batAdcMaxMV = 2050;
constexpr uint16_t batAdcMinMV = 1600;
constexpr uint8_t batMinPerc = 0;
constexpr uint8_t batMaxPerc = 100;

byte blockData[16] = { 0x01 };
char msgToSend[MSG_MAX_LEN];

rmt_item32_t signalToSendBuf[MAX_SUBGHZ_SIGNAL_SIZE];

class IRSignal {
public:
  decode_type_t prot;
  uint64_t code;
  int nbits;
  IRSignal() {}
  void set(decode_type_t type, uint64_t val, uint16_t bits) {
    prot = type;
    code = val;
    nbits = bits;
  }
};

class BTHelper : public BluetoothSerial {
private:
  SemaphoreHandle_t mutex;
  static constexpr uint8_t LOG_MAX_LEN = 10;
public:
  BTHelper() {
    mutex = xSemaphoreCreateMutex();
  }
  void write(const char* msg) {
    size_t msgLen = strlen(msg);
    xSemaphoreTake(mutex, portMAX_DELAY);
    BluetoothSerial::write(msgLen);
    BluetoothSerial::write((const uint8_t*)msg, msgLen);
    xSemaphoreGive(mutex);
  }
  void log(LogCode code) {
    buildMsg(msgToSend, LOG_MAX_LEN, code);
    write(msgToSend);
  }
  void empty() {
    while (BluetoothSerial::available())
      BluetoothSerial::read();
  }
  ~BTHelper() {
    vSemaphoreDelete(mutex);
  }
};

BTHelper BT;
State state;
IRSignal irSignal;
String subghzSignalStr; 
//Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET, &Wire);
Adafruit_PN532 nfc(PN532_RESET, &Serial2);
IRsend irsend(sendPin);  // Set the GPIO to be used to sending the message.
IRrecv irrecv(recvPin);
SPIClass hspi(HSPI);
CC1101 radio = new Module(CS_PIN, IRQ_PIN, RADIOLIB_NC, IRQ2_PIN, hspi);

Mode determineMode(const String& modeString) {
  Mode mode = Mode::INVALID;
  if (modeString == "ir")
    mode = Mode::IR;
  else if (modeString == "nfc")
    mode = Mode::NFC;
  else if (modeString == "ghz")
    mode = Mode::SUB_GHZ;

  return mode;
}

inline void buildMsg(char* msg, const decode_results& irResults) {
  snprintf(msg, MSG_MAX_LEN, "ir:%d:%" PRIu64 ":%" PRIu16, irResults.decode_type, irResults.value, irResults.bits);
}

inline void buildMsg(char* msg, const uint8_t batteryLevel) {
  snprintf(msg, MSG_MAX_LEN, "bat:%" PRIu8, batteryLevel);
}

inline void buildMsg(char* msg, const Uid uid) {
  snprintf(msg, MSG_MAX_LEN, "nfc:%02X%02X%02X%02X", uid[0], uid[1], uid[2], uid[3]);
}

inline void buildMsg(char* msg, const double frequency, const char* code) {
  snprintf(msg, MSG_MAX_LEN, "ghz:%" PRIu32 ":%s", (uint32_t)(frequency * 100.0), code);
}

inline void buildMsg(char* msg, size_t msgLen, LogCode code) {
  snprintf(msg, msgLen, "log:%" PRIu8, code);
}

inline void changeBlockUid(Uid newUid, byte* outBlockData, uint32_t blockNum) {
  memcpy(outBlockData, newUid, MIFARE_CLASSIC_UID_LEN);
  outBlockData[4] = calcBCC(newUid);
}

void printState(const esp_err_t state, const char* msgSuccess, const char* msgFail) {
  if (state == ESP_OK) {
    Serial.println(msgSuccess);
  } else {
    Serial.printf("%s:", msgFail);
    Serial.println(esp_err_to_name(state));
  }
}

void init_rmt(void) {
  esp_err_t state;
  constexpr size_t rx_buf_size_in_tx_mode = (size_t)0;
  constexpr int default_flags = 0;
  rmt_config_t _rmt_config = RMT_DEFAULT_CONFIG_TX((gpio_num_t)IRQ_PIN, RMT_CHANNEL_0);
  state = rmt_config(&_rmt_config);
  printState(state, "Config success", "Config fail");
  assert(state == ESP_OK);
  state = rmt_driver_install(RMT_CHANNEL_0, rx_buf_size_in_tx_mode, default_flags);
  printState(state, "driver install success", "driver install fail");
  assert(state == ESP_OK);
}

void initSubghzSignalBuf(rmt_item32_t* signalToSendBuf)
{
  for (uint16_t i=0;i<MAX_SUBGHZ_SIGNAL_SIZE;i++)
  {
    signalToSendBuf[i].level0=1;
    signalToSendBuf[i].level1=0;
  }
}

bool fillSubghzSignalBuf(rmt_item32_t* signalToSendBuf,uint16_t signalSize,String* lvlTimes)
{
  for (uint16_t i=0,j=0;i<signalSize;i++,j+=2)
  {
    int dur0=atoi(lvlTimes[j].c_str());
    if(dur0>(int)MAX_SUBGHZ_DURATION || dur0<=0)
      return false;
    signalToSendBuf[i].duration0=dur0;
    int dur1=atoi(lvlTimes[j+1].c_str());
    if(dur1>(int)MAX_SUBGHZ_DURATION || dur1<=0)
      return false;
    signalToSendBuf[i].duration1=dur1;
  }
  return true;
}

bool fillSubhzSignalStr(String& outSubghzStr,uint16_t strMaxSize,rmt_item32_t* signalToSendBuf,uint16_t signalSize)
{
  outSubghzStr.remove(0);
  char durStr[10];
  bool first=true;
  for(uint16_t i=0;i<signalSize;i++)
  {
    if(first)
    {
      snprintf(durStr, sizeof(durStr), "%u", signalToSendBuf[i].duration0);
      first=false;
    }
    else
    {
      snprintf(durStr, sizeof(durStr), "_%u", signalToSendBuf[i].duration0);
    }
    outSubghzStr+=durStr;
    snprintf(durStr, sizeof(durStr), "_%u", signalToSendBuf[i].duration1);
    outSubghzStr+=durStr;
    if(outSubghzStr.length()>strMaxSize)
      return false;
  }
  return true;
}


uint16_t stringSplit(String str, const String delimiter,String* tokens,uint16_t maxTokens) {
  int16_t index;
  uint8_t i = 0;
  while ((index = str.indexOf(delimiter)) != -1 && i < maxTokens) {
    tokens[i] = str.substring(0, index);
    str = str.substring(index + delimiter.length());
    i++;
  }
  if (i < maxTokens)
    tokens[i] = str;
  return i + 1;
}

inline uint8_t getBatLvl(uint16_t analogVal) {
  analogVal = constrain(analogVal,batAdcMinMV,batAdcMaxMV);
  return map(analogVal, batAdcMinMV, batAdcMaxMV, batMinPerc, batMaxPerc);
}

uint16_t getBlockAverage(uint8_t pin, uint8_t samples = 5) {
  uint32_t sum = 0;
  for (uint8_t i = 0; i < samples; i++) {
    sum += analogReadMilliVolts(pin);
    ets_delay_us(50); // Small delay to let the ADC sample-and-hold capacitor stabilize
  }
  uint16_t batAvg = sum / samples;
  return batAvg;
}

inline uint8_t approx(uint8_t val,uint8_t step = 5)
{
  return ( (val+step/2)/step*step );
}

void getAndSendBatLvl() {
  char msgToSend[MSG_MAX_LEN];
  uint8_t batLvl = approx( getBatLvl (getBlockAverage(batPin) ) );
  Serial.printf("bat lvl:%hhu",batLvl);
  buildMsg(msgToSend, batLvl);
  BT.write(msgToSend);
}

void batteryTimerCallback(TimerHandle_t xTimer) {
  static bool first = true;

  if (first)
    first = false;
  else
    getAndSendBatLvl();
}

inline byte calcBCC(Uid uid) {
  byte BCC = uid[0];
  for (uint8_t i = 1; i < MIFARE_CLASSIC_UID_LEN; i++)
    BCC ^= uid[i];
  return BCC;
}

bool nfcBruteForceAuth(Uid uid, uint32_t blockNum, bool& cancelled) {
  cancelled = false;
  bool success = false;
  bool isKeyB = false;
  constexpr uint8_t commonKeysSize = sizeof(commonKeys) / sizeof(commonKeys[0]);
  for (uint8_t i = 0; i < 2; i++) {
    for (uint8_t j = 0; j < commonKeysSize; j++) {
      Serial.printf("Trying key %hhu...\n", j);
      success = nfc.mifareclassic_AuthenticateBlock(uid, MIFARE_CLASSIC_UID_LEN, blockNum, isKeyB, (byte*)commonKeys[j]);
      if (success)
        return true;
      if (BT.available() && BT.read() == 'c') {
        cancelled = true;
        return false;
      }
    }
    isKeyB = true;
  }
  return success;
}

int8_t hexCharToDec(char hexChar) {
  constexpr uint8_t decDigitAsciiOffset = 48;
  constexpr uint8_t hexDigitAsciiOffset = 55;
  int8_t ret = -1;  // indicates that hex char wasnt an appropriate value
  //hexChar = toupper(hexChar);
  if (hexChar <= '9' && hexChar >= '0')
    ret = hexChar - decDigitAsciiOffset;
  else if (hexChar <= 'F' && hexChar >= 'A')
    ret = hexChar - hexDigitAsciiOffset;
  return ret;
}


bool hexStrToUid(const String& str, Uid uid) {
  uint8_t j = 0;
  uint32_t strLen = str.length();
  if (strLen != 8)
    return false;
  for (uint8_t i = 0; i < strLen; i += 2) {
    int8_t firstChar = hexCharToDec(str[i]);
    int8_t secondChar = hexCharToDec(str[i + 1]);
    if (firstChar == -1 || secondChar == -1)
      return false;
    uid[j++] = (uint8_t)((firstChar << 4) | secondChar);
  }
  return true;
}

void printPN532Version(uint32_t versiondata) {
  Serial.print("Found chip PN5");
  Serial.println((versiondata >> 24) & 0xFF, HEX);
  Serial.print("Firmware ver. ");
  Serial.print((versiondata >> 16) & 0xFF, DEC);
  Serial.print('.');
  Serial.println((versiondata >> 8) & 0xFF, DEC);
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(batPin,INPUT);
  BT.begin("Pwny");
  BT.enableSSP();
  BT.setTimeout(1500);
  irrecv.enableIRIn();  // Start the receiver
  irsend.begin();
  irrecv.pause();
  Serial2.begin(115200, SERIAL_8N1, rx2_pin, tx2_pin);
  while (!Serial2);
  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    Serial.print("Didn't find PN532 board");
    //halt();
  } else
    printPN532Version(versiondata);
  hspi.begin();
  int status = radio.begin();
  if (status == RADIOLIB_ERR_NONE) {
    Serial.println(F("CC1101 init success"));
    Serial.print("Freq err code:");
    Serial.println(radio.setFrequency(433.92));  // Change to 315.0 if in the US/China
    Serial.print("Mod err code:");
    Serial.println(radio.setOOK(true));
  }
  status = radio.transmitDirectAsync();
  if (status == RADIOLIB_ERR_NONE) {
    Serial.println(F("CC1101 transmit mode set!"));
  } else {
    Serial.print(F("CC1101 transmit mode set failed:"));
    Serial.println(status);
  }
  
  init_rmt();
  initSubghzSignalBuf(signalToSendBuf);
  // Set ADC resolution to 12-bit (0 to 4095)
  analogReadResolution(12); 
  // Set attenuation to 11 dB (expands full-scale range up to ~3.1V)
  analogSetAttenuation(ADC_11db);
  TimerHandle_t batteryTimer = xTimerCreate("Battery", pdMS_TO_TICKS(BAT_LVL_PERIOD_MS), pdTRUE, 0, batteryTimerCallback);
  xTimerStart(batteryTimer, 0);
  subghzSignalStr.reserve(MAX_SUBGHZ_SIGNAL_STR_SIZE);
}

void loop() {
// put your main code here, to run repeatedly:
  //Serial.printf("min:%u,max:%u,current:%u\n",batAdcMin,batAdcMax,analogRead(batPin));
beginning:
  if (BT.available()) {
    //BT.readString().toCharArray(msg,MSG_MAX_LEN);
    //if(!splitString(msg,":",tokens))
    String recdMsg = BT.readStringUntil('\n');
    Serial.println(recdMsg);
    if (recdMsg == "bat") {
      getAndSendBatLvl();
      //goto beginning;
    } else if ((tokenNum = stringSplit(recdMsg, ":", tokens,MAX_TOKENS)) < 2) {
      Serial.println(F("Wrong format, try again!"));
      //BT.write("dbg:Wrong message format");
      BT.empty();
      //goto beginning;
    } else {
      const String& modeString = tokens[0];
      Mode mode = determineMode(modeString);
      const String& command = tokens[1];
      if (command == "cap" && ((mode == Mode::IR && tokenNum == (uint8_t)TokenNumCap::IR) || (mode == Mode::NFC && tokenNum == (uint8_t)TokenNumCap::NFC) || (mode == Mode::SUB_GHZ && tokenNum == (uint8_t)TokenNumCap::SUB_GHZ))) {
        state = State::CAPTURE;
        Serial.println(F("Capture command received!"));
      } else if (command == "rep" && ((mode == Mode::IR && tokenNum == (uint8_t)TokenNumRep::IR) || (mode == Mode::NFC && tokenNum == (uint8_t)TokenNumRep::NFC) || (mode == Mode::SUB_GHZ && tokenNum == (uint8_t)TokenNumRep::SUB_GHZ))) {
        state = State::REPLAY;
        Serial.println(F("Replay command received!"));
      } else
        state = State::UNREC;

      switch (state) {
        case State::CAPTURE:
          switch (mode) {
            case Mode::IR:
              {
                irrecv.resume();  // Receive the next value
                Serial.println(F("Capturing the ir signal..."));
                BT.log(LogCode::IR_CAP_START);
again:
                while (!irrecv.decode(&results)) {
                  if (BT.available() && BT.read() == 'c') {
                    BT.empty();
                    BT.log(LogCode::IR_CAP_CANCEL);
                    goto beginning;
                  }
                }
                if (results.value == IR_REPEAT_CODE) {
                  irrecv.resume();
                  goto again;
                }
                if (results.decode_type != UNKNOWN) {
                  buildMsg(msgToSend, results);
                  BT.write(msgToSend);
                  BT.log(LogCode::IR_CAP_SUCCESS);
                } else {
                  Serial.println("ir:unknown code");
                  BT.log(LogCode::IR_UNKNOWN_CODE);
                  irrecv.resume();
                  goto again;
                }
                break;
              }
            case Mode::NFC:
              {
                Serial.println(F("Capturing the nfc signal..."));
                BT.log(LogCode::NFC_UID_READ_START);
                bool reading = true;
                while (reading) {
                  bool uidReadSuccess = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, nfcUid, &nfcUidLen, NFC_READ_TIMEOUT_MS);
                  if (uidReadSuccess) {
                    if (nfcUidLen != MIFARE_CLASSIC_UID_LEN) {
                      Serial.println("Not a MIFARE classic tag!");
                      BT.log(LogCode::NFC_NOT_MIFARE_CLASSIC);
                    } else {
                      buildMsg(msgToSend, nfcUid);
                      BT.write(msgToSend);
                      BT.log(LogCode::NFC_UID_READ_SUCCESS);
                    }
                    reading = false;
                  }
                  if (BT.available() && BT.read() == 'c') {
                    reading = false;
                    if (!uidReadSuccess) {
                      Serial.println("UID read cancelled!");
                      BT.log(LogCode::NFC_UID_READ_CANCEL);
                    }
                  }
                }
                break;
              }
            case Mode::SUB_GHZ:
            {
              Serial.println(F("Capturing the sub ghz signal..."));
              BT.log(LogCode::SUBGHZ_CAP_START);
              delay(5000);
              uint16_t signalSize = (sizeof(test_signal) / sizeof(test_signal[0]));
              if(fillSubhzSignalStr(subghzSignalStr, MAX_SUBGHZ_SIGNAL_STR_SIZE, (rmt_item32_t*)test_signal, signalSize))
              {
                buildMsg(msgToSend,atof(tokens[2].c_str()),subghzSignalStr.c_str());
                BT.write(msgToSend);
                BT.log(LogCode::SUBGHZ_CAP_SUCCESS);
              }
              break;
            }
          }
          break;
        case State::REPLAY:
          switch (mode) {
            case Mode::IR:
              {
                if (irsend.send((decode_type_t)atoi(tokens[2].c_str()), strtoull(tokens[3].c_str(), NULL, 10), (uint16_t)strtoul(tokens[4].c_str(), NULL, 10))) {
                  Serial.println(F("Sent the ir signal!"));
                  BT.log(LogCode::IR_SEND_SUCCESS);
                } else {
                  Serial.println(F("Failed sending the ir signal!"));
                  BT.log(LogCode::IR_SEND_FAIL);
                }
                break;
              }
            case Mode::NFC:
              {
                bool isNfcCodeValid = hexStrToUid(tokens[2], nfcUid);
                Uid dummyUid;
                uint8_t dummyUidLen;
                for (uint8_t i = 0; i < MIFARE_CLASSIC_UID_LEN; i++)
                  Serial.print(nfcUid[i], HEX);
                Serial.println();
                if (!isNfcCodeValid) {
                  Serial.println(F("Invalid NFC code"));
                  BT.log(LogCode::NFC_INV_UID);
                  break;
                }
                BT.log(LogCode::NFC_WRITE_START);
                uint64_t startTime = millis();
                bool targetInlisted = false;
                while (!(targetInlisted = nfc.readPassiveTargetID(0, dummyUid, &dummyUidLen, 100U))) {
                  if (BT.available() && BT.read() == 'c')
                    break;
                }
                if (!targetInlisted) {
                  Serial.println(F("NFC writing cancelled"));
                  BT.log(LogCode::NFC_WRITE_CANCEL);
                  nfc.SAMConfig();
                  break;
                } else {
                  Serial.println("Target inlisted!");
                }
                bool authCancelled;
                if (!nfcBruteForceAuth(dummyUid, blockNum, authCancelled)) {
                  if (authCancelled) {
                    Serial.println(F("NFC write cancelled"));
                    BT.log(LogCode::NFC_WRITE_CANCEL);
                  } else {
                    Serial.println(F("NFC auth failure"));
                    BT.log(LogCode::NFC_AUTH_FAIL);
                  }
                  break;
                }
                /*if(!nfc.inListPassiveTarget())
                {
                  Serial.println("NFC target not detected!");
                  break;
                }*/
                if (!nfc.mifareclassic_ReadDataBlock(blockNum, blockData)) {
                  Serial.println(F("NFC read failure"));
                  BT.log(LogCode::NFC_READ_FAIL);
                  break;
                }
                changeBlockUid(nfcUid, blockData, blockNum);
                if (!nfc.mifareclassic_WriteDataBlock(blockNum, blockData)) {
                  Serial.println(F("NFC write failure"));
                  BT.log(LogCode::NFC_WRITE_FAIL);
                  break;
                }

                Serial.println(F("NFC write success"));
                BT.log(LogCode::NFC_WRITE_SUCCESS);
                break;
              }
            case Mode::SUB_GHZ:
            {
              radio.setFrequency(atof(tokens[2].c_str()));
              uint16_t lvlTimesSize=stringSplit(tokens[3],"_",lvlTimes,MAX_LVL_TIMES);
              uint16_t signalSize=lvlTimesSize/2;
              bool success=fillSubghzSignalBuf(signalToSendBuf,signalSize,lvlTimes);
              for(int i=0;i<signalSize;i++)
              {
                Serial.printf("%d:%d %d:%d\n",signalToSendBuf[i].level0, signalToSendBuf[i].duration0, signalToSendBuf[i].level1, signalToSendBuf[i].duration1);
              }
              esp_err_t state;
              if(success)
                state = rmt_write_items(RMT_CHANNEL_0, signalToSendBuf, (int)(signalSize), true);
              else
                state = ESP_FAIL;
              printState(state, "Transmit success", "Transmit failed");
              if (state == ESP_OK)
                BT.log(LogCode::SUBGHZ_SEND_SUCCESS);
              else
                BT.log(LogCode::SUBGHZ_SEND_FAIL);
              break;
            }
          }
          break;
        case State::UNREC:
          Serial.println(F("Error: Unrecognized command..."));
          BT.log(LogCode::UNREC_COMMAND);
          BT.empty();
          break;
      }
    }
    //BT.empty();
  }
}