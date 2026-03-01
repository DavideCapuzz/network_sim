#pragma once

#include <omnetpp.h>
#include <inet/common/packet/Packet.h>
#include <inet/transportlayer/contract/udp/UdpSocket.h>
#include <inet/networklayer/common/L3Address.h>
#include <vector>

using namespace omnetpp;

/**
 * Robot application that sends/receives messages to/from external Docker containers
 * and handles V2V communication within the simulation
 */
class RobotApp : public cSimpleModule, public inet::UdpSocket::ICallback
{
protected:
    // Robot identification
    int robotId = -1;

    // UDP sockets
    inet::UdpSocket socket;  // Combined socket for all communication

    // External Docker settings
    std::string externalHost;
    int externalPort;
    int localPort;

    // Timers
    cMessage *pollTimer = nullptr;
    cMessage *heartbeatTimer = nullptr;

protected:
    virtual void initialize(int stage) override;
    virtual int numInitStages() const override { return inet::NUM_INIT_STAGES; }
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;

    // External communication
    void pollExternal();
    void sendToExternal(const std::vector<uint8_t>& payload);
    void handleExternalMessage(inet::Packet *packet);

    // V2V communication
    void handleV2VMessage(inet::Packet *packet);
    void broadcastV2V(const std::vector<uint8_t>& payload);

    // UdpSocket::ICallback interface
    virtual void socketDataArrived(inet::UdpSocket *socket, inet::Packet *packet) override;
    virtual void socketErrorArrived(inet::UdpSocket *socket, inet::Indication *indication) override;
    virtual void socketClosed(inet::UdpSocket *socket) override;

public:
    virtual ~RobotApp();
};