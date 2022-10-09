/* Copyright (c) 2010 BitBank Software, Inc. */
/* Project started 12/21/2010 */
/* Written by Larry Bank */
//#include <odroid_go.h>
#include <stdint.h>
#include "my_windows.h"
#include "bb_spi_lcd.h" // custom SPI LCD library
//#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <stdio.h>
//int get_nprocs(void);
#include "smartgear.h"
#include "emu.h"
#include "emuio.h"
#include "sound.h"
#include <BitBang_I2C.h>

extern BB_SPI_LCD lcd;

#define NUNCHUCK_ADDR 0x52
typedef struct ncdata
{
   int LX, LY; // left joystick x/y position
   int RX, RY; // right joystick x/y position
   uint16_t u16Buttons; // button states
   uint8_t u8DPad; // d-pad
} NCDATA;

// Nintendo controller structures
static BBI2C bb;
static NCDATA ncd;
static SPILCD *pLCD;

static char *version = (char *)"beta 1.01";
static int iNumProcessors = 1;
static uint32_t u32Regions[64]; // changed tile bits
extern void LoadCheats();
extern int SG_SavePNG(char* file_name, unsigned char *pBitmap, int iWidth, int iHeight, int iPitch);
char szFileName[256];
BOOL SG_Sleep(uint64_t ns);
void DrawFrame(void);
void * SG_LCD(void *unused);
void SPI_LCD_Update(void);
void SG_TerminateGame(void);
void DrawBool(int x, int y, int value);
uint64_t SG_Clock(void);
static unsigned char *pAltScreen, /**pStretchScreen,*/ *pScreen, *pFB /* , *pFBMenu */;
//static int iFBPitch;
//static int fbfd; // framebuffer handle
static int iScreenSize, iTileWidth, iTileHeight;
//static int iKBHandle = -1;
static int iAudioVolume = 80;
int iROMSet;
//static int iScale = 1; // DEBUG
static int bDetectScroll = FALSE;
static int iScrollScale;
// shared by the emulator modules; needs to be NOT static
int iDisplayWidth, iDisplayHeight;
int sample_rate; // DEBUG - used by MAME sound engine
unsigned char *pISO; // pointer to CD image
unsigned int iISOSize; // size in bytes of the image
extern unsigned char *SG_LoadISO(char *, unsigned int *);
extern int iLCDType, bLCDFlip, iSPIChan, iSPIFreq, iDC, iReset, iLED; // GPIO lines used for SPI LCD display
extern int iGamma;
extern int iDisplayType;
extern int iLCDX, iLCDY;
extern int iDispOffX, iDispOffY;
static int iLCDOrientation;
extern int bVerbose;
static char szDir[256];
static char szGame[256];

//
// Current configuration for GPIO/Keyboard/GamePad inputs
//
extern int iButtonPins[CTRL_COUNT];
extern int iButtonKeys[CTRL_COUNT];
extern int iButtonGP[CTRL_COUNT];
extern int iButtonGPMap[16]; // map gamepad buttons back to SmartGear events
extern int iButtonMapping[CTRL_COUNT];

// for Gamepad / network access
enum _network_packet_types
{
SGNETWORK_NOP=0, // Nop
SGNETWORK_BUTTONS, // opponents button info (for P2 input on client/server setup)
SGNETWORK_FILEINFO, // file size info, creates output file
SGNETWORK_FILEDATA, // file data packets, closed when all data is received
SGNETWORK_MEMDATA, // for client/server 2P games
SGNETWORK_GAMEDATA, // for networked games (e.g. GBC link cable)
SGNETWORK_START, // begin client/server 2P game
SGNETWORK_END, // end client/server 2P game
SGNETWORK_PING, // used for benchmark tests
};

#define MAX_BLOCK_SIZE 3072
#define MAX_MEM_AREAS 10
uint32_t lMyIP;
unsigned char *pGameData = NULL; // compressed game delta info

extern unsigned char pszHome[];
char *szGameFourCC[10] = {(char*)"????",(char*)"NES_",(char*)"GG__",(char*)"GBC_",(char*)"TG16",(char*)"GEN_",(char*)"COIN",(char*)"CLEC",(char*)"SNES",(char*)"MSX_"};
char pszCapture[256], pszCurrentDir[256], pszLastDir[256], pszGame[256];
int iGameType;
extern int iAudioHead, iAudioTail, iAudioTotal, iAudioAvailable, iAudioSampleSize;
extern unsigned char *pAudioBuffer;
// bluetooth variables
int iGameSocket;
int iDeviceType;
int iTrace;

BOOL bDone = FALSE;
volatile BOOL bLCDChanged, bLCDBusy = FALSE;
BOOL bPerf = FALSE;
volatile BOOL bRunning = FALSE;
volatile BOOL bQuit = FALSE;
BOOL bAutoLoad = FALSE;
extern BOOL bHead2Head;
BOOL bShowFPS = FALSE;
BOOL bRegistered = TRUE;
extern BOOL bAudio;
extern BOOL bFramebuffer;
extern BOOL bStretch;
BOOL bUserAbort;
int iScreenX;
int iScreenY;
int iVideoSize;
int iPitch;
int iFrame;
uint64_t llStartTime;
volatile uint32_t u32PrevBits, u32GPIOBits, u32KeyBits, u32Joy0Bits, u32Joy1Bits, u32iCadeBits, u32ControllerBits;
volatile uint32_t u32RawJoy0Bits, u32RawJoy1Bits;
extern int iP1Control, iP2Control;
BOOL bStereo = FALSE;
BOOL b16BitAudio = TRUE;
BOOL bSlave = FALSE;
BOOL bMaster = FALSE;
BOOL bUseSound = TRUE;
int iSampleRate = 2; // 44400 - DEBUG
//PIL_PAGE pp; // the source bitmap setup of the current game
//PIL_VIEW pv; // the bitmap of the display
unsigned char *pBitmap;
SERIALQ *InQ, *OutQ; // used for game link; simulate for now
unsigned short *pColorConvert;
#define MAX_PREV_GAMES 14
GAME_BLOB blobs[MAX_PREV_GAMES+1];
char szGameNames[MAX_PREV_GAMES+1][64]; // first in the list is the most recently played
char szGameFiles[MAX_PREV_GAMES+1][256];
char pszSAVED[256];
int iPrevGame;
char *szSystemNames[] = {(char*)"NES",(char*)"GameGear/SMS",(char*)"GameBoy",(char*)"TG16/PC-Engine",(char*)"Genesis",(char*)"CoinOp",NULL};
unsigned short usPalConvert[256];
void wait_vsync(void);
int iAudioRate, iSoundBlock;
unsigned char *pSoundBuf;
//static void SG_InitJoysticks(void);
void SG_ResetGame(unsigned char *pszGame);
void SG_GetLeafName2(char *fname, char *leaf);

#ifdef FUTURE
int NES_Init(GAME_BLOB *, char *, int);
void NES_Terminate(GAME_BLOB *);
void NES_Play(GAME_BLOB *, BOOL, BOOL, uint32_t);
void NES_PostLoad(GAME_BLOB *);

int GG_Init(GAME_BLOB *, char *, int);
void GG_Terminate(GAME_BLOB *);
void GG_Play(GAME_BLOB *, BOOL, BOOL, uint32_t);
void GG_PostLoad(GAME_BLOB *);
#endif // FUTURE

//extern "C" {
int GB_Init(GAME_BLOB *, char *, int);
void GB_Terminate(GAME_BLOB *);
void GB_Play(GAME_BLOB *, BOOL, BOOL, uint32_t);
void GB_PostLoad(GAME_BLOB *);
//}

#ifdef FUTURE
int GEN_Init(GAME_BLOB *, char *, int);
void GEN_Terminate(GAME_BLOB *);
void GEN_Play(GAME_BLOB *, BOOL, BOOL, uint32_t);
void GEN_PostLoad(GAME_BLOB *);
#endif // FUTURE

int EMUTestName(char *);
void * SG_RunFrames(void *);
void SG_WritePrevList(void);
int SG_InitGame(unsigned char *pszROM);
int iWindowX, iWindowY;
//void MSXPlay(HANDLE hInst, HWND hWnd, char *szGame);
//void MSXReset(void);
//BOOL MSXLoadGame(int);

//void SNESPlay(HANDLE hInst, HWND hWnd, char *szGame);
//void SNESReset(void);
//BOOL SNESLoadGame(int);
#ifdef FUTURE
int CoinOp_Init(GAME_BLOB *, char *, int);
void CoinOp_Terminate(GAME_BLOB *);
void CoinOp_Play(GAME_BLOB *, BOOL, BOOL, uint32_t);
void CoinOp_PostLoad(GAME_BLOB *);

extern "C" {
int Pac_Init(GAME_BLOB *pBlob, char *pszROM, int iGameLoad);
void Pac_Terminate(GAME_BLOB *pBlob);
void Pac_Play(GAME_BLOB *pBlob, BOOL bAudio, BOOL bVideo, uint32_t ulSysKeys);
void PacPostLoad(void);
};
#endif // FUTURE

#ifdef FUTURE
int PCE_Init(GAME_BLOB *, char *, int);
void PCE_Terminate(GAME_BLOB *);
void PCE_Play(GAME_BLOB *, BOOL, BOOL, uint32_t);
void PCE_PostLoad(GAME_BLOB *);
#endif // FUTURE
// structure holding info for all of the emulators
EMULATOR emulators[7] = {{NULL, NULL, NULL, NULL, NULL}, // SYS_UNKNOWN
{NULL, NULL, NULL, NULL, NULL},//                        {NES_Init, NES_Terminate, NES_Play, NES_PostLoad},
{NULL, NULL, NULL, NULL, NULL},//                        {GG_Init, GG_Terminate, GG_Play, GG_PostLoad},
                        {GB_Init, GB_Terminate, GB_Play, GB_PostLoad, NULL},
{NULL, NULL, NULL, NULL, NULL},//                        {PCE_Init, PCE_Terminate, PCE_Play, PCE_PostLoad},
{NULL, NULL, NULL, NULL, NULL},//                        {GEN_Init, GEN_Terminate, GEN_Play, GEN_PostLoad},
{NULL, NULL, NULL, NULL, NULL}
//{CoinOp_Init, CoinOp_Terminate, CoinOp_Play, CoinOp_PostLoad, NULL}
                        };

unsigned char ucMirror[256]=
     {0, 128, 64, 192, 32, 160, 96, 224, 16, 144, 80, 208, 48, 176, 112, 240,
      8, 136, 72, 200, 40, 168, 104, 232, 24, 152, 88, 216, 56, 184, 120, 248,
      4, 132, 68, 196, 36, 164, 100, 228, 20, 148, 84, 212, 52, 180, 116, 244,
      12, 140, 76, 204, 44, 172, 108, 236, 28, 156, 92, 220, 60, 188, 124, 252,
      2, 130, 66, 194, 34, 162, 98, 226, 18, 146, 82, 210, 50, 178, 114, 242,
      10, 138, 74, 202, 42, 170, 106, 234, 26, 154, 90, 218, 58, 186, 122, 250,
      6, 134, 70, 198, 38, 166, 102, 230, 22, 150, 86, 214, 54, 182, 118, 246,
      14, 142, 78, 206, 46, 174, 110, 238, 30, 158, 94, 222, 62, 190, 126, 254,
      1, 129, 65, 193, 33, 161, 97, 225, 17, 145, 81, 209, 49, 177, 113, 241,
      9, 137, 73, 201, 41, 169, 105, 233, 25, 153, 89, 217, 57, 185, 121, 249,
      5, 133, 69, 197, 37, 165, 101, 229, 21, 149, 85, 213, 53, 181, 117, 245,
      13, 141, 77, 205, 45, 173, 109, 237, 29, 157, 93, 221, 61, 189, 125, 253,
      3, 131, 67, 195, 35, 163, 99, 227, 19, 147, 83, 211, 51, 179, 115, 243,
      11, 139, 75, 203, 43, 171, 107, 235, 27, 155, 91, 219, 59, 187, 123, 251,
      7, 135, 71, 199, 39, 167, 103, 231, 23, 151, 87, 215, 55, 183, 119, 247,
      15, 143, 79, 207, 47, 175, 111, 239, 31, 159, 95, 223, 63, 191, 127, 255};

#ifdef FUTURE
int CoinOp_Init(GAME_BLOB *pBlob, char *pszROM, int iGameLoad)
{
  iROMSet = 0; // Original PacMan
  return Pac_Init(pBlob, pszROM, iGameLoad);
} /* CoinOp_Init() */

void CoinOp_Terminate(GAME_BLOB *pBlob)
{
  Pac_Terminate(pBlob);
} /* CoinOp_Terminate() */

void CoinOp_Play(GAME_BLOB *pBlob, BOOL bAudio, BOOL bVideo, uint32_t ulKeys)
{
  Pac_Play(pBlob, bAudio, bVideo, ulKeys); 
} /* CoinOp_Play() */

void CoinOp_PostLoad(GAME_BLOB *)
{
  PacPostLoad(); 
} /* CoinOp_PostLoad() */
#endif // FUTURE

//
// Send the init sequence to the Nunchuck
//
void NCInit(void)
{
uint8_t ucTemp[2];

  ucTemp[0] = 0xf0; ucTemp[1] = 0x55;
  I2CWrite(&bb, NUNCHUCK_ADDR, ucTemp, 2);
  delay(10);
  ucTemp[0] = 0xfb; ucTemp[1] = 0x00;
  I2CWrite(&bb, NUNCHUCK_ADDR, ucTemp, 2);
  delay(10);
} /* NCInit() */

//
// Read a data packet and "decode" the bytes
//
void NCReadPacket(NCDATA *ncd)
{
//int i;
//uint8_t c;
uint8_t pData[6], ucTemp[2];

  I2CRead(&bb, NUNCHUCK_ADDR, pData, 6);
  ucTemp[0] = ucTemp[1] = 0;
  I2CWrite(&bb, NUNCHUCK_ADDR, ucTemp, 2);
//  for (i=0; i<6; i++)
//  {
//     Serial.print(", 0x"); Serial.print(pData[i], HEX);
//  }
//  Serial.println(" ");

  ncd->LX = pData[0] & 0x3f;
  ncd->LY = pData[1] & 0x3f;
  ncd->RX = (pData[0] >> 3) & 0x18;
  ncd->RX |= (pData[2] >> 7);
  ncd->RX |= ((pData[1] >> 5) & 0x6);
  ncd->RY = pData[2] & 0x1f;
  ncd->u16Buttons = ((pData[4] >> 1) & 0x1f);
  ncd->u16Buttons |= ((pData[5] >> 2) << 5);
  ncd->u8DPad = pData[4] >> 6; // upper 2 bits = R/D of D-pad
  ncd->u8DPad |= ((pData[5] & 3) << 2); // L/U
  ncd->u8DPad ^= 0xf; // invert so that 1 = pressed
  ncd->u16Buttons ^= 0x7ff;
} /* NCReadPacket() */

// Set up the GPIO pins
int InitGPIO(void)
{
  // Initialize the classic controller
  bb.bWire = 1;
  bb.iSDA = -1;
  bb.iSCL = -1;
  I2CInit(&bb, 100000L);
  NCInit();

  // configure digital inputs for buttons
  // DEBUG
//  pinMode(0, INPUT);
//  pinMode(39, INPUT);
//  pinMode(13, INPUT_PULLUP);
//  pinMode(27, INPUT_PULLUP);
//  pinMode(32, INPUT_PULLUP);
//  pinMode(33, INPUT_PULLUP);

  return 1; // success
} /* InitGPIO() */

