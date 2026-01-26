#ifndef __SIMPLEROBOT_APP_H
#define __SIMPLEROBOT_APP_H

#include <omnetpp.h>
#include "inet/applications/base/ApplicationBase.h"
#include "inet/transportlayer/contract/udp/UdpSocket.h"
#include "inet/networklayer/common/L3Address.h"
#include "inet/common/packet/Packet.h"
#include "inet/common/packet/chunk/BytesChunk.h"
#include <sys/socket.h>
#include <netinet/in.h>

using namespace omnetpp;
using namespace inet;

class RobotApp : public ApplicationBase
{
private:
    UdpSocket robotSocket;  // For robot-to-robot communication

    // External app communication
    int externalSock;
    struct sockaddr_in externalAddr;
    const char* externalHost;
    int externalPort;

    int localPort;
    int destPort;

    cMessage *sendTimer;
    std::string robotId;

protected:
    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    virtual void initialize(int stage) override;
    virtual void handleMessageWhenUp(cMessage *msg) override;
    virtual void finish() override;

    void setupExternalSocket();
    void sendToExternalApp(const char* message);
    void receiveFromExternalApp();
    void broadcastToRobots(const char* message);
    void sendGPSToRobots();

public:
    RobotApp();
    virtual ~RobotApp();
};

#endif