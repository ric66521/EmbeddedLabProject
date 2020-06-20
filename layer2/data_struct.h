//! This structure is used as a temporary buffer for bit receiving before the bits are written to data_node instance (i.e. The packet). 
/**
 * This struct stores temporarily the bits received before a byte is accumulated and further processed. <br>
 * An array is provided for the temporary buffer, because the program needs to cater for an extreme condition that, it may be impossible to write the 8 bits to the data_node struct, before receiving a new bit or even byte from the neighbouring node.
*/
struct receive_buffer
{
    unsigned char *buffer; ///< This is an array of temporary byte buffer for storing received bits before a complete byte is received. 
    unsigned char receiveBitIndex; ///< This denotes the index of the incoming bit. 
    unsigned char receiveByteIndex; ///< This denotes the index on the location of the currently using byte buffer in array. 
    unsigned char writeByteIndex; ///< This denotes which bytes in the buffer array is ready to be processed. 
    unsigned char writeToStructFlag; ///< This flag represents that a byte is ready to be processed. 
};

//! This structure is used for controlling the flow of the receiving or sending process. 
/**
 * This structure stores control data for the purpose of controlling sending and receiving processes. <br>
 * for receivng: type 0 is header, type 1 is payload. <br>
 * for sending: type 0 is premeable, type 1 is header, type 2 is payload. <br>
 * premeableRead is used to store the last 8 bits received when the device is not receiving a packet. <br>
 * premeableRead is used to compare with the pattern of premeable at every pinInterrupt, whenever a premeable is detected, premeableRead is reset. <br>
 * active denotes whether the sending or receiving process is active.
*/
struct comm_control 
{
    int active; ///< This denotes whether sending or receiving is active. 
    int type; ///< This denotes which part of the message is being read or sent. 
    int index; ///< This denotes, for receiving the byte index of incoming byte, or for sending the bit index of the next bit to send. 
    unsigned char premeableRead; ///< This is only for managing receiving process. This is the buffer for storing read bits at premeable detection when receiving procedure is not activated. 
};

//! This structure represents a data link level packet and acts as a node in a linked list at the send queue. 
/**
 * 
 */
struct data_node
{
    struct data_node* next; ///< This is the pointer to the next node in the queue. 
    unsigned char *header; ///< This is the header of the packet. 
    unsigned char *payload; ///< This is the payload of the packet. 
    unsigned char length; ///< This is the length of the payload in byte. 
    int datalock; ///< This is the mutex lock. 
    int toRead; ///< This is the flag on whether this packet should be read after receiving this packet in its entirety. 
    int sendBackOff; ///< This denotes whether a send method has failed to get the mutex. 
    int writeBackOff; ///< This denotes whether a receive method has failed to get the mutex. 
};



struct data_node* popSendQueue();



void pushSendQueue(struct data_node *node);


void jumpSendQueue(struct data_node *node);
