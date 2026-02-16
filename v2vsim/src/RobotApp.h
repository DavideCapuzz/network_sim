#pragma once

#include <omnetpp.h>
#include <inet/common/packet/Packet.h>

using namespace omnetpp;

class RobotApp : public cSimpleModule
{
protected:
    int robotId = -1;
    cMessage *pollTimer = nullptr;

    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;

    void pollExternal();
    void injectPacket(const std::vector<uint8_t>& payload);
};
