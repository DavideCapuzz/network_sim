#include "RobotApp.h"
#include "inet/common/ModuleAccess.h"
#include "inet/common/packet/Packet.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include "inet/common/packet/chunk/BytesChunk.h"
#include "inet/mobility/contract/IMobility.h"
#include "inet/common/geometry/common/Coord.h"
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

Define_Module(RobotApp);

RobotApp::RobotApp()
{
    sendTimer = nullptr;
    externalSock = -1;
}

RobotApp::~RobotApp()
{
    cancelAndDelete(sendTimer);
    if (externalSock >= 0) {
        close(externalSock);
    }
}

void RobotApp::initialize(int stage)
{
    ApplicationBase::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {
        localPort = par("localPort");
        destPort = par("destPort");
        externalHost = par("externalHost");
        externalPort = par("externalPort");

        robotId = getParentModule()->getFullName();

        sendTimer = new cMessage("sendTimer");
    }
    else if (stage == INITSTAGE_APPLICATION_LAYER) {
        // Setup robot-to-robot socket (INET 4.5.2 API)
        robotSocket.setOutputGate(gate("socketOut"));
        robotSocket.bind(localPort);

        // Setup callback for received packets
        robotSocket.setCallback(this);

        // Setup external app socket
        setupExternalSocket();

        // Start periodic tasks
        scheduleAt(simTime() + 1.0, sendTimer);

        EV_INFO << robotId << " initialized" << endl;
    }
}

void RobotApp::setupExternalSocket()
{
    externalSock = socket(AF_INET, SOCK_DGRAM, 0);
    if (externalSock < 0) {
        EV_ERROR << "Failed to create external socket" << endl;
        return;
    }

    // Set non-blocking
    fcntl(externalSock, F_SETFL, O_NONBLOCK);

    memset(&externalAddr, 0, sizeof(externalAddr));
    externalAddr.sin_family = AF_INET;
    externalAddr.sin_port = htons(externalPort);
    inet_pton(AF_INET, externalHost, &externalAddr.sin_addr);

    EV_INFO << "External socket connected to " << externalHost
            << ":" << externalPort << endl;
}

void RobotApp::handleMessageWhenUp(cMessage *msg)
{
    if (msg->isSelfMessage()) {
        // Periodic check for messages from external app
        receiveFromExternalApp();

        // Periodically send our GPS position to other robots
        sendGPSToRobots();

        // Reschedule
        scheduleAt(simTime() + 0.1, msg);
    }
    else {
        // Message received from another robot via wireless (INET 4.5.2)
        Packet *pkt = check_and_cast<Packet *>(msg);

        // Extract GPS data from packet
        const auto& payload = pkt->peekData<BytesChunk>();

        if (payload != nullptr) {
            std::vector<uint8_t> bytes = payload->getBytes();
            std::string data(bytes.begin(), bytes.end());

            EV_INFO << robotId << " received GPS from another robot: " << data << endl;

            // Forward to external app
            char buffer[1024];
            snprintf(buffer, sizeof(buffer), "GPS_RECEIVED:%s:%s",
                     robotId.c_str(), data.c_str());
            sendToExternalApp(buffer);
        }

        delete pkt;
    }
}

void RobotApp::sendGPSToRobots()
{
    // Get current position from mobility module (INET 4.5.2 API)
    auto mobility = check_and_cast<IMobility*>(
        getParentModule()->getSubmodule("mobility"));

    if (!mobility) return;

    Coord pos = mobility->getCurrentPosition();

    // Create GPS message
    char gpsData[256];
    snprintf(gpsData, sizeof(gpsData), "GPS:%s:%.6f,%.6f,%.2f",
             robotId.c_str(), pos.x, pos.y, pos.z);

    // Create packet with BytesChunk (INET 4.5.2)
    auto pkt = new Packet("GPSBroadcast");

    std::vector<uint8_t> bytes(gpsData, gpsData + strlen(gpsData));
    auto payload = makeShared<BytesChunk>(bytes);
    pkt->insertAtBack(payload);

    // Broadcast to all robots
    robotSocket.sendTo(pkt, Ipv4Address::ALLONES_ADDRESS, destPort);
}

void RobotApp::sendToExternalApp(const char* message)
{
    sendto(externalSock, message, strlen(message), 0,
           (struct sockaddr*)&externalAddr, sizeof(externalAddr));
}

void RobotApp::receiveFromExternalApp()
{
    char buffer[4096];
    ssize_t recv_len = recvfrom(externalSock, buffer, sizeof(buffer) - 1,
                               MSG_DONTWAIT, nullptr, nullptr);

    if (recv_len > 0) {
        buffer[recv_len] = '\0';

        EV_INFO << robotId << " received from external: " << buffer << endl;

        // Parse GPS position update: UPDATE:robot[0]:lat,lon,alt:x,y,z
        if (strncmp(buffer, "UPDATE:", 7) == 0) {
            char rid[32];
            double lat, lon, alt, x, y, z;

            if (sscanf(buffer, "UPDATE:%31[^:]:%lf,%lf,%lf:%lf,%lf,%lf",
                      rid, &lat, &lon, &alt, &x, &y, &z) == 7) {

                // Check if this update is for us
                if (strcmp(rid, robotId.c_str()) == 0) {
                    // Update our mobility position
                    auto mobility = check_and_cast<IMobility*>(
                        getParentModule()->getSubmodule("mobility"));

                    if (mobility) {
                        // For INET 4.5.2, you need ExternalMobility or similar
                        // that supports setPosition() - StationaryMobility won't work

                        EV_INFO << robotId << " GPS update received: "
                               << "(" << lat << ", " << lon << ") -> "
                               << "(" << x << ", " << y << ")" << endl;

                        // Note: Position update happens in ExternalMobility module
                        // via its own UDP socket (see ExternalMobility from earlier)
                    }
                }
            }
        }
        // Parse message send command: SEND:target:message
        else if (strncmp(buffer, "SEND:", 5) == 0) {
            char target[32], message[1024];
            if (sscanf(buffer, "SEND:%31[^:]:%1023[^\n]", target, message) == 2) {

                // Create packet (INET 4.5.2 API)
                auto pkt = new Packet("RobotMessage");

                std::vector<uint8_t> bytes(message, message + strlen(message));
                auto payload = makeShared<BytesChunk>(bytes);
                pkt->insertAtBack(payload);

                // Determine destination
                L3Address destAddr;
                if (strcmp(target, "broadcast") == 0) {
                    destAddr = Ipv4Address::ALLONES_ADDRESS;
                } else {
                    // Resolve robot name to IP (INET 4.5.2)
                    L3AddressResolver resolver;
                    destAddr = resolver.resolve(target);
                }

                // Send via wireless
                robotSocket.sendTo(pkt, destAddr, destPort);

                EV_INFO << robotId << " sent wireless message to "
                        << target << ": " << message << endl;
            }
        }
    }
}

void RobotApp::broadcastToRobots(const char* message)
{
    // INET 4.5.2 API
    auto pkt = new Packet("Broadcast");

    std::vector<uint8_t> bytes(message, message + strlen(message));
    auto payload = makeShared<BytesChunk>(bytes);
    pkt->insertAtBack(payload);

    robotSocket.sendTo(pkt, Ipv4Address::ALLONES_ADDRESS, destPort);
}

void RobotApp::finish()
{
    ApplicationBase::finish();
}