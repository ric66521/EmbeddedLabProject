/**
 * @file interrupt_handler.c
 * @author David Ng 550084
 * @brief This component contains logics for handling each pin change interrupt and time interrupt
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
#include "../uart/uart_init.h"
#include "../layer4/transport.h"
#include "../irq/clock_init.h"
#include "../layer3/network.h"
#include "../crc/crc.h"
#include "../layer2/data_link.h"
#include "../irq/interrupt_handler.h"
#include "../layer1/physical.h"

extern int printMode;
extern unsigned int globalPeriodStamp;

/**
* This function is triggered whenever a timer interrupt is triggered. 
* When called, this function negates the clock signal and toggle LED output. 
* After that, it increments the globalPeriodStamp, and triggers clockTickSendDecisionMaker and periodClockUpdate functions. 
* @brief This method handles actions to be taken when a timer interrupt is fired. 
*/
void timeInterruptFunction()
{
    PORTB ^= (1 << PB4);
    PORTC = ~PORTC; // negate LED output
    /*
    static int counter = 0;
    if (printMode == 3)
    {
	    printf("C%d", PORTB);
	    counter++;
    }
    else
        counter = 0;
	if (counter == 10)
	{
		counter = 0;
		printf("\r\n");	
	}
    */
    clockTickSendDecisionMaker();
    globalPeriodStamp++;
}

/**
* This function is triggered whenever a pin change interrupt is triggered (most possibly by toggled clock signal from neighbour node). 
* When called, this function reads input from PD5. 
* After that, it triggers receiveBitClassification function with the read input passed as argument. 
* @brief This method handles actions to be taken when a pin change interrupt is fired. 
*/
void pinInterruptFunction()
{
	static int counter = 0;
    volatile int data = (PIND >> PD5) & 1;
    unsigned char data1 = data;
    unsigned volatile char clock = (PIND >> PD4) & 1;
    /*
    if (printMode == 2)
    {
	    printf("R%d%d", data1, clock);
	    counter++;
    }
    else
        counter = 0;
	if (counter == 10)
	{
		counter = 0;
		printf("\r\n");	
	}
    */
    receiveBitClassification(data1);
}
