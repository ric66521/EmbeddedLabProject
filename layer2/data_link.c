/**
 * @file data_link.c
 * @author David Ng 550084
 * @brief This component contains logics related to handling sending and receiving packets on data link layer
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

extern struct receive_buffer bufferReceive;

extern struct data_node *forwardDataQueue, *forwardDataQueueEnd; 
extern struct data_node *sendDataQueue, *sendDataQueueEnd; 
extern struct data_node *sendDataNode; 
extern struct data_node *receiveDataNode; 

extern const int ADDRESS;

/** 
 * @brief This method constructs an instance of data node struct. 
* @param payload This is the payload data for calculation.
* @param length This is the length of the payload data.
* @return An instance of struct of data_node. 
*/
struct data_node* dataNodeConstructor(unsigned char length, unsigned char *payload)
{
    unsigned long crc = calculateCRC(payload, length);
    unsigned char *header = calloc(5, sizeof(char));
    int i;
    for (i = 0; i < 4; i++)
    {
        header[i] = (crc >> (24 - i * 8)) & 0xFF; // dismantle crc into 4 characters
    }
    header[4] = length;
    struct data_node *node = calloc(1, sizeof(struct data_node));
    node->length = length;
    node->payload = payload;
    node->header = header;
		//printf("Len%d", node->length);
    return node;
}

/** 
* @brief This method prepares to construct a data node struct and push the node to send queue. 
* @param payload This is the payload data for calculation.
* @param length This is the length of the payload data.
*/
void prepareDataNodeForSending(unsigned char length, unsigned char *payload) 
{
    // printf("Now send: %s\n", payload+2);
    struct data_node *node = dataNodeConstructor(length, payload);
    pushSendQueue(node); // put it to normal queue
}

/**
 * When sendControl.active is true, i.e. The device is sending a packet, the function invokes prepareSendBit method to send the next bit. <br>
 * Else when the device is not sending a packet, the function checks whether there is packet in queue waiting to be sent. <br>
 * If yes, the sendControl.active is set to true and the program invokes prepareSendBit to send the first bit of premeable. <br>
 * If no, then 0 is sent.
 * @brief This method makes bit send decision whenever a timer interrup is triggered. 
 */
void clockTickSendDecisionMaker() 
{ 
    if (sendControl.active) // when sending packet not finished
        prepareSendBit();
    else
    {
        struct data_node *tempNode = popSendQueue(); // check if something has been added to queue and sends it
        if (tempNode != NULL)
        {
            sendControl.active = 1;
            sendDataNode = tempNode;
            prepareSendBit();
        }
        else
        {
            sendBit(0);
        }

    }
}

/**
 * Whenever this function is triggered, a bit is given as parameter. <br>
 * Then the premeableRead is updated by shifting the existing value to left by 1 bit and disjunct it with the newly received bit.<br>
 * When 0x7E presents at the premeableRead variable, premeableRead is reset to 0 and receiveControl.active is set to one, thus the procedures to receive a packet starts. <br>
 * A new instance of data_node is assigned to receiveDataNode to prepare for receiving packet. 
 * @brief This method detects whether a premeable is received. 
 * @param bit The bit that has just been received at pin change interrupt. 
 */
void detectPremeable(unsigned char bit)
{

    receiveControl.premeableRead = (receiveControl.premeableRead << 1) | bit;
    if (receiveControl.active == 0);
        // printf("%d", bit);
    if (receiveControl.premeableRead == 0x7E) // if premeable detected, start receiving header
    {
        receiveControl.active = 1; // activate the receiving logic
        receiveControl.premeableRead = 0;
        receiveDataNode = (struct data_node*)calloc(1, sizeof(struct data_node)); // initialise the data_node struct to store the receiving packet
        receiveDataNode->next = NULL;
        receiveDataNode->header = (char*)calloc(5, sizeof(char));
        receiveDataNode->toRead = 1;
    }
		
}

/** 
 * @brief This method resets control data after a data_node is completely sent. 
*/
void sendWrapUp()
{
    sendControl.type = sendControl.index = sendControl.active = 0;
    sendDataNode = NULL;
}

/**
 * This function controls the incrementation of bit index at sending process. <br>
 * When premeable is completely sent, sendControl.type will be set to 1 and bit index is reset to 0. <br>
 * When header is completely sent, sendControl.type will be set to 2 and bit index is reset to 0. <br>
 * When payload is completely sent, sendWrapUp will be invoked to reset all send control settings. 
 * @brief This function checks if the bit index needs to be reset and the send control type needs to be incremented. 
 */
