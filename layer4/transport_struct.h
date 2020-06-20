/**
 * @file transport_struct.h
 * @author David Ng 550084
 * @brief This component is responsible for providing data structures on transport layer
 * @date 2020-02-20
 * @copyright Copyright (c) 2020
 * 
 */

/// This structure stores transport layer messages that have been sent by the device. 
struct transport_node
{
    unsigned int sentPeriodStamp; ///< This is the period stamp during which the message is sent. 
    int length; ///< This denotes the lengoth of the layer-4 payload in bytes. 
    unsigned char flag; ///< This denotes the flag of the message. 
    unsigned char *msg; ///< This denotes the payload messages to send. 
    unsigned char destination; ///< This denotes the address of the message receiver. 
};