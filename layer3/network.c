/**
 * @file network.c
 * @author David Ng 550084
 * @brief This component handles functionalities on network layer, which includes addressing management
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
#include "network.h"
#include "../crc/crc.h"
#include "../layer2/data_link.h"
#include "../irq/interrupt_handler.h"
#include "../layer1/physical.h"

extern struct data_node *forwardDataQueue, *forwardDataQueueEnd;
extern struct data_node *sendDataQueue, *sendDataQueueEnd; // Queue for node to be sent
extern struct data_node *sendDataNode; // Node currently being sent
extern struct data_node *receiveDataNode; // Where received data goes

extern const int ADDRESS;

/**
 * This function inserts sender and receiver addresses to the head of the payload from transport layer. <br>
 * Then prepareDataNodeForSending in data link layer is invoked for further processing before sending. 
 * @brief This function inserts source and destination addresses into payload before passing it to data link layer. 
 * @param dest The destination address in integer. 
 * @param length The length of the payload without addresses. 
 * @param dataArr Payload to send as character array. 
 */
void prepareDataSend(int dest, int length, unsigned char *dataArr)
{
    unsigned char *payload = calloc(length + 2, sizeof(char)); // 2 bytes longer due to destination and source addresses
    payload[0] = dest, payload[1] = ADDRESS;
    int i;
    for (i = 0; i < length; i++) // copy everything from transport layer info to the network layer
        payload[i + 2] = dataArr[i];

    free(dataArr);
    prepareDataNodeForSending(length + 2, payload);
}

/**
 * @brief A decision maker function to determine which function on transport layer to invoke depending on the types of packet received.
 * @param data The completely received data packet as an instance of data_node. 
 * @param crcMatched A flag to denote whether CRC is correct. 
 */
void networkDataProcessing(struct data_node *data, int crcMatched)
{
    if (crcMatched)
    {
        printf("CRC GEKLAPPT!\r\n");
        if (data->payload[1] == ADDRESS && data->payload[0]) // when a packet is sent from this device and the recipient does not exist
        {
            char tempAddress = data->payload[0];
            notifyFailSend(data->payload + 2, tempAddress);
        }
        else if (data->payload[0] == ADDRESS || (data->payload[0] == 0 && data->payload[1] != ADDRESS)) 
        // when this device is the intended recipient or a broadcast message is received
        {
            transportProcessing(data->payload[1], data->payload[0], data->header[4] - 2, data->payload + 2); // pass to transport layer for processing
            // In above statement, "data->header[4] - 2" is passed because it is necessary to deduct the length of origin and destination addresses
            // "data->payload + 2" is for skipping the first 2 bytes of payload, which carry destination and source addresses
        }
        else if (data->payload[1] == ADDRESS && data->payload[0] == 0) // when the broadcast message sent by this device is returned
            notifySuccessBroadcast(data->header[4] - 2, data->payload + 2);
				
    }
    else
    {
        printf("schade: CRC not matched\r\n");
        
    }
}

/**
 * @brief This is to determine whether the receiving node needs to be forwarded or read based on the sender and receiver addresses. 
 * @param payload This is the pointer to the payload in packet as a pointer of character array. 
 */
void checkIfNeedForwardOrRead(unsigned char *payload)
{
    printf("\r\nS:%d R:%d\r\n", payload[1], payload[0]);
    if (payload[0] && payload[0] != ADDRESS && payload[1] != ADDRESS) // not broadcast and this atmega is not the intended recipient
    {
        receiveDataNode->toRead = 0;
        jumpSendQueue(receiveDataNode); // start forwarding
    }
    else if (payload[0] == 0) // check if it is a broadcast message
    {
        if (payload[1] != ADDRESS) // continuing forwarding if it is not the broadcast message circulated back
            jumpSendQueue(receiveDataNode);
    }
}
