#include "RobotApp.h"
#include <inet/common/packet/chunk/BytesChunk.h>
#include <inet/networklayer/common/L3AddressResolver.h>
#include <inet/common/ModuleAccess.h>

Define_Module(RobotApp);

RobotApp::~RobotApp()
{
    cancelAndDelete(pollTimer);
    cancelAndDelete(heartbeatTimer);
}

void RobotApp::initialize(int stage)
{
    if (stage == inet::INITSTAGE_LOCAL) {
        robotId = getParentModule()->getIndex();

        externalHost = par("externalHost").stdstringValue();
        externalPort = par("externalPort").intValue();
        localPort = par("localPort").intValue();

        pollTimer = new cMessage("pollExternal");
        heartbeatTimer = new cMessage("heartbeat");

        EV_INFO << "RobotApp initialized for robot " << robotId << "\n";
    }
    else if (stage == inet::INITSTAGE_APPLICATION_LAYER) {
        // Set up socket using proper INET interface
        socket.setOutputGate(gate("socketOut"));
        socket.setCallback(this);
        socket.bind(localPort);

        EV_INFO << "Robot " << robotId << " socket bound to port " << localPort << "\n";

        // Start timers
        scheduleAt(simTime() + uniform(0, 0.1), pollTimer);
        scheduleAt(simTime() + 1.0, heartbeatTimer);
    }
}

void RobotApp::handleMessage(cMessage *msg)
{
    if (msg == pollTimer) {
        pollExternal();
        scheduleAt(simTime() + 0.1, pollTimer);  // Poll every 100ms
    }
    else if (msg == heartbeatTimer) {
        // Send heartbeat to external container
        std::vector<uint8_t> heartbeat = {0xFF, 0x00, (uint8_t)robotId};
        sendToExternal(heartbeat);
        scheduleAt(simTime() + 1.0, heartbeatTimer);
    }
    else {
        // Let socket handle incoming packets
        socket.processMessage(msg);
    }
}

void RobotApp::pollExternal()
{
    // Request data from external Docker container
    // Format: [MSG_TYPE=0x01, ROBOT_ID]
    std::vector<uint8_t> request = {0x01, (uint8_t)robotId};
    sendToExternal(request);
}

void RobotApp::sendToExternal(const std::vector<uint8_t>& payload)
{
    auto packet = new inet::Packet("external");

    auto chunk = inet::makeShared<inet::BytesChunk>(payload.data(), payload.size());
    packet->insertAtBack(chunk);

    inet::L3Address destAddr = inet::L3AddressResolver().resolve(externalHost.c_str());
    socket.sendTo(packet, destAddr, externalPort);

    EV_DEBUG << "Robot " << robotId << " sent " << payload.size()
             << " bytes to external " << externalHost << ":" << externalPort << "\n";
}

void RobotApp::handleExternalMessage(inet::Packet *packet)
{
    auto chunk = packet->peekDataAsBytes();
    std::vector<uint8_t> data(chunk->getBytes().begin(), chunk->getBytes().end());

    EV_INFO << "Robot " << robotId << " received external message: "
            << packet->getByteLength() << " bytes\n";

    if (data.size() > 0) {
        uint8_t msgType = data[0];

        switch (msgType) {
            case 0x02: // V2V message to broadcast
                if (data.size() > 1) {
                    std::vector<uint8_t> payload(data.begin() + 1, data.end());
                    broadcastV2V(payload);
                }
                break;

            case 0x03: // Status update
                EV_INFO << "Robot " << robotId << " status update received\n";
                break;

            default:
                EV_WARN << "Robot " << robotId << " unknown message type: "
                        << (int)msgType << "\n";
                break;
        }
    }
}

void RobotApp::handleV2VMessage(inet::Packet *packet)
{
    auto chunk = packet->peekDataAsBytes();
    std::vector<uint8_t> data(chunk->getBytes().begin(), chunk->getBytes().end());

    EV_INFO << "Robot " << robotId << " received V2V message: "
            << packet->getByteLength() << " bytes from another robot\n";

    // Forward to external Docker container
    // Format: [MSG_TYPE=0x04, ...V2V_DATA...]
    std::vector<uint8_t> forwarded;
    forwarded.push_back(0x04);
    forwarded.insert(forwarded.end(), data.begin(), data.end());

    sendToExternal(forwarded);
}

void RobotApp::broadcastV2V(const std::vector<uint8_t>& payload)
{
    auto packet = new inet::Packet("v2v");

    auto chunk = inet::makeShared<inet::BytesChunk>(payload.data(), payload.size());
    packet->insertAtBack(chunk);

    // Broadcast to all robots via multicast
    inet::L3Address destAddr = inet::L3Address(inet::Ipv4Address("255.255.255.255"));
    socket.sendTo(packet, destAddr, localPort);

    EV_INFO << "Robot " << robotId << " broadcasted V2V message: "
            << payload.size() << " bytes\n";
}

// UdpSocket::ICallback interface implementation
void RobotApp::socketDataArrived(inet::UdpSocket *sock, inet::Packet *packet)
{
    // Determine message source and handle accordingly
    // For simplicity, treat all incoming as external messages
    // In production, you'd check source address
    handleExternalMessage(packet);
    delete packet;
}

void RobotApp::socketErrorArrived(inet::UdpSocket *sock, inet::Indication *indication)
{
    EV_WARN << "Robot " << robotId << " socket error\n";
    delete indication;
}

void RobotApp::socketClosed(inet::UdpSocket *sock)
{
    EV_INFO << "Robot " << robotId << " socket closed\n";
}

void RobotApp::finish()
{
    socket.close();
}