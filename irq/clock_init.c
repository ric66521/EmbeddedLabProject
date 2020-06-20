/**
 * @file clock_init.c
 * @author David Ng 550084
 * @brief This component is responsible for initialising time and pin change interrupts
 * @date 2020-02-20
 * 
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
#include "../uart/uart_init.h"
#include "../layer4/transport.h"
#include "clock_init.h"
#include "../layer3/network.h"
#include "../crc/crc.h"
#include "../layer2/data_link.h"
#include "interrupt_handler.h"
#include "../layer1/physical.h"

/**
* This function enables and initiates the clock interrupt with following settings: <br>
* The clock is run on Mode 4, CTC on OCR1A. <br>
* The interrupt is triggered by compare match of values. <br>
* The prescaler is set to 256. <br>
* The OCR1A value is subjected to input from mode
* @brief This method initialises the timer interrupt.
* @param mode This dictates the frequency of triggering an interrupt <br>
* 1 denotes 0.2 sec, 2 denotes 0.04 sec, 3 denotes 0.02 sec, 4 denotes 0.01 sec, and 5 denotes 0.005 sec, per interrupt
*/
void timeInterruptInit(int mode)
{
    int valueToPut = 0;
    switch(mode)
    {
        case 1:
        valueToPut = 9374; // 0.2 sec
        break;
        case 2:
        valueToPut = 1874; // 0.04 sec
        break;
        case 3:
        valueToPut = 936; // 0.02 sec
        break;
        case 4:
        valueToPut = 460; // 0.01 sec 
        break;
        case 5:
        valueToPut = 233; // 0.005 sec
        break;
        default:
        valueToPut = 350;
        break;
    }
    // OCR1A = 9374; // executes every 0.2 second -> 9374 every 0.008 -> 374 0.04 -> 1874
    OCR1A = valueToPut; 
    TCCR1B |= (1 << WGM12);
    // Mode 4, CTC on OCR1A
    // No Normal mode as it wastes CPU resource
    TIMSK1 |= (1 << OCIE1A);
    //Set interrupt on compare match
    TCCR1B |= (1 << CS12);
    // set prescaler to 256 and start the timer
}

/*
* This function enables pin change interrupt for port PD4. 
* for the purpose of triggering data-read when clock signal from neighbouring node changes. 
* @brief This method initialises pin change interrupt.
*/
void pinInterruptInit(void)
{
    PCICR = 1 << PCIE2; // Enable interrupt for port D
    PCMSK2 = 1 << PCINT20; // Enable interrupt for PD4 only
}

/**
 * This function triggers the pinInterruptInit and timeInterruptInit functions. 
 * @brief This method initialises time and pin change interrupt. 
 * @param mode This dictates the frequency of triggering an interrupt <br>
 * 1 denotes 0.2 sec, 2 denotes 0.04 sec, 3 denotes 0.02 sec, 4 denotes 0.01 sec, and 5 denotes 0.005 sec, per interrupt
 */
void interruptInit(int mode)
{
    pinInterruptInit();
    timeInterruptInit(mode);
}
