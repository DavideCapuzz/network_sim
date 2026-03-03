#include "RobotApp.h"
#include <inet/common/packet/chunk/BytesChunk.h>
#include <inet/networklayer/common/L3AddressResolver.h>
#include <inet/common/ModuleAccess.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>

Define_Module(RobotApp);

RobotApp::~RobotApp()
{
    cancelAndDelete(pollExternalTimer_);
    closeExternalSocket();
}

void RobotApp::initialize(int stage)
{
    if (stage == inet::INITSTAGE_LOCAL) {
        robotId_ = getParentModule()->getIndex();

        // INET socket port (for wireless V2V between robots in OMNeT++)
        localPort_ = par("localPort").intValue();

        // External socket ports (for communication with Docker apps)
        externalListenPort_ = par("externalListenPort").intValue();
        externalSendPort_ = par("externalSendPort").intValue();
        externalHost_ = par("externalHost").stdstringValue();

        externalSockfd_ = -1;
        pollExternalTimer_ = new cMessage("pollExternal");

        EV_INFO << "RobotApp init for robot " << robotId_ << "\n";
        EV_INFO << "  Wireless V2V port: " << localPort_ << "\n";
        EV_INFO << "  External listen port: " << externalListenPort_
                << " (receives from Docker)\n";
        EV_INFO << "  External send to: " << externalHost_ << ":"
                << externalSendPort_ << " (sends to Docker)\n";
    }
    else if (stage == inet::INITSTAGE_APPLICATION_LAYER) {
        // Setup INET socket for wireless V2V communication
        socket_.setOutputGate(gate("socketOut"));
        socket_.setCallback(this);
        socket_.bind(inet::L3Address(), localPort_);
        socket_.setBroadcast(true);  // Enable broadcast

        // Setup raw socket for external Docker communication
        setupExternalSocket();

        EV_INFO << "Robot " << robotId_ << " ready - acting as gateway\n";
        EV_INFO << "  [External App] → UDP:" << externalListenPort_
                << " → [OMNeT++] → Wireless → [Other Robots]\n";

        // Start polling external socket for messages from Docker
        scheduleAt(simTime() + 0.05, pollExternalTimer_);
    }
}

void RobotApp::setupExternalSocket()
{
    // Create UDP socket for receiving messages from external Docker app
    externalSockfd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (externalSockfd_ < 0) {
        EV_ERROR << "Robot " << robotId_ << " failed to create external socket: "
                 << strerror(errno) << "\n";
        return;
    }

    // Set non-blocking mode (so polling doesn't block simulation)
    int flags = fcntl(externalSockfd_, F_GETFL, 0);
    fcntl(externalSockfd_, F_SETFL, flags | O_NONBLOCK);

    // Bind to external listen port
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;  // Listen on all interfaces
    addr.sin_port = htons(externalListenPort_);

    if (bind(externalSockfd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        EV_ERROR << "Robot " << robotId_ << " failed to bind external socket to port "
                 << externalListenPort_ << ": " << strerror(errno) << "\n";
        close(externalSockfd_);
        externalSockfd_ = -1;
        return;
    }

    EV_INFO << "Robot " << robotId_ << " external socket listening on port "
            << externalListenPort_ << "\n";
}

void RobotApp::closeExternalSocket()
{
    if (externalSockfd_ >= 0) {
        close(externalSockfd_);
        externalSockfd_ = -1;
    }
}

void RobotApp::handleMessage(cMessage *msg)
{
    if (msg == pollExternalTimer_) {
        // Poll for messages from external Docker app
        pollExternalMessages();

        // Reschedule (poll every 50ms)
        scheduleAt(simTime() + 0.05, pollExternalTimer_);
    }
    else {
        // Message from wireless (another robot in OMNeT++)
        socket_.processMessage(msg);
    }
}

void RobotApp::pollExternalMessages()
{
    if (externalSockfd_ < 0) return;

    uint8_t buffer[4096];
    struct sockaddr_in from_addr;
    socklen_t from_len = sizeof(from_addr);

    // Try to receive data (non-blocking)
    ssize_t n = recvfrom(externalSockfd_, buffer, sizeof(buffer), 0,
                         (struct sockaddr*)&from_addr, &from_len);

    if (n > 0) {
        std::vector<uint8_t> data(buffer, buffer + n);

        char src_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &from_addr.sin_addr, src_ip, INET_ADDRSTRLEN);

        EV_INFO << "Robot " << robotId_ << " received " << n
                << " bytes from external app at " << src_ip << ":"
                << ntohs(from_addr.sin_port) << "\n";

        // Handle message from external app
        handleExternalMessage(data);
    }
    else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        // Real error (not just "no data available")
        EV_WARN << "Robot " << robotId_ << " external recvfrom error: "
                << strerror(errno) << "\n";
    }
}

