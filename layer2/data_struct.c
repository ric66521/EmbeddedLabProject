/**
 * @file data_struct.c
 * @author David Ng 550084
 * @brief This component is responsible for maintaining data structures that support the functionalities on data link layer
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
#include "data_struct.h"
#include "../uart/uart_init.h"
#include "../layer4/transport.h"
#include "../irq/clock_init.h"
#include "../layer3/network.h"
#include "../crc/crc.h"
#include "data_link.h"
#include "../irq/interrupt_handler.h"
#include "../layer1/physical.h"

extern struct comm_control sendControl;
extern struct comm_control receiveControl;

extern struct data_node *forwardDataQueue, *forwardDataQueueEnd; 
extern struct data_node *sendDataQueue, *sendDataQueueEnd; 
extern struct data_node *sendDataNode; 
extern struct data_node *receiveDataNode; 

extern const int ADDRESS;

/**
* This function checks if there is any node left to be sent. <br>
* Firstly it looks for queue dedicated to nodes being forwarded as they are prioritised. <br>
* Function pops first node and returns it if there is node in such queue. <br>
* Then it looks for queue dedicated to nodes that originates in this device. <br>
* Function pops first node and returns it if there is node in such queue. <br>
* If both queues are empty, null is returned.
* @brief This function returns an instance of data_node if there exists data node to be sent. 
* @return The pointer to the data node which will be sent soon. 
*/ 
struct data_node* popSendQueue() // return null when no more node to send; return the node when need sending
{
    struct data_node *temp = NULL;
    if (forwardDataQueue != NULL) // check forward queue as these nodes are prioritised
    {
		//// printf("Pop Forward\r\n");
        temp = forwardDataQueue;
        if (forwardDataQueue == forwardDataQueueEnd)
            forwardDataQueue = forwardDataQueueEnd = NULL;
        else
            forwardDataQueue = forwardDataQueue->next;
    }
    else if (sendDataQueue != NULL) // check sent queue later as they are of lower priority
    {
		//// printf("Pop Normal\r\n");
        temp = sendDataQueue;
        if (sendDataQueue == sendDataQueueEnd)
            sendDataQueue = sendDataQueueEnd = NULL;
        else
            sendDataQueue = sendDataQueue->next;
    }
    return temp;
}


/**
 * @brief This function pushes a data node to forwardDataQueue. This is only invoked when a node is to forward. 
 * @param node Pointer to the instance of data_node which will be forwarded. 
 */
void jumpSendQueue(struct data_node *node) // for forwarding packet
{
    if (forwardDataQueue == NULL)
        forwardDataQueue = forwardDataQueueEnd = node;
    else
    {
        forwardDataQueueEnd->next = node;
        forwardDataQueueEnd = node;
    }
}

/**
 * @brief This function pushes a data node to sendDataQueue. This is only invoked when a node is sent by user action. 
 * @param node Pointer to the instance of data_node which will be sent. 
 */
void pushSendQueue(struct data_node *node) // for sending packet
{
    if (sendDataQueue == NULL)
        sendDataQueue = sendDataQueueEnd = node;
    else
    {
        sendDataQueueEnd->next = node;
        sendDataQueueEnd = node;
    }
}
