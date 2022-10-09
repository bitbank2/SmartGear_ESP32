 /***************************************************************************
 *                                                                          *
 * MODULE:  EMUIO.C                                                         *
 *                                                                          *
 * DESCRIPTION: Windows CE Generic I/O & Memory                             *
 *                                                                          *
 * FUNCTIONS:                                                               *
 *            EMUOpen - Open a file for reading or writing                  *
 *            EMUCreate - Create a file for writing                         *
 *            EMUDelete - Delete a file                                     *
 *            EMUClose - Close a file                                       *
 *            EMUDelete - Delete a file                                     *
 *            EMURead - Read a block of data from a file                    *
 *            EMUWrite - write a block of data to a file                    *
 *            EMUSeek - Seek to a specific section in a file                *
 *            EMUAlloc - Allocate a block of memory                         *
 *            EMUFree - Free a block of memory                              *
 * COMMENTS:                                                                *
 *            Created 10/14/99 - Larry Bank                                 *
 *            Copyright (c) 2000 BitBank Software, Inc.                     *
 *            Updated for Android NDK 11/10/10                              *
 ****************************************************************************/
//#include <stdint.h>
//#include <windows.h>
#include <SD.h>
#include <SPI.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define LOG_MEM
#ifdef LOG_MEM
//static int iMemTotal = 0;
#endif // LOG_MEM
//int rand(void)
//{
//	return (int)lrand48();
//} /* rand() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUOpenRO(char *)                                          *
 *                                                                          *
 *  PURPOSE    : Opens a file for reading only.                             *
 *                                                                          *
 *  PARAMETERS : filename                                                   *
 *                                                                          *
 *  RETURNS    : Handle to file if successful, -1 if failure                *
 *                                                                          *
 ****************************************************************************/
File EMUOpenRO(char * fname)
{
   File ihandle;

   ihandle = SD.open(fname); //, FILE_READ);
   return ihandle;

} /* EMUOpenRO() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUOpen(char *)                                            *
 *                                                                          *
 *  PURPOSE    : Opens a file for reading or writing                        *
 *                                                                          *
 *  PARAMETERS : filename                                                   *
 *                                                                          *
 *  RETURNS    : Handle to file if successful, -1 if failure                *
 *                                                                          *
 ****************************************************************************/
File EMUOpen(char * fname)
{
   File ihandle;

//   ihandle = fopen(fname, "r+b");
   ihandle = SD.open(fname); //, FILE_WRITE | FILE_READ);

   if (ihandle == 0)
      ihandle = SD.open(fname, FILE_READ); /* Try readonly */
   return ihandle;

} /* EMUOpen() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUCreate(char *)                                          *
 *                                                                          *
 *  PURPOSE    : Creates and opens a file for writing                       *
 *                                                                          *
 *  PARAMETERS : filename                                                   *
 *                                                                          *
 *  RETURNS    : Handle to file if successful, -1 if failure                *
 *                                                                          *
 ****************************************************************************/
File EMUCreate(char * fname)
{
File ohandle;

   ohandle = SD.open(fname, FILE_WRITE);
   return ohandle;

} /* EMUCreate() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUSeek(void *, signed long, int)                          *
 *                                                                          *
 *  PURPOSE    : Seeks within an open file                                  *
 *                                                                          *
 *  PARAMETERS : File Handle                                                *
 *               Offset                                                     *
 *               Method - 0=from beginning, 1=from current spot, 2=from end *
 *                                                                          *
 *  RETURNS    : New offset within file.                                    *
 *                                                                          *
 ****************************************************************************/
uint32_t EMUSeek(File iHandle, signed long lOffset, int iMethod)
{
//   int iType;
   uint32_t ulNewPos;

//   if (iMethod == 0) iType = SEEK_SET;
//   else if (iMethod == 1) iType = SEEK_CUR;
//   else iType = SEEK_END;

   if (iMethod == 0) // from beginning
     iHandle.seek((uint32_t)lOffset);
   else if (iMethod == 1) // from current pos
   {
     ulNewPos = iHandle.position();
     ulNewPos += lOffset;
     iHandle.seek(ulNewPos);
   }
   else // from end
   {
      ulNewPos = iHandle.size();
      ulNewPos += lOffset;
      iHandle.seek(ulNewPos); 
   }
   ulNewPos = iHandle.position();

   return ulNewPos;

} /* EMUSeek() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUDelete(char *)                                          *
 *                                                                          *
 *  PURPOSE    : Delete a file.                                             *
 *                                                                          *
 *  PARAMETERS : filename                                                   *
 *                                                                          *
 *  RETURNS    : 0 if successful, -1 if failure.                            *
 *                                                                          *
 ****************************************************************************/
