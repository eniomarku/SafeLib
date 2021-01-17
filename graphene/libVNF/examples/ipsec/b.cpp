#include <iostream>
#include <libvnf/core.hpp>
#include <netdb.h>
#include <netinet/in.h>

#include <unistd.h>

using namespace vnf;

struct BState
{
    char *req;
    void *dsreq;
    int val;
    int aCoreId;
    int aSocketId;
};

static pthread_mutex_t log_mutex;

string mmeIp;
int mmePort;
string neighbour1Ip;
int neighbour1Port;

int getKeyId(int coreId, int socketId)
{
    int keyId;
    if (coreId >= 1 && coreId <= 7)
    {
        keyId = coreId * 100000 + socketId;
    }
    else
    {
        keyId = 800000 + socketId;
    }
    return keyId;
}

void onPacketReceivedFromA(ConnId &aConnId, int reqObjId, void *requestObject, char *packet, int packetLen, int errCode, int streamNum)
{
    // cout << "\n\nonPacketReceivedFromA function" << endl;
    pthread_mutex_lock(&log_mutex);
    printf("%.*s", packetLen, packet);
    pthread_mutex_unlock(&log_mutex);

    // char value[] = "Dear A, thank you for your message, Lots of love.";
    // int valueLen = strlen(value);

    char value[1024] = {0,};
    memcpy(value, packet, packetLen);
    int valueLen = packetLen;  

    // printf("packet sent: %.*s\n", valueLen, value);
    // allocate request object and bind it to A's connection
    requestObject = aConnId.allocReqObj(1);
 
    BState *state = static_cast<BState *>(requestObject);
    int val = aConnId.socketId;
   for (int i = 1; i < 20000000; i++)
   {
     val = val + i;
   }
    state->val = val;
    state->req = (char *)aConnId.setPktDNE((void *)packet);
    state->aSocketId = aConnId.socketId;
    state->aCoreId = aConnId.coreId;

    // store key value pair in data store
    int keyId = getKeyId(aConnId.coreId, aConnId.socketId);
    
    aConnId.storeData("", keyId, LOCAL, (void *)value, valueLen, NULL);
    aConnId.unsetPktDNE((void *)state->req);
    char *buffer = aConnId.getPktBuf();

      memcpy((void *)buffer, value, valueLen);
      aConnId.sendData(buffer, valueLen);

  //    printf("packet sent: %.*s\n", valueLen, value);

  //    aConnId.freeReqObj(1).delData("", keyId, LOCAL).closeConn();
}

int main(int argc, char *argv[])
{
    pthread_mutex_init(&log_mutex, NULL);

    // init libvnf
    vector<int> dataStorePorts;
    dataStorePorts.push_back(7000);
    dataStorePorts.push_back(7001);
    dataStorePorts.push_back(7002);
    dataStorePorts.push_back(7003);
    initLibvnf(4, 128, "127.0.0.1", dataStorePorts, 131072, false);

    printf("RIDER REPORT: AAAAAAA\n");
    fflush(stdout);
    // init network parameters
    mmeIp = argv[1];
    mmePort = atoi(argv[2]);

    printf("RIDER REPORT: BBBBBBB\n");
    fflush(stdout);

    // create server
    ConnId serverId = initServer("", mmeIp, mmePort, "tcp");

    printf("RIDER REPORT: CCCCCCCC\n");
    fflush(stdout);
    // ConnId serverId1 = initServer("", mmeIp, 6000, "tcp");

    // register callback to handle packets coming from A
    registerCallback(serverId, READ, onPacketReceivedFromA);    
    // registerCallback(serverId1, READ, onPacketReceivedFromA);

    // request object declaration
    int requestObjectSizes[1] = {sizeof(struct BState)};
    initReqPool(requestObjectSizes, 1);

    // start vnf
    startEventLoop();


    return 0;
}
