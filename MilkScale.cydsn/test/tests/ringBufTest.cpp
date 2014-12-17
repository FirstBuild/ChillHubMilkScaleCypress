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