void RobotApp::handleExternalMessage(const std::vector<uint8_t>& data)
{
    // Expected format from external Docker app:
    // [x:4bytes][y:4bytes][z:4bytes][counter:4bytes]
    // Total: 16 bytes (3 floats + 1 uint32)

    if (data.size() < 16) {
        EV_WARN << "Robot " << robotId_ << " received malformed external message ("
                << data.size() << " bytes, expected 16)\n";
        return;
    }

    // Parse position (x, y, z) and counter from external app
    float x, y, z;
    uint32_t counter;

    memcpy(&x, &data[0], sizeof(float));
    memcpy(&y, &data[4], sizeof(float));
    memcpy(&z, &data[8], sizeof(float));
    memcpy(&counter, &data[12], sizeof(uint32_t));

    EV_INFO << "Robot " << robotId_ << " got external message:\n";
    EV_INFO << "  Position: (" << x << ", " << y << ", " << z << ")\n";
    EV_INFO << "  Counter: " << counter << "\n";

    std::cout << "[EXTERNAL→OMNET] Robot " << robotId_
              << " pos=(" << x << "," << y << "," << z << ") msg#" << counter
              << std::endl;

    // Broadcast this message via wireless to all other robots
    broadcastViaWireless(x, y, z, counter);
}

void RobotApp::broadcastViaWireless(float x, float y, float z, uint32_t counter)
{
    // Create V2V wireless message with format:
    // [robot_id:1byte][x:4bytes][y:4bytes][z:4bytes][counter:4bytes]
    // Total: 17 bytes

    std::vector<uint8_t> data;
    data.push_back(robotId_);  // Who is sending this

    // Add position (3 floats)
    uint8_t* xBytes = (uint8_t*)&x;
    uint8_t* yBytes = (uint8_t*)&y;
    uint8_t* zBytes = (uint8_t*)&z;
    data.insert(data.end(), xBytes, xBytes + sizeof(float));
    data.insert(data.end(), yBytes, yBytes + sizeof(float));
    data.insert(data.end(), zBytes, zBytes + sizeof(float));

    // Add counter (1 uint32)
    uint8_t* counterBytes = (uint8_t*)&counter;
    data.insert(data.end(), counterBytes, counterBytes + sizeof(uint32_t));

    // Create INET packet for wireless transmission
    auto packet = new inet::Packet("V2V-msg");
    auto chunk = inet::makeShared<inet::BytesChunk>();
    chunk->setBytes(data);
    packet->insertAtBack(chunk);

    // Broadcast via wireless (ALLONES_ADDRESS = 255.255.255.255)
    inet::L3Address broadcastAddr = inet::Ipv4Address::ALLONES_ADDRESS;
    socket_.setBroadcast(true);
    socket_.sendTo(packet, broadcastAddr, localPort_);

    EV_INFO << "Robot " << robotId_ << " broadcasted via WIRELESS:\n";
    EV_INFO << "  Position: (" << x << ", " << y << ", " << z << ")\n";
    EV_INFO << "  Counter: " << counter << "\n";

    std::cout << "[OMNET→WIRELESS] Robot " << robotId_
              << " broadcasting pos=(" << x << "," << y << "," << z << ") msg#" << counter
              << std::endl;
}