void UpdateGPIOButtons(void)
{
//int i;
	if (iP1Control != CONTROLLER_GPIO && iP2Control !=CONTROLLER_GPIO)
		return; // GPIO buttons not in use
  u32GPIOBits = 0;
  NCReadPacket(&ncd);
//    Serial.printf("DPAD=%02x\n", ncd.u8DPad);
//    Serial.printf("Butts=%04x\n", ncd.u16Buttons);
  if (ncd.u8DPad & 4)
     u32GPIOBits |= RKEY_UP_P1;
  if (ncd.u8DPad & 1)
     u32GPIOBits |= RKEY_DOWN_P1;
  if (ncd.u8DPad & 8)
     u32GPIOBits |= RKEY_LEFT_P1;
  if (ncd.u8DPad & 2)
     u32GPIOBits |= RKEY_RIGHT_P1;
  if (ncd.u16Buttons & 8)
     u32GPIOBits |= RKEY_SELECT_P1;
  if (ncd.u16Buttons & 2)
     u32GPIOBits |= RKEY_START_P1;
  if (ncd.u16Buttons & 0x40)
     u32GPIOBits |= RKEY_BUTTC_P1;
  if (ncd.u16Buttons & 0x100)
     u32GPIOBits |= RKEY_BUTTD_P1;
  if (ncd.u16Buttons & 0x80)
     u32GPIOBits |= RKEY_BUTTA_P1;
  if (ncd.u16Buttons & 0x200)
     u32GPIOBits |= RKEY_BUTTB_P1;
#ifdef FUTURE
  // brute force it
  if (digitalRead(0) == LOW)
     u32GPIOBits |= RKEY_BUTTC_P1; // volume button
  if (digitalRead(13) == LOW)
     u32GPIOBits |= (RKEY_SELECT_P1 | RKEY_START_P1); // quit / menu
  if (digitalRead(27) == LOW)
     u32GPIOBits |= RKEY_SELECT_P1;
  if (digitalRead(39) == LOW)
     u32GPIOBits |= RKEY_START_P1;
  if (digitalRead(32) == LOW)
     u32GPIOBits |= RKEY_BUTTA_P1;
  if (digitalRead(33) == LOW)
     u32GPIOBits |= RKEY_BUTTB_P1;
  i = analogRead(34); // left/right
  if (i > 1500 && i < 3000)
     u32GPIOBits |= RKEY_RIGHT_P1;
  else if (i > 4000)
     u32GPIOBits |= RKEY_LEFT_P1;
  i = analogRead(35); // up/down
  if (i > 1500 && i < 3000)
     u32GPIOBits |= RKEY_DOWN_P1;
  else if (i > 4000)
     u32GPIOBits |= RKEY_UP_P1;
#endif // FUTURE
} /* UpdateGPIOButtons() */

void SG_Rectangle(int x, int y, int w, int h, uint16_t usColor, int bFill)
{
	if (bFill)
     lcd.fillRect(x, y, w, h, usColor);
  else
     lcd.drawRect(x, y, w, h, usColor);
} /* SG_Rectangle() */

void SG_WriteString(int x, int y, char *pString, uint16_t usFGColor, uint16_t usBGColor, int bLarge)
{
//extern unsigned char ucFont[];

		lcd.setTextColor(usFGColor, usBGColor);
    lcd.setFont((bLarge)? FONT_16x32: FONT_8x8);
    if (bLarge) {
      lcd.setCursor(x, y);
	  	lcd.print(pString);
    } else {
      lcd.drawStringFast(pString, x, y);
      if (strlen(pString) > 32) {
        x += 256;
        lcd.drawStringFast(&pString[32], x, y);
      }
    }
} /* SG_WriteString() */

void GUIDrawBits(int x, int y, uint32_t u32Bits, int bHex)
{
char *cName = (char *)"UDLRESS1234";
char *cHexName = (char *)"0123456789ABCDEF";
char cTemp[2];
int i, iCount;
char *s;
unsigned short usColor;

	cTemp[1] = '\0';
	iCount = (bHex)? 16: CTRL_COUNT;
	s = (bHex) ? cHexName : cName;
	for (i=0; i<iCount; i++)
	{
		cTemp[0] = s[i];
		if (bHex)
		{
			if (u32Bits & (1<<i))
				usColor = 0xf800; // pressed = red
			else
				usColor = 0x7e0; // unpressed = green
		}
		else
		{
			if (u32Bits & iButtonMapping[i])
				usColor = 0xf800;
			else
				usColor = 0x7e0;
		}
		SG_WriteString(x+i*8, y, cTemp, usColor, 0, 0); 
	}
} /* GUIDrawBits() */

void GUIDrawControls(void)
{
	SG_WriteString(8,0,(char*)"     SmartGear",0xf800,0,1);
	SG_WriteString(8,32,(char*)" Controller Test", 0xffff,0,0);
	SG_WriteString(8,40,(char*)"start+select to exit", 0xf81f, 0, 0);
	SG_WriteString(8,64,(char*)"Keyboard:",0xffff,0,0);
	SG_WriteString(8,72,(char*)"Raw GP 0:", 0xffff,0,0);
	SG_WriteString(8,80,(char*)"Raw GP 1:", 0xffff,0,0);
	SG_WriteString(8,88,(char*)"GP 0:", 0xffff, 0,0);
	SG_WriteString(8,96,(char*)"GP 1:", 0xffff, 0,0);
	SG_WriteString(8,104,(char*)"GPIO:", 0xffff,0,0);
	GUIDrawBits(108, 64, u32KeyBits, 0);
	GUIDrawBits(108, 72, u32RawJoy0Bits, 1);
	GUIDrawBits(108, 80, u32RawJoy1Bits, 1);
	GUIDrawBits(108, 88, u32Joy0Bits, 0);
	GUIDrawBits(108, 96, u32Joy1Bits, 0);
	GUIDrawBits(108, 104, u32GPIOBits, 0);

} /* GUIDrawControls() */

void ControllerTest(void)
{
uint32_t u32Bits, u32Raw, u32PrevRaw, u32Quit;
int bDone = 0;

      if (iLCDOrientation == LCD_ORIENTATION_0)
      	SG_Rectangle(0, 0, iLCDX, iLCDY, 0, 1);
      else
        SG_Rectangle(0,0,iLCDY,iLCDX,0,1);
      GUIDrawControls();
      u32Bits = u32Raw = u32PrevRaw = 0;
      u32Quit = RKEY_SELECT_P1 | RKEY_START_P1; // quit signal

      while (!bDone)
      {
         UpdateGPIOButtons();
         u32Bits = u32Joy0Bits | u32KeyBits | u32GPIOBits;
	 u32Raw = u32RawJoy0Bits | u32RawJoy1Bits;
         if (u32Bits == u32PrevBits && u32Raw == u32PrevRaw)
         {
            delayMicroseconds(10000);
	    continue;
         }
	 u32PrevBits = u32Bits;
	 u32PrevRaw = u32Raw;
         GUIDrawControls();	
         if ((u32Bits & u32Quit) == u32Quit)
            bDone = 1; 
     } // while !bDone

} /* ControllerTest() */

//
// Mark the current bitmap as dirty so that it will get a complete repaint
//
void MarkDirty(void)
{
GAME_BLOB *pBlob = &blobs[MAX_PREV_GAMES];

	// Set backing bitmap to unlikely color
	memset(pAltScreen, 0xfd, pBlob->iHeight*pBlob->iPitch);
} /* MarkDirty() */

//
// Display copright info
//
int AboutMenu(void)
{
int i, w, x, y, rc = 0;
//time_t t;
//char *szTime, szTemp[64];
unsigned short usColors[8] = {0xffff, 0x1f, 0x7e0, 0xf800, 0x7ff, 0xffe0, 0xf81f, 0x0000};

// Clear the screen
   lcd.fillScreen(0);

	if (iLCDX < 320)
	{
		x = (iLCDY-144)/2;
		w = (iLCDY-184)/2;
		y = (iLCDX - 140)/2;
	}
	else
	{
		x = (iLCDX-144)/2;
		w = (iLCDX-184)/2;
		y = (iLCDY-140)/2;
	}

        SG_WriteString(w, y+40, (char*)"Copyright (c) 2010-2018", 0x7e0,0,0);
        SG_WriteString(w+4, y+55, (char*)"BitBank Software, Inc.", 0x7e0, 0,0);
        SG_WriteString(w+8, y+70, (char*)"Written by Larry Bank", 0x7e0, 0, 0);
	SG_WriteString(w+24, y+85, (char*)"bitbank@pobox.com", 0x7e0,0,0);
	SG_WriteString(w+20, y+95, (char*)"Version: ", 0x7e0,0,0);
	SG_WriteString(w+90, y+95, version, 0x7e0,0,0);
//	SG_WriteString(w-40, y+105,(char*)"Expires: ", 0x7e0,0,0);
	// convert expiration time/date into a time value
//	t = mktime(&expire_time);
//	szTime = ctime(&t);
//	SG_WriteString(w+30, y+105, szTime, 0x7e0,0,0);
//	rc = CheckExpired(); // expired ?
//	if (rc)
//	{
//		SG_WriteString(w+60, y+115, (char*)"Expired!", 0xf800,0,0);
//	}
//	sprintf(szTemp, (char*)"%d gamepad(s), %d-bit CPU", SDL_NumJoysticks(), (sizeof(char *) == 4) ? 32:64);
//	SG_WriteString(w+2, y+125, szTemp, 0xffe0,0,0);
//	sprintf(szTemp, (char*)"LCD: %dx%d 16-Bpp", iLCDX, iLCDY);
//	SG_WriteString(w+8, y+135, szTemp, 0xffe0,0,0);
	for (i=0; i<57; i++)
	{
		SG_WriteString(x,y,(char*)"SmartGear", usColors[i&7], 0, 1);	
		delayMicroseconds(50000);
	}
	delayMicroseconds(3000000);
	return rc;
} /* AboutMenu() */

//
// Quit menu
//
// returns 0=resume, 1=quit, 2=shutdown, 3=reboot
//
int QuitMenu(void)
{
int iSel = 0;
uint32_t u32Bits, u32PrevBits;
int bMenuDone = 0;
uint16_t usFG1, usFG2;
int bChanged = 0;
int x, w;
int y = 40;

        w = (iLCDOrientation == LCD_ORIENTATION_0) ? iLCDX : iLCDY;
        x = (w-120)/2;
// Draw a rectangle over the current game screen with the menu in it
        SG_Rectangle(x, y, 120, 72, 0, 1); // fill with black
        SG_Rectangle(x, y, 120, 72, 0xffff, 0); // white rectangle
        SG_WriteString(x+36, y-4, (char*)"Quit?", 0xf800, 0, 0); // red
        usFG1 = 0x7e0; // unselected = green
        usFG2 = 0xffff; // selected = white
        u32PrevBits = u32Joy0Bits | u32KeyBits | u32GPIOBits;
        while (!bMenuDone)
        {
        // draw the menu options
                SG_WriteString(x+8,y+6,(char*)"Resume",(iSel == 0) ? usFG1:usFG2,0,0);
		SG_WriteString(x+8,y+16,(char*)"Terminal",(iSel==1) ? usFG1:usFG2,0,0);
                SG_WriteString(x+8, y+26, (char*)"Quit", (iSel == 2) ? usFG1:usFG2,0,0);
                SG_WriteString(x+8, y+36, (char*)"Shut down", (iSel == 3) ? usFG1:usFG2, 0, 0);
		SG_WriteString(x+8,y+46, (char*)"Reboot", (iSel == 4) ? usFG1:usFG2, 0,0);
		SG_WriteString(x+8,y+56, (char*)"About", (iSel == 5) ? usFG1:usFG2, 0,0);
                bChanged = 0;
                while (!bChanged)
                {
                     UpdateGPIOButtons();

                        // main thread is keeping the kb/gp events pumping
                        u32Bits = u32Joy0Bits | u32KeyBits | u32GPIOBits;
                        if (u32Bits == u32PrevBits)
                        {
                                delayMicroseconds(50000);
                                continue;
                        }
                        if (u32Bits & RKEY_UP_P1 && !(u32PrevBits & RKEY_UP_P1))
                        {
                                if (iSel > 0)
                                {
                                        iSel--;
                                        bChanged = 1;
                                }
                        }
                        if (u32Bits & RKEY_DOWN_P1 && !(u32PrevBits & RKEY_DOWN_P1))
                        {
                                if (iSel < 5)
                                {
                                        iSel++;
                                        bChanged = 1;
                                }
                        }
                        if (u32Bits & RKEY_BUTT1_P1 && !(u32PrevBits & RKEY_BUTT1_P1))
                        {
                                bChanged = 1; // return with result
				if (iSel < 5)
                                	bMenuDone = 1;
				else
				{
					AboutMenu();
					bMenuDone = 1; //goto draw_quit_menu;
				}
                        }
                        u32PrevBits = u32Bits;
                } // while !bChanged
        } // while !bMenuDone
return iSel;
} /* QuitMenu() */

//
// Pause menu
//
void PauseMenu(void)
{
int iSel = 0;
uint32_t u32Bits, u32PrevBits;
int bMenuDone = 0;
uint16_t usFG1, usFG2;
int bChanged = 0;
int x, w;
int y = 40;
GAME_BLOB *pBlob = &blobs[MAX_PREV_GAMES];
char szTemp[8];

//	pthread_mutex_lock(&lcd_busy_mutex); // don't step on background thread

	bRunning = FALSE; // don't let sound get out of sync
	w = (iLCDOrientation == LCD_ORIENTATION_0) ? iLCDX : iLCDY;
	x = (w-120)/2;
// Draw a rectangle over the current game screen with the menu in it
	SG_Rectangle(x, y, 120, 92, 0, 1); // fill with black
	SG_Rectangle(x, y, 120, 92, 0xffff, 0); // white rectangle
	SG_WriteString(x+36, y-4, (char*)"Paused", 0xf800, 0, 0); // red
        usFG1 = 0x7e0; // unselected = green
        usFG2 = 0xffff; // selected = white
        u32PrevBits = u32Joy0Bits | u32KeyBits | u32GPIOBits;
        while (!bMenuDone)
        {
        // draw the menu options
                SG_WriteString(x+8,y+6,(char*)"Resume",(iSel == 0) ? usFG1:usFG2,0,0);
		sprintf(szTemp,(char*)"%d ", iAudioVolume);
		SG_WriteString(x+72,y+16,szTemp,(iSel==1)?usFG1:usFG2,0,0);
		SG_WriteString(x+8,y+16,(char*)"Volume:",(iSel==1) ? usFG1:usFG2,0,0);
		SG_WriteString(x+8,y+26,(char*)"Restart", (iSel == 2) ? usFG1:usFG2,0,0);
                SG_WriteString(x+8, y+36, (char*)"Quit", (iSel == 3) ? usFG1:usFG2,0,0);
                SG_WriteString(x+8, y+46, (char*)"Screenshot", (iSel == 4) ? usFG1:usFG2, 0, 0);
                SG_WriteString(x+8, y+56, (char*)"Load state", (iSel == 5) ? usFG1:usFG2, 0, 0);
                SG_WriteString(x+8, y+66, (char*)"Save state", (iSel == 6) ? usFG1:usFG2, 0, 0);
		SG_WriteString(x+8, y+76, (char*)"Rewind", (iSel == 7) ? usFG1:usFG2, 0, 0);
                bChanged = 0;
                while (!bChanged)
                {
			// main thread is keeping the kb/gp events pumping
                        u32Bits = u32Joy0Bits | u32KeyBits | u32GPIOBits;      
                        if (u32Bits == u32PrevBits)
                        {
                                delayMicroseconds(50000);
                                continue;
                        }
                        if (u32Bits & RKEY_UP_P1 && !(u32PrevBits & RKEY_UP_P1))
                        {
                                if (iSel > 0)
                                {
                                        iSel--;
                                        bChanged = 1;
                                }
                        }
                        if (u32Bits & RKEY_DOWN_P1 && !(u32PrevBits & RKEY_DOWN_P1))
                        {
                                if (iSel < 7)
                                {
                                        iSel++;
                                        bChanged = 1;
                                }
                        }
			if (u32Bits & RKEY_BUTT2_P1 && !(u32PrevBits & RKEY_BUTT2_P1))
			{
				if (iSel == 1) // volume decrement
				{
					iAudioVolume -= 5;
					if (iAudioVolume < 0) iAudioVolume = 100;
					bChanged = 1;
				}
			}
                        if (u32Bits & RKEY_BUTT1_P1 && !(u32PrevBits & RKEY_BUTT1_P1))
                        {
                                if (iSel == 0)
				{
					bChanged = 1; // resume
					bMenuDone = 1;
				}
				else if (iSel == 1) // volume
				{
					iAudioVolume += 5;
					if (iAudioVolume > 100) iAudioVolume = 0;
					bChanged = 1;
				}
				else if (iSel == 2) // restart
				{
					SG_ResetGame(NULL);
					bChanged = 1; // resume
					bMenuDone = 1; // exit from pause menu
				}
                                else if (iSel == 3)
				{
					bQuit = 1;
					bMenuDone = 1;
					bChanged = 1;
				}
                                else if (iSel == 4) // screenshot
				{
                                char *p, szName[256];
//                                        p = getcwd(szName, 256);
          p = NULL; // DEBUG
					if (p != NULL) {}
                                        strcat(szName, (char*)"/");
                                        strcat(szName, pBlob->szGameName);
					strcat(szName, (char*)".png");
					if (SG_SavePNG(szName, (unsigned char *)pBlob->pBitmap, pBlob->iWidth, pBlob->iHeight, pBlob->iPitch))
                                                SG_WriteString(x+8,66,(char*)"Error!",0xf800,0,1);
                                        else
                                                SG_WriteString(x+8,66,(char*)"Saved!",0x7e0,0,1);
                                        delayMicroseconds(1000000);
                                        SG_Rectangle(x, y, 120, 72, 0, 1); // fill with black
                                        SG_Rectangle(x, y, 120, 72, 0xffff, 0); // white outline
                                        bChanged = 1;

				}
                                else if (iSel == 5) // load state
				{
                                char *p, szName[256];
//                                        p = getcwd(szName, 256);
        p = NULL; // DEBUG
					if (p != NULL) {}
                                        strcat(szName, "/");
                                        strcat(szName, pBlob->szGameName);
					if (!SGLoadGame(szName, pBlob, 0))
					{
						(*emulators[iGameType].pfnPostLoad)(&blobs[MAX_PREV_GAMES]);
						bMenuDone = bChanged = 1; // play it
					}
				}
				else if (iSel == 6) // save state
				{
				char *p, szName[256];
//					p = getcwd(szName, 256);
          p = NULL; // DEBUG
					if (p != NULL) {}
					strcat(szName, "/");
					strcat(szName, pBlob->szGameName);
					if (SGSaveGame(szName, pBlob, 0))
						SG_WriteString(x+8,66,(char*)"Error!",0xf800,0,1);
					else
						SG_WriteString(x+8,66,(char*)"Saved!",0x7e0,0,1);
					delayMicroseconds(1000000);
					SG_Rectangle(x, y, 120, 72, 0, 1); // fill with black
					SG_Rectangle(x, y, 120, 72, 0xffff, 0); // white outline
					bChanged = 1;
				}
				else if (iSel == 7) // rewind
				{
				}
                        }
                        u32PrevBits = u32Bits;
                } // while !bChanged
        } // while !bMenuDone
//	pthread_mutex_unlock(&lcd_busy_mutex);
	MarkDirty(); // force a total repaint
	if (!bQuit)
	{
		bRunning = TRUE; // get sound going again
	}
} /* PauseMenu() */

