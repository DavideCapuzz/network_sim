#include "RobotApp.h"
#include <inet/common/packet/chunk/BytesChunk.h>

Define_Module(RobotApp);

void RobotApp::initialize()
{
    robotId = par("robotId");
    pollTimer = new cMessage("pollExternal");
    scheduleAt(simTime(), pollTimer);
}

void RobotApp::handleMessage(cMessage *msg)
{
    if (msg == pollTimer) {
        pollExternal();
        scheduleAt(simTime() + SimTime(1, SIMTIME_MS), pollTimer);
        return;
    }

    auto pkt = check_and_cast<inet::Packet *>(msg);

    EV_INFO << "Robot " << robotId << " received packet of "
            << pkt->getByteLength() << " bytes\n";

    delete pkt;
}

void RobotApp::pollExternal()
{
    // TODO: replace with real FlatBuffers + UDP
    // Dummy packet injection for testing

    std::vector<uint8_t> dummyPayload = {0x01, 0x02, 0x03};

    injectPacket(dummyPayload);
}

void RobotApp::injectPacket(const std::vector<uint8_t>& payload)
{
    auto packet = new inet::Packet("v2v");

    auto chunk = inet::makeShared<inet::BytesChunk>(
        payload.data(), payload.size());

    packet->insertAtBack(chunk);

    send(packet, "lowerOut");
}
