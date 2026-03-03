#pragma once

#include <omnetpp.h>
#include <inet/common/packet/Packet.h>
#include <inet/transportlayer/contract/udp/UdpSocket.h>
#include <inet/networklayer/common/L3Address.h>
#include <vector>
#include <string>

using namespace omnetpp;

/**
 * Gateway RobotApp - Bridges external Docker apps and OMNeT++ wireless simulation
 *
 * Message Flow:
 * 1. External App → UDP (externalListenPort) → RobotApp
 * 2. RobotApp → Wireless Broadcast → Other Robots in OMNeT++
 * 3. Other Robots → Wireless → RobotApp
 * 4. RobotApp → UDP (externalSendPort) → External App
 *
 * Message Format (Position + Counter):
 * External:  [x:4][y:4][z:4][counter:4] = 16 bytes
 * Wireless:  [id:1][x:4][y:4][z:4][counter:4] = 17 bytes
 */
class RobotApp : public cSimpleModule, public inet::UdpSocket::ICallback
{
protected:
    // Robot identification
    int robotId_ = -1;

    // INET socket for wireless V2V communication (between robots in OMNeT++)
    inet::UdpSocket socket_;
    int localPort_;  // Wireless V2V port (same for all: 5000)

    // Raw UDP socket for external Docker communication
    int externalSockfd_;
    int externalListenPort_;  // Port to receive FROM Docker (5000 + robotId)
    int externalSendPort_;    // Port to send TO Docker (9000 + robotId)
    std::string externalHost_;  // Docker host IP

    // Timer for polling external socket
    cMessage *pollExternalTimer_ = nullptr;

protected:
    virtual void initialize(int stage) override;
    virtual int numInitStages() const override { return inet::NUM_INIT_STAGES; }
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;

    // External socket management
    void setupExternalSocket();
    void closeExternalSocket();
    void pollExternalMessages();

    // Message handling
    void handleExternalMessage(const std::vector<uint8_t>& data);
    void handleV2VMessage(inet::Packet *packet);

    // Message forwarding
    void broadcastViaWireless(float x, float y, float z, uint32_t counter);
    void forwardToExternal(uint8_t senderId, float x, float y, float z, uint32_t counter);

    // INET UdpSocket::ICallback interface (for wireless messages)
    virtual void socketDataArrived(inet::UdpSocket *socket, inet::Packet *packet) override;
    virtual void socketErrorArrived(inet::UdpSocket *socket, inet::Indication *indication) override;
    virtual void socketClosed(inet::UdpSocket *socket) override;

public:
    virtual ~RobotApp();
};
