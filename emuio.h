/************************************************************/
/*--- Generic I/O and memory routines for Windows CE     ---*/
/* Copyright 1999, BitBank Software, Inc.                   */
/* 11/10/10 - updated for Android NDK                       */
/************************************************************/
File EMUOpen(char *);
File EMUOpenRO(char *);
File EMUCreate(char *);
uint32_t EMUSeek(File, signed long, int);
unsigned int EMURead(File, void *, unsigned int);
unsigned int EMUWrite(File, void *, unsigned int);
void EMUClose(File);
int EMUDelete(char *szFile);
void * EMUAlloc(int);
void EMUFree(void *);