void DrawBool(int x, int y, int value)
{
	if (value)
		SG_WriteString(x, y, (char*)"on ", 0x7e0, 0,0);
	else
		SG_WriteString(x, y, (char*)"off", 0xf800, 0,0);
} /* DrawBool() */

//
// Test the GPIO speed
//
float GPIOSpeedTest(void)
{
int i, j;
float fGPS;
uint64_t ulTime;

	ulTime = SG_Clock();
	j = 0;
	for (i=0; i<10000; i++)
	{
		j |= 1; // keep this loop from getting removed by the compiler's optimizer
//		spilcdSetMode(&lcd, 0); // causes a GPIO write
	}
	ulTime = SG_Clock() - ulTime;
 	if (j > 2) // don't let the compiler toss the loop
		ulTime += 1;
	fGPS = 10000000.0/(float)(ulTime / 1000000);
	return fGPS;	
} /* GPIOSpeedTest() */

//
// Test the display speed
//
float LCDSpeedTest(void)
{
int i;
float fFPS;
uint64_t ullTime;

	ullTime = SG_Clock();
		for (i=0; i<5; i++)
		{
			lcd.fillScreen(0xffff);
			lcd.fillScreen(0);
		}
	ullTime = SG_Clock() - ullTime;
	fFPS = 10000.0/(float)(ullTime / 1000000UL);
	return fFPS;
} /* LCDSpeedTest() */

//
// Configure the game options
//
void Configure(void)
{
int iSel = 0;
float fFPS = 0.0;
float fGPS = 0.0;
uint32_t u32Bits;
int bDone = 0;
uint16_t usFG1, usFG2;
char szTemp[32];
int bChanged = 0;

	usFG1 = 0x7e0; // unselected = green
	usFG2 = 0xffff; // selected = white
	if (iLCDOrientation == LCD_ORIENTATION_0)
		SG_Rectangle(0,0,iLCDX, iLCDY, 0, 1);
	else
		SG_Rectangle(0,0,iLCDY,iLCDX,0,1);
        if (iLCDY == 160)
                SG_WriteString(8,0,(char*)"SmartGear",0xf800,0,1);
        else
                SG_WriteString(80,0,(char*)"SmartGear",0xf800,0,1);
	while (!bDone)
	{
	// draw the configuration option bits
		SG_WriteString(8,40,(char*)"Configuration",0xffff,0,0);
		if (fFPS != 0.0)
		{
			sprintf(szTemp, (char*)"%03.2f FPS", fFPS);
			SG_WriteString(180, 40, szTemp, 0xffff,0,0);
		}
		if (fGPS != 0.0) // GPIOs per second
		{
		char szTemp[32];
			sprintf(szTemp, (char*)"%03.2f GPIO/s", fGPS);
			SG_WriteString(180, 52, szTemp, 0xffff,0,0);
		}
		DrawBool(8,56,bHead2Head);
		SG_WriteString(40, 56, (char*)"Head-2-Head", (iSel == 0) ? usFG1:usFG2,0,0);
		DrawBool(8,66, bAudio);
		SG_WriteString(40, 66, (char*)"Audio", (iSel == 1) ? usFG1:usFG2, 0, 0);
		sprintf(szTemp, "%d ", iAudioVolume);
		SG_WriteString(8,76, szTemp, usFG1, 0, 0);
		SG_WriteString(40,76, (char*)"Audio Volume", (iSel == 2) ? usFG1:usFG2, 0, 0);
		DrawBool(8,86, bShowFPS);
		SG_WriteString(40, 86, (char*)"Show FPS", (iSel == 3) ? usFG1:usFG2, 0, 0);
		DrawBool(8,96, bStretch);
		SG_WriteString(40, 96, (char*)"Stretch 2x", (iSel == 4) ? usFG1:usFG2, 0, 0);
//		SG_WriteString(40, 106, (char*)"P1 Ctrl:", (iSel == 5) ? usFG1:usFG2, 0, 0);
//		SG_WriteString(112, 106,szCtrlNames[iP1Control], 0xffff,0,0);
//                SG_WriteString(40, 116, (char*)"P2 Ctrl:", (iSel == 6) ? usFG1:usFG2, 0, 0);
//                SG_WriteString(112, 116,szCtrlNames[iP2Control], 0xffff,0,0);
		SG_WriteString(40, 126, (char*)"LCD Speed Test", (iSel == 7) ? usFG1:usFG2, 0,0);
		SG_WriteString(40, 136, (char*)"GPIO Speed Test", (iSel == 8) ? usFG1:usFG2,0,0);
		SG_WriteString(40, 146, (char*)"Exit Configuration", (iSel == 9) ? usFG1:usFG2, 0, 0);
		bChanged = 0;
		while (!bChanged)
		{
			UpdateGPIOButtons();
			u32Bits = u32Joy0Bits | u32KeyBits | u32GPIOBits;	
			if (u32Bits == u32PrevBits)
			{
				delayMicroseconds(50000);
				continue;
			}
			if (u32Bits & RKEY_UP_P1 && !(u32PrevBits & RKEY_UP_P1))
			{
				if (iSel > 0)
				{
					iSel--;
					bChanged = 1;
				}
			}
                        if (u32Bits & RKEY_DOWN_P1 && !(u32PrevBits & RKEY_DOWN_P1))
                        {
                                if (iSel < 9)
                                {
                                        iSel++;
                                        bChanged = 1;
                                }
                        }
			if (u32Bits & RKEY_BUTT2_P1 && !(u32PrevBits & RKEY_BUTT2_P1))
			{
				if (iSel == 2) // audio volume
				{
					iAudioVolume -= 5;
					if (iAudioVolume < 0) iAudioVolume = 100;
					bChanged = 1;
				}
			}
			if (u32Bits & RKEY_BUTT1_P1 && !(u32PrevBits & RKEY_BUTT1_P1))
			{
				if (iSel == 0) bHead2Head = !bHead2Head;
				else if (iSel == 1) bAudio = !bAudio;
				else if (iSel == 2)
				{
					iAudioVolume += 5;
					if (iAudioVolume > 100) iAudioVolume = 0;
				}
				else if (iSel == 3) bShowFPS = !bShowFPS;
				else if (iSel == 4) bStretch = !bStretch;
				else if (iSel == 5) {iP1Control++; if (iP1Control >= CONTROLLER_COUNT) iP1Control = 0;}
				else if (iSel == 6) {iP2Control++; if (iP2Control >= CONTROLLER_COUNT) iP2Control = 0;}
				else if (iSel == 7) fFPS = LCDSpeedTest();
				else if (iSel == 8) fGPS = GPIOSpeedTest();
				else if (iSel == 9) bDone = 1;
				bChanged = 1;
			}
			u32PrevBits = u32Bits;
		} // while !bChanged
	} // while !bDone
} /* Configure() */

void GUIDrawNames(char *pDir, char *pDirNames, char *pFileNames, int iDirCount, int iFileCount, int iSelected)
{
int i, iLen, iNumLines, iNumCols;
int iCurrent;
unsigned short usFG, usBG;
char szTemp[512];

  usFG = usBG = 0;
  i = (iLCDX - (9*16))/2; // center it
	SG_WriteString(i,0,(char*)"SmartGear",0xf800,0,1);
	iNumLines = (iLCDY - 48)/8;
	iNumCols = iLCDX/8;
	SG_WriteString(8,12,(char*)"L",0x7e0,0,0);
	SG_WriteString(20,8,(char*)"Ctrl", 0xf800,0,0);
	SG_WriteString(20,16,(char*)"Test",0xf800,0,0);
	if (iLCDX == iLCDY) // TinyPi Pro
	{
		SG_WriteString(176,12,(char*)"R",0x7e0,0,0);
		SG_WriteString(192,12,(char*)"Config",0xf800,0,0);
	}
	else
	{
		SG_WriteString(256,12,(char*)"R",0x7e0,0,0);
		SG_WriteString(268,12,(char*)"Config",0xf800,0,0);
	}
	strcpy(szTemp, pDir);
	strcat(szTemp, "                                        ");
	szTemp[iNumCols] = 0;
        SG_WriteString(0,32,szTemp,0xffff,0x1f,0);
        if (iSelected >= iNumLines) iCurrent = iSelected-(iNumLines-1);
        else iCurrent = 0;
        for (i=0; i<iNumLines; i++) // draw all lines on the display
        {
           usBG = 0; // black background
           if (iCurrent >= (iDirCount+iFileCount)) // short list, fill with spaces
           {
                 strcpy(szTemp, (char*)"                                         ");
           }
           else
           {
              if (iCurrent >= iDirCount) // use filenames
              {
                 usFG = 0x7ff; // cyan text
                 strcpy(szTemp, &pFileNames[(iCurrent-iDirCount)*256]);
              }
              else // use dir names
              {
                 usFG = 0x7e0; // green text
                 strcpy(szTemp, &pDirNames[iCurrent*256]);
              }
              iLen = strlen(szTemp);
              if (iLen < iNumCols) // fill with spaces to erase old data
              {
                 strcat(szTemp, "                                        ");
              }
              szTemp[iNumCols] = 0;
              if (iCurrent == iSelected) // highlight the selected name
              {
                 usBG = usFG; // invert color
		             usFG = 0;
              }
           }
           SG_WriteString(0, 40+(i*8), szTemp, usFG, usBG, 0);
           iCurrent++;
        } 
} /* GUIDrawNames() */

//
// Adjust the give path to point to the parent directory
//
void GetParentDir(char *szDir)
{
int i, iLen;
	iLen = strlen(szDir);
	for (i=iLen-1; i>=1; i--)
        {
		if (szDir[i] == '/') // look for the next slash 'up'
			break;
	}
	if (i != 1 && szDir[i] == '/')
		szDir[i] = '\0'; // terminate it there
} /* GetParentDir() */

int name_compare(const void *ina, const void *inb)
{
char *a = (char *)ina;
char *b = (char *)inb;

 while (*a && *b) {
        if (tolower(*a) != tolower(*b)) {
            break;
        }
        ++a;
        ++b;
    }
    return (int)(tolower(*a) - tolower(*b));
} /* name_compare() */

//
// Allow the user to navigate the file system and choose a game to run
// Currently only supports gamepad access
// press start+A to quit
//
int GamepadGUI(char *cDir, char *szDestName)
{
File root, dir;
int iSelected;
int iReturn = 0;
int iDirCount, iFileCount, iDir, iFile;
int bDone = 0;
int iRepeat = 0;
char *pDirNames = NULL;
char *pFileNames = NULL;
char szTemp[256];
uint32_t u32Quit;
volatile uint32_t u32Bits;
int iMaxSelection;
int bDirValid;

   u32Quit = RKEY_SELECT_P1 | RKEY_START_P1; // quit signal
   // Need to go 'wide' if the display is too narrow
//   if (iLCDX < 320 && iLCDX != iLCDY)
//   {
//      spilcdSetOrientation(&lcd, LCD_ORIENTATION_0);
//      iLCDOrientation = LCD_ORIENTATION_90;
//      SG_Rectangle(0,0,iLCDY,iLCDX,0,1);
//   }
//   else
//   {
//      spilcdSetOrientation(&lcd, LCD_ORIENTATION_0);
      iLCDOrientation = LCD_ORIENTATION_0;
//      SG_Rectangle(0,0,iLCDX,iLCDY,0,1);
//   }
//   spilcdScrollReset(&lcd);
   lcd.fillScreen(0);
   iDir = iFile = iDirCount = iMaxSelection = iFileCount = 0;
   u32PrevBits = u32Joy0Bits | u32KeyBits | u32GPIOBits;

   while (!bDone)
   {
      root = SD.open(cDir);
      if (root)
      {
         dir = root.openNextFile();
         if (dir)
         {
            // count the number of non-hidden directories and files
            iDirCount = 1;
            iFileCount = 0;
            while (dir)
            {
              SG_GetLeafName2((char *)dir.name(), szTemp);
               if (dir.isDirectory() && szTemp[0] != '.') iDirCount++;
               else if (!dir.isDirectory() && szTemp[0] != '.') iFileCount++;
               dir.close();
              dir = root.openNextFile();
              delay(5);
            }
            root.rewindDirectory();
            if (pDirNames)
            {
               free(pDirNames);
               free(pFileNames);
            }
            Serial.printf("dirs = %d, files = %d\n", iDirCount, iFileCount);
            Serial.flush();
            pDirNames = (char *)malloc(256 * (iDirCount+1));
            pFileNames = (char *)malloc(256 * (iFileCount+1));
            // now capture the names
            iDir = 1; // store ".." as the first dir
            strcpy(pDirNames, "..");
            iFile = 0;
            dir = root.openNextFile();
            while (dir)
            {
               SG_GetLeafName2((char *)dir.name(), szTemp);
               if (dir.isDirectory() && szTemp[0] != '.')
               {
                  strcpy(&pDirNames[iDir*256], szTemp);
                  iDir++;
               } 
               else if (!dir.isDirectory() && szTemp[0] != '.')
               { // only display ZIP files
 //                 int iLen = strlen(szTemp);
                  if (1) //strcmp(&szTemp[iLen-4], ".zip") == 0 || strcmp(&szTemp[iLen-4],".ZIP") == 0)
                  {
                     strcpy(&pFileNames[iFile*256], szTemp);
                     iFile++;
                  }
               }
               dir.close();
               dir = root.openNextFile();
               delay(5);
            }
         }
         root.close();
         iDirCount = iDir;
         iFileCount = iFile; // get the usable names count
         iMaxSelection = iDirCount + iFileCount;
	// Sort the names
	qsort(pDirNames, iDirCount, 256, name_compare);
	qsort(pFileNames, iFileCount, 256, name_compare);
      } // dir opened
restart:
      iSelected = 0;
      GUIDrawNames(cDir, pDirNames, pFileNames, iDirCount, iFileCount, iSelected);
      bDirValid = 1;
      while (bDirValid)
      {
	 UpdateGPIOButtons();
	 u32Bits = u32Joy0Bits | u32KeyBits | u32GPIOBits;
         if (u32Bits == u32PrevBits)
         {
            delayMicroseconds(5000);
            iRepeat++;
            if (iRepeat < 200) // 1 second starts a repeat
               continue;
         }
	 else // change means cancel repeat
         {
            iRepeat = 0;
         }
         if (((u32Bits & u32Quit) == u32Quit) && ((u32PrevBits & u32Quit) != u32Quit))
         { // quit SmartGear signal
		int rc, i = QuitMenu();
		if (i==0){bDirValid = 0; continue;}
//		else if (i==1) {Terminal(); continue;}
		else if (i==2) {bDirValid=0; bDone=1; continue;}
//		else if (i==3) {spilcdFill(0); rc = system("sudo shutdown now");}
//		else if (i==4) rc = system("sudo reboot");
		else {bDirValid=0; continue;} // continue after 'About'
		if (rc < 0) {};
         } 
	if (((u32Bits & RKEY_EXIT) == RKEY_EXIT) && ((u32PrevBits & RKEY_EXIT) != RKEY_EXIT))
	{
	// quit menu - 0=resume, 1=quit, 2=shutdown
		int rc, i = QuitMenu();
		if (i == 0) {bDirValid = 0; continue;}
		else if (i== 1) {bDirValid=0; bDone=1; continue;}
//		else if (i == 2) rc = system("sudo shutdown now");
//		else if (i==3) rc = system("sudo reboot");
		else {bDirValid=0; continue;} // continue after 'About'
		if (rc < 0) {};
	}
	if (u32Bits & RKEY_LEFT_P1 && !(u32PrevBits & RKEY_LEFT_P1)) // controller test
	{
		ControllerTest();
		SG_Rectangle(0,0,iLCDX, iLCDY, 0, 1);
		goto restart;
	}
	if (u32Bits & RKEY_RIGHT_P1 && !(u32PrevBits & RKEY_RIGHT_P1)) // configure
	{
		Configure();
		SG_Rectangle(0,0,iLCDX, iLCDY, 0, 1);
		goto restart;
	}
         if (u32Bits & RKEY_UP_P1 && (iRepeat || !(u32PrevBits & RKEY_UP_P1)))
         { // navigate up
            if (iSelected > 0)
            {
               iSelected--;
               GUIDrawNames(cDir, pDirNames, pFileNames, iDirCount, iFileCount, iSelected);
            }
         }
         if (u32Bits & RKEY_DOWN_P1 && (iRepeat || !(u32PrevBits & RKEY_DOWN_P1)))
         { // navigate down
            if (iSelected < iMaxSelection-1)
            {
               iSelected++;
               GUIDrawNames(cDir, pDirNames, pFileNames, iDirCount, iFileCount, iSelected);
            }
         }
         if (u32Bits & RKEY_BUTT1_P1 && !(u32PrevBits & RKEY_BUTT1_P1))
         {
            bDirValid = 0;
            if (iSelected == 0) // the '..' dir goes up 1 level
            {
              Serial.print("cdir = ");
              Serial.println(cDir);
               if (strcmp(cDir, "/home") != 0 && strcmp(cDir, "/") != 0) // navigating to root will mess things up
               {
                  GetParentDir(cDir);
               }
            }
            else
            {
               if (iSelected < iDirCount) // user selected a directory
               {
                  if (strcmp(cDir, "/") != 0)
                     strcat(cDir, "/");
                  strcat(cDir, &pDirNames[iSelected*256]);
               }
               else // user selected a file, leave
               {
//                  strcpy(szDestName, "/sd");
                 // strcpy(szDestName, "/");
//                  strcat(szDestName, cDir);
                  strcpy(szDestName, cDir);
                  if (strcmp(cDir, "/") != 0)
                     strcat(szDestName, "/");
                  strcat(szDestName, &pFileNames[(iSelected-iDirCount)*256]);
                  bDone = 1; // exit the main loop
                  iReturn = 1;
               }
            }
         }
         u32PrevBits = u32Bits;
      } // while bDirValid
   }
   if (iLCDOrientation == LCD_ORIENTATION_0)
      SG_Rectangle(0,0,iLCDX, iLCDY, 0, 1); // erase to black before starting
   else
      SG_Rectangle(0,0,iLCDY, iLCDX, 0, 1);
   return iReturn;
} /* GamepadGUI() */

