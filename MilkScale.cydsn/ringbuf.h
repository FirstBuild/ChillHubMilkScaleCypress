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

#include <stdint.h>

typedef struct T_RingBufferCB {
   uint8_t head;
   uint8_t tail;
   uint8_t size;
   uint8_t bytesUsed;
   uint8_t empty;
   uint8_t full;
   uint8_t *pBuf;
} T_RingBufferCB; 

#define RING_BUFFER_INIT_FAILURE 0
#define RING_BUFFER_INIT_SUCCESS 1
#define RING_BUFFER_IS_FULL 1
#define RING_BUFFER_NOT_FULL 0
#define RING_BUFFER_IS_EMPTY 1
#define RING_BUFFER_NOT_EMPTY 0
#define RING_BUFFER_ADD_FAILURE 0
#define RING_BUFFER_ADD_SUCCESS 1
#define RING_BUFFER_BAD_CONTROL_BLOCK_POINTER 2

uint8_t RingBuffer_Init(T_RingBufferCB *pControlBlock, uint8_t *pBuf, uint8_t size);
uint8_t RingBuffer_Write(T_RingBufferCB *pControlBlock, uint8_t val);
uint8_t RingBuffer_Read(T_RingBufferCB *pControlBlock);
uint8_t RingBuffer_IsEmpty(T_RingBufferCB *pControlBlock);
uint8_t RingBuffer_IsFull(T_RingBufferCB *pControlBlock);
uint8_t RingBuffer_Peek(T_RingBufferCB *pControlBlock, uint8_t pos);
uint8_t RingBuffer_BytesUsed(T_RingBufferCB *pControlBlock);
uint8_t RingBuffer_BytesAvailable(T_RingBufferCB *pControlBlock);


