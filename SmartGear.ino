#include <SD.h>
#include <SPI.h>
#include <bb_spi_lcd.h>
#include <BitBang_I2C.h>

BB_SPI_LCD lcd;

#ifdef TEENSYDUINO
extern "C" uint8_t external_psram_size;
EXTMEM uint8_t pCartMem[0x100000]; // 1MB of RAM to store game code+data
#else
#define BUILTIN_SDCARD 1
#endif

// 3 frames worth of audio
#define AUDIO_BUFFER_SIZE 735*3
extern int sg_main(char *szGame);
//TaskHandle_t Task1;
int iAudioTail, iAudioHead, iAudioAvailable, iAudioTotal, iAudioSampleSize;
unsigned char *pAudioBuffer;
//hw_timer_t * timer = NULL;
//portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
//SemaphoreHandle_t xSemaphore = NULL;
//
// Task running on the second CPU core
//
#ifdef FUTURE
void IRAM_ATTR onTimer() 
{
  unsigned char cSample;
  static unsigned char cOldSample;
  
   if (iAudioAvailable) // data available
   {
//     cSample = (unsigned char)((pAudioBuffer[(iAudioHead*2)] + pAudioBuffer[(iAudioHead*2)+1]) >> 9); // convert from uint16_t to uint8_t
//     cSample = (unsigned char)(pAudioBuffer[iAudioHead] >> 8);
     cSample = (pAudioBuffer[iAudioHead] >> 2) + 0x80;
//     cSample = pAudioBuffer[iAudioHead];
     iAudioHead++; // skip stereo samples
     iAudioAvailable--;
     if (iAudioHead == AUDIO_BUFFER_SIZE) // need to wrap around
        iAudioHead = 0;
   }
   else // no data, write silence
   {
      cSample = 0x80; // middle value   
   }
   if (cSample != cOldSample) // only write if it changed
   {
      cOldSample = cSample;
      dacWrite(26, cSample);
   }
} /* onTimer() */
void Task1code(void * parameter)
{
  // Set up a timer interrupt for the DAC output loop
  timer = timerBegin(0, 80, true);          // use timer 0, pre-scaler is 80, count up
  timerAttachInterrupt(timer, &onTimer, true);    // P3= edge triggered
  timerAlarmWrite(timer, 40, true);               // will trigger 25000 times per second
  timerAlarmEnable(timer);                        // enable

  while (1) // run forever
  {
    delay(100); // do nothing
  }
} /* Task1code() */
#endif

void setup() {

  iAudioTail = iAudioHead = iAudioAvailable = 0; // audio circular buffer
  iAudioTotal = AUDIO_BUFFER_SIZE; // number of samples that fit in the buffer
 // pAudioBuffer = (unsigned char *)malloc(AUDIO_BUFFER_SIZE*1);
//  pinMode(25, OUTPUT);
//  digitalWrite(25, HIGH);
//  Serial.println("About to init SD card");

#ifndef TEENSYDUINO
  // put your setup code here, to run once:
//#define CLK_PIN 18
//#define MISO_PIN 19
//#define MOSI_PIN 23
//#define SD_CS 22
//#define LCD_CS 5
//#define LCD_DC 21
//#define LCD_LED 14
//#define LCD_RESET -1
// FeatherS2/S3
#define LCD_WR 7
#define LCD_RD -1
#define LCD_CS 38
//#define LCD_CS 33
#define LCD_DC 3
#define LCD_RST 33
//#define LCD_RST 38
#define SD_CS 1
#define LCD_TYPE LCD_ILI9341
#define LCD_FLAGS FLAGS_NONE
//#define LCD_FLAGS FLAGS_SWAP_RB
#define BUS_WIDTH 8
//uint8_t u8Pins[BUS_WIDTH] = {10,11,12,14,17,18,5,6}; // FeatherS2/S3
uint8_t u8Pins[BUS_WIDTH] = {10,11,5,6,12,14,18,17}; // FeatherS2/S3
  lcd.beginParallel(LCD_TYPE, LCD_FLAGS, LCD_RST, LCD_RD, LCD_WR, LCD_CS, LCD_DC, BUS_WIDTH, u8Pins);
   lcd.setRotation(90);
   lcd.fillScreen(0);
   lcd.setFont(FONT_12x16);
   lcd.setTextColor(0x7e0,0x0000);

//static uint8_t ucTXBuf[4096];
//  spilcdSetTXBuffer(ucTXBuf, sizeof(ucTXBuf));
//  spilcdInit(&lcd, LCD_ILI9341, FLAGS_NONE, 40000000, LCD_CS, LCD_DC, LCD_RESET, LCD_LED, MISO_PIN, MOSI_PIN, CLK_PIN);
//  SPI.begin(CLK_PIN, MISO_PIN, MOSI_PIN, SD_CS);
#endif
lcd.println("About to init SD card");
  if(!SD.begin(BUILTIN_SDCARD, SPI, 20000000))
  {
    lcd.println("mount failed!");
    return;
  }
  else
  {
    lcd.println("mount succeeded!");
  }
  lcd.print("PSRAM Size: ");
#ifdef TEENSYDUINO
  lcd.print(external_psram_size, DEC);
#else
  lcd.print(ESP.getPsramSize(), DEC);
#endif
  lcd.println(" MBytes");
//  vSemaphoreCreateBinary  ( xSemaphore ); // create a semaphore to synchronize the audio queue
int i = 0;
  Serial.begin(115200);
  while (!Serial && i < 4) {
    i++;
    delay(1000);    
  };
#ifdef FUTURE
  // Start a second task to handle audio
  xTaskCreatePinnedToCore(
      Task1code, /* Function to implement the task */
      "Task1", /* Name of the task */
      2000,  /* Stack size in words */
      NULL,  /* Task input parameter */
      0,  /* Priority of the task */
      &Task1,  /* Task handle. */
      0); /* Core where the task should run (Core1 is where loop() is run) */
#endif
      
} // setup()

void loop() {
    // put your main code here, to run repeatedly:
//   sg_main("/DRILLER.ZIP"); // start SmartGear
//   sg_main("/PACMAN.ZIP");
   sg_main(NULL);
}