// Save the image of the game along with the state
void SG_SaveGameImage(int iGame)
{
#ifdef FUTURE
PIL_PAGE pp1, pp2;
PIL_FILE pf;
int rc;
char szOut[256];
//unsigned char *pTempBitmap;
int iWidth = blobs[MAX_PREV_GAMES].iWidth;
int iHeight = blobs[MAX_PREV_GAMES].iHeight;

//__android_log_print(ANDROID_LOG_VERBOSE, "SG_SaveGameImage()", "entering");

   memset(&pp1, 0, sizeof(PIL_PAGE));
   memset(&pp2, 0, sizeof(PIL_PAGE));
   pp1.iWidth = iWidth;
   pp1.iHeight = iHeight;
   pp1.cBitsperpixel = 16;
   pp1.iPitch = iPitch;
   pp1.pData = pBitmap;
   pp1.iDataSize = iPitch * iHeight;
   pp1.cCompression = PIL_COMP_NONE;
   pp2.cCompression = PIL_COMP_PNG;
   rc = PILConvert(&pp1, &pp2, 0, NULL, NULL);
   if (rc == 0)
   {
	   sprintf(szOut, (char*)"%s%s%s%02d.png", pszSAVED, blobs[MAX_PREV_GAMES].szGameName, szGameFourCC[iGameType], iGame);
//	   __android_log_print(ANDROID_LOG_VERBOSE, (char*)"SG_SaveGameImage()", "width=%d, height=%d, name=%s", iWidth, iHeight, szOut);
	   rc = PILCreate((char *)szOut, &pf, 0, PIL_FILE_PNG);
	   if (rc == 0)
	   {
		   PILWrite(&pf, &pp2, 0);
	       PILClose(&pf);
	   }
	   PILFree(&pp2);
   }
   else
   {
//	   __android_log_print(ANDROID_LOG_VERBOSE, "SG_SaveGameImage", "Error compressing bitmap = %d", rc);
   }

//   __android_log_print(ANDROID_LOG_VERBOSE, "ScreenShot", "Leaving");
#endif // FUTURE
} /* SG_SaveGameImage() */

void SG_PushSamples(unsigned char *pSamples, int iCount)
{
//  char szTemp[256];
//  Serial.println("Entering SG_PushSamples");
//  sprintf(szTemp, "pSamples=%08x, iCount=%d", (int)pSamples, iCount);
//  Serial.println(szTemp);
//  sprintf(szTemp, "iAudioAvailable=%d, iAudioTotal=%d", iAudioAvailable, iAudioTotal);
//  Serial.println(szTemp);
	if (iAudioAvailable + iCount > iAudioTotal)
	{
//		printf("too much audio generated\n");
		return; // too much audio, throw it away
	}
//	printf("entering SG_PushSamples(), iAudioTail = %d, pSamples =%d, pAudioBuffer=%d\n",iAudioTail, (int)pSamples, (int)pAudioBuffer);
	if (iAudioTail + iCount <= iAudioTotal) // simple case, no wrap around
	{
		memcpy(&pAudioBuffer[iAudioTail*iAudioSampleSize], pSamples, iCount*iAudioSampleSize);
		iAudioTail += iCount;
		if (iAudioTail == iAudioTotal) // need to wrap
			iAudioTail = 0; 
	}
	else // have to wrap around
	{
		int iFirst = iAudioTotal - iAudioTail;
		memcpy(&pAudioBuffer[iAudioTail], pSamples, iFirst*iAudioSampleSize);
		memcpy(pAudioBuffer, &pSamples[iFirst*iAudioSampleSize], (iCount-iFirst)*iAudioSampleSize);
		iAudioTail = iCount - iFirst;
	}
	iAudioAvailable += iCount;
//	sprintf(szTemp, "leaving SG_PushSamples(), head=%d, tail=%d\n", iAudioHead, iAudioTail);
//  Serial.println(szTemp);
} /* SG_PushSamples() */

void SG_PopSamples(unsigned char *pSamples, int iCount)
{
// 	if (iAudioAvailable < iCount) printf("not enough audio available\n");
	if (iAudioHead + iCount <= iAudioTotal) // simple case, no wrap around
	{
//		memcpy(pSamples, &pAudioBuffer[iAudioHead*iAudioSampleSize], iCount*iAudioSampleSize);
		iAudioHead += iCount;
		if (iAudioHead == iAudioTotal)
			iAudioHead = 0;
	}
	else // must wrap around
	{
		int iFirst = iAudioTotal - iAudioHead;
//		memcpy(pSamples, &pAudioBuffer[iAudioHead*iAudioSampleSize], iFirst*iAudioSampleSize);
//		memcpy(&pSamples[iFirst*iAudioSampleSize], pAudioBuffer, (iCount-iFirst)*iAudioSampleSize);
		iAudioHead = iCount - iFirst;
	}
	iAudioAvailable -= iCount;
//	printf("leaving SG_PopSamples(), head=%d, tail=%d\n", iAudioHead, iAudioTail);
} /* SG_PopSamples() */

void SG_StopAudio()
{
// DEBUG

} /* SG_StopAudio() */

void SG_QuitGame()
{
	//bQuit = FALSE;
	bRunning = FALSE;
	SG_TerminateGame(); // clean up resources of this game
	SG_Sleep(20000L); // allow second thread to see that we're done running
	//printf("About to free bitmap memory\n");
//	if (m_id != 0)
//	{
//		SDL_CloseAudioDevice(m_id);
//	}
//	EMUFree(pAltScreen);
//	if (bStretch) EMUFree(pStretchScreen);
	EMUFree(pScreen);
	llStartTime = SG_Clock() - llStartTime;
} /* SG_QuitGame() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : SG_Rewind(void)                                            *
 *                                                                          *
 *  PURPOSE    : Rewind the currently running game.                         *
 *                                                                          *
 ****************************************************************************/
void SG_Rewind(void)
{
int i, j;
// find the rewind buffer that's at least 4 seconds away
//   __android_log_print(ANDROID_LOG_VERBOSE, "SG_Rewind", "current frame = %d, 0=%d, 1=%d, 2=%d, 3=%d", iFrame, blobs[MAX_PREV_GAMES].iRewindFrame[0],blobs[MAX_PREV_GAMES].iRewindFrame[1], blobs[MAX_PREV_GAMES].iRewindFrame[2],blobs[MAX_PREV_GAMES].iRewindFrame[3]);
   for (i=0; i<4; i++)
      {
      j = iFrame - blobs[MAX_PREV_GAMES].iRewindFrame[i];
      if (j >= 480 && j < 720) // at least 8 seconds, but not more than 12
         {
//    	 __android_log_print(ANDROID_LOG_VERBOSE, "SG_Rewind", "loading slot %d", i);
         SGLoadGame(NULL, &blobs[MAX_PREV_GAMES], i);
         iFrame = blobs[MAX_PREV_GAMES].iRewindFrame[i]; // set current frame to this one so that it can be rewound further
         return;
         }
      }
 // no valid rewind buffers found
// __android_log_print(ANDROID_LOG_VERBOSE, "SG_Rewind", "no valid rewind slot found");
} /* SG_Rewind() */


/****************************************************************************
 *                                                                          *
 *  FUNCTION   : SG_ResetGame(void)                                         *
 *                                                                          *
 *  PURPOSE    : Reset the currently running game.                          *
 *                                                                          *
 ****************************************************************************/
void SG_ResetGame(unsigned char *pszGame)
{
    (*emulators[iGameType].pfnInit)(&blobs[MAX_PREV_GAMES], NULL, -1);

} /* SG_ResetGame() */

