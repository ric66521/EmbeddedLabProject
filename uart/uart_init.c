/**
 * @file uart_init.c
 * @author David Ng 550084
 * @brief This component is responsible for initialising the UART interface for communication and providing an interface between stdio and uart. 
 * @date 2020-02-20
 * @copyright Copyright (c) 2020
 * 
 */

#define F_CPU 12000000UL
#define BAUD 9600
#define MYUBRR F_CPU/16/BAUD-1


#include <avr/io.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <util/delay.h>
#include <util/setbaud.h>
#include <util/atomic.h>
#include <avr/interrupt.h>
#include "../layer2/data_struct.h"
#include "uart_init.h"
#include "../layer4/transport.h"
#include "../irq/clock_init.h"
#include "../layer3/network.h"
#include "../crc/crc.h"
#include "../layer2/data_link.h"
#include "../irq/interrupt_handler.h"
#include "../layer1/physical.h"

extern const int ADDRESS;

/**
 * @brief This function forwards STDIO input to UART send function and invoked whenever a bit is written to printf. 
 * @param c The character to be sent to rasperrypi. 
 * @param stream The STDIO stream. 
*/
void uart_putchar(char c, FILE *stream) 
{
    loop_until_bit_is_set(UCSR0A, UDRE0);
    UDR0 = c;
}

/**
 * @brief This function forwards UART receive function to STDIO output and invoked whenever a bit is received from UART. 
 * The received bit will be returned when getchar function is invoked. 
 * @param stream The STDIO stream. 
 * @return The received character from raspberry pi. 
*/
char uart_getchar(FILE *stream) 
{
    loop_until_bit_is_set(UCSR0A, RXC0); 
    return UDR0;
}

/**
 * @brief This function initialises UART. 
*/
void uart_init(void) 
{
    UBRR0H = UBRRH_VALUE; // configure levels using BAUD & processor rate
    UBRR0L = UBRRL_VALUE;
    UCSR0A &= ~(_BV(U2X0)); // set single channel
    UCSR0C = _BV(UCSZ01) | _BV(UCSZ00); // set 8 bit data 1 stop bit
    UCSR0B = _BV(RXEN0) | _BV(TXEN0);   // enable receiver and transmitter
}
