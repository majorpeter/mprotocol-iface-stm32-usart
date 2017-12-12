/*
 * UsartSerialInterface.cpp
 *
 *  Created on: 2017. okt. 24.
 *      Author: peti
 */

#include "UsartSerialInterface.h"
#include <mprotocol-server/AbstractUpLayer.h>
#include <stm32f10x.h>
#include <stddef.h>

static UsartSerialInterface* usart1Iface = NULL;

UsartSerialInterface::UsartSerialInterface(uint32_t baudRate) {
    readIndex = writeIndex = 0;

    GPIO_InitTypeDef GPIOA_InitStruct;
    GPIOA_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIOA_InitStruct.GPIO_Pin = GPIO_Pin_9;
    GPIOA_InitStruct.GPIO_Speed = GPIO_Speed_10MHz;
    GPIO_Init(GPIOA, &GPIOA_InitStruct);
    GPIOA_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIOA_InitStruct.GPIO_Pin = GPIO_Pin_10;
    GPIO_Init(GPIOA, &GPIOA_InitStruct);

    USART_InitTypeDef USART1_InitStruct;
    USART1_InitStruct.USART_BaudRate = baudRate;
    USART1_InitStruct.USART_WordLength = USART_WordLength_8b;
    USART1_InitStruct.USART_StopBits = USART_StopBits_1;
    USART1_InitStruct.USART_Parity = USART_Parity_No;
    USART1_InitStruct.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART1_InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_Init(USART1, &USART1_InitStruct);

    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);

    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    usart1Iface = this;

    USART_Cmd(USART1, ENABLE);
}

/**
 * forwards received bytes to upper layer (if any)
 * @note thread safe if called from a single thread, allows long evaluation of received data without blocking the IRQ
 */
void UsartSerialInterface::handler() {
    if (uplayer == NULL) {
        return; // no receiver connected
    }

    // ensure a single access to the volatile pointer
    uint16_t bufferedWriteIndex = writeIndex;
    if (bufferedWriteIndex == readIndex) {
        return; // buffer is empty
    }

    if (bufferedWriteIndex < readIndex) {
        uplayer->receiveBytes(&buffer[readIndex], bufferSize - readIndex);
        readIndex = 0;
    }

    if (readIndex < bufferedWriteIndex) {
        uplayer->receiveBytes(&buffer[readIndex], bufferedWriteIndex - readIndex);
        readIndex = bufferedWriteIndex;
    }
}

bool UsartSerialInterface::writeBytes(const uint8_t* bytes, uint16_t length) {
    while (length > 0) {
        while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
        USART_SendData(USART1, bytes[0]);

        bytes++;
        length--;
    }
    return true;
}

/**
 * puts one byte into receive buffer
 * @note thread safe if called from a single thread
 * @param byte byte to append to buffer
 */
void UsartSerialInterface::receiveByteFromIrq(uint8_t byte) {
    uint16_t nextWriteIndex = writeIndex + 1;
    if (nextWriteIndex == bufferSize) {
        nextWriteIndex = 0;
    }

    if (nextWriteIndex == readIndex) {
        return; // buffer full, byte is lost
    }

    buffer[writeIndex] = byte;
    writeIndex = nextWriteIndex;
}

extern "C" void USART1_IRQHandler(void) {
    if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) {
        USART_ClearITPendingBit(USART1, USART_IT_RXNE);

        uint16_t c = USART_ReceiveData(USART1);
        if (usart1Iface != NULL) {
            usart1Iface->receiveByteFromIrq(c);
        }
    } else {
        /**
         * This read on DR is required if the IRQ is asserted without RXNE pending.
         * The interrupt may be asserted because of an overrun error (ORE), and its flag can be cleared by this read operation.
         * @see STMicroelectronics RM0008 Reference manual, page 820
         */
        USART_ReceiveData(USART1);
    }
}
