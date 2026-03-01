#include "RobotExternalMobility.h"
#include "inet/common/ModuleAccess.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

using namespace omnetpp;

namespace v2vsim {

Define_Module(RobotExternalMobility);

RobotExternalMobility::~RobotExternalMobility()
{
    cancelAndDelete(moveTimer);
    cancelAndDelete(pollTimer);
    closeSocket();
}

void RobotExternalMobility::initialize(int stage)
{
    MobilityBase::initialize(stage);

    if (stage == inet::INITSTAGE_LOCAL) {
        moveTimer = new cMessage("moveTimer");
        pollTimer = new cMessage("pollTimer");
        sockfd = -1;

        robotId = getParentModule()->getIndex();
        maxSpeed = par("maxSpeed").doubleValue();

        externalHost = par("externalHost").stdstringValue();
        externalPort = par("externalPort").intValue();
        listenPort = par("listenPort").intValue();

        // Read initial position from parameters
        double initX = par("initialX").doubleValue();
        double initY = par("initialY").doubleValue();
        double initZ = par("initialZ").doubleValue();

        // Set initial position and target
        lastPosition = inet::Coord(initX, initY, initZ);
        targetPosition = lastPosition;

        currentVelocity = inet::Coord::ZERO;
        currentAcceleration = inet::Coord::ZERO;

        currentAngularPosition = inet::Quaternion::IDENTITY;
        currentAngularVelocity = inet::Quaternion(0, 0, 0, 0);
        currentAngularAcceleration = inet::Quaternion(0, 0, 0, 0);

        lastUpdate = simTime();

        EV_INFO << "RobotExternalMobility initialized for robot " << robotId
                << " at initial position " << lastPosition << "\n";
    }
    else if (stage == inet::INITSTAGE_APPLICATION_LAYER) {
        // Set up UDP socket to receive position updates from Docker
        setupSocket();

        EV_INFO << "Robot " << robotId << " listening on UDP port " << listenPort << "\n";

        // Start timers
        scheduleAt(simTime() + 0.1, moveTimer);
        scheduleAt(simTime() + 0.05, pollTimer);
    }
}

void RobotExternalMobility::setupSocket()
{
    // Create UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        EV_ERROR << "Robot " << robotId << " failed to create socket\n";
        return;
    }

    // Set non-blocking
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

    // Bind to port
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(listenPort);

    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        EV_ERROR << "Robot " << robotId << " failed to bind to port " << listenPort << "\n";
        close(sockfd);
        sockfd = -1;
    }
}

void RobotExternalMobility::closeSocket()
{
    if (sockfd >= 0) {
        close(sockfd);
        sockfd = -1;
    }
}

void RobotExternalMobility::handleMessage(cMessage *msg)
{
    if (msg->isSelfMessage()) {
        handleSelfMessage(msg);
    }
    else {
        MobilityBase::handleMessage(msg);
    }
}

void RobotExternalMobility::handleSelfMessage(cMessage *msg)
{
    if (msg == moveTimer) {
        move();
        scheduleAt(simTime() + 0.1, moveTimer);
    }
    else if (msg == pollTimer) {
        pollExternalPosition();
        scheduleAt(simTime() + 0.05, pollTimer);
    }
}

void RobotExternalMobility::pollExternalPosition()
{
    if (sockfd < 0) return;

    // Try to receive data
    uint8_t buffer[1024];
    struct sockaddr_in from_addr;
    socklen_t from_len = sizeof(from_addr);

    ssize_t n = recvfrom(sockfd, buffer, sizeof(buffer), 0,
                         (struct sockaddr*)&from_addr, &from_len);

    if (n > 0) {
        // Print source address
        char src_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &from_addr.sin_addr, src_ip, INET_ADDRSTRLEN);
        EV_INFO << "Robot " << robotId << " received " << n
                << " bytes from " << src_ip << ":" << ntohs(from_addr.sin_port) << "\n";

        std::vector<uint8_t> data(buffer, buffer + n);
        parsePositionUpdate(data);
    }
}

void RobotExternalMobility::move()
{
    simtime_t now = simTime();
    simtime_t dt = now - lastUpdate;

    if (dt > 0) {
        // Calculate velocity towards target
        inet::Coord direction = targetPosition - lastPosition;
        double distance = direction.length();

        if (distance > 0.01) {
            // Move towards target at max speed
            direction.normalize();
            currentVelocity = direction * std::min(maxSpeed, distance / dt.dbl());

            // Update position
            lastPosition = lastPosition + currentVelocity * dt.dbl();
        }
        else {
            currentVelocity = inet::Coord::ZERO;
            lastPosition = targetPosition;
        }

        lastUpdate = now;
        emitMobilityStateChangedSignal();
    }
}

void RobotExternalMobility::updatePosition(const inet::Coord& newPosition)
{
    targetPosition = newPosition;
    EV_DEBUG << "Robot " << robotId << " received new target position: "
             << newPosition << "\n";
}

void RobotExternalMobility::parsePositionUpdate(const std::vector<uint8_t>& data)
{
    // TODO: Replace with FlatBuffers deserialization
    // For now, expect simple format: [x(4 bytes), y(4 bytes), z(4 bytes)]

    if (data.size() >= 12) {
        float x, y, z;
        memcpy(&x, &data[0], sizeof(float));
        memcpy(&y, &data[4], sizeof(float));
        memcpy(&z, &data[8], sizeof(float));

        updatePosition(inet::Coord(x, y, z));

        EV_INFO << "Robot " << robotId << " position update: ("
                << x << ", " << y << ", " << z << ")\n";
    }
    else {
        EV_WARN << "Robot " << robotId << " received malformed position update ("
                << data.size() << " bytes)\n";
    }
}

void RobotExternalMobility::finish()
{
    closeSocket();
    MobilityBase::finish();
}

} // namespace v2vsim