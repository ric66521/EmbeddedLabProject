

void prepareDataSend(int dest, int length, unsigned char *dataArr);



void networkDataProcessing(struct data_node *data, int crcMatched);


void checkIfNeedForwardOrRead( unsigned char *payload);