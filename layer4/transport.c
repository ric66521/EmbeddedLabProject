/**
 * @file transport.c
 * @author David Ng 550084
 * @brief This component is responsible for handling functionalities on transport layer. 
 * @date 2020-02-20
 * @copyright Copyright (c) 2020
 * 
 */

#define F_CPU 12000000UL
#define BAUD 9600
#define MYUBRR F_CPU/16/BAUD-1


#include <limits.h>
#include <avr/io.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <util/delay.h>
#include <util/setbaud.h>
#include <util/atomic.h>
#include <avr/interrupt.h>
#include "../layer2/data_struct.h"
#include "../uart/uart_init.h"
#include "transport.h"
#include "../irq/clock_init.h"
#include "../layer3/network.h"
#include "../crc/crc.h"
#include "../layer2/data_link.h"
#include "../irq/interrupt_handler.h"
#include "../layer1/physical.h"
#include "transport_struct.h"

extern const int ADDRESS;
extern unsigned int msgWaitingPeriod;
extern unsigned int globalPeriodStamp;

struct transport_node **sentMessagesCache = NULL; ///< An array of transport_node to store all sent messages that are pending for respective ACK messages
int nextAvailableSlot;

/**
 * @brief This is to initialise the array that stores sent messages. 
 */
void transportCacheArrayInit()
{
    sentMessagesCache = malloc(sizeof(struct transport_node*) * 256);
    nextAvailableSlot = 0;
    for (int i = 0; i < 256; i++)
        sentMessagesCache[i] = NULL;
}

/**
 * @brief This function is to calculate the length of the message. 
 * @param data The piece of data as pointer to character array. 
 * @return The length of data in bytes as integer. 
*/
int calculateLength(char *data)
{
    int index = 0;
    while (data[index])
        index++;
    return index;
}

/**
 * Normally the result is simply calculated by subtracting the comparator value from the globalPeriodStamp.  <br>
 * However, in case the globalPeriodStamp has overflowed and wrapped around to 0, the erroneous result will be subtracted from UINT_MAX. 
 * @brief This is to calculate the difference between period stamps. 
 * @param comparator The period stamp to compare with the global system period stamp. 
 * @return The difference in period stamp in integer. 
*/
unsigned int periodDiffCalculator(unsigned int comparator)
{
    unsigned int result = globalPeriodStamp - comparator;
    if (globalPeriodStamp < comparator)
        result = UINT_MAX - result;
    return result;
}

/**
 * This function calculates the difference between the sentPeriodStamp in each transport_node and globalPeriodStamp. <br>
 * Then if the type of message in transport_node is not 2, and the calculated difference exceeds the threshold, which denotes time-out, the message is sent again.
 * @brief This function checks whether a sent message becomes timed out.
 */
void periodClockUpdate()
{
    for (int i = 0; i < 256; i++)
    {
        if (sentMessagesCache[i] == NULL)
            continue;
        unsigned int periodDiff = periodDiffCalculator(sentMessagesCache[i]->sentPeriodStamp);
        if (periodDiff >= msgWaitingPeriod)
        {
            if (sentMessagesCache[i]->flag != 2)
            {
			    prepareDataSend(sentMessagesCache[i]->destination, sentMessagesCache[i]->length, sentMessagesCache[i]->msg);
                sentMessagesCache[i]->sentPeriodStamp = globalPeriodStamp;
            }
            else
            {
                free(sentMessagesCache[i]);
                sentMessagesCache[i] = NULL;
            }
            
        }
    }
}

/**
 * @brief This function constructs a node of TransportNode. 
 * @param type Flags of the transport layer message as prescripted in specification. 
 * @param data The payload data to send. 
 * @param target The intended receiver of this message. 
 * @param length The length of the message. 
 * */
