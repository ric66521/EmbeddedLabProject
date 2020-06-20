/**
 * @file rasp_net.c
 * @author David Ng 550084
 * @brief This component contains the main program and various global variables. 
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
#include <string.h>
#include <util/delay.h>
#include <util/setbaud.h>
#include <util/atomic.h>
#include <avr/interrupt.h>
#include "layer2/data_struct.h"
#include "uart/uart_init.h"
#include "layer4/transport.h"
#include "irq/clock_init.h"
#include "layer3/network.h"
#include "crc/crc.h"
#include "layer2/data_link.h"
#include "irq/interrupt_handler.h"
#include "layer1/physical.h"

// 64

// use stdint.h

struct comm_control sendControl = {0, 0, 0, 0}; ///< This is an instance of comm_control for maintaining control data for send procedures. 
struct comm_control receiveControl = {0, 0, 0, 0}; ///< This is an instance of comm_control for maintaining control data for receive procedures. 

struct receive_buffer bufferReceive = {NULL, 0, 0, 0, 0}; ///< This is an instance of receive_buffer for maintaining temporarily read bits. 

struct data_node *forwardDataQueue = NULL, *forwardDataQueueEnd = NULL; ///< This is a queue of data_node to be forwarded. 
struct data_node *sendDataQueue = NULL, *sendDataQueueEnd = NULL; ///< This is a queue of data_node to be sent.
struct data_node *receiveDataNode = NULL; ///< This is the instance of data_node that is being written by received bytes. 
struct data_node *sendDataNode = NULL; ///< This is the instance of data_node that is being sent. 

const int ADDRESS = 15; ///< This denotes the address of the current device. 
int sendSpeed = 1; ///< This is the flag of the period of interrupt, thus how long would it take to send one bit. 
unsigned int globalPeriodStamp = 0; ///< This denotes how many timer interrupts have been triggered. 

static FILE uart_output = FDEV_SETUP_STREAM(uart_putchar, NULL, _FDEV_SETUP_WRITE); ///< This forwards the output of stdio to UART output. 
static FILE uart_input = FDEV_SETUP_STREAM(NULL, uart_getchar, _FDEV_SETUP_READ); ///< This forwards the UART input to the input of stdio. 

int printMode = 0; 
unsigned int msgWaitingPeriod = 2048 * 2 * 2; ///< This denotes the threshold number of elasped interrupts. When the period stamp difference is greater than this period, it denotes that the message has timed out. 


/**
 * Also it asks the user to input the desired period of timer interrupt. <br>
 * After that, the interruptInit will be triggered to initalise pin change and timer interrupts.  <br>
 * Finally it enables interrupt globally and invokes transportCacheArrayInit to initalise transport layer. 
 * @brief This function initalises send and receiving pins and LED outputs. Also it configures the length of a time interrupt (i.e. Transmission speed). 
 */
void generalInit()
{
    DDRB |= (1 << PB5 | 1 << PB4); // Set PB4 PB5 as output 4: Clock 5: Data
    DDRD &= ~(1 << PD5 | 1 << PD4); // set PD4 PD5 as input
	DDRC = 1 << DDC3;
    PORTC = 0;
    PORTD = 0;
    bufferReceive.buffer = (unsigned char*)calloc(sizeof(unsigned char), 5);
    uart_init();
	stdin = &uart_input;
	stdout = &uart_output;
    printf("Please key in speed 1-5 one slowest\r\n");
    char *speedBuffer = malloc(2);
    speedBuffer[0] = getchar();
    speedBuffer[1] = 0;
    sendSpeed = strtol(speedBuffer, NULL, 0);
    // Attention: Need connect PD0 to one of buffer and put jumper to relevant output pins
    interruptInit(sendSpeed);
    sei(); // enable Interrupt globally
    transportCacheArrayInit();
}

/**
 * In a while loop, it processes: <br>
 * 1. User input for sending a message. Firstly the user should type the address of the receiver, and press ENTER. <br>
 * Then the user should type the message to send, and press ENTER. After that, initiateSend function on transport layer will be invoked to start the sending procedures. <br>
 * 2. If sendBackOff in sendDataNode has the value of 1, it will retry the invocation of prepareSendBit to send a bit again. <br>
 * 3. If writeBackOff in receiveDataNode has the value of 1, it will retry the invocation of writeByteToStruct to write a byte again. <br>
 * 4. If the period stamp has been updated, it will call periodClockUpdate to check if a sent message is timed out.
 * @brief This is the main function. It begins all initialisation processes and contains an infinite loop to process user inputs, missed inputs, and missed outputs
 */
int main(void)
{
    generalInit();
    unsigned char *messageBuffer = malloc(128);
    int index = 0;
    int typeMessage = 0; // 0 is target address 1 is message
    int address = 0;
    int type = 0;
	int inputMode = 0;
	int clockComparator = globalPeriodStamp;
    while (1)
    {		
        if (UCSR0A >> 7) // check if data exists
        {
            unsigned char temp = getchar();
            if (temp == '\r')
            {
                if (inputMode == 2) // when accepting message body
                {
                    messageBuffer[index++] = '\0';
                    initiateSend(address, type, messageBuffer, index);
                    messageBuffer = malloc(128), index = 0, inputMode = 0;
                }
                else if (inputMode == 1) // when accepting type of message (flag in transport layer)
                {
                    type = strtol(messageBuffer, NULL, 0);
                    free(messageBuffer);
                    messageBuffer = malloc(128), index = 0, inputMode = 2;
                }
                else // when accepting address of recipient
                {
                    address = strtol(messageBuffer, NULL, 0);
                    free(messageBuffer);
                    messageBuffer = malloc(128), index = 0, inputMode = 1;
                }
                
            }
            else if (temp == '\b') // Remove one character from buffer when "backspace" is taped
            {
                messageBuffer[index] = '\0';
                if (index)
                    index--;
            }
            else
                messageBuffer[index++] = temp;
        }
        if (sendDataNode != NULL && sendDataNode->sendBackOff) // When send process fails to get a bit from sendDataNode to send
        {
            prepareSendBit();
            sendDataNode->sendBackOff = 0;
        }
        if (receiveDataNode != NULL && (bufferReceive.writeToStructFlag || receiveDataNode->writeBackOff)) // When receive process fails to write the bit to receiveDataNode
        {
            writeByteToStruct();
        }
		if (globalPeriodStamp != clockComparator)
		{
			clockComparator = globalPeriodStamp;
		    periodClockUpdate();
		}
    }
    return 0;
}

// Timer interrupt (Clock)
ISR (TIMER1_COMPA_vect)
{
    timeInterruptFunction();
}

// Pin Change Interrupt (Subject to clock)
ISR (PCINT2_vect)
{
    pinInterruptFunction();
}
