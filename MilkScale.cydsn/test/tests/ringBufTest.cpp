#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"
#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string>
#include <cstring>
//#include <iostream>

using namespace std;

extern "C"
{
#include "ringbuf.h"
}

static T_RingBufferCB rbcb;
static uint8_t buf[10];

TEST_GROUP(ringBufTests)
{
   void fillBuffer() 
   {
      uint8_t i;

      RingBuffer_Init(&rbcb, &buf[0], sizeof(buf));

      for (i=0; i<sizeof(buf); i++) 
      {
         RingBuffer_Write(&rbcb, i);
      }
   }

   void setup()
   {
   }

   void teardown()
   {
   }
};

TEST(ringBufTests, compile)
{
}

TEST(ringBufTests, ringBufInitCompile)
{
   RingBuffer_Init(&rbcb, &buf[0], sizeof(buf));
}

TEST(ringBufTests, checkpBufNullReturnsZero)
{
   BYTES_EQUAL(RING_BUFFER_INIT_FAILURE, RingBuffer_Init(&rbcb, NULL, sizeof(buf)));
}

TEST(ringBufTests, checkSizeGreaterThanZero)
{
   BYTES_EQUAL(RING_BUFFER_INIT_FAILURE, RingBuffer_Init(&rbcb, &buf[0], 0));
}

TEST(ringBufTests, checkNullControlBlockReturnsZero)
{
   BYTES_EQUAL(RING_BUFFER_INIT_FAILURE, RingBuffer_Init(NULL, &buf[0], sizeof(buf)));
}

TEST(ringBufTests, checkControlBlockInitializedCorrectly)
{
   memset((uint8_t*)&rbcb, 42, sizeof(rbcb));

   RingBuffer_Init(&rbcb, &buf[0], sizeof(buf));
   BYTES_EQUAL(0, rbcb.head);
   BYTES_EQUAL(0, rbcb.tail);
   BYTES_EQUAL(RING_BUFFER_NOT_FULL, rbcb.full);
   BYTES_EQUAL(RING_BUFFER_IS_EMPTY, rbcb.empty);
   BYTES_EQUAL(sizeof(buf), rbcb.size);
   POINTERS_EQUAL(&buf[0], rbcb.pBuf);
   BYTES_EQUAL(0, rbcb.bytesUsed);
}

TEST(ringBufTests, compileRingBufferWrite)
{
   RingBuffer_Write(&rbcb, 42);
}

TEST(ringBufTests, ringBufferCheckNullControlBlockPointer)
{
   BYTES_EQUAL(RING_BUFFER_ADD_FAILURE, RingBuffer_Write(NULL, 42));
}

TEST(ringBufTests, checkByteAddedToBuffer)
{
   uint8_t i;

   memset(&buf[0], 42, sizeof(buf));
   RingBuffer_Init(&rbcb, &buf[0], sizeof(buf));

   for (i=0; i<sizeof(buf); i++) {
      RingBuffer_Write(&rbcb, i);
      BYTES_EQUAL(i, buf[i]);
      BYTES_EQUAL(i+1, rbcb.bytesUsed);
   }

   BYTES_EQUAL(RING_BUFFER_NOT_EMPTY, rbcb.empty);
   BYTES_EQUAL(0, rbcb.tail);
}

TEST(ringBufTests, checkBufferFull)
{
   fillBuffer();
   BYTES_EQUAL(RING_BUFFER_IS_FULL, rbcb.full);
}

TEST(ringBufTests, checkThatAddToFullBufferFails)
{
   fillBuffer();
   BYTES_EQUAL(RING_BUFFER_ADD_FAILURE, RingBuffer_Write(&rbcb, 42));
}

TEST(ringBufTests, compileRead)
{
   RingBuffer_Read(&rbcb);
}

TEST (ringBufTests, checkNullPointer)
{
   BYTES_EQUAL(0xff, RingBuffer_Read(NULL));
}

TEST (ringBufTests, readFromEmptyBufferReturnsFF)
{
   RingBuffer_Init(&rbcb, &buf[0], sizeof(buf));

   BYTES_EQUAL(0xff, RingBuffer_Read(&rbcb));
}

TEST (ringBufTests, checkThatReadReturnsValue)
{
   RingBuffer_Init(&rbcb, &buf[0], sizeof(buf));
   RingBuffer_Write(&rbcb, 42);
   BYTES_EQUAL(42, RingBuffer_Read(&rbcb));
}

TEST (ringBufTests, checkThatReadDecrementsBytesUsed)
{
   fillBuffer();
   RingBuffer_Read(&rbcb);
   BYTES_EQUAL(9, rbcb.bytesUsed);
}

TEST (ringBufTests, checkReadOfFullBufferContents)
{
   uint8_t i;

   fillBuffer();

   for(i=0; i<sizeof(buf); i++) {
      BYTES_EQUAL(i, RingBuffer_Read(&rbcb));
   }
}

TEST (ringBufTests, checkReadFromFullBufferResultsInNotFull)
{
   fillBuffer();

   RingBuffer_Read(&rbcb);
   BYTES_EQUAL(RING_BUFFER_NOT_FULL, rbcb.full);
}

TEST(ringBufTests, afterReadingFullBufferHeadPointsToZero)
{
   uint8_t i;

   fillBuffer();

   for(i=0; i<sizeof(buf); i++) 
   {
      RingBuffer_Read(&rbcb);
   }

   BYTES_EQUAL(0, rbcb.head);
}