void sendBitManagement()
{
    if (sendControl.type == 2) // when sending payload
    {
        if (sendControl.index == sendDataNode->length * 8) // when everything in payload is sent
            sendWrapUp();
    }
    else if (sendControl.type == 1) // when sending header
    {
        if (sendControl.index == 40) // when everything in header is sent (8 * 5 = 40 bits))
        {
            sendControl.type = 2;
            sendControl.index = 0;
            //
        }
    }
    else // when sending premeable
    {
        if (sendControl.index == 8) // when premeable is sent
        {
            sendControl.type = 1;
            sendControl.index = 0;
        }
    }
}

/**
 * When the datalock of the node is 1, it represents that the mutex has already been locked. <br>
 * Then, if the method calling getMutex is handling bit receiving, writeBackOff in the data_node will be set to 1. <br>
 * Else if the method calling getMutex is handling bit sending, sendBackOff in the data_node will be set to 1. <br>
 * When the datalock of the node is 0, it is set to 1 to complete the mutex lock process. 
 * @brief This method is triggered whenever a mutex is needed for accessing protected area. 
 * @param receiving This is a flag to denote whether the method calling this function is receiving a bit. 
 * @param node The data node of which a mutex is needed. 
 */
void getMutex(unsigned char receiving, struct data_node *node)
{
    if (node->datalock) // if failed to get mutex
    {
        if (receiving) // prepare another attempt to rewrite
            node->writeBackOff = 1;    // prompt sending program to resend after sending this bit is finished
        else
            node->sendBackOff = 1;
    }
    else
        node->datalock = 1; // get mutex
}

/**
 * Whenever a bit is received, it is stored to a temporary bit buffer. <br>
 * When 8 bits are written to the temporary bit buffer, the freshly written byte will become ready for processing at receiveByte. <br>
 * Then the program sets the writeToStructFlag in receive_node, so that at next loop in main, the device will invoke writeByteToStruct to properly invoke receiveByte. 
 * @brief This method writes a received bit to a temporary byte buffer. 
 * @param bit This is the bit to be written to a temporary byte buffer. 
 */
void writeBitToBuffer(unsigned char bit)
{
    bufferReceive.buffer[bufferReceive.receiveByteIndex] |= bit << 7 - bufferReceive.receiveBitIndex; // push the new bit to the byte buffer
    bufferReceive.receiveBitIndex++;
    if (bufferReceive.receiveBitIndex == 8) // when a byte is completely read
    {
        bufferReceive.receiveBitIndex = 0; // reset receive bit index
        bufferReceive.writeByteIndex = bufferReceive.receiveByteIndex; // point the write byte index to the newly read receive byte index
        if (bufferReceive.receiveByteIndex == 4) // set the receive byte index to the next available byte buffer
            bufferReceive.receiveByteIndex = 0;
        else
            bufferReceive.receiveByteIndex++;
        bufferReceive.writeToStructFlag = 1; // to allow main loop to process the freshly ready byte
    }
}

/**
 * This function triggers receiveByte to process the newly ready byte. <br>
 * If 0 is returned as failflag, writeToStructFlag in receive_node, and temporary buffer storing the newly ready byte are reset to 0. 
 * @brief This method writes a byte to a data_node instance whenever the byte in temporary buffer is completed. 
 */
void writeByteToStruct()
{
    unsigned char failFlag = receiveByte(bufferReceive.buffer[bufferReceive.writeByteIndex]);
    if (!failFlag)
    {
        bufferReceive.buffer[bufferReceive.writeByteIndex] = 0;
        bufferReceive.writeToStructFlag = 0;
        receiveDataNode->writeBackOff = 0;
    }
}

/**
 * This function firstly tries to get a mutex, if it fails to get one, it terminates. <br>
 * Then it will look for the next available bit from either payload, header, or premeable, based on sendControl.type. <br>
 * Then it will invoke sendBitManagement to manage sendBitIndex and sendControl.type. <br>
 * After that, it will release the mutex, and invoke sendBit. 
 * @brief This method extracts a bit from the node that is being sent. 
 */
void prepareSendBit() 
{
    // printf("Sending");
    unsigned char dataBit = 0;
    ATOMIC_BLOCK(ATOMIC_FORCEON) // atomic in entire action of getting mutex
    {
        getMutex(0, sendDataNode);
    }
    if (sendDataNode->sendBackOff) // sendBackOff means fail to get mutex, then back off
        return;
    if (sendControl.type == 2) // send payload
        dataBit = (sendDataNode->payload[sendControl.index / 8] >> (7 - (sendControl.index % 8))) & 1;
    else if (sendControl.type == 1) // send header
        dataBit = (sendDataNode->header[sendControl.index / 8] >> (7 - (sendControl.index % 8))) & 1;
    else // send premeable
    {
        const int header = 0x7E;
        dataBit = (header >> (7 - sendControl.index)) & 1;
    }
    // printf("%d", sendControl.bitIndex);
    sendControl.index++;
    sendBitManagement(); // check if need to reset send bit, or if sending has finished
    sendDataNode->datalock = 0; // release mutex
    sendBit(dataBit);
}