void RobotApp::handleV2VMessage(inet::Packet *packet)
{
    // Received wireless message from another robot in OMNeT++
    auto chunk = packet->peekDataAsBytes();
    const auto& bytes = chunk->getBytes();
    std::vector<uint8_t> data(bytes.begin(), bytes.end());

    if (data.size() < 17) {
        EV_WARN << "Robot " << robotId_ << " received malformed V2V message\n";
        return;
    }

    // Parse V2V message
    uint8_t senderId = data[0];

    if (senderId == robotId_) {
        // Ignore our own broadcast (we sent this)
        return;
    }

    // Extract position and counter
    float x, y, z;
    uint32_t counter;

    memcpy(&x, &data[1], sizeof(float));
    memcpy(&y, &data[5], sizeof(float));
    memcpy(&z, &data[9], sizeof(float));
    memcpy(&counter, &data[13], sizeof(uint32_t));

    EV_INFO << "Robot " << robotId_ << " received WIRELESS message from robot "
            << (int)senderId << ":\n";
    EV_INFO << "  Position: (" << x << ", " << y << ", " << z << ")\n";
    EV_INFO << "  Counter: " << counter << "\n";

    std::cout << "[WIRELESS→OMNET] Robot " << robotId_ << " ← Robot " << (int)senderId
              << " pos=(" << x << "," << y << "," << z << ") msg#" << counter
              << std::endl;

    // Forward to external Docker app
    forwardToExternal(senderId, x, y, z, counter);
}

void RobotApp::forwardToExternal(uint8_t senderId, float x, float y, float z, uint32_t counter)
{
    if (externalSockfd_ < 0) return;

    // Format to send to external Docker app:
    // [sender_id:1byte][x:4bytes][y:4bytes][z:4bytes][counter:4bytes]
    // Total: 17 bytes

    uint8_t buffer[17];
    buffer[0] = senderId;
    memcpy(&buffer[1], &x, sizeof(float));
    memcpy(&buffer[5], &y, sizeof(float));
    memcpy(&buffer[9], &z, sizeof(float));
    memcpy(&buffer[13], &counter, sizeof(uint32_t));

    // Send to external Docker app
    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(externalSendPort_);
    inet_pton(AF_INET, externalHost_.c_str(), &dest_addr.sin_addr);

    ssize_t sent = sendto(externalSockfd_, buffer, sizeof(buffer), 0,
                          (struct sockaddr*)&dest_addr, sizeof(dest_addr));

    if (sent < 0) {
        EV_WARN << "Robot " << robotId_ << " failed to forward to external: "
                << strerror(errno) << "\n";
    } else {
        EV_INFO << "Robot " << robotId_ << " forwarded to external app\n";
        std::cout << "[OMNET→EXTERNAL] Robot " << robotId_
                  << " forwarding Robot " << (int)senderId
                  << "'s message to Docker" << std::endl;
    }
}

// INET UdpSocket::ICallback implementation (for wireless messages)
void RobotApp::socketDataArrived(inet::UdpSocket *sock, inet::Packet *packet)
{
    std::cout << "!!! ROBOT " << robotId_ << " socketDataArrived() CALLED !!!" << std::endl;

    handleV2VMessage(packet);
    delete packet;
}

void RobotApp::socketErrorArrived(inet::UdpSocket *sock, inet::Indication *indication)
{
    EV_DEBUG << "Robot " << robotId_ << " socket error: " << indication->str() << "\n";
    delete indication;
}

void RobotApp::socketClosed(inet::UdpSocket *sock)
{
    EV_INFO << "Robot " << robotId_ << " socket closed\n";
}

void RobotApp::finish()
{
    socket_.close();
    closeExternalSocket();
    EV_INFO << "Robot " << robotId_ << " shutting down\n";
}