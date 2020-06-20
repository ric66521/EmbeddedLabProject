# Embedded System Lab Documentation

## General information
This program is designed based on the specification of RASPNet (available on https://osg.informatik.tu-chemnitz.de/lehre/emblab/protocol.pdf). It is capable of receiving a message from another node, sending message to another node, and forwarding message for which intended recipient is not the device running this program. The detailed behaviours of this program in taking these actions are documented below. 

## Copyright and legal notice
This project is licensed under MIT Licence. Please see LICENSE file for more details. 

## Setup
### Wiring configuration
This program is designed for use on Gertboard on which a Atmega328p chip is installed. Before compiling and flashing this program to the device, you must configure the wirings of the program as follows:

Connect PD4 to PB4 of another device. This serves as a receiving medium of clock signal. 
Connect PD5 to PB5 of another device. This serves as a receiving medium of data signal. 


Lastly, a wire must connect anyone of ground pin of your device to anyone ground pin of other device. 

### How to install this program
This project comes with a makefile. To compile and flash this program to the Gertboard, simply type
```bash
make
```
at a linux console. 
At this process, various o, elf, and hex files are generated at the current directory. After the program has successfully been flashed to the Gertboard, you can type
```bash
make clear
```
to delete those files. 

## How to use this program
### To gain access
You can type
```bash
sudo minicom -b 9600 -D /dev/ttyAMA0 -o
```
at linux console to gain access to the communication interface between your device and the Gertboard. 
### Startup
Firstly, you will be asked to key in the desired speed. You are allowed to key in a single integer ranging from 1 to 5. 1 - 5 denotes an interrupt period of 200ms, 40ms, 20ms, 10ms, and 5ms respectively. By keying in the period of interrupt, you have inexplicitly defined the data transmission rate of this program. One bit is sent at each interrupt, so the values of 1-5 also represent the data transmission rate of 5, 25, 50, 100, 500 bits per second. 

Note: The highest tolerated data transmission rate of this program is 100 bits per second, (i.e. 10ms per interrupt). If you choose to run at data transmission of 500 bits per second, unexpected behaviour may occur. 

To key in the desired period, simply press the number, without pressing enter. 

### To send something
To send a message, you need to specify the destination address, the type of address (i.e. The flag as required on layer 4), and the string message. Firstly, you need to key in the destination address, and press enter. Then you need to key in the type of the message, and press enter. Finally you need to key in the string of message (you are allowed to type any character as defined in ASCII, except enter key), and press enter to send the message. 

Afther that, the message will be sent automatically. 

### To receive something
You need to take no actions in order to receive message. In case a message is sent, or broadcasted, to your device, when the message is not corrupted, it will be displayed to you on screen automatically. If the message is corrupted, you will be informed of receiving a corrupted message; however, the content of the message will not be displayed.

## Characteristics
This program consists of the bottom 4 layers under the OSI model (Physical, Data Link, Network, and Transport), and Supporting modules (CRC Calculator, Interrupt Handler, and UART). 

### Interrupts
In this program, 2 interrupts, namely Pin change interrupt and timer interrupt, are used. 

Pin change interrupt is triggered by a change in input values at PD4, which receives the clock tick signal from the previous node in loop. Whenever a pin change interrupt is triggered, the program examines the current reading of PD5, extracts the reading of PD5 as a bit value, and passes it to physical layer for processing. 

Timer interrupt is triggered by elapsing of a fixed period of time. The period of each interrupt is defined by users at startup. In each timer interrupt, the program will negate the current output of pin PB4 as clock-tick action. If the program is not sending a packet, the program will attempt to dequeue a packet. If a packet is dequeued, then the program will activate the sending procedure and start sending the first bit. If the program is already in the progress of sending a packet, it will extract a bit from the packet and send it. 

### CRC
This module is responsible for calculating the CRC checksum to provide for the possibility to check the integrity of the payload. The algorithm for calculating CRC is adopted from http://www.sunshine2k.de/articles/coding/crc/understanding_crc.html. 

### Main function
The main function is responsible for initialising all interrupts and UART communication interface between Raspberrypi and Gertboard. Also the main loop takes care of the user input related to providing parameters at start up, as well as inputting required data for sending messages. 
Also, the main function has an infinite loop to check for toggled flag. Subject to flag toggled, the main function initiates the process of retrying bit extraction from packet being sent, retrying writing byte(s) to packet being received, and check if a sent message at transport layer is expired. 

### Transport layer
RASPNet requires that all messages, except for data gram and broadcast messages, should be stored before a corresponding acknowledgement message is received. In order to provide this functionality, each message is stored as a struct of transport_node, carrying the destination address, type of the message, and the period stamp of sending the message. These instances of transport_node are stored to an array as a message cache. 
Period stamp refers to the number of interrupts that have occured since startup. For the sake of simplicity in evaluating whether a message has been timed out, instead of keeping track of how many milliseconds have passed since startup, this program keeps track of how many timer interrupt have elasped since start-up. 
The index of the message cache array represents the identification of the message at transport layer. 

### Data link layer
On this layer, an instance of the struct of data_node represents a packet. It contains the header and payload as required by RASPNet. 
In order to save computation power from copying data between buffers, in case a packet needs to be forwarded, the same instance of data_node is enqueued to the send waiting queue. For the sake of mitigating the possible damages caused by race condition, a mutex is employed in protecting the integrity of the data. Whenever a bit is written or extracted from the packet, the calling function must secure the mutex before the relevant action takes place. In case the calling function cannot secure the mutex, it will back off and toggle its relevant flag. The main loop will detect the flag toggled, and the retry action will be conducted in the very short future. 

In order to provide for prioritisation of forwarding packets, 2 queues are maintained for message waiting to transmit. Whenever a dequeue operation occurs, the program looks for the queue storing packets pending to forward first, thereafter the queue storing packets that are pending to send from the current device. 

## Workflow at each layer
### Receive actions
#### Physical layer
This module processes reading pin values for data transmission purpose. Whenever a pin change interrupt is triggered, this layer receives the pin value of PD5 from interrupt module. If this program is in the progress of receiving a packet, the received bit is passed to data link layer for processing; otherwise, the received bit is passed to data link layer for premeable detection. 

#### Data link layer
This module is responsible for receiving bits at packet level. When the program is not receiving a packet, the received bit is stored to a temporary buffer of size 1 byte, along with the last 7 bits received. Then the 8 bits are used to compare with the predefined premeable value (0x7E). If the 8 bits match with the premeable value, it indicates that a packet is currently being sent from the previous node. Then function at this layer will initialise a struct of data_node, and activate the procedures of receiving a packet. 

If the program is in the progress of receiving a packet, the received bit will be stored to a temporary buffer. When 8 bits has been accumulated, the freshly available byte will be written to the struct of data_node. The reason of not writing directly the bit to the data_node struct is to minimise the length of execution statements at a pin change interrupt. 

When this module has received the first 2 bytes of payload, the 2 bytes of payload will be passed to network layer for processing to determine whether the packet should be read and forwarded. If network layer has decided that the receiving packet needs to be forwarded, the same instance of data_node will be pushed to the prioritised queue for forwarding. 

When the entirety of the data packet has been received, if the packet is to read, a CRC value of the payload will be calculated and checked against the received CRC value. Then the comparison result and the payload will be passed to network layer for processing. 

#### Network layer
This module is responsible for maintaining addressing. At the receiving process, when the first 2 bytes of the payload are received from data link layer, they are passed to this layer to check if they should be read or forwarded. If the recipient of the packet is not the current device, or the packet is a broadcast message, this packet will be forwarded. If the current device is the target of this packet, or the packet is a broadcast message, this packet will be read.

When the entire payload from data link layer is passed to this layer, the program will decide on how to process this packet at transport layer. If the received CRC value of the packet does not match the calculated CRC value from the payload, this message is discarded; otherwise this module will decide on how to process this payload at transport layer, based on the sender and receiver addresses. Finally, the payload without the addresses will be passed to transport layer for further processing. 

#### Transport layer
This module is responsible for final processing. Based on the decision on network layer, if the packet is an acknowledgement packet, it indicates that the sender of the ACK message had successfully received another message from the current device. Based on the identification of the ACK message, the relevant saved message at the sent message cache will be removed. 

If the received message is a datagram or a broadcast message, the message is read and discarded. 

If the received message is of other type, the message is read and a corresponding ACK message is sent to the sender of the message. 

### Sending actions
#### Transport layer
Firstly the type, destination, and the payload message are received from user input. Based on the type of the message, if the message is a datagram, or a broadcast message, then no entry of the payload message at sent message cache will be created. For other types of messages, an entry of the payload message is created and stored to sent message cache along with the current period stamp, and the destination address. 

Then the message is passed to network layer along with the destination address and the length of the message. 

As per requirements of transport layer, in case the message is timed out (i.e. No ACK packet received for corresponding message), the message will be sent again, and the corresponding period stamp will be updated to the period during which the message is sent again. 

#### Network layer
When the message is passed to this layer, the address of the current device and the destination address will be inserted to the front of the message. After that, the concatenated message and its length will be passed to data link layer for further processing and sending. 

#### Data link layer
The message from network layer becomes the payload of the packet. Before the sending process starts, a struct of data_node is created to form the components of a packet. With the payload of the packet, the CRC of the packet is calculated. Then the CRC value and the length of the payload form the header of the packet. 

After building the packet as a form of data_node instance, the packet is pushed to the normal send queue awaiting to be sent. 
When the packet is poped from queue, the sending process is activated and the program sends the predefined premeable, header, and payload accordingly. At each timer interrupt, a bit is extracted from the packet and transferred to physical layer. 

#### Physical layer
Bit sending operation is executed here. If the program is in the progress of sending a packet, a bit will be received from data link layer. Before sending the bit through setting the output value of PB5, as per RASPNet requirements, the program will sleep for half of the length of the interrupt period. If no packet is actively being sent at this moment, 0 is sent through PB5 output. 