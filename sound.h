void EMUOpenSound(int, int, int);
void EMUDoSound(void);
void EMUCloseSound(void);
void EMUPlaySamples(SNDSAMPLE *, signed short *pAudioBuf, int iSampleCount, int iVoiceShift, int bMerge);
void EMUStartSample(SNDSAMPLE *, int); /* Activate this sample */
void EMUStopSample(SNDSAMPLE *, int); /* De-activate this sample */
#define MAX_AUDIO_BLOCKS 8
#define AUDIO_BLOCK_SIZE 800

void EMUSetVolume(int iLevel);
extern int iSoundBlock, iSoundHead, iSoundTail;
extern signed int iSoundBlocks;
extern int iNumSamples;
extern int iVoiceShift;
extern unsigned char *pSoundBuf;
