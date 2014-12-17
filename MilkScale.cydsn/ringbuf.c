/*
 * Yet another ring buffer implementation for bytes.
 *
 * Author: Rob Bultman
 * email : rob@firstbuild.com
 *
 * Copyright (c) 2014 FirstBuild
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "ringbuf.h"
#include <stdlib.h>

uint8_t RingBuffer_Init(T_RingBufferCB *pControlBlock, uint8_t *pBuf, uint8_t size) {
   if (pBuf == NULL) {
      return RING_BUFFER_INIT_FAILURE;
   }
   if (size == 0) {
      return RING_BUFFER_INIT_FAILURE;
   }
   if (pControlBlock == NULL) {
      return RING_BUFFER_INIT_FAILURE;
   }

   pControlBlock->head = 0;
   pControlBlock->tail = 0;
   pControlBlock->size = size;
   pControlBlock->pBuf = pBuf;
   pControlBlock->full = RING_BUFFER_NOT_FULL;
   pControlBlock->empty = RING_BUFFER_IS_EMPTY;

   return RING_BUFFER_INIT_SUCCESS;
}

uint8_t RingBuffer_Write(T_RingBufferCB *pControlBlock, uint8_t val) {
   if (pControlBlock == NULL) {
      return RING_BUFFER_ADD_FAILURE;
   }
   if (pControlBlock->full == RING_BUFFER_IS_FULL) {
      return RING_BUFFER_ADD_FAILURE;
   }

   pControlBlock->pBuf[pControlBlock->tail++] = val;
   pControlBlock->empty = RING_BUFFER_NOT_EMPTY;
   if (pControlBlock->tail >= pControlBlock->size) {
      pControlBlock->tail = 0;
   }
   if (pControlBlock->head == pControlBlock->tail) {
      pControlBlock->full = RING_BUFFER_IS_FULL;
   }

   return RING_BUFFER_ADD_SUCCESS;
}

uint8_t RingBuffer_Read(T_RingBufferCB *pControlBlock) {
   uint8_t retVal = 0xff;

   if (pControlBlock == NULL) {
      return retVal;
   }
   if (pControlBlock->empty == RING_BUFFER_IS_EMPTY) {
      return retVal;
   }

   retVal = pControlBlock->pBuf[pControlBlock->head++];
   pControlBlock->full = RING_BUFFER_NOT_FULL;
   if (pControlBlock->head >= pControlBlock->size) {
      pControlBlock->head = 0;
   }
   if (pControlBlock->head == pControlBlock->tail) {
      pControlBlock->empty = RING_BUFFER_IS_EMPTY;
   }

   return retVal;
}

uint8_t RingBuffer_IsEmpty(T_RingBufferCB *pControlBlock) {
   if (pControlBlock == NULL) {
      return RING_BUFFER_BAD_CONTROL_BLOCK_POINTER;
   }

   return pControlBlock->empty;
}

uint8_t RingBuffer_IsFull(T_RingBufferCB *pControlBlock) {
   if (pControlBlock == NULL) {
      return RING_BUFFER_BAD_CONTROL_BLOCK_POINTER;
   }

   return pControlBlock->full;
}


