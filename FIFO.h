#ifndef _FIFO_H_
#define _FIFO_H_

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct FIFO{
  int size;
  int readIndex;
  int writeIndex;
  char* buffer;
}FIFO_Buf;

extern bool InitialFIFO(int size, char* buffer, FIFO_Buf* fifobuffer);
extern bool FIFOIsEmpty(FIFO_Buf* fifo);
extern bool FIFOIsFull(FIFO_Buf* fifo);
extern int FIFOFilledNumber(FIFO_Buf* fifo);
extern bool FIFOPutByte(FIFO_Buf* fifo, char byte);
extern bool FIFOGetByte(FIFO_Buf* fifo, char* byte);
extern int FIFOPutBytes(FIFO_Buf* fifo, char* bytes, int count);
extern int FIFOGetBytes(FIFO_Buf* fifo, char* bytes, int count);

#ifdef __cplusplus
}
#endif

#endif // !_FIFO_H_