TEST(ringBufTests, afterReadingLastByteBufferIsEmpty)
{
   RingBuffer_Init(&rbcb, &buf[0], sizeof(buf));
   RingBuffer_Write(&rbcb, 42);
   RingBuffer_Read(&rbcb);
   BYTES_EQUAL(RING_BUFFER_IS_EMPTY, rbcb.empty);
}

TEST(ringBufTests, compileRingBufferIsEmpty)
{
   RingBuffer_IsEmpty(&rbcb);
}

TEST(ringBufTests, isEmptyReturnsBadPointer)
{
   BYTES_EQUAL(RING_BUFFER_BAD_CONTROL_BLOCK_POINTER, RingBuffer_IsEmpty(NULL));
}

TEST(ringBufTests, emptyRingBufferReturnsEmpty)
{
   RingBuffer_Init(&rbcb, &buf[0], sizeof(buf));

   BYTES_EQUAL(RING_BUFFER_IS_EMPTY, RingBuffer_IsEmpty(&rbcb));
}

TEST(ringBufTests, notEmptyRingBufferReturnsNotEmpty)
{
   RingBuffer_Init(&rbcb, &buf[0], sizeof(buf));
   RingBuffer_Write(&rbcb, 42);

   BYTES_EQUAL(RING_BUFFER_NOT_EMPTY, RingBuffer_IsEmpty(&rbcb));
}

TEST(ringBufTests, compileRingBufferFull)
{
   RingBuffer_IsFull(&rbcb);
}

TEST(ringBufTests, isFullReturnsBadPointer)
{
   BYTES_EQUAL(RING_BUFFER_BAD_CONTROL_BLOCK_POINTER, RingBuffer_IsFull(NULL));
}

TEST(ringBufTests, isFullReturnsFullWhenFull) {
   fillBuffer();
   BYTES_EQUAL(RING_BUFFER_IS_FULL, RingBuffer_IsFull(&rbcb));
}

TEST(ringBufTests, isFullReturnsNotFullWhenNotFull) {
   RingBuffer_Init(&rbcb, &buf[0], sizeof(buf));
   BYTES_EQUAL(RING_BUFFER_NOT_FULL, RingBuffer_IsFull(&rbcb));
}

TEST(ringBufTests, compilePeek)
{
   RingBuffer_Peek(&rbcb, 0);
}

TEST(ringBufTests, peekNullPointerCheck)
{
   BYTES_EQUAL(0xff, RingBuffer_Peek(NULL, 0));
}

TEST(ringBufTests, peekBadPositionReturnsFail)
{
   BYTES_EQUAL(0xff, RingBuffer_Peek(&rbcb, sizeof(buf) + 1));
}

TEST(ringBufTests, peekFromEmptyBufReturnsFail)
{
   RingBuffer_Init(&rbcb, &buf[0], sizeof(buf));
   BYTES_EQUAL(0xff, RingBuffer_Peek(&rbcb, 1));
}

TEST(ringBufTests, peekAttemptToReadBeyondAvailableFails)
{
   RingBuffer_Init(&rbcb, &buf[0], sizeof(buf));
   RingBuffer_Write(&rbcb, 42);
   BYTES_EQUAL(0xff, RingBuffer_Peek(&rbcb, 1));
}

TEST(ringBufTests, peekCheckPeekNormal)
{
   uint8_t i;

   fillBuffer();

   for(i=0; i<sizeof(buf); i++) {
      BYTES_EQUAL(i, RingBuffer_Peek(&rbcb, i));
   }
}

TEST(ringBufTests, peekCheckWrapCase)
{
   uint8_t i;
   uint8_t retVal;

   fillBuffer();

   for (i=0; i<4; i++) {
      RingBuffer_Read(&rbcb);
   }

   i = 10;
   do {
      retVal = RingBuffer_Write(&rbcb, i++);
   } while(retVal != RING_BUFFER_ADD_FAILURE);

   BYTES_EQUAL(13, RingBuffer_Peek(&rbcb, 8));
}


TEST(ringBufTests, compileBytesUsed)
{
   RingBuffer_BytesUsed(&rbcb);
}

TEST(ringBufTests, bytesUsedCheckNullPointer)
{
   BYTES_EQUAL(0xff, RingBuffer_BytesUsed(NULL));
}

TEST(ringBufTests, bytesUsedReturns0WhenBufEmpty)
{
   RingBuffer_Init(&rbcb, &buf[0], sizeof(buf));
   BYTES_EQUAL(0, RingBuffer_BytesUsed(&rbcb));
}

TEST(ringBufTests, bytesUsedReturnsSizeOfWhenFull)
{
   fillBuffer();

   BYTES_EQUAL(sizeof(buf), RingBuffer_BytesUsed(&rbcb));
}

TEST(ringBufTests, compileBytesAvailable)
{
   RingBuffer_BytesAvailable(&rbcb);
}

TEST(ringBufTests, bytesAvailableNullPointerCheck)
{
   BYTES_EQUAL(0xff, RingBuffer_BytesAvailable(NULL));
}

TEST(ringBufTests, bytesAvailableReturnsBufSizeWhenEmpty)
{
   RingBuffer_Init(&rbcb, &buf[0], sizeof(buf));
   BYTES_EQUAL(sizeof(buf), RingBuffer_BytesAvailable(&rbcb));
}

TEST(ringBufTests, bytesAvailableReturns0WhenFull)
{
   fillBuffer();
   BYTES_EQUAL(0, RingBuffer_BytesAvailable(&rbcb));
}