void SG_InitSDLAudio(void)
{
#ifdef FUTURE
	m_SWantedSpec.freq = blobs[MAX_PREV_GAMES].iAudioSampleRate;
	if (blobs[MAX_PREV_GAMES].b16BitAudio)
	{
		m_SWantedSpec.format = AUDIO_S16LSB;
	}
	else
	{
		m_SWantedSpec.format = AUDIO_U8;
	}
	m_SWantedSpec.channels = blobs[MAX_PREV_GAMES].iSoundChannels;
	m_SWantedSpec.silence = 0;
// A value of 256 works find on x86 Linux machines, but causes the RPi to
// slow the framerate to 37fps. The value which works on RPi is 512
	m_SWantedSpec.samples = 512; //1024; // must be a power of 2
	m_SWantedSpec.callback = sdlCallback;
	m_SWantedSpec.userdata = NULL;
	m_id = SDL_OpenAudioDevice(NULL, 0, &m_SWantedSpec, &m_SSDLspec, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
//        m_id = 0;
//	if (SDL_OpenAudio(&m_SWantedSpec, &m_SSDLspec) < 0)
	if (m_id == 0) // something went wrong
	{
		bAudio = FALSE; // can't use audio
		printf("SDL_OpenAudio: %s\n", SDL_GetError());
	}
	else
	{
		const char *pName = SDL_GetAudioDeviceName(m_id, 0);
		if (bVerbose) printf("SDL audio device name: %s\n", pName);
		// push one frame of silence to get started
//		SG_PushSamples((unsigned char *)blobs[MAX_PREV_GAMES].pAudioBuf, blobs[MAX_PREV_GAMES].iSampleCount);
		if (bVerbose)
			printf("Audio started - %d bps, samples=%d, chan=%d, freq=%d, format=%08x\n", blobs[MAX_PREV_GAMES].b16BitAudio ? 16:8, m_SSDLspec.samples, m_SSDLspec.channels, m_SSDLspec.freq, m_SSDLspec.format);
		SDL_PauseAudioDevice(m_id, 0);
//		SDL_PauseAudio(0); // start it playing
	}
 #endif // FUTURE
} /* SG_InitSDLAudio() */

void SG_InitSPIGraphics(GAME_BLOB *pBlob)
{

	iPitch = pBlob->iPitch = (pBlob->iWidth + 32) * 2;
  if (bVerbose)
  {
    Serial.print("iPitch = ");
    Serial.println(iPitch);
  }
//	pAltScreen = (unsigned char *)EMUAlloc(iPitch * (pBlob->iHeight + 32));
//	if (bStretch)
//	{
//		pStretchScreen = (unsigned char *)EMUAlloc(iPitch * 2 * (pBlob->iHeight*2));
//	}
//	pScreen = (unsigned char *)EMUAlloc(iPitch * (pBlob->iHeight /*+ 32*/));
  pScreen = (unsigned char *)EMUAlloc(iPitch * 32); // allocate enough for one row of tiles
	pBlob->pBitmap = pScreen;
  pBlob->iBitmapFrame0 = 0;
  if (bVerbose)
  {
    Serial.print("pBitmap = ");
    Serial.println((uint32_t)pBlob->pBitmap);
  }
  lcd.fillScreen(0);
} /* SG_InitSPIGraphics() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : SG_InitGame(unsigned char *)                               *
 *                                                                          *
 *  PURPOSE    : Detect game type and try to load a game ROM.               *
 *                                                                          *
 *  RETURNS    : 0 for success, -1 for error                                *
 *                                                                          *
 ****************************************************************************/
int SG_InitGame(unsigned char *pszROM)
{
char pszTemp[256], cTemp[256];
int iError = SG_NO_ERROR;
int iGame;
int i;
File ihandle;
int iLen;

	strcpy(pszGame, (const char *)pszROM);
  if (bVerbose)
  {
	  sprintf(pszTemp, "entering SG_InitGame(), file=%s\n", pszGame);
    Serial.println(pszTemp);
  }

	iLen = strlen(pszGame);
	if (strcmp(&pszGame[iLen-6],(char*)".sgsav") == 0) // it's a save game file, extract the rom name
	{
		char szName[260];
		ihandle = EMUOpenRO(pszGame);
		if (ihandle != 0)
		{
			EMURead(ihandle, szName, 260); // read 4-byte game type + 256 name
			EMUClose(ihandle);
			strcpy(pszGame, &szName[4]); // get the original ROM name
		}
		else
		{
			iError = SG_GENERAL_ERROR;
			goto sg_initexit;
		}
	}

    iGameType = EMUTestName(pszGame); // see if the file is for NES, GameBoy or GameGear (or something else)
	if (bVerbose)
  {
    sprintf(pszTemp, (char*)"iGameType=%d\n", iGameType);
    Serial.println(pszTemp);
  }
    if (iGameType) // if the game file has a .NES, .GG, GB, or .GBC extension, then try to load it
    {
//        InQ = (SERIALQ *)EMUAlloc(4096);
//        OutQ = (SERIALQ *)EMUAlloc(4096);
// DEBUG
//        bHead2Head = FALSE; //gtk_toggle_button_get_active(head2headTb);
        bAudio = TRUE; //gtk_toggle_button_get_active(audioTb);
	bFramebuffer = FALSE; //gtk_toggle_button_get_active(framebufferTb);
        blobs[MAX_PREV_GAMES].bHead2Head = bHead2Head;
        blobs[MAX_PREV_GAMES].b4WayJoystick = 0; // assume 8-way joysticks
	bUseSound = bAudio;
        if (bUseSound)
           {
//           if (iGameType == SYS_GENESIS || iGameType == SYS_COINOP || iGameType == SYS_TG16 || iGameType == SYS_GAMEBOY)
//              {
//              bStereo = TRUE; // these systems need 16-bit stereo always
//              b16BitAudio = TRUE;
//	      iAudioSampleSize = 4;
//              }
//	   else
//	      {
	      bStereo = FALSE;
	      b16BitAudio = FALSE; //TRUE;
	      iAudioSampleSize = 1;
//              }
	   iAudioRate = 25000; // DEBUG
           blobs[MAX_PREV_GAMES].iSoundChannels = (bStereo) ? 2:1;
           blobs[MAX_PREV_GAMES].iAudioSampleRate = iAudioRate;
//           EMUOpenSound(blobs[MAX_PREV_GAMES].iAudioSampleRate, b16BitAudio ? 16:8, blobs[MAX_PREV_GAMES].iSoundChannels);
// DEBUG
  iSoundBlock = iAudioRate/60;
  pSoundBuf = (unsigned char *)malloc(iSoundBlock);
           blobs[MAX_PREV_GAMES].pAudioBuf = (signed short *)pSoundBuf;
           blobs[MAX_PREV_GAMES].iAudioBlockSize = iSoundBlock;
           blobs[MAX_PREV_GAMES].iSampleCount = iSoundBlock;
           if (blobs[MAX_PREV_GAMES].iSoundChannels == 2)
              blobs[MAX_PREV_GAMES].iSampleCount >>= 1;
           blobs[MAX_PREV_GAMES].b16BitAudio = b16BitAudio;
           if (b16BitAudio)
              blobs[MAX_PREV_GAMES].iSampleCount >>= 1;
	if (bVerbose)
  {
    sprintf(pszTemp, "samplecount=%d, blocksize=%d\n", blobs[MAX_PREV_GAMES].iSampleCount, iSoundBlock);
    Serial.println(pszTemp);
    sprintf(pszTemp, "iAudioTotal=%d, iAudioSampleSize=%d\n", iAudioTotal, iAudioSampleSize);
    Serial.println(pszTemp);
  }
           if (!bPerf && bAudio)
              {
	          SG_InitSDLAudio();
              }
           else
              {
 //                 m_id = 0;
              }
           }
//	if (bVerbose)
//		printf((char*)"audio device id=%d\n", m_id);
        // See if the file passed to us is a savegame file
        iGame = bAutoLoad ? 0 : -1;
        strcpy(pszTemp, pszGame);
        i = strlen(pszGame);
        if (strcmp(&pszGame[i-5],(char*)"sgsav") == 0) // it's a savegame file
           {
           // get the rom filename from the savegame file
           ihandle = EMUOpenRO(pszGame);
           EMUSeek(ihandle, 4, 0); // seek to filename
           EMURead(ihandle, cTemp, 256);
           EMUClose(ihandle);
           strcpy(pszTemp, cTemp);
           i = strlen(pszGame);
           iGame = (int)(pszGame[i-7] - '0');
           }

	iFrame = 0;
	llStartTime = SG_Clock();
	u32iCadeBits = u32GPIOBits = u32Joy0Bits = u32Joy1Bits = u32KeyBits = 0;
	u32RawJoy0Bits = u32RawJoy1Bits = 0;

	Serial.println("About to call game init");
        iError = (*emulators[iGameType].pfnInit)(&blobs[MAX_PREV_GAMES], (char *)pszTemp, iGame);
	if (bVerbose)
  {
		sprintf(pszTemp, (char*)"Returned from game init - cx=%d, cy=%d\n", blobs[MAX_PREV_GAMES].iWidth, blobs[MAX_PREV_GAMES].iHeight);
    Serial.println(pszTemp);
  }
        if (iError == SG_NO_ERROR)
        {
			// process 1 frame so that TurboGfx-16 games can set the video mode
			(*emulators[iGameType].pfnPlay)(&blobs[MAX_PREV_GAMES], FALSE, FALSE, 0);
        	// allocate 2 bitmaps for double buffering
			blobs[MAX_PREV_GAMES].iBitmapFrame0 = -1;
			blobs[MAX_PREV_GAMES].iBitmapFrame1 = -1;
//			blobs[MAX_PREV_GAMES].pBitmap = blobs[MAX_PREV_GAMES].pBitmap0;
			pBitmap = (unsigned char *)blobs[MAX_PREV_GAMES].pBitmap;
//                        memset(&pp, 0, sizeof(pp)); // set up source image as a PIL_PAGE
//			pp.pData = pBitmap;
//			pp.iWidth = blobs[MAX_PREV_GAMES].iWidth;
//			pp.iHeight = blobs[MAX_PREV_GAMES].iHeight;
//			pp.iPitch = iPitch;
//			pp.cBitsperpixel = 16;
//			pp.iDataSize = pp.iPitch * pp.iHeight;
//			pp.cCompression = PIL_COMP_NONE;
//		    printf("pBitmap=0x%08x, iPitch=%d\n", (int)pBitmap, iPitch);
            // allocate 4 buffers for "rewind" save state data
//	        for (i=0; i<4; i++)
//	           {
//	           blobs[MAX_PREV_GAMES].pRewind[i] = (unsigned char *)EMUAlloc(MAX_SAVE_SIZE);
//	           }
//	        blobs[MAX_PREV_GAMES].iRewindIndex = 0;
            bUserAbort = FALSE;
		if (iDisplayType != DISPLAY_LCD) // erase the framebuffer to black
		{
			memset(pFB, 0, iScreenSize);
		}
        }

    }
    else // not recognized
    {
    	iError = SG_ERROR_UNSUPPORTED;
    }

    sg_initexit:

	return iError;
} /* SG_InitGame() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : SG_PlayGame(bool, bool, uint32_t)                     *
 *                                                                          *
 *  PURPOSE    : Execute a single frame of the current game.                *
 *                                                                          *
 ****************************************************************************/
void SG_PlayGame(BOOL bUseAudio, BOOL bVideo, uint32_t ulKeys)
{
GAME_BLOB *pBlob = &blobs[MAX_PREV_GAMES];
//int iFrameUsed;
//volatile int iTimeOut;
//    __android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "entering SG_PlayGame(), iGameType=%d", iGameType);

//	wait_vsync();

//	iTimeOut = 20;
//	while (pBlob->iBitmapFrame1 != -1 && pBlob->iBitmapFrame0 != -1 && iTimeOut > 0) // opengl thread is still using the buffers we created
//	{
//		iTimeOut--; // allow up to 10ms for opengl thread to finish its work
//		delayMicroseconds(500);
//	}
//	if (pBlob->iBitmapFrame1 == -1) // use the buffer that's already been used by the render thread
//	{
//		pBitmap = pBlob->pBitmap = pBlob->pBitmap1;
//		iFrameUsed = 1;
//	}
//	else
//	{
//		pBitmap = pBlob->pBitmap = pBlob->pBitmap0;
//		iFrameUsed = 0;
//	}

//	if (iTimeOut == 0) // OpenGL is dragging, don't draw anything
//		bVideo = FALSE;
	(*emulators[iGameType].pfnPlay)(pBlob, bUseAudio, bVideo, ulKeys); // Execute 1 frame of the game

//	if (iFrameUsed == 1) // we used frame buffer 1
//    	pBlob->iBitmapFrame1 = iFrame;
//    else
 //   	pBlob->iBitmapFrame0 = iFrame;

    if ((iFrame & 0xff) == 0) // save the state every 4 seconds for rewind
       {
//       __android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "calling SGSaveGame at frame %d for rewind function", iFrame);
//       pBlob->iRewindFrame[pBlob->iRewindIndex] = iFrame;
//       SGSaveGame(NULL, pBlob, 0);
       }
    iFrame++;
//	printf("Frame = %d\n", iFrame);
    pBlob->iFrame = iFrame; // for Genesis ROM patching
//    if (!bRegistered && iFrame > 2*60*61)     // stop play after two minutes
 //      bUserAbort = TRUE;
//    __android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "exiting SG_PlayGame(), frame=%d", iFrame);

} /* SG_PlayGame() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : SG_GetLeafName(char *, char *)                             *
 *                                                                          *
 *  PURPOSE    : Retrieve the leaf name from a fully qualified path name.   *
 *  			 (removes the path and file extension - if any)             *
 *                                                                          *
 ****************************************************************************/
void SG_GetLeafName(char *fname, char *leaf)
{
int i, iLen;

   iLen = strlen(fname);
   for (i=iLen-1; i>=0; i--)
      {
#ifdef _WIN32
      if (fname[i] == '\\')
#else
      if (fname[i] == '/')
#endif
         break;
      }
   strcpy(leaf, &fname[i+1]);
   // remove the filename extension
   iLen = strlen(leaf);
   for (i=iLen-1; i>=0; i--)
      {
      if (leaf[i] == '.')
         {
         leaf[i] = 0;
         break;
         }
      }
} /* SG_GetLeafName() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : SG_GetLeafName2(char *, char *)                            *
 *                                                                          *
 *  PURPOSE    : Retrieve the leaf name from a fully qualified path name.   *
 *         (removes the path and file extension - if any)             *
 *                                                                          *
 ****************************************************************************/
void SG_GetLeafName2(char *fname, char *leaf)
{
int i, iLen;

   iLen = strlen(fname);
   for (i=iLen-1; i>=0; i--)
      {
      if (fname[i] == '/')
         break;
      }
   strcpy(leaf, &fname[i+1]);
} /* SG_GetLeafName2() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : SG_SavePrevPlay(GAME_BLOB)                                 *
 *                                                                          *
 *  PURPOSE    : Save the previously played game info.                      *
 *                                                                          *
 ****************************************************************************/
void SG_SavePrevPlay(GAME_BLOB *pBlob)
{
//PIL_FILE pf;
//PIL_PAGE pp1, pp2;
//PIL_VIEW pv;
char pszTemp[256], pszLeaf[256];
int i, j; //, iThumbCX;

#ifdef FUTURE
//	__android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "entering SG_SavePrevPlay()");
   iThumbCX = 160;
   // save the thumbnail image of the game
   memset(&pp1, 0, sizeof(pp1));
   pp1.pData = (unsigned char *)pBlob->pBitmap;
//   if (iCurrentSystem == 4) // TG16 uses an offset pointer
//      pp1.pData += 32;
   pp1.cBitsperpixel = 16;
   pp1.iHeight = pBlob->iHeight;
   pp1.iWidth = pBlob->iWidth;
   pp1.cCompression = PIL_COMP_NONE;
   pp1.iPitch = pBlob->iPitch;
   // squeeze it into a 80/160 pixel wide thumbnail image
   pv.iScaleX = (pBlob->iWidth*256)/iThumbCX;
   pv.iScaleY = pv.iScaleX;
   pv.pBitmap = EMUAlloc(iThumbCX*iThumbCX*2);
   pv.cFilter = PIL_VIEWFLAGS_AVERAGE; // do pixel averaging
   pv.iWidth = iThumbCX;
   pv.iPitch = iThumbCX*2;
   pv.iHeight = (pBlob->iHeight*256)/pv.iScaleX;
   pv.iOrientation = 0;
   pv.iWinX = 0;
   pv.iWinY = 0;
   PILDraw(&pp1, &pv, TRUE, NULL);
   // place the scaled down image into a new page structure
   memset(&pp2, 0, sizeof(pp2));
   pp2.cBitsperpixel = 16;
   pp2.cCompression = PIL_COMP_NONE;
   pp2.pData = pv.pBitmap;
   pp2.iWidth = pv.iWidth;
   pp2.iHeight = pv.iHeight;
   pp2.iPitch = pv.iPitch;
   // Convert it to JPEG
   PILModify(&pp2, PIL_MODIFY_COLORS, 24, 0);
   pp1.cCompression = PIL_COMP_LZW;
   PILConvert(&pp2, &pp1, 0, NULL, NULL);
   // save the thumbnail image
   SG_GetLeafName(pszGame, pszLeaf); // get the rom leaf name
   sprintf(pszTemp, "%s%s.tif", pszSAVED, pszLeaf);
   PILCreate((char *)pszTemp, &pf, 0, PIL_FILE_TIFF);
   PILWrite(&pf, &pp1, 0);
   PILClose(&pf);
   PILFree(&pp1);
   PILFree(&pp2);
   EMUFree(pv.pBitmap);
#endif // FUTURE
   // Adjust the previously played games list
   // see if the filename was already there in the top spot
   for (i=0; i<MAX_PREV_GAMES; i++)
      {
      if (strcmp(szGameFiles[i], pszGame) == 0)
         break;
      }
   if (i == 0) // no change, return
      return;
   if (i == MAX_PREV_GAMES) // not found, move everything down and insert new info
      {
      if (szGameFiles[MAX_PREV_GAMES-1][0]) // list is full, delete old image file at bottom
         {
         SG_GetLeafName(szGameFiles[MAX_PREV_GAMES-1], pszLeaf); // get the rom leaf name
         sprintf(pszTemp, "%s%s.tif", pszSAVED, pszLeaf);
         EMUDelete(pszTemp);
         }
      for (i=MAX_PREV_GAMES-2; i>=0; i--)
         {
         strcpy(szGameNames[i+1], szGameNames[i]);
         strcpy(szGameFiles[i+1], szGameFiles[i]);
         }
      }
   else // it was found, but in a different position
      {
      for (j=i; j>0; j--) // cover up old spot
         {
         strcpy(szGameNames[j], szGameNames[j-1]);
         strcpy(szGameFiles[j], szGameFiles[j-1]);
         }
      }
   // put new game in top position
   strcpy(szGameNames[0], (const char *)pBlob->szGameName);
   strcpy(szGameFiles[0], pszGame);
//   SG_WritePrevList(); // save to registry
   // since we just played this game, make it the current on the main window
   iPrevGame = 0;
//	__android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "exiting SG_SavePrevPlay()");
} /* SG_SavePrevPlay() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : SG_TerminateGame(void)                                     *
 *                                                                          *
 *  PURPOSE    : Free the resources of the current game.                    *
 *                                                                          *
 ****************************************************************************/
void SG_TerminateGame(void)
{
//int i;

//	__android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "entering SG_TerminateGame()");

// DEBUG - re-enable this at some future time
//	SGSaveGame(blobs[MAX_PREV_GAMES].szGameName, &blobs[MAX_PREV_GAMES], 0); // always save the last state
    EMUFree((void *)blobs[MAX_PREV_GAMES].pBitmap);
    blobs[MAX_PREV_GAMES].pBitmap = NULL;
    (*emulators[iGameType].pfnTerminate)(&blobs[MAX_PREV_GAMES]);
//    if (blobs[MAX_PREV_GAMES].iGameType != SYS_COINOP)
//    	SG_SavePrevPlay(&blobs[MAX_PREV_GAMES]);
#ifndef _WIN32
    delayMicroseconds(16666); // give video frame time to use the last buffer
#endif
//    blobs[MAX_PREV_GAMES].pBitmap0 -= (32 + 16*iPitch);
//    blobs[MAX_PREV_GAMES].pBitmap1 -= (32 + 16*iPitch);
//    EMUFree((void *)blobs[MAX_PREV_GAMES].pBitmap0);
//    EMUFree((void *)blobs[MAX_PREV_GAMES].pBitmap1);
    blobs[MAX_PREV_GAMES].pBitmap0 = NULL;
    blobs[MAX_PREV_GAMES].pBitmap1 = NULL;
    pBitmap = NULL;
//    EMUFree(InQ);
//    EMUFree(OutQ);
    // free the rewind buffers
//    for (i=0; i<4; i++)
//       {
//       EMUFree(blobs[MAX_PREV_GAMES].pRewind[i]);
//       }

    if (bUseSound)
       {
//       EMUCloseSound();
       }
//    if (iDisplayType != DISPLAY_LCD) // clear the framebuffer
//    {
//        memset(pFB, 0, iScreenSize);
//    }
//    if (bMaster) // Tell the slave to stop
//       {
//       SGSendNetPacket(NULL, 0, SGNETWORK_END);
//       }
//    __android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "exiting SG_TerminateGame()");
  lcd.fillScreen(0);
} /* SG_TerminateGame() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUSaveBRAM(char *, int)                                   *
 *                                                                          *
 *  PURPOSE    : Save the game's backup RAM to disk.                        *
 *                                                                          *
 ****************************************************************************/
void EMUSaveBRAM(GAME_BLOB *pBlob, char *pData, int iLen, char *szSystem)
{
char pszTemp[256];
File oHandle;

    sprintf(pszTemp, "%s%s.%s.bak", pszSAVED, pBlob->szGameName, szSystem);
    oHandle = EMUCreate(pszTemp);
    if (oHandle == 0)
    {
//        __android_log_print(ANDROID_LOG_VERBOSE, "EMUSaveBRAM", "Error creating file %s", pszTemp);
    }
    else
    {
    	EMUWrite(oHandle, pData, iLen);
    	EMUClose(oHandle);
    }

} /* EMUSaveBRAM() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMULoadBRAM(char *, int)                                   *
 *                                                                          *
 *  PURPOSE    : Load the game's backup RAM from disk.                      *
 *                                                                          *
 *  RETURNS    : FALSE if success, TRUE if failure.                         *
 *                                                                          *
 ****************************************************************************/
BOOL EMULoadBRAM(GAME_BLOB *pBlob, char *pData, int iLen, char *szSystem)
{
char pszTemp[256];
File iHandle;
int iReadLen;

    sprintf(pszTemp, "%s%s.%s.bak", pszSAVED, pBlob->szGameName, szSystem);
    iHandle = EMUOpenRO(pszTemp);
    if (iHandle == 0)
       return TRUE;
    iReadLen = EMURead(iHandle, pData, iLen);
    EMUClose(iHandle);
    if (iReadLen != iLen) // file was not what we expected (maybe the save data size changed), abort
    {
    	memset(pData, 0xff, iReadLen); // clear out the data we did actually read
    	return TRUE; // return failure
    }
    return FALSE;

} /* EMULoadBRAM() */

//
// Memory area functions used for synchronizing a master/slave game
//
void SGPrepMemAreas(GAME_BLOB *pBlob)
{
int i;

   i = 0;
   while (pBlob->mem_areas[i].iAreaLength)
      {
      pBlob->mem_areas[i].pCompareArea = (unsigned char *)EMUAlloc(pBlob->mem_areas[i].iAreaLength);
      i++;
      }
//   pGameData = EMUAlloc(MAX_BLOCK_SIZE);

} /* SGPrepMemAreas() */

void SGFreeMemAreas(GAME_BLOB *pBlob)
{
int i;

// Free the temporary areas used for comparison
   i = 0;
   while (pBlob->mem_areas[i].iAreaLength)
      {
      EMUFree(pBlob->mem_areas[i].pCompareArea);
      i++;
      }
//   EMUFree(pGameData); // free temp data area

} /* SGFreeMemAreas() */

// Compress the data into skip values (byte+1) up to 127
// and copy values (-byte), then transmit to slave
void SGCompressMemAreas(GAME_BLOB *pBlob)
{
int i, iArea, iAreaLen, iStart, iLen, iCount;
unsigned char *s, *d;

   iLen = 0;
   // Compress each memory area
   iArea = 0;
   while (pBlob->mem_areas[iArea].iAreaLength)
      {
      s = pBlob->mem_areas[iArea].pPrimaryArea;
      d = pBlob->mem_areas[iArea].pCompareArea;
      iAreaLen = pBlob->mem_areas[iArea].iAreaLength;
      i = 0;
      while (i < iAreaLen)
         {
         iCount = 0;
         while (i < iAreaLen && s[i] == d[i]) // count up the area without changes
            {
            i++;
            iCount++;
            }
         if (iCount >= 128) // there area unchanged areas
            {
            pGameData[iLen++] = (unsigned char)(0x80 + (iCount >> 8)); // store skip values >= 128 with 2 bytes
            pGameData[iLen++] = (unsigned char)(iCount & 0xff);
            }
         else // count <= 127 (including 0)
            {
            pGameData[iLen++] = iCount;
            }
         if (i == iAreaLen)
            break; // don't compress a zero length changed area if we're at the end of the area
         iCount = 0; // count changed bytes
         iStart = i; // start of disimilar area
         while (i < iAreaLen && s[i] != d[i]) // count up the area with changes
            {
            i++;
            iCount++;
            }
         if (iCount >= 128) // store change count
            {
            pGameData[iLen++] = (unsigned char)(0x80 + (iCount >> 8)); // store skip values >= 128 with 2 bytes
            pGameData[iLen++] = (unsigned char)(iCount & 0xff);
            }
         else // count <= 127 (including 0)
            {
            pGameData[iLen++] = iCount;
            }
         if (iCount)
            {
            memcpy(&pGameData[iLen], &s[iStart], iCount);
            memcpy(&d[iStart], &s[iStart], iCount); // update the comparison area
            iLen += iCount;
            }
         }
      iArea++;
      }
// DEBUG
//   SGSendNetPacket(pGameData, iLen, SGNETWORK_MEMDATA); // send to slave

} /* SGCompressMemAreas() */

void SGDecompressMemAreas(GAME_BLOB *pBlob, unsigned char *pOut, int iLen)
{
int i, iArea, iCount;
unsigned char *d;

   iArea = 0;
   while (pBlob->mem_areas[iArea].iAreaLength)
      {
      if (pBlob->mem_areas[iArea].iAreaLength) // is it in use?
         {
         i = 0;
         d = pBlob->mem_areas[iArea].pPrimaryArea;
         while (i < pBlob->mem_areas[iArea].iAreaLength)
            {
            iCount = *pOut++;
            if (iCount & 0x80) // count is >= 128
               {
               iCount = ((iCount & 0x7f) << 8); // high byte
               iCount += *pOut++; // low byte
               }
            i += iCount; // skip area unchanged
            if (i < pBlob->mem_areas[iArea].iAreaLength) // if we're not at the end already
               { // get the changed bytes
               iCount = *pOut++;
               if (iCount & 0x80) // count is >= 128
                  {
                  iCount = ((iCount & 0x7f) << 8); // high byte
                  iCount += *pOut++; // low byte
                  }
               memcpy(&d[i], pOut, iCount); // copy the changed bytes
               pOut += iCount;
               i += iCount;
               }
            } // while in the current area
         } // if area is in use
      iArea++;
      } // for each area
} /* SGDecompressMemAreas() */

void SG_GetTime(int *iDay, int *iHour, int *iMinute, int *iSecond)
{
#ifdef FUTURE
time_t currenttime;
struct tm * timeinfo;
int i, iTotalDays;
static unsigned char ucMonthLen[12] = {31,28,31,30,31,30,31,31,30,31,30,31};

   currenttime = time(NULL);
   timeinfo = localtime(&currenttime);
   // calculate the day of the year
   iTotalDays = 0;
   for (i=timeinfo->tm_mon-1; i>0; i--) // add up everything previous
      {
      iTotalDays += ucMonthLen[i-1];
      }
   *iDay = iTotalDays + timeinfo->tm_mday;
   *iHour = timeinfo->tm_hour;
   *iMinute = timeinfo->tm_min;
   *iSecond = timeinfo->tm_sec;
#endif
} /* SG_GetTime() */
/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUFreeVideoBuffer(char *)                                 *
 *                                                                          *
 *  PURPOSE    : Free the buffer used to hold video memory.                 *
 *                                                                          *
 ****************************************************************************/
void EMUFreeVideoBuffer(unsigned char *pBuffer)
{
//	EMUFree(pBuffer);
} /* EMUFreeVideoBuffer() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUCreateVideoBuffer(int,int,int,char **)                  *
 *                                                                          *
 *  PURPOSE    : Create the buffer used to hold video memory.               *
 *                                                                          *
 ****************************************************************************/
void EMUCreateVideoBuffer(int iScreenX, int iScreenY, int iScreenBPP, unsigned char **pBuffer)
{

       if (iScreenBPP == 16)
          {
          iPitch = iScreenX*2;
          }
       if (iScreenBPP == 8)
          {
          iPitch = (iScreenX + 3) & 0xfffc; /* Dword align */
          }
//       *pBuffer = EMUAlloc(iPitch*iScreenY);

} /* EMUCreateVideoBuffer() */

#ifdef BLUETOOTH
#ifndef __MACH__
static void ConnectBT (GtkWidget *widget, GtkWidget* pWindow)
{
int sock;
inquiry_info *ii = NULL;
char addr[19] = {0};
char name[248] = {0};
int len, i, flags;
int dev_id, max_rsp, num_rsp;

	//printf("Entering ConnectBT\n");

	dev_id = hci_get_route(NULL);
	sock = hci_open_dev(dev_id);
	if (dev_id < 0 || sock < 0)
	{
		printf("Error opening socket\n");
	}
	printf("dev_id=%d, sock=%d\n", dev_id, sock);
	len = 8;
	max_rsp = 255;
	flags = IREQ_CACHE_FLUSH;
	ii = (inquiry_info *)malloc(max_rsp * sizeof(inquiry_info));
	printf("About to call hci_inquiry\n");
	num_rsp = hci_inquiry(dev_id, len, max_rsp, NULL, &ii, flags);
	printf("Returned from hci_inquiry, num_rsp = %d\n", num_rsp);
	if (num_rsp < 0) printf("hci_inquiry error\n");

	for (i=0; i<num_rsp; i++)
	{
		ba2str(&(ii+i)->bdaddr, addr);
		memset(name, 0, sizeof(name));
		if (hci_read_remote_name(sock, &(ii+i)->bdaddr, sizeof(name), name, 0) < 0)
			strcpy(name, "[unknown]");
		printf("%s %s\n", addr, name);
		if (strcmp(name, "Zeemote: SteelSeries FREE") == 0)
		{
			break;
		}
	} // for each device found

	if (num_rsp >= 1) // found something
	{
		struct sockaddr_rc btaddr = {0};
		char zeemote[32] = "28:9A:4B:00:03:99";
		int status, iTotal;
		iGameSocket = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
		btaddr.rc_family = AF_BLUETOOTH;
		btaddr.rc_channel = (uint8_t) 1;
//		str2ba(addr, &btaddr.rc_bdaddr);
		str2ba(zeemote, &btaddr.rc_bdaddr);
		printf("About to connect to %s\n", name);
		status = connect(iGameSocket, (struct sockaddr *)&btaddr, sizeof(btaddr));
		if (status == 0) // worked
		{
			unsigned char cBuf[32];
			printf("Socket connected!\n");
			iTotal = 0;
			while (iTotal < 100)
			{
				i = read(iGameSocket, cBuf, 20);
				printf("Received %d bytes\n", i);
				if (i > 0) iTotal += i;
			}
		}
	}
	free(ii);
	close( sock );

} /* ConnectBT() */
#endif // __MACH__
#endif // BLUETOOTH

static int openFile (char *filename)
{
int rc, iFrame;
int64_t delta, targettime, /* curtime,*/ frametime;
uint32_t u32Quit = RKEY_SELECT_P1 | RKEY_START_P1; // quit signal

  frametime = 16667; // frame time in microseconds
  
    strcpy(szFileName, filename);
    rc = SG_InitGame((unsigned char *)szFileName);
    if (rc == SG_NO_ERROR)
    {
	// Clear previous audio
//	memset(pAudioBuffer, 0, 44100*4); // maximum buffer size = 1 second
	iAudioTail = iAudioHead = 0;
	iAudioAvailable = 0;

//	if (iLCDY > iLCDX) //blobs[MAX_PREV_GAMES].iWidth > iLCDX || blobs[MAX_PREV_GAMES].iWidth == 160) // need to rotate the display
//	{
//		spilcdSetOrientation(&lcd, LCD_ORIENTATION_90);
//		if (iLCDX > 128)
//		{
		//DEBUG - bDetectScroll = TRUE;
//		iScrollScale = (blobs[MAX_PREV_GAMES].iWidth == 160) ? 2:1;
//		}
//	}
//	else
//	{
//		spilcdSetOrientation(&lcd, LCD_ORIENTATION_0);
		bDetectScroll = FALSE;
//	}
 if (bVerbose)
 {
	Serial.println("About to start running\n");
 }
	bRunning = TRUE;
  iFrame = 0;
//	LoadCheats(); // try to load cheat codes for this game
// DEBUG
  rc = 0;
  SG_InitSPIGraphics(&blobs[MAX_PREV_GAMES]);
  targettime = micros() /*esp_timer_get_time()*/ + frametime;
  while (bRunning)
  {
//    rc++;
//    Serial.print("Frame ");
//    Serial.println(rc);
    UpdateGPIOButtons();
    u32ControllerBits = u32GPIOBits;
    SG_PlayGame(/*TRUE*/FALSE, iFrame & 1, u32ControllerBits);
    //SG_PushSamples((unsigned char *)blobs[MAX_PREV_GAMES].pAudioBuf, blobs[MAX_PREV_GAMES].iSampleCount);
    if ((u32ControllerBits & u32Quit) == u32Quit)
       bRunning = FALSE;
    delta = targettime - micros(); //esp_timer_get_time();
   // if (delta > 1000) // we have at least a millisecond to kill
   //    delayMicroseconds(delta);
   // if (delta < 0 || iAudioAvailable < blobs[MAX_PREV_GAMES].iSampleCount) // problem - we're falling behind, so render a frame with just audio
   // {
   //   SG_PlayGame(TRUE, FALSE, u32ControllerBits);
   //   SG_PushSamples((unsigned char *)blobs[MAX_PREV_GAMES].pAudioBuf, blobs[MAX_PREV_GAMES].iSampleCount);
   //   targettime += frametime;      
   // }
//    memset(u32OldRegions, 0xff, 32 * 4);
    targettime += frametime;
    iFrame++;
  }
  SG_TerminateGame();
	return 0;
    }
	return -1;
} /* openFile() */

#ifdef FUTURE
static gboolean idlecallback(gpointer notused, int toggle)
{
SDL_Event event;
     while (SDL_PollEvent(&event))
     {
          if (event.type == SDL_KEYDOWN)
              printf("key = %d", event.key.keysym.sym);
	  if (event.type == SDL_WINDOWEVENT)
	  {
		if (event.window.event == SDL_WINDOWEVENT_MOVED)
		{
			int top, left;//, bottom, right;
			iWindowX = (int)event.window.data1;
			iWindowY = (int)event.window.data2;
// DEBUG - only available on SDL 2.0.5 and above
//			SDL_GetWindowBordersSize(sdlwindow, &top, &left, &bottom, &right);
			top = 30; left = 1;
			iWindowX += left;
			iWindowY += top;
//			printf("window moved, x=%d, y=%d\n", iWindowX, iWindowY);
		}
	  }
     }

    return TRUE;
} /* idlecallback() */

static void activate (GtkApplication *app, gpointer user_data)
{
  GtkToolItem *openTb;
//  GtkToolItem *saveTb;
  GtkToolItem *sep;
  GtkToolItem *exitTb;
  GtkToolItem *resetTb;
  GtkToolItem *isoTb;
//  GtkToolItem *prevTb;
//  GdkRGBA color;
  GError *err;

//  color.red = 0xffff;
//  color.blue = 0xffff;
//  color.green = 0xffff;
//  color.alpha = 0xffff;

  window = (GtkWindow *)gtk_application_window_new (app);
 g_signal_connect(window,"destroy",G_CALLBACK(endApp),NULL);
  gtk_window_set_title (GTK_WINDOW (window), "SmartGear");
  gtk_window_set_default_size (GTK_WINDOW (window), DEFAULT_WIN_WIDTH, DEFAULT_WIN_HEIGHT);
//  gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
  gtk_window_set_icon_from_file(window, "smartgear.png", &err);
//  gtk_widget_override_background_color(GTK_WIDGET(window), GTK_STATE_NORMAL, &color);

  box = (GtkBox *)gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(box));

  toolbar = gtk_toolbar_new();
  gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);
  openTb = gtk_tool_button_new(gtk_image_new_from_icon_name("document-open", GTK_ICON_SIZE_MENU), NULL);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), openTb, -1);
  perfTb = gtk_check_button_new_with_label("performance test");
