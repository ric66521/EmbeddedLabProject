
struct data_node* dataNodeConstructor(unsigned char length, unsigned char *payload);

void prepareDataNodeForSending(unsigned char length, unsigned char *payload);

void clockTickSendDecisionMaker();

void detectPremeable(unsigned char bit);

void sendWrapUp();

void prepareSendBit();

void getMutex(unsigned char receiving, struct data_node *node);

void writeBitToBuffer(unsigned char bit);

void writeByteToStruct();

void sendBitManagement();

void receiveWrapUp();

void receiveByteManagement();

int receiveByte(unsigned char byte);