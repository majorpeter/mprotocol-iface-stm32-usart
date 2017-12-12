/*
 * UsartSerialInterface.h
 *
 *  Created on: 2017. okt. 24.
 *      Author: peti
 */

#ifndef MPROTOCOL_IFACE_STM32_USART_USARTSERIALINTERFACE_H_
#define MPROTOCOL_IFACE_STM32_USART_USARTSERIALINTERFACE_H_

#include <mprotocol-server/AbstractSerialInterface.h>

class UsartSerialInterface: public AbstractSerialInterface {
public:
    UsartSerialInterface(uint32_t baudRate = 115200);
    virtual ~UsartSerialInterface() {}

    virtual void handler();
    virtual bool writeBytes(const uint8_t* bytes, uint16_t length);

    void receiveByteFromIrq(uint8_t byte);
private:
    static const uint16_t bufferSize = 512;

    uint8_t buffer[bufferSize];
    volatile uint16_t readIndex;
    volatile uint16_t writeIndex;
};

#endif /* MPROTOCOL_IFACE_STM32_USART_USARTSERIALINTERFACE_H_ */