//  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), perfTb, -1);
  g_signal_connect(perfTb, "clicked", G_CALLBACK(perfToggled), NULL);
  head2headTb = gtk_check_button_new_with_label("Head-2-Head (GBC/GG)");
  audioTb = gtk_check_button_new_with_label("enable audio");
  gtk_toggle_button_set_active(audioTb, TRUE); // default to audio on
  framebufferTb = gtk_check_button_new_with_label("use framebuffer");
  gtk_toggle_button_set_active(framebufferTb, FALSE);
  stretchTb = gtk_check_button_new_with_label("Smooth Stretch 2X");
  gtk_toggle_button_set_active(stretchTb, TRUE); // default to on
  p1controlsTb = gtk_combo_box_text_new();
  gtk_combo_box_set_title(p1controlsTb, "P1 Controls");
  p2controlsTb = gtk_combo_box_text_new();
  gtk_combo_box_set_title(p2controlsTb, "P2 Controls");
  gtk_combo_box_text_append (p1controlsTb, NULL, "Keyboard");
  gtk_combo_box_text_append (p1controlsTb, NULL, "iCade");
  gtk_combo_box_text_append (p1controlsTb, NULL, "Joystick 0");
  gtk_combo_box_text_append (p1controlsTb, NULL, "Joystick 1");
  gtk_combo_box_text_append (p2controlsTb, NULL, "Keyboard");
  gtk_combo_box_text_append (p2controlsTb, NULL, "iCade");
  gtk_combo_box_text_append (p2controlsTb, NULL, "Joystick 0");
  gtk_combo_box_text_append (p2controlsTb, NULL, "Joystick 1");
  gtk_combo_box_set_active (p1controlsTb, 0);
  gtk_combo_box_set_active (p2controlsTb, 2);

