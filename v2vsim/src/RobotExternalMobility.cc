#include "RobotExternalMobility.h"

using namespace omnetpp;
using namespace inet;

namespace v2vsim {

Define_Module(RobotExternalMobility);

void RobotExternalMobility::initialize(int stage)
{
    if (stage == 0) {
        mobilityId = getId(); // OMNeT++ module ID
        maxSpeed = par("maxSpeed").doubleValue();

        currentPosition = Coord::ZERO;
        lastPosition = Coord::ZERO;
        currentVelocity = Coord::ZERO;
        currentAcceleration = Coord::ZERO;

        currentAngularPosition = Quaternion::IDENTITY;
        currentAngularVelocity = Quaternion(0, 0, 0, 0);
        currentAngularAcceleration = Quaternion(0, 0, 0, 0);

        constraintAreaMin = Coord(-1e6, -1e6, -1e6);
        constraintAreaMax = Coord( 1e6,  1e6,  1e6);

        lastUpdate = simTime();
    }
}

void RobotExternalMobility::handleMessage(cMessage *msg)
{
    simtime_t now = simTime();
    simtime_t dt = now - lastUpdate;

    if (dt > 0) {
        lastPosition = currentPosition;

        // Example placeholder motion (REPLACE with external data)
        currentVelocity = Coord(1, 0, 0);

        if (currentVelocity.length() > maxSpeed)

            currentVelocity.normalize();
        currentVelocity *= maxSpeed;

        currentPosition += currentVelocity * dt.dbl();
    }

    lastUpdate = now;
    delete msg;
}

// ================= IMobility API =================

int RobotExternalMobility::getId() const
{
    return getId();  // OMNeT++ module ID
}

double RobotExternalMobility::getMaxSpeed() const
{
    return maxSpeed;
}

const Coord& RobotExternalMobility::getCurrentPosition()
{
    return currentPosition;
}

const Coord& RobotExternalMobility::getCurrentVelocity()
{
    return currentVelocity;
}

const Coord& RobotExternalMobility::getCurrentAcceleration()
{
    return currentAcceleration;
}

const Quaternion& RobotExternalMobility::getCurrentAngularPosition()
{
    return currentAngularPosition;
}

const Quaternion& RobotExternalMobility::getCurrentAngularVelocity()
{
    return currentAngularVelocity;
}

const Quaternion& RobotExternalMobility::getCurrentAngularAcceleration()
{
    return currentAngularAcceleration;
}

const Coord& RobotExternalMobility::getConstraintAreaMin() const
{
    return constraintAreaMin;
}

const Coord& RobotExternalMobility::getConstraintAreaMax() const
{
    return constraintAreaMax;
}

} // namespace v2vsim
