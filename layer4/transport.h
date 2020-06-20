
void transportCacheArrayInit();


int calculateLength(char *data);


unsigned int periodDiffCalculator(unsigned int comparator);


void periodClockUpdate();


struct transport_node* constructTransportNode(unsigned char type, unsigned char *data, unsigned char target, int length);

void updateCacheArrIndex();


void initiateSend(int address, unsigned char type, unsigned char *data, int length);

void sendACK(int address, unsigned char id);

void transportProcessing(unsigned char srcAddress, unsigned char targetAddress, int length, unsigned char *data);


void notifyFailSend(char *payload, char dest);


void notifySuccessBroadcast(int length, unsigned char *data);