//  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), head2headTb, -1);
//  saveTb = gtk_tool_button_new(gtk_image_new_from_icon_name("document-save", GTK_ICON_SIZE_MENU), NULL);
//  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), saveTb, -1);
//  prevTb = gtk_tool_button_new(gtk_image_new_from_icon_name("media-skip-backward", GTK_ICON_SIZE_MENU), NULL);
//  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), prevTb, -1);
//  pageButton = gtk_tool_button_new(NULL, NULL);
//  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), pageButton, -1);
  isoTb = gtk_tool_button_new(NULL, "ISO");
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), isoTb, -1);
  resetTb = gtk_tool_button_new(NULL, "Reset");
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), resetTb, -1);
  sep = gtk_separator_tool_item_new();
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), sep, -1);
//  gtk_tool_item_set_expand(GTK_TOOL_ITEM(sep), -1);
//  gtk_separator_tool_item_set_draw(GTK_SEPARATOR_TOOL_ITEM(separator), FALSE);
  exitTb = gtk_tool_button_new(gtk_image_new_from_icon_name("application-exit", GTK_ICON_SIZE_MENU), NULL);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), exitTb, -1);

  gtk_box_pack_start(box, toolbar, FALSE, FALSE, 4);
  gtk_box_pack_start (box, head2headTb, FALSE, FALSE, 4);
  gtk_box_pack_start (box, perfTb, FALSE, FALSE, 4);
  gtk_box_pack_start (box, audioTb, FALSE, FALSE, 4);
  gtk_box_pack_start (box, framebufferTb, FALSE, FALSE, 4);
  gtk_box_pack_start (box, stretchTb, FALSE, FALSE, 4);
  gtk_box_pack_start (box, p1controlsTb, FALSE, FALSE, 4);
  gtk_box_pack_start (box, p2controlsTb, FALSE, FALSE, 4);
  darea = gtk_drawing_area_new();
//  gtk_widget_override_background_color(darea, GTK_STATE_NORMAL, &color);

//  gtk_box_pack_start(GTK_BOX(vbox), darea, FALSE, FALSE, 1);

  gtk_widget_add_events(darea, GDK_SCROLL_MASK |
				GDK_BUTTON1_MASK |
				 GDK_POINTER_MOTION_MASK |
				 GDK_BUTTON_PRESS_MASK |
				 GDK_BUTTON_RELEASE_MASK |
				GDK_BUTTON_MOTION_MASK );
//  g_signal_connect (darea, "button_release_event", G_CALLBACK (mouseReleaseEvent), NULL);
//  g_signal_connect (darea, "button_press_event", G_CALLBACK (mousePressEvent), NULL);
//  g_signal_connect (darea, "motion_notify_event", G_CALLBACK (mouseMoveEvent), NULL);
//  g_signal_connect (darea, "scroll_event", G_CALLBACK (mouseScrollEvent), NULL);
//  gtk_widget_set_size_request(darea, 400, 300);
//  theimage = cairo_image_surface_create_from_png("/home/pi/Documents/TIFF/Gold.png");

//  gtk_widget_set_double_buffered(darea, FALSE);
//  gtk_container_add(GTK_CONTAINER(window), darea);

  gtk_widget_set_app_paintable(darea, TRUE);
  gtk_widget_show(darea);
//  gtk_container_set_resize_mode(darea, GTK_RESIZE_IMMEDIATE);
  gtk_box_pack_end(box, darea, TRUE, TRUE, 0);

  g_signal_connect (openTb, "clicked", G_CALLBACK (openClick), NULL);
  g_signal_connect (exitTb, "clicked", G_CALLBACK (exitClick), NULL);
//  g_signal_connect (prevTb, "clicked", G_CALLBACK (prevPage), NULL);
  g_signal_connect (resetTb, "clicked", G_CALLBACK (resetClick), NULL);
  g_signal_connect (isoTb, "clicked", G_CALLBACK (isoClick), NULL);

//  g_signal_connect(G_OBJECT(darea), "draw", G_CALLBACK(on_draw_event), NULL);

//  g_signal_connect(darea, "size-allocate", G_CALLBACK(dareaChanged), NULL);
//    g_signal_connect(G_OBJECT(window), "configure-event", G_CALLBACK(windowChanged), NULL);
//  g_signal_connect(G_OBJECT(window), "key_press_event", G_CALLBACK(onKeyPress), NULL);
//  g_signal_connect(G_OBJECT(window), "key_release_event", G_CALLBACK(onKeyRelease), NULL);
//g_idle_add (G_CALLBACK(idlecallback), NULL);
g_timeout_add(33, G_CALLBACK(idlecallback), NULL);
  gtk_widget_show_all(GTK_WIDGET(box));
  gtk_window_present(window);
} /* activate() */
#endif // FUTURE

#ifdef FUTURE
void ProcessOptions(int argc, char **argv)
{
int i = 1;
char *p;

	// set default values
	iSPIChan = 0;
	iSPIFreq = 0;
	iLCDType = -1;
	bLCDFlip = 0; // assume not flipped
	iDC = 16;
	iReset = 18;
	iLED = 11;
	bVerbose = 0;
	p = getcwd(szDir, 256); // start in the current working directory
	if (p != NULL) {}
	szGame[0] = '\0';
        iP1Control = CONTROLLER_JOY0;
        iP2Control = CONTROLLER_JOY1;

	while (i < argc)
	{
		if (0 != strncmp("--", argv[i], 2))
			break;
		if (0 == strcmp("--p1", argv[i]))
		{
			if (0 == strcmp("kb", argv[i+1]))
				iP1Control = CONTROLLER_KEYBOARD;
			else if (0==strcmp("js1", argv[i+1]))
				iP1Control = CONTROLLER_JOY1;
			i += 2;
		}
		else if (0 == strcmp("--p2", argv[i]))
		{
                        if (0 == strcmp("kb", argv[i+1]))
                                iP2Control = CONTROLLER_KEYBOARD;
                        else if (0==strcmp("js0", argv[i+1]))
                                iP2Control = CONTROLLER_JOY0;
                        i += 2;
		}
		else if (0 == strcmp("--dc", argv[i]))
		{
			iDC = atoi(argv[i+1]);
			i += 2;
		}
                else if (0 == strcmp("--reset", argv[i]))
                {
                        iReset = atoi(argv[i+1]);
                        i += 2;
                }
                else if (0 == strcmp("--led", argv[i]))
                {
                        iLED = atoi(argv[i+1]);
                        i += 2;
                }
                else if (0 == strcmp("--verbose", argv[i]))
                {
                        bVerbose = 1;
                        i++;
                }
		else if (0 == strcmp("--game", argv[i]))
		{
                        if (argv[i+1][0] != '/') // relative path
                        {
			char *p;
				p = getcwd(szGame, 256);
				if (p != NULL) {}
                                strcat(szGame, "/");
                                strcat(szGame, argv[i+1]);
                        }
                        else // absolute path
                        {
                                strcpy(szGame, argv[i+1]);
                        }
                        i += 2;
		}
		else if (0 == strcmp("--dir", argv[i]))
		{
			if (argv[i+1][0] != '/') // relative path
			{
				strcat(szDir, "/");
				strcat(szDir, argv[i+1]);
			}
			else // absolute path
			{
				strcpy(szDir, argv[i+1]);
			}
			i += 2;
		}
		else i++;
	}
} /* ProcessOptions() */
#endif

//
// Return 0 for success, 1 for failure
//
int InitDisplay(void)
{
  pLCD = lcd.getLCDStruct();
//	if (spilcdInit(iLCDType, bLCDFlip, iSPIChan, iSPIFreq, iDC, iReset, iLED, iMISO, iMOSI, iSCLK))
//  if (spilcdInit(LCD_ILI9341, 1, 20000000, 5, 21, -1, 14, 19, 23, 18)) // ODROID-GO
//  if (spilcdInit(LCD_ILI9341, 1, 20000000, 27, 15, -1, 14, 19, 23, 18))
#ifdef TEENSYDUINO
static uint8_t ucTXBuf[4096];
  spilcdSetTXBuffer(ucTXBuf, sizeof(ucTXBuf));
  if (spilcdInit(&lcd, LCD_ILI9341, FLAGS_NONE, 80000000, -1, 9, 8, -1,  -1, -1, -1))
	    return 1;
//	spilcdSetGamma(iGamma); // set the gamma curve (0/1)
  iLCDX = 240;
  iLCDY = 320;
#else // ESP32 ttgo t-camera-plus
//#define LCD_CS 12
//#define LCD_DC 15
//#define LCD_LED 2
//  spilcdInit(LCD_ST7789, 0, 0, 0, 32000000, LCD_CS, LCD_DC, -1, LCD_LED, -1, -1, -1);
  iLCDX = lcd.width();
  iLCDY = lcd.height();
 // spilcdWriteString(&lcd, 0,0,(char *)"Hello", 0xf800, 0, FONT_8x8, DRAW_TO_LCD);
 // delay(5000);
#endif

//  spilcdWriteString(0,0,(char *)"Hello", 0x6e0, 0, FONT_STRETCHED, 1);
//  delay(5000);
	return 0;
} /* InitDisplay() */

void DisplayShutdown(void)
{
	//spilcdShutdown(&lcd);
} /* DisplayShutdown() */
//
// Shut down gracefully when CTRL-C is pressed
//
int sg_main(char *szArg)
{
  int retval = 0;
//  pthread_t tinfo;
//  int iTimeout;
  char szTemp[256];

  Serial.begin(115200);
  bVerbose = 1; // DEBUG
	iNumProcessors = 1; //get_nprocs(); // number of CPU cores
	szGame[0] = '\0';
	if (InitDisplay())
        {
                Serial.println("Display failed to initialize\n");
                retval = -1;
                goto quit;
        }
        else
        {
                if (bVerbose)
                        Serial.println("Display initialized successfully\n");
        }
        iP1Control = iP2Control = CONTROLLER_GPIO; // DEBUG
        if (iP1Control == CONTROLLER_GPIO || iP2Control == CONTROLLER_GPIO)
        {
                if (!InitGPIO())
                {
                        Serial.println("Error initializing GPIO buttons\n");
                        return -1;
                }
		else if (bVerbose) Serial.println("GPIO buttons initialized successfully\n");
        }

//	if (iP1Control == CONTROLLER_KEYBOARD || iP2Control == CONTROLLER_KEYBOARD)
//	{
//		if (!InitKeyboard(1))
//		{
//			printf("Error initializing raw keyboard mode\n");
//			return -1;
//		}
//		else if (bVerbose) printf("Keyboard initialized successfully\n");
//	}

//  if (bVerbose)
//  {
//  if (sizeof(char *) == 4)
//	printf("SmartGear - running on 32-bit system\n");
//  else
//	printf("SmartGear - running on 64-bit system\n");
//  }

// Start SDL audio+video
#ifdef FUTURE
	SDL_zero(m_SWantedSpec);
	SDL_zero(m_SSDLspec);
	SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1"); // if not set, slow systems won't get button events
	if (SDL_Init(SDL_INIT_AUDIO  | /* SDL_INIT_VIDEO  | */ SDL_INIT_TIMER | SDL_INIT_JOYSTICK))
	{
		printf("Could not initialize SDL - %s\n", SDL_GetError());
	}
	else
	{
		if (bVerbose)
			printf("SDL initialized; num_joysticks=%d\n", SDL_NumJoysticks());
	}
        if (AboutMenu()) // display about
        {       
                printf("SmartGear has expired; get a newer version\n");
                goto quit;
        }

//	SG_InitJoysticks();
//	SDL_SetEventFilter(SG_SDLEventFilter, NULL);
//	SDL_AddEventWatch(SG_SDLEventFilter, NULL); // capture events from the SDL window
//	pAudioBuffer = EMUAlloc(44100*4); // maximum buffer size = 1 second
#endif // FUTURE
//	iAudioTail = iAudioHead = 0;
//	iAudioTotal = 44100; // total buffer size
//	iAudioAvailable = 0;

// start a second thread to do the actual emulator work/timing
//        pthread_create(&tinfo, NULL, SG_RunFrames, NULL);
// start a third thread to send the image to the SPI LCD
//	pthread_create(&tinfo, NULL, SG_LCD, NULL);
strcpy(szDir, "/"); // start in root dir
        if (szArg == NULL || szArg[0] == '\0')
        {
           while (GamepadGUI(szDir, szTemp))
           {
              openFile(szTemp);
           }
           goto quit;
        }
        else
        {
          openFile(szArg);
        }
#ifdef FUTURE
        else
        {
	   if (!openFile(szGame))
	   {
            		bQuit = FALSE;
                 while (!bQuit)
                 {
            		     UpdateGPIOButtons();
                     delayMicroseconds(5000);
                 } // while !bQuit
		        SG_QuitGame(); // close audio and free memory
           }
           else
           {
//		printf("Error running %s, exiting\n", szGame);
		retval = -1;
		goto quit;
	   }
        }
#endif // FUTURE

quit:
//  if (iP1Control == CONTROLLER_KEYBOARD || iP2Control == CONTROLLER_KEYBOARD)
//     InitKeyboard(0); // restore console mode
  delayMicroseconds(10000);
//  if (bVerbose)
//    printf("shutting down...\n");
  DisplayShutdown();
  bDone = TRUE;
//  pthread_cond_signal(&lcd_changed); // cause LCD thread to quit gracefully
  delayMicroseconds(10000);
  return retval;
} // main()


uint64_t SG_Clock()
{
	uint64_t ns;
//	struct timespec time;

//#ifdef	__MACH__
//	clock_serv_t cclock;
//	mach_timespec_t mts;
//	host_get_clock_services(mach_host_self(), CALENDAR_CLOCK, &cclock);
//	clock_get_time(cclock, &mts);
//	mach_port_deallocate(mach_task_self(), cclock);
//	time.tv_sec = mts.tv_sec;
//	time.tv_nsec = mts.tv_nsec;
//#else
//	clock_gettime(CLOCK_MONOTONIC, &time);
//#endif
//	ns = time.tv_nsec + (time.tv_sec * 1000000000LL);
  ns = micros() * 1000LL;
	return ns;
} /* SG_Clock() */

BOOL SG_Sleep(uint64_t ns)
{
//struct timespec ts;

	if (ns <= 100LL || ns > 999999999LL) return FALSE;
//	ts.tv_sec = 0;
//	ts.tv_nsec = ns;
//	nanosleep(&ts, NULL);
	return TRUE;

} /* SG_Sleep() */

#ifdef FUTURE
//
// Copy a 16x16 RGB565 tile from current frame to previous
//
static void SG_CopyTile(int x, int y, int cx, int cy, unsigned char *pSrc, unsigned char *pDst, int iPitch)
{
int dy, dx;
uint32_t *s, *d;
	for (dy=0; dy<cy; dy++)
	{
		s = (uint32_t *)&pSrc[((dy+y)*iPitch)+x*2];
		d = (uint32_t *)&pDst[((dy+y)*iPitch)+x*2];
		for (dx=0; dx<cx; dx+=2)
		{
			*d++ = *s++; 
		}
	} // for dy
} /* SG_CopyTile() */
#endif // FUTURE
//
// Calculate a checksum for a 24x24 tile
//
uint32_t GetTileSum(unsigned char *pTile, int iPitch)
{
uint32_t u32Sum, u32, *s;
int x, y, shift;

  u32Sum = 0;
  shift = 0;
  for (y=0; y<24; y++)
  {
    s = (uint32_t *)&pTile[y * iPitch];
    for (x=0; x<12; x++) // 12 dwords wide = 24 shorts
    {
      u32 = *s++;
      u32Sum += ((u32 >> shift) | (u32 << shift)); // shift around to to account for moving pixels
      shift = ((shift + 1) & 0x1f);
    } // for x
  } // for y
  return u32Sum;
} /* GetTileSum() */

