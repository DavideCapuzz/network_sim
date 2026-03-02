#include "RobotApp.h"
#include <inet/common/packet/chunk/BytesChunk.h>
#include <inet/networklayer/common/L3AddressResolver.h>
#include <inet/common/ModuleAccess.h>

Define_Module(RobotApp);

RobotApp::~RobotApp()
{
    cancelAndDelete(broadcastTimer_);
}

void RobotApp::initialize(int stage)
{
    if (stage == inet::INITSTAGE_LOCAL) {
        robotId_ = getParentModule()->getIndex();
        messageCounter_ = 0;
        localPort_ = par("localPort").intValue();
        remotePort_ = par("remotePort").intValue();
        broadcastTimer_ = new cMessage("broadcast");

        EV << "RobotApp INIT for robot " << robotId_ << "\n";
    }
    else if (stage == inet::INITSTAGE_APPLICATION_LAYER) {
        socket_.setOutputGate(gate("socketOut"));
        socket_.setCallback(this);
        socket_.bind(inet::L3Address(), localPort_);
        socket_.setBroadcast(true);

        EV << "Robot " << robotId_ << " SOCKET READY\n";

        // Start broadcasting
        scheduleAt(simTime() + 1.0 + robotId_ * 0.5, broadcastTimer_);
    }
}

void RobotApp::handleMessage(cMessage *msg)
{
    if (msg == broadcastTimer_) {
        EV << "*** Robot " << robotId_ << " TIMER FIRED ***\n";
        broadcastMessage();
        scheduleAt(simTime() + 2.0, broadcastTimer_);
    }
    else {
        EV << "*** Robot " << robotId_ << " RECEIVED MESSAGE: " << msg->getClassName() << " ***\n";
        socket_.processMessage(msg);
    }
}

void RobotApp::broadcastMessage()
{
    messageCounter_++;

    // Create simple message
    std::vector<uint8_t> data;
    data.push_back(robotId_);
    uint32_t counter = messageCounter_;
    data.push_back((counter >> 0) & 0xFF);
    data.push_back((counter >> 8) & 0xFF);
    data.push_back((counter >> 16) & 0xFF);
    data.push_back((counter >> 24) & 0xFF);

    auto packet = new inet::Packet("V2V-msg");
    auto chunk = inet::makeShared<inet::BytesChunk>();
    chunk->setBytes(data);
    packet->insertAtBack(chunk);

    // Try UNICAST first (more reliable for testing)
    int otherRobot = (robotId_ == 0) ? 1 : 0;
    char ipStr[20];
    sprintf(ipStr, "10.0.0.%d", otherRobot + 1);
    // inet::L3Address destAddr = inet::Ipv4Address("10.0.0.255");
    inet::L3Address destAddr = inet::Ipv4Address::ALLONES_ADDRESS;

    EV << "Robot " << robotId_ << " SENDING message #" << messageCounter_ <<
        "  Destination: " << destAddr << ":" << localPort_ <<
        "  Packet size: " << packet->getByteLength() << " bytes\n";
    socket_.setBroadcast(true);
    socket_.sendTo(packet, destAddr, localPort_);
}

void RobotApp::handleV2VMessage(inet::Packet *packet)
{
    auto chunk = packet->peekDataAsBytes();
    const auto& bytes = chunk->getBytes();
    std::vector<uint8_t> data(bytes.begin(), bytes.end());

    if (data.size() >= 5) {
        uint8_t senderId = data[0];
        uint32_t counter = data[1] | (data[2] << 8) | (data[3] << 16) | (data[4] << 24);

        if (senderId != robotId_) {
            EV << "SUCCESS! Robot " << robotId_ << " received message!" <<
                "  From: Robot " << (int)senderId <<
                "  Counter: " << counter << "\n";

            std::cout << "✓✓✓ Robot " << robotId_ << " ← Robot " << (int)senderId
                      << " (msg #" << counter << ") ✓✓✓" << std::endl;
        }
    }
}

void RobotApp::socketDataArrived(inet::UdpSocket *sock, inet::Packet *packet)
{
    // EV << "========================================\n";
    // EV << "socketDataArrived() CALLED for Robot " << robotId_ << "\n";
    // EV << "  Packet: " << packet->getName() << "\n";
    // EV << "  Size: " << packet->getByteLength() << " bytes\n";
    // EV << "========================================\n";

    std::cout << "!!! ROBOT " << robotId_ << " socketDataArrived() CALLED !!!" << std::endl;

    handleV2VMessage(packet);
    delete packet;
}

void RobotApp::socketErrorArrived(inet::UdpSocket *sock, inet::Indication *indication)
{
    EV << "========================================\n";
    EV << "socketErrorArrived() for Robot " << robotId_ << "\n";
    EV << "  Error: " << indication->str() << "\n";
    EV << "========================================\n";
    delete indication;
}

void RobotApp::socketClosed(inet::UdpSocket *sock)
{
    EV << "socketClosed() for Robot " << robotId_ << "\n";
}

void RobotApp::finish()
{
    socket_.close();
    EV << "Robot " << robotId_ << " sent " << messageCounter_ << " messages\n";
}