struct transport_node* constructTransportNode(unsigned char type, unsigned char *data, unsigned char target, int length)
{
    struct transport_node *result = malloc(sizeof(struct transport_node));
    result->sentPeriodStamp = globalPeriodStamp;
    result->flag = type;
    result->msg = data;
    result->destination = target;
    result->length = length;
    return result;
}

/**
 * @brief This function updates the index number for array sentMessageCache, which will be used when another message is sent in the future. 
 */
void updateCacheArrIndex()
{
    int oldValue = nextAvailableSlot;
    while (sentMessagesCache[nextAvailableSlot] != NULL)
    {
        nextAvailableSlot++;
        if (nextAvailableSlot == 256)
            nextAvailableSlot = 0;
        if (oldValue == nextAvailableSlot)
            ; // error
    }
}

/**
 * @brief This function is triggered when a new message is sent. 
 * @param address The address of the message receiver. 
 * @param type The flag of the payload as required in specification. 
 * @param data The payload data to send. 
 * @param length The length of the payload data. 
 */
void initiateSend(int address, unsigned char type, unsigned char *data, int length)
{
	if (type != 2 && address)
	    sentMessagesCache[nextAvailableSlot] = constructTransportNode(type, data, address, length);
    int newLength = length + 2;
    unsigned char *payload = malloc(newLength);
    payload[0] = nextAvailableSlot;
    payload[1] = type;
	for (int i = 0; i < length; i++)
		payload[i + 2] = data[i];
    updateCacheArrIndex(); // This function is called anyway because the id number is used in the message even when the message is not saved
    prepareDataSend(address, newLength, payload);
}

/**
 * @brief This function sends an ACK message without registering the message in sentMessageCache. 
 */
void sendACK(int address, unsigned char id)
{
	char *payload = malloc(3);
	payload[0] = id, payload[1] = 1, payload[2] = 0;
	prepareDataSend(address, 3, payload);
}

/**
 * If the flag of newly received message is 1 (which denotes ACK), it means that the sender has received a previously sent message successfully. <br>
 * Then the corresponding instance of transport_node in sentMessageCache is removed. <br>
 * If the flag of newly received message is 2 (which denotes datagram), the received message is printed and discarded. <br>
 * If other types of message are received, the message is printed and an ACK message will be sent back to the sender.
 * @brief This function is triggered to process message received from other node.
 * @param srcAddress The sender address of the data packet that is being processed.
 * @param targetAddress The receiver of the data packet that is being processed.
 * @param length The length of the payload data.
 * @param data The payload data.
 */
void transportProcessing(unsigned char srcAddress, unsigned char targetAddress, int length, unsigned char *data)
{
    if (targetAddress == ADDRESS)
    {
        switch (data[1])
        {
            case 1:
                printf("Node %d received message: %s\r\n", srcAddress, sentMessagesCache[data[0]]->msg);
                free(sentMessagesCache[data[0]]);
                sentMessagesCache[data[0]] = NULL;
            break;
            case 0xfc: // future use
            default:
				;
				sendACK(srcAddress, data[0]);
            case 2:
                printf("From %d received message: %s\r\n", srcAddress, data+2);
            break;
        }
    }
    else if (srcAddress == 0)
    {
        printf("Received broadcast message: %s\r\n", data);
    }
    else
        ; // error
    
    
}

/**
 * @brief This function is to notify user whenever a sent non-broadcast message is returned. 
 * @param payload The payload data which has not been sent successfully. 
 * @param dest The false destination of the message. 
 */
void notifyFailSend(char *payload, char dest)
{
    free(sentMessagesCache[payload[0]]);
    sentMessagesCache[payload[0]] = NULL;
    printf("Send failed: %d does not exist\r\n", dest);
}

/**
 * @brief This function is triggered when a broadcast is successful. 
 * @param length The length of the successfully broadcasted message. 
 * @param data The message broadcasted successfully. 
 */
void notifySuccessBroadcast(int length, unsigned char *data)
{
    printf("Message: %s\r\nAbove message is successfully broadcasted\r\n", data + 2);
}