//
// Draw a single row of tiles
// Checks if the tile changed before drawing it
//
void SPI_LCD_DrawRow(int iRow)
{
GAME_BLOB *pBlob = &blobs[MAX_PREV_GAMES];
int iWidth, iPitch;
int x, y, cx, cy, xoff, yoff;
unsigned char *pSrc;
//int iTileIndex;
//uint32_t u32Sum;

  iWidth = pBlob->iWidth;
  iPitch = pBlob->iPitch;
//  iTileIndex = (iRow / 24) * 7;
  
    if (iWidth == 160) // GBC/GG scaled
    {
      yoff = 0; // vertical center
      xoff = 16; // this gets scaled up by 5/3
    }
  y = iRow;
  pSrc = (unsigned char *)pBlob->pBitmap;
  cx = cy = 24;
  for (x=0; x<160+23; x += 24)
  {
    if ((x + cx) > 160)
       cx = 160 - x;
//    u32Sum = GetTileSum(&pSrc[x*2], iPitch);
//    if (u32Regions[iTileIndex] != u32Sum) // tile changed, draw it
//    {
//      u32Regions[iTileIndex] = u32Sum;
// DEBUG
      spilcdDraw53Tile(pLCD, x+xoff,y+yoff, cx, cy, &pSrc[x*2], iPitch, DRAW_TO_LCD);
//    }
//    iTileIndex++;
  }
} /* SPI_LCD_DrawRow() */

void SPI_LCD_Update(void)
{
int x, y, cx, cy, xoff, yoff;
uint32_t u32Flags, *pRegions = NULL;
//uint64_t llTime;
int iCount = 0;
GAME_BLOB *pBlob = &blobs[MAX_PREV_GAMES];
int iWidth, iHeight, iPitch;
unsigned char *pSrc = pAltScreen; //(unsigned char *)pBlob->pBitmap;
//unsigned char *pDest = pAltScreen;

	iWidth = pBlob->iWidth;
	iHeight = pBlob->iHeight;
	iPitch = pBlob->iPitch;
//	pRegions = u32OldRegions;

//	llTime = SG_Clock();
//	if (iScrollX) spilcdScroll(iScrollX*iScrollScale, 0);

	// Center the game on the LCD
	xoff = yoff = 0;
	if (iLCDX == 240 && iLCDY == 240) // TinyPi Pro
	{
		if (iWidth == 160) // GBC/GG scaled
		{
			yoff = (240-216)/2;
			xoff = 0;
		}
		else
		{
			xoff = 0;
			yoff = (240 - pBlob->iHeight)/2;
		}
	}
	if ((iLCDX == 240 && iLCDY == 320) || (iLCDX == 320 && iLCDY == 240)) // PiPlay Portable setup
	{
		if (iWidth == 160) // GBC/GG scaled
		{
			yoff = 0; // vertical center
			xoff = 16; // this gets scaled up by 5/3
		}
		else if (iWidth < 320)
		{
			xoff = (320-iWidth)/2;
			if (iHeight > 240) // scaled down (e.g. PacMan)
				yoff = 0;
			else
				yoff = (240 - iHeight)/2; 
		}
		else if (iHeight < 240) // iWidth >= 320
		{
			yoff = (240 - iHeight)/2;
		}
	}
	if (iWidth == 384 && (iLCDX == 320 || iLCDY == 320)) // IREM & CPS1
	{
		int iVMask = 0xffff;
		int dy = 0;
		int yinc = 16;
		if (iHeight > 240)
		{
			yinc = 15;
			iVMask = 0xfffe;
		}
		for (y=0; y<iHeight; y+=16)
		{
			int iMask, dx = 0;
			const int iMasks[3] = {0xf7df, 0xd77d, 0x7df7};
			iMask = 0;
			for (x=0; x<iWidth; x+= 16)
			{
				spilcdDrawMaskedTile(pLCD, dx+xoff, dy+yoff, &pSrc[(y*iPitch)+x*2], iPitch, iMasks[iMask], iVMask, DRAW_TO_LCD);
				dx += __builtin_popcount(iMasks[iMask]);
				iMask++; if (iMask >= 3) iMask = 0;
			} // for x
			dy += yinc;
		} // for y
		return;
	}
	cx = iTileWidth; cy = iTileHeight;
	// Draw the changed tiles and update the backup display with the new image
	for (y=0; y<iHeight; y+=iTileHeight)
	{
		u32Flags = *pRegions++; // next set of row tile flags
		for (x=0; x<iWidth; x+=iTileWidth)
		{
			if (u32Flags & 1) // this tile is dirty
			{
				iCount++;
				if (iWidth == 160) // GBC/GG
				{
					if (iLCDX == 128) {
						spilcdDrawSmallTile(pLCD, x+xoff, y+yoff, &pSrc[(y*iPitch)+x*2], iPitch, DRAW_TO_LCD);
          } else {
						cx = iTileWidth; cy = iTileHeight;
						if (x+cx > 160) cx = 160 - x;
						if (y+cy > 144) cy = 144 - y;
//						spilcdDrawScaledTile(x+xoff, y+yoff, cx, cy, &pSrc[(y*iPitch)+x*2], iPitch);
						if (iLCDX == iLCDY)
						   spilcdDrawTile150(pLCD, x+xoff, y+yoff, cx, cy, &pSrc[(y*iPitch)+x*2], iPitch, DRAW_TO_LCD);
						else
						   spilcdDraw53Tile(pLCD, x+xoff,y+yoff, cx, cy, &pSrc[(y*iPitch)+x*2], iPitch, DRAW_TO_LCD);
					}
				}
				else
				{
					if (iLCDX == 240 && iHeight > 240) {
						spilcdDrawRetroTile(pLCD, x+xoff, y+yoff, &pSrc[(y*iPitch)+x*2], iPitch, DRAW_TO_LCD);
          } else {
						cx = iTileWidth; cy = iTileHeight;
						if (x+cx > iWidth) cx = iWidth - x;
						if (y+cy > iHeight) cy = iHeight - y;
						if (iLCDX == iLCDY) // TinyPiPro
						{
						unsigned char *p = &pSrc[(y*iPitch)+x*2];
							if (x==0) // show half tiles from the edges
								spilcdDrawTile(pLCD, x, y+yoff, 8, cy, p+16, iPitch, DRAW_TO_LCD);
							else
							{
								if (x == 240) cx = 8;
								spilcdDrawTile(pLCD, x-8, y+yoff, cx, cy, p, iPitch, DRAW_TO_LCD);
							}
							 
						}
						else
						{
						   spilcdDrawTile(pLCD, x+xoff, y+yoff, cx, cy, &pSrc[(y*iPitch)+x*2], iPitch, DRAW_TO_LCD);
						}
					}
				}
//				SG_CopyTile(x, y, cx, cy, pSrc, pDest, iPitch);
			} // dirty tile
			u32Flags >>= 1; // shift down to next bit flag	
		} // for x
	} // for y
//	llTime = SG_Clock() - llTime;
//	x = (iWidth + iTileWidth-1)/iTileWidth;
//	y = (iHeight + iTileHeight-1)/iTileHeight;
//	if (iCount == x*y) printf("SPI time = %ull ns\n", llTime);
} /* SPI_LCD_Update() */

//
// Send the current frame to the display device
//
void DrawFrame(void)
{
int iChanged, iScrollX = 0;
static int iScroll = 0;
GAME_BLOB *pBlob = &blobs[MAX_PREV_GAMES];

        if (pBlob->iWidth == 160) // GBC/GG
        {
		if (iLCDX == 240 && iLCDY == 240) // TinyPi Pro
		{
			iTileWidth = 16; // 1.5x on output
			iTileHeight = 16; // 1.5x on output
		}
		else
		{
                	iTileWidth = 24; // 2x on output
                	iTileHeight = 24; // 1.5x on output
		}
        }
	else if (pBlob->iHeight == 288) // namco
	{
		iTileWidth = iTileHeight = 16;
	}
        else
        {
		if (iLCDX == 240 && iLCDY == 240)
		{
			iTileWidth = iTileHeight = 16;
		}
		else
		{
               		iTileWidth = 64;
                	iTileHeight = 32;
		}
        }

	if (iDisplayType == DISPLAY_LCD)
	{
		iChanged = EMUFindChangedRegion((unsigned char *)pBlob->pBitmap, pAltScreen, pBlob->iWidth, pBlob->iHeight, pBlob->iPitch, iTileWidth, iTileHeight, u32Regions, (bDetectScroll) ? &iScrollX : NULL, NULL);
		if (iChanged) // some area of the image changed
		{
			int i, j, k;
			iScroll += iScrollX;
//                printf("%d tiles changed, iScroll = %d\n", iChanged, iScroll);
//			memcpy(pAltScreen, (char *)pBlob->pBitmap, pBlob->iHeight * pBlob->iPitch);
			// Prepare for second thread to copy changed pixels to LCD
			// copy changed region flags
			// Wait for second thread to finish with last frame
			if (iNumProcessors > 1) // need to lock the mutex to make sure LCD thread is idle
			{
//				pthread_mutex_lock(&lcd_busy_mutex); // wait for worker thread to be idle
			}
//			memcpy(u32OldRegions, u32Regions, 18*sizeof(uint32_t));
			k = 0;
			for (i=0; i<pBlob->iHeight; i+= iTileHeight)
			{
				if (u32Regions[k++])
				{
					j = iTileHeight;
					if (i+j > pBlob->iHeight) j = pBlob->iHeight -i;
					memcpy(&pAltScreen[i*pBlob->iPitch], (void *)&pBlob->pBitmap[i*pBlob->iPitch], j * pBlob->iPitch); // copy regions which changed
				}
			}
			if (iNumProcessors == 1)
			{
				SPI_LCD_Update();
	//			pthread_yield(); // give the system some time
			}
			else // signal the background thread
			{
//				pthread_cond_signal(&lcd_changed);
//				pthread_mutex_unlock(&lcd_busy_mutex);
//				while (bLCDBusy) // need to wait for last thread
//				{
//					SG_Sleep(1000000); // wait 1ms
//				}
//				bLCDChanged = TRUE; // signal thread
			}
		}
	} // LCD
} /* DrawFrame() */

//
// Background thread to send image to SPI LCD
//
void * SG_LCD(void *unused)
{
	bLCDBusy = FALSE;
	while (!bDone) // while program is running
	{
//		if (bLCDChanged) // draw the changes
//		pthread_mutex_lock(&lcd_busy_mutex);
//		pthread_cond_wait(&lcd_changed, &lcd_busy_mutex); // unlock the mutex and wait to be told that there is work to do
		if (bRunning)
		{	// when condition is signalled, the mutex is atomically locked again
//			bLCDChanged = FALSE;
//			bLCDBusy = TRUE;
			SPI_LCD_Update();
//			bLCDBusy = FALSE;
		}
//		pthread_mutex_unlock(&lcd_busy_mutex);
//		else
//		{
//			SG_Sleep(1000000LL); // sleep 1ms
//		}
	} // while !bDone
	return NULL;
} /* SG_LCD() */

void * SG_RunFrames(void *unused)
{
int64_t llTargetTime = 0;
int64_t llFrameDelta;
int64_t llTime, llOldTime;
int iVideoFrames;
float fps;
//SDL_Event event;

	llFrameDelta = 1000000000 / 60; // time slice in nanoseconds
	llOldTime = 0;
	iVideoFrames = 0;
	fps = 0.0;

	while (!bDone)
	{
		if (bRunning)
		{
			int64_t ns;
			if (blobs[MAX_PREV_GAMES].pBitmap == NULL)
			{
				SG_InitSPIGraphics(&blobs[MAX_PREV_GAMES]);
			}

			if (bQuit) // user just quit a game; clean it up
			{
				SG_QuitGame();
				llTargetTime = 0;
				continue;
			}
			if (llTargetTime == 0) llTargetTime = SG_Clock() + llFrameDelta;
			if (iAudioAvailable + blobs[MAX_PREV_GAMES].iSampleCount <= iAudioTotal)
			{
			// combine the controller bits from all sources
			if (iP1Control == CONTROLLER_ICADE)
				u32ControllerBits = u32iCadeBits;
			else if (iP1Control == CONTROLLER_GPIO)
				u32ControllerBits = u32GPIOBits;
			else if (iP1Control == CONTROLLER_KEYBOARD)
				u32ControllerBits = u32KeyBits;
			else if (iP1Control == CONTROLLER_JOY0)
				u32ControllerBits = u32Joy0Bits;
			else if (iP1Control == CONTROLLER_JOY1)
				u32ControllerBits = u32Joy1Bits;
			if (iP2Control == CONTROLLER_ICADE)
				u32ControllerBits |= (u32iCadeBits << PLAYER_SHIFT);
			else if (iP2Control == CONTROLLER_GPIO)
				u32ControllerBits |= (u32GPIOBits << PLAYER_SHIFT);
			else if (iP2Control == CONTROLLER_KEYBOARD)
				u32ControllerBits |= (u32KeyBits << PLAYER_SHIFT);
			else if (iP2Control == CONTROLLER_JOY0)
				u32ControllerBits |= (u32Joy0Bits << PLAYER_SHIFT);
			else if (iP2Control == CONTROLLER_JOY1)
				u32ControllerBits |= (u32Joy1Bits << PLAYER_SHIFT);
			if (((u32ControllerBits & (RKEY_SELECT_P1 | RKEY_START_P1)) ==
				(RKEY_SELECT_P1 | RKEY_START_P1)) ||
				((u32ControllerBits & RKEY_EXIT) == RKEY_EXIT))
			{
				PauseMenu();
				llTargetTime = SG_Clock();
				//bQuit = TRUE; // start+select on joystick = quit
				continue;
			}
			SG_PlayGame(bAudio, TRUE, u32ControllerBits);
			iVideoFrames++; // count how many sent to display
			DrawFrame();
// push this block of audio
				if (!bPerf)
				{
#ifdef FUTURE
				   if (m_id != 0)
				   {
					SG_PushSamples((unsigned char *)blobs[MAX_PREV_GAMES].pAudioBuf, blobs[MAX_PREV_GAMES].iSampleCount);
					if (iAudioAvailable < blobs[MAX_PREV_GAMES].iSampleCount*2)
					{ // need more audio
//						printf("needed more audio\n");
						SG_PlayGame(bAudio, FALSE, u32ControllerBits);
						SG_PushSamples((unsigned char *)blobs[MAX_PREV_GAMES].pAudioBuf, blobs[MAX_PREV_GAMES].iSampleCount);
					}			
				   }
#endif // FUTURE
				llTime = SG_Clock();
				ns = llTargetTime - llTime;
				if (bShowFPS)
				{
//				char ucTemp[32];
//					sprintf(ucTemp,"%02.1f FPS", fps);
//					SG_WriteString(0,(iLCDOrientation == LCD_ORIENTATION_PORTRAIT) ? iLCDY-8:iLCDX-8,ucTemp,0xffff,0xf800,0);
					if ((llTime - llOldTime) > 1000000000LL) // update every second
					{
						fps = (float)iVideoFrames;
						fps = fps * 1000000000.0;
						fps = fps / (float)(llTime-llOldTime);
//						printf("%02.1f FPS\n", fps);
						iVideoFrames = 0;
						llOldTime = llTime;
					}
				}
				if (ns > 4000LL)
				{
					SG_Sleep(ns-4000LL);	// don't trust sleep for the last 4us
				}
				else if (ns < 0) // we need to catch up (aka drop frames)
				{
					while (ns < 0)
					{
						SG_PlayGame(bAudio, FALSE, u32ControllerBits);
						if (bAudio)
						{
							SG_PushSamples((unsigned char *)blobs[MAX_PREV_GAMES].pAudioBuf, blobs[MAX_PREV_GAMES].iSampleCount);
						}
						ns += llFrameDelta;
						llTargetTime += llFrameDelta;
					}
				}
				llTargetTime += llFrameDelta;
				}
			}
			else
			{// we're getting ahead of the framerate, sleep a little
				SG_Sleep(1000LL);
			}
		}
		else // not running
		{
			llTime = SG_Clock();
      llTargetTime = llTime + llFrameDelta;
			SG_Sleep(llFrameDelta * 10LL);
		}
	} // while !bDone
//	SG_StopAudio();
	return NULL;
} /* game_thread() */
