/* XDC module Headers */
#include <xdc/std.h>
#include <xdc/runtime/System.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "FIFO.h"

/**
  * @brief  Initial FIFO buffer.
  * @param size : the size of FIFO buffer.
  * @param buffer : the pointer to user provided memory.
  * @param the poiter of FIFO buffer structure
  * @return true when initial FIFO buffer success; false when initial FIFO buffer fail
  */
bool InitialFIFO(int size, char* buffer, FIFO_Buf* fifobuffer)
{
  if(fifobuffer)
  {
      if(buffer)
      {
          fifobuffer->size = size;
          fifobuffer->readIndex = 0;
          fifobuffer->writeIndex = 0;
          fifobuffer->buffer = buffer;
          return true;
      }
      else
      {
          return false;
      }
  }
  else
  {
      return false;
  }
}

/**
  * @brief  To check FIFO buffer is empty or not.
  * @param fifo : the poiter of FIFO buffer structure.
  * @return true when FIFO buffer is empty; false when FIFO buffer is not empty
  */
bool FIFOIsEmpty(FIFO_Buf* fifo)
{
  if(fifo->readIndex == fifo->writeIndex)
  {
   // System_printf("FIFO is empty\r\n");
    return true;
  }
  else
    return false;
}

/**
  * @brief  To check FIFO buffer is full or not.
  * @param fifo : the poiter of FIFO buffer structure.
  * @return true when FIFO buffer is full; false when FIFO buffer is not full
  */
bool FIFOIsFull(FIFO_Buf* fifo)
{
  if((fifo->writeIndex == (fifo->size - 1) && fifo->readIndex == 0) || (fifo->writeIndex == (fifo->readIndex - 1)))
  {
#ifdef DEBUG
    System_printf("FIFO is full\n");
#endif
    return true;
  }
  else
    return false;
}

/**
  * @brief  return the number of items which is filled with FIFO buffer.
  * @param fifo : the poiter of FIFO buffer structure.
  * @return the number of items which is filled with FIFO buffer
  */
int FIFOFilledNumber(FIFO_Buf* fifo)
{
  if(FIFOIsEmpty(fifo))
    return 0;
  else if(FIFOIsFull(fifo))
    return fifo->size;
  else if(fifo->writeIndex < fifo->readIndex)
    return (fifo->writeIndex) + (fifo->size - fifo->readIndex);
  else
    return fifo->writeIndex - fifo->readIndex; 
}

/**
  * @brief  Put one byte data into FIFO buffer
  * @param fifo : the poiter of FIFO buffer structure.
  * @param byte : the one byte data which want to be filled with FIFO buffer
  * @return true when successful; false when fail
  */
bool FIFOPutByte(FIFO_Buf* fifo, char byte)
{
  if(fifo == NULL)
  {
#ifdef DEBUG
    System_printf("FIFO is NULL\n");
#endif
    return false;
  }
  if(FIFOIsFull(fifo) == true)
    return false;
  
  fifo->buffer[fifo->writeIndex] = byte;
  fifo->writeIndex++;
  if(fifo->writeIndex == fifo->size)
    fifo->writeIndex = 0;
  return true;
}

/**
  * @brief  Get one byte data from FIFO buffer
  * @param fifo : the poiter of FIFO buffer structure.
  * @param byte : the pointer of byte value which store one byte data from FIFO buffer
  * @return true when successful; false when fail
  */
bool FIFOGetByte(FIFO_Buf* fifo, char* byte)
{
  if(fifo == NULL)
  {
#ifdef DEBUG
    System_printf("FIFO is NULL\n");
#endif
    return false;
  }
  if(FIFOIsEmpty(fifo) == true)
    return false;

  *byte = fifo->buffer[fifo->readIndex];
  fifo->readIndex++;
  if(fifo->readIndex == fifo->size)
    fifo->readIndex = 0;
  return true;
}

/**
  * @brief  Put bytes data into FIFO buffer
  * @param fifo : the poiter of FIFO buffer structure.
  * @param bytes : the pointer of bytes buffer which want to be filled with FIFO buffer
  * @param count : the size of bytes data buffer
  * @return -1 when fail; the real number of byte data are filled with FIFO buffer when successful
  */
int FIFOPutBytes(FIFO_Buf* fifo, char* bytes, int count)
{
  if(fifo == NULL)
  {
#ifdef DEBUG
    System_printf("FIFO is NULL\n");
#endif
    return -1;
  }
  if(bytes == NULL)
  {
#ifdef DEBUG
    System_printf("Input buffer is NULL\n");
#endif
    return -1;
  }
  
  for(int i = 0; i < count; i++)
  {
    if(FIFOPutByte(fifo, bytes[i]) == false)
      return i;
  }

  return count;
}

/**
  * @brief  Get bytes data from FIFO buffer
  * @param fifo : the poiter of FIFO buffer structure.
  * @param bytes : the pointer of bytes data buffer which store bytes data from FIFO buffer
  * @param count : the size of bytes data buffer
  * @return -1 when fail; the real number of byte data are gotten from FIFO buffer when successful
  */
int FIFOGetBytes(FIFO_Buf* fifo, char* bytes, int count)
{
  if(fifo == NULL)
  {
#ifdef DEBUG
    System_printf("FIFO is NULL\n");
#endif
    return -1;
  }
  if(bytes == NULL)
  {
#ifdef DEBUG
    System_printf("Output buffer is NULL\n");
#endif
    return -1;
  }

  for(int i = 0; i < count; i++)
  {
    if(FIFOGetByte(fifo, bytes + i) == false)
      return i;
  }

  return count;
}