/**
 * Firstly this function resets control data for receiving packets. <br>
 * Then if the newly received packet is to read, the CRC Value of the payload will be calculated and compared with the received CRC Value. <br>
 * Finally the data packet will be passed to layer 3 by invoking networkDataProcessing. 
 * @brief This method resets control data and invokes function on layer 3 when needed after a packet has been received in its entirety. 
 */
void receiveWrapUp()
{
    receiveControl.active = receiveControl.type = bufferReceive.receiveBitIndex = bufferReceive.receiveByteIndex = receiveControl.index = 0; // reset data receiving parameters
    if (receiveDataNode->toRead) // if need to pass received data to network
    {
        unsigned char i = receiveDataNode->length;
        unsigned long receivedCRC = (unsigned long)receiveDataNode->header[0] << 24 | (unsigned long)receiveDataNode->header[1] << 16 | (unsigned long)receiveDataNode->header[2] << 8 | (unsigned long)receiveDataNode->header[3];
        unsigned long crc = calculateCRC(receiveDataNode->payload, i);
        printCRCByByte(crc);
        printCRCByByte(receivedCRC);
        // printf("%s\r\n", receiveDataNode->payload + 2);
/*
        for (i = 0; i < 4; i++)
            printf("%X ", receiveDataNode->header[i]);
        printf("KFC %X %X", calculatedCRC, receivedCRC);
*/
        if (crc == receivedCRC)
            networkDataProcessing(receiveDataNode, 1);
        else
            networkDataProcessing(receiveDataNode, 0);
    }
    // receiveDataNode = NULL;
    
}

/**
 * This function resets receive bit index when header has been completely read. <br>
 * Then it initialises the buffer for receiving payload depending on the length from received header value. <br>
 * When first 2 bytes of payload has been received, they are sent to network layer to check if the packet is to read. <br>
 * When the entire payload has been received, receiveWrapUp is called for final processing. 
 * @brief This function checks if the bit index needs to be reset and the receive control type needs to be incremented. 
 */ 
void receiveByteManagement()
{
    if (receiveControl.type == 1) // when receiving payload
    {
        if (receiveControl.index == 2) // when both destination and source addresses have been received
            checkIfNeedForwardOrRead(receiveDataNode->payload);
        if (receiveControl.index == receiveDataNode->length) // when finished receiving the entirety of payload
            receiveWrapUp();
    }
    else
    {
        if (receiveControl.index == 5) // when finished receiving header
        {
            receiveControl.type = 1; // change to receive payload
            receiveControl.index = 0; // reset bit index for receiving payload
            receiveDataNode->length = receiveDataNode->header[4]; // put length into proper field in structure
            receiveDataNode->payload = calloc(receiveDataNode->length, 1); // initialise memory to receive payload
            
        }
    }
}

/**
 * Firstly this function gets a mutex. In case it fails, the function terminates. <br>
 * Then the newly read byte is put to either payload or header buffer depending on the value in receiveControl.type <br>
 * After that it releases the mutex, and invoke receiveByteManagement. 
 * @brief This method writes a byte from temporary buffer to an instance of the data_node. 
 * @param byte The byte to write to the data_node instance. 
 * @return Whether the write operation is successful. 
 */
int receiveByte(unsigned char byte)
{
    // printf("%d", bit);
    // if (receiveDataNode == NULL) // at beginning of receiving header
    ATOMIC_BLOCK(ATOMIC_FORCEON) // get mutex
    {
        getMutex(1, receiveDataNode);
    }
    if (receiveDataNode->writeBackOff) // terminate when failed to get mutex
    {
        return 1;
    }
    if (receiveControl.type) // when receiving payload
        receiveDataNode->payload[receiveControl.index] = byte;
    else // when receiving header
        receiveDataNode->header[receiveControl.index] = byte;
    receiveControl.index++;
    receiveDataNode->datalock = 0; // release mutex
    receiveByteManagement();
    /*
    if (receiveDataNode->sendBackOff) // if sending is cut off resume it now
    {
        prepareSendBit();
        receiveDataNode->sendBackOff = 0;
    }
    */
    return 0;
}