int EMUDelete(char *szFile)
{
   if (SD.remove(szFile))
      return 0; // success
   else
      return -1;
} /* EMUDelete() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMURead(void *, void *, int)                               *
 *                                                                          *
 *  PURPOSE    : Read a block from an open file                             *
 *                                                                          *
 *  PARAMETERS : File Handle                                                *
 *               Buffer pointer                                             *
 *               Number of bytes to read                                    *
 *                                                                          *
 *  RETURNS    : Number of bytes read                                       *
 *                                                                          *
 ****************************************************************************/
unsigned int EMURead(File iHandle, void * lpBuff, unsigned int iNumBytes)
{
   unsigned int iBytes;

//   iBytes = (int)fread(lpBuff, 1, iNumBytes, iHandle);
   iBytes = iHandle.read((uint8_t *)lpBuff, iNumBytes);
   return iBytes;

} /* EMURead() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUWrite(void *, void *, int)                              *
 *                                                                          *
 *  PURPOSE    : Write a block from an open file                            *
 *                                                                          *
 *  PARAMETERS : File Handle                                                *
 *               Buffer pointer                                             *
 *               Number of bytes to write                                   *
 *                                                                          *
 *  RETURNS    : Number of bytes written                                    *
 *                                                                          *
 ****************************************************************************/
unsigned int EMUWrite(File iHandle, void * lpBuff, unsigned int iNumBytes)
{
   unsigned int iBytes;

//   iBytes = (unsigned int)fwrite(lpBuff, 1, iNumBytes, iHandle);
   iBytes = (unsigned int)iHandle.write((const uint8_t *)lpBuff, (size_t)iNumBytes);
   return iBytes;

} /* EMUWrite() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUClose(void *)                                           *
 *                                                                          *
 *  PURPOSE    : Close a file                                               *
 *                                                                          *
 *  PARAMETERS : File Handle                                                *
 *                                                                          *
 *  RETURNS    : NOTHING                                                    *
 *                                                                          *
 ****************************************************************************/
void EMUClose(File iHandle)
{
   if (iHandle == 0)
	   return; // system crashes if you try to close these handles
     iHandle.flush();
     iHandle.close();

} /* EMUClose() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUAlloc(long)                                             *
 *                                                                          *
 *  PURPOSE    : Allocate a 16-byte aligned block of writable memory.       *
 *                                                                          *
 ****************************************************************************/
void * EMUAlloc(int size)
{
//void *p = NULL;
//unsigned long i;

   if (size == 0)
   {
      return NULL; // Linux seems to return a non-NULL pointer for 0 size
   }
   return malloc(size);

#ifdef FUTURE
   i = (unsigned long)malloc(size+4);
   if (i)
   {
   uint32_t *pul = (uint32_t *)i;
      p = (void *)(i+4);
      *pul = (uint32_t) size;
      memset(p, 0, size);
#ifdef LOG_MEM
     {
      char pszTemp[256];      
      iMemTotal += (int)size;
      sprintf(pszTemp, "alloc %08x bytes at %08lx, new total = %08x\n", (int)size, (long)p, iMemTotal);
      Serial.println(pszTemp);
     }
#endif // LOG_MEM
   }
   return p;
#endif // future
} /* EMUAlloc() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUFree(void *)                                            *
 *                                                                          *
 *  PURPOSE    : Free a block of writable memory.                           *
 *                                                                          *
 ****************************************************************************/
void EMUFree(void *p)
{
   free(p);
#ifdef FUTURE
uint32_t *pul = (uint32_t *)p;

   if (p == NULL || p == (void *)-1)
       return; /* Don't try to free bogus pointer */
#ifdef LOG_MEM
     {
      char pszTemp[256];
        iMemTotal -= pul[-1];
	    sprintf(pszTemp, "free %08x bytes at %08lx, new total = %08x\n", (int)pul[-1], (long)pul, iMemTotal);
      Serial.println(pszTemp);
     }
#endif // LOG_MEM
   free((void *)&pul[-1]);
#endif // FUTURE
} /* EMUFree() */
