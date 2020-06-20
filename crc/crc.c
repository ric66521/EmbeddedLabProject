/**
 * @file crc.c
 * @author David Ng 550084
 * @brief This component is responsible for handling logics related to calculating CRC and printing CRC
 * @date 2020-02-20
 * @copyright Copyright (c) 2020
 * 
 */

#define F_CPU 12000000UL
#define BAUD 9600
#define MYUBRR F_CPU/16/BAUD-1
#define ADDRESS 69


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
#include "crc.h"
#include "../layer2/data_link.h"
#include "../irq/interrupt_handler.h"
#include "../layer1/physical.h"

const int CRCVal = 32;

/// This method prints CRC by byte whereby each hexadecimal value represents 
/**This function prints the unsigned long CRC Value. 
 * It prints from the first byte to the last byte in 4 iterations. 
 * @param crcValue The value of CRC in unsigned long. 
 * 
 */
void printCRCByByte(unsigned long crcValue)
{
	uint32_t crc = 0, generator = 0x4C11DB7;
		printf("CRC: ");
		int i;
		for (i = 0; i < 4; i++)
			printf("%X ", crcValue >> (24 - i * 8) & 0xFF);
		printf("\r\n");
}

/// This method calculates the CRC32.
/**
 * The CRC caluclation algorithm is adopted from: http://www.sunshine2k.de/articles/coding/crc/understanding_crc.html
 * @param payload This is the payload data for calculation.
 * @param length This is the length of the payload data.
 * @return The CRC Value in unsigned long.
 * 
 */
unsigned long calculateCRC(unsigned char *payload, unsigned char length)
{
	// printf("Calculating for: %s\r\n", payload + 2);
	unsigned long crc = 0, generator = 0x4C11DB7;
	int i;
	for (i = 0; i < length; i++)
	{
		crc ^= ((uint32_t)payload[i]) << 24;
		int j;
		for (j = 0; j < 8; j++)
		{
			if ((crc & 0x80000000))
				crc = (unsigned long)((crc << 1) ^ generator);
			else
				crc <<= 1;
		}
	}
	return crc & 0xFFFFFFFF;
}
