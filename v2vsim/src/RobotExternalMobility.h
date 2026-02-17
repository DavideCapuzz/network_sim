#pragma once

#include <omnetpp.h>
#include "inet/mobility/base/MobilityBase.h"
#include "inet/common/geometry/common/Coord.h"
#include "inet/common/geometry/common/Quaternion.h"
#include <vector>

using namespace omnetpp;

namespace v2vsim {

/**
 * External mobility module that receives position updates from Docker containers
 * Uses a simple polling mechanism with file-based or shared memory communication
 */
class RobotExternalMobility : public inet::MobilityBase
{
protected:
    // External communication settings
    std::string externalHost;
    int externalPort;
    int listenPort;

    // Motion state
    inet::Coord targetPosition;
    inet::Coord currentVelocity;
    inet::Coord currentAcceleration;

    inet::Quaternion currentAngularPosition;
    inet::Quaternion currentAngularVelocity;
    inet::Quaternion currentAngularAcceleration;

    double maxSpeed;
    int robotId;

    simtime_t lastUpdate;
    cMessage *moveTimer;
    cMessage *pollTimer;

    // Socket file descriptor for UDP (using raw sockets)
    int sockfd;

protected:
    virtual int numInitStages() const override { return inet::NUM_INIT_STAGES; }
    virtual void initialize(int stage) override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;

    // Required by MobilityBase
    virtual void handleSelfMessage(cMessage *msg) override;

    // Update position from external data
    void updatePosition(const inet::Coord& newPosition);

    // Move towards target position (from MobilityBase)
    virtual void move();

    // Poll for position updates from external source
    void pollExternalPosition();

    // Parse incoming position update (placeholder for FlatBuffers)
    void parsePositionUpdate(const std::vector<uint8_t>& data);

    // Setup UDP socket
    void setupSocket();
    void closeSocket();

public:
    virtual ~RobotExternalMobility();

    // IMobility interface (required pure virtual methods)
    virtual double getMaxSpeed() const override { return maxSpeed; }
    virtual const inet::Coord& getCurrentPosition() override { return lastPosition; }
    virtual const inet::Coord& getCurrentVelocity() override { return currentVelocity; }
    virtual const inet::Coord& getCurrentAcceleration() override { return currentAcceleration; }
    virtual const inet::Quaternion& getCurrentAngularPosition() override { return currentAngularPosition; }
    virtual const inet::Quaternion& getCurrentAngularVelocity() override { return currentAngularVelocity; }
    virtual const inet::Quaternion& getCurrentAngularAcceleration() override { return currentAngularAcceleration; }
};

} // namespace v2vsim