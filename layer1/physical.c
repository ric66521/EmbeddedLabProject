/**
 * @file physical.c
 * @author David Ng 550084
 * @brief This component is responsible for handling bit sending and receiving at physical level
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
#include "physical.h"

extern struct comm_control sendControl;
extern struct comm_control receiveControl;

extern struct data_node *forwardDataQueue, *forwardDataQueueEnd; 
extern struct data_node *sendDataQueue, *sendDataQueueEnd;
extern struct data_node *sendDataNode; 
extern struct data_node *receiveDataNode; 

extern int sendSpeed;
extern int printMode;
extern const int ADDRESS;

/*
* This function is responsible for sending bit to neighbour node. <br>
* This function receives a parameter of char as the data bit to be sent. <br>
* When it is called, it delays for 37.5% of the length of the time interrupt period. <br>
* After that, the data bit is written to port PB5 as output. 
* @brief This method sends a bit to the next node. 
* @param bit The bit to be sent. 
*/
void sendBit(unsigned char bit)
{
    int i;
    int sendSpeedComparator = 0;
    switch(sendSpeed)
    { // 
        case 1:
        sendSpeedComparator = 100; // for a period of 0.2 sec
        break;
        case 2:
        sendSpeedComparator = 20; // for a period of 0.04 sec
        break;
        case 3:
        sendSpeedComparator = 10; // for a period of 0.02 sec
        break;
        case 4:
        sendSpeedComparator = 5; // for a period of 0.01 sec 
        break;
        case 5:
        sendSpeedComparator = 2; // for a period of 0.005 sec
        break;
        default:
        sendSpeedComparator = 2;
        break;
    }
    for (i = 0; i < sendSpeedComparator; i++) // delay send after clock change
        _delay_ms(1); // stop for 1ms 
    int clockNow = (PORTB >> PB4) & 1;
    /*
    static int counter = 0;
    if (printMode == 1)
    {
        printf("S%d", bit);
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
    PORTB = (bit << PB5) | (clockNow << PB4);
}

/*
* This function receives a parameter of char as the received data bit. <br>
* When it is called, if this device is in the process of receiving packet, writeBitToBuffer function is triggered. <br>
* else, detectPremeable function is invoked. 
* @brief This method is executed whenenver a bit is read from pin change interrupt. 
* @param bit The bit that has just been read from pin change interrupt. 
*/
void receiveBitClassification(unsigned char bit)
{
    // printf("%d", bit);
    if (receiveControl.active) // don't check for premeable when receiving contents of packet
        writeBitToBuffer(bit);
    else
        detectPremeable(bit); // check if possible sending starts
}
