# OMNeT++ INET Multi-Robot Simulation - Complete Setup Guide

## Table of Contents
1. [Overview](#overview)
2. [Project Structure](#project-structure)
3. [File-by-File Explanation](#file-by-file-explanation)
4. [Communication Flow](#communication-flow)
5. [Setup Instructions](#setup-instructions)
6. [Running the Simulation](#running-the-simulation)

---

## Overview

This is a complete OMNeT++ simulation setup that allows multiple robots to:
- Receive their positions from an **external simulator** (like SUMO, Gazebo, ROS, or custom Python scripts)
- Communicate with each other using **custom messages** over wireless networks
- Simulate realistic **IEEE 802.11 wireless communication** using INET framework

**Key Innovation**: Robot positions don't come from OMNeT++ mobility models—they come from YOUR external simulator via network sockets.

---

## Project Structure

```
multirobot/                          # Main project directory
│
├── simulations/                     # Simulation scenarios
│   ├── MultiRobotNetwork.ned        # Network topology definition
│   └── omnetpp.ini                  # Simulation configuration
│
├── nodes/                           # Robot node definitions
│   └── RobotNode.ned                # Individual robot module
│
├── mobility/                        # Position management
│   ├── ExternalMobility.ned         # Mobility module interface
│   ├── ExternalMobility.h           # C++ header
│   └── ExternalMobility.cc          # C++ implementation
│
├── applications/                    # Robot applications
│   ├── RobotApplication.ned         # Application interface
│   ├── RobotApplication.h           # C++ header
│   ├── RobotApplication.cc          # C++ implementation
│   └── RobotMessage.msg             # Message definition
│
└── external/                        # External simulator
    └── external_simulator.py        # Python position provider
```

---

## File-by-File Explanation

### 1. **MultiRobotNetwork.ned** (Network Definition)

**Purpose**: Defines the overall simulation network topology.

**What it contains**:
```ned
network MultiRobotNetwork {
    parameters:
        int numRobots = default(3);  // How many robots to simulate
    
    submodules:
        visualizer: IntegratedCanvasVisualizer { }     // GUI visualization
        configurator: Ipv4NetworkConfigurator { }      // IP address assignment
        radioMedium: Ieee80211ScalarRadioMedium { }    // Wireless medium simulation
        physicalEnvironment: PhysicalEnvironment { }   // Physical world model
        robot[numRobots]: RobotNode { }                // Array of robot nodes
}
```

**Key Points**:
- `numRobots`: Parameter you can change to add more robots
- `visualizer`: Shows robots moving and communicating in GUI
- `radioMedium`: Simulates how radio waves propagate between robots
- `configurator`: Automatically assigns IP addresses (10.0.0.1, 10.0.0.2, etc.)
- `robot[numRobots]`: Creates an array of identical robot nodes

**Think of it as**: The "world" where your robots live—it sets up the wireless environment and creates the robots.

---

### 2. **RobotNode.ned** (Individual Robot Definition)

**Purpose**: Defines what each robot contains.

**What it contains**:
```ned
module RobotNode extends AdhocHost {
    parameters:
        mobility.typename = "ExternalMobility";  // Use our custom mobility!
        
        // Wireless interface settings
        numWlanInterfaces = 1;                   // One WiFi interface
        wlan[0].radio.transmitter.power = 2mW;   // Transmission power
        wlan[0].radio.receiver.sensitivity = -85dBm;  // Minimum signal strength
        
    submodules:
        robotApp: RobotApplication { }           // Our custom application
        externalInterface: ExternalInterface { } // Interface to outside world
}
```

**Key Points**:
- `extends AdhocHost`: Inherits all WiFi networking capabilities from INET
- `ExternalMobility`: This is OUR custom module that gets positions externally
- `wlan[0]`: First (and only) wireless interface configuration
- `robotApp`: Our custom application that sends/receives messages
- Power and sensitivity determine communication range

**Think of it as**: The blueprint for each individual robot—what hardware and software it has.

---

### 3. **ExternalMobility.ned** (Mobility Module Interface)

**Purpose**: Declares the parameters for the external mobility module.

**What it contains**:
```ned
simple ExternalMobility extends MovingMobilityBase {
    parameters:
        string externalHost = default("localhost");  // Where external sim runs
        int externalPort = default(9999);            // UDP port number
        double updateInterval @unit(s) = default(0.1s);  // How often to update
        
        // Starting position if external sim not ready
        double initialX @unit(m) = default(0m);
        double initialY @unit(m) = default(0m);
        double initialZ @unit(m) = default(0m);
}
```

**Key Points**:
- `externalHost`: IP address of computer running external simulator
- `externalPort`: UDP port for communication
- `updateInterval`: How frequently to request position updates (100ms = 10 Hz)
- `initialX/Y/Z`: Fallback position if connection fails

**Think of it as**: The configuration settings for how robots talk to the external position provider.

---

### 4. **ExternalMobility.h & .cc** (Mobility Implementation)

**Purpose**: The actual C++ code that gets positions from external simulators.

**Key Functions**:

#### **initialize()**
```cpp
void ExternalMobility::initialize(int stage) {
    // Stage 1: Read parameters
    externalHost = par("externalHost");
    externalPort = par("externalPort");
    
    // Stage 2: Connect to external simulator
    connectToExternalSimulator();
    scheduleAt(simTime() + updateInterval, updateTimer);
}
```
Sets up the UDP socket connection and schedules first position request.

#### **connectToExternalSimulator()**
```cpp
void ExternalMobility::connectToExternalSimulator() {
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);  // Create UDP socket
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(externalPort);
    inet_pton(AF_INET, externalHost, &servaddr.sin_addr);
}
```
Creates a UDP network socket to communicate with external simulator.

#### **handleSelfMessage()**
```cpp
void ExternalMobility::handleSelfMessage(cMessage *msg) {
    requestPositionUpdate();           // Send "GET_POS:robot[0]"
    
    double x, y, z;
    if (receivePositionUpdate(x, y, z)) {  // Wait for "POS:100,200,0"
        targetPosition.x = x;
        targetPosition.y = y;
        targetPosition.z = z;
        move();                        // Update position in simulation
    }
    
    scheduleAt(simTime() + updateInterval, updateTimer);  // Schedule next update
}
```
Called every `updateInterval` to request and apply new position.

#### **requestPositionUpdate()**
```cpp
void ExternalMobility::requestPositionUpdate() {
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "GET_POS:%s", getFullPath().c_str());
    sendto(sockfd, buffer, strlen(buffer), 0, ...);
}
```
Sends UDP message like `"GET_POS:MultiRobotNetwork.robot[0]"` to external simulator.

#### **receivePositionUpdate()**
```cpp
bool ExternalMobility::receivePositionUpdate(double &x, double &y, double &z) {
    char buffer[256];
    int n = recvfrom(sockfd, buffer, sizeof(buffer), 0, ...);
    
    if (n > 0) {
        buffer[n] = '\0';
        // Parse "POS:123.45,67.89,0.0"
        if (sscanf(buffer, "POS:%lf,%lf,%lf", &x, &y, &z) == 3) {
            return true;
        }
    }
    return false;
}
```
Waits for response like `"POS:123.45,67.89,0.0"` and parses coordinates.

**Think of it as**: The "GPS receiver" that asks an external system "where am I?" and updates the robot's position.

---

### 5. **RobotApplication.ned** (Application Interface)

**Purpose**: Declares the robot's communication application.

**What it contains**:
```ned
simple RobotApplication extends ApplicationBase {
    parameters:
        int destPort = default(5000);          // Port for receiving messages
        int localPort = default(5000);         // Port for sending messages
        string messageTypes = default("STATUS,POSITION,TASK,ALERT,COMMAND");
        double messageInterval @unit(s) = default(1s);  // Send interval
        
    gates:
        input socketIn;   // Receives network packets
        output socketOut; // Sends network packets
}
```

**Key Points**:
- `destPort/localPort`: UDP ports for communication (all robots use 5000)
- `messageTypes`: What kinds of messages robots can send
- `messageInterval`: How often robots send automatic status messages
- Gates connect to the network stack

**Think of it as**: The "app" running on each robot that handles communication.

---

### 6. **RobotApplication.h & .cc** (Application Implementation)

**Purpose**: The C++ code that sends and receives custom messages.

**Key Functions**:

#### **initialize()**
```cpp
void RobotApplication::initialize(int stage) {
    if (stage == INITSTAGE_LOCAL) {
        destPort = par("destPort");
        messageInterval = par("messageInterval");
    }
    else if (stage == INITSTAGE_APPLICATION_LAYER) {
        socket.setOutputGate(gate("socketOut"));
        socket.bind(localPort);  // Listen on port 5000
        
        sendTimer = new cMessage("sendTimer");
        scheduleAt(simTime() + messageInterval, sendTimer);
    }
}
```
Sets up UDP socket and schedules first message transmission.

#### **sendMessage()**
```cpp
void RobotApplication::sendMessage() {
    RobotMessage *msg = new RobotMessage();
    msg->setSourceId(getParentModule()->getFullName());
    msg->setDestinationId("broadcast");
    msg->setMessageType("STATUS");
    msg->setPayload("Robot operational");
    msg->setSequenceNumber(messageSequence++);
    msg->setTimestamp(simTime());
    
    // Add current position from mobility module
    IMobility *mobility = check_and_cast<IMobility*>(
        getParentModule()->getSubmodule("mobility"));
    Coord pos = mobility->getCurrentPosition();
    msg->setPositionX(pos.x);
    msg->setPositionY(pos.y);
    
    // Broadcast to all robots
    socket.sendTo(msg, L3Address(Ipv4Address::ALLONES_ADDRESS), destPort);
}
```
Creates and broadcasts a custom message with current position.

#### **processMessage()**
```cpp
void RobotApplication::processMessage(RobotMessage *msg) {
    EV_INFO << "Received " << msg->getMessageType() 
            << " from " << msg->getSourceId()
            << ": " << msg->getPayload() << endl;
    
    // Custom logic based on message type
    if (strcmp(msg->getMessageType(), "ALERT") == 0) {
        // Handle alert
    }
    else if (strcmp(msg->getMessageType(), "COMMAND") == 0) {
        // Execute command
    }
}
```
Handles incoming messages from other robots.

**Think of it as**: The communication software that formats, sends, and processes messages.

---

### 7. **RobotMessage.msg** (Message Structure)

**Purpose**: Defines the structure of messages robots exchange.

**What it contains**:
```msg
class RobotMessage extends inet::FieldsChunk {
    string sourceId;           // Who sent it (e.g., "robot[0]")
    string destinationId;      // Who should receive it ("robot[1]" or "broadcast")
    string messageType;        // Type: STATUS, POSITION, TASK, ALERT, COMMAND
    string payload;            // The actual message content
    int sequenceNumber;        // Message number (for ordering)
    simtime_t timestamp;       // When it was sent
    double positionX;          // Sender's X coordinate
    double positionY;          // Sender's Y coordinate
    double positionZ;          // Sender's Z coordinate
}
```

**OMNeT++ processes this file** and generates C++ classes automatically with getters/setters:
- `msg->setSourceId("robot[0]")`
- `const char* id = msg->getSourceId()`
- etc.

**Think of it as**: The "envelope format" for messages—what information each message carries.

---

### 8. **omnetpp.ini** (Configuration File)

**Purpose**: Configures all simulation parameters.

**Key Sections**:

#### **General Settings**
```ini
[General]
network = multirobot.simulations.MultiRobotNetwork
sim-time-limit = 300s          # Run for 5 minutes
record-eventlog = true          # Enable detailed logging
```

#### **Visualization**
```ini
*.visualizer.*.mobilityVisualizer.displayMobility = true
*.visualizer.*.mobilityVisualizer.displayMovementTrails = true
*.visualizer.*.mediumVisualizer.displaySignals = true
```
Shows robots moving, their trails, and wireless signals in GUI.

#### **Robot Count**
```ini
*.numRobots = 3  # Create 3 robots
```

#### **External Mobility Configuration**
```ini
*.robot[*].mobility.typename = "ExternalMobility"
*.robot[*].mobility.externalHost = "localhost"
*.robot[*].mobility.externalPort = 9999
*.robot[*].mobility.updateInterval = 0.1s

*.robot[0].mobility.initialX = 100m
*.robot[0].mobility.initialY = 100m
*.robot[1].mobility.initialX = 300m
*.robot[1].mobility.initialY = 150m
```
Tells each robot to use external positions, where to connect, and initial positions.

#### **Wireless Configuration**
```ini
*.robot[*].wlan[0].radio.transmitter.power = 2mW
*.robot[*].wlan[0].radio.receiver.sensitivity = -85dBm
*.robot[*].wlan[0].radio.typename = "Ieee80211ScalarRadio"
*.robot[*].wlan[0].radio.bandName = "2.4 GHz"
```
Configures WiFi radios—transmission power determines range.

#### **Application Configuration**
```ini
*.robot[*].app[0].typename = "RobotApplication"
*.robot[*].app[0].localPort = 5000
*.robot[*].app[0].destPort = 5000
*.robot[*].app[0].messageInterval = 1s
```
Sets up the communication app on each robot.

#### **IP Configuration**
```ini
*.configurator.config = xml("<config><interface hosts='**' address='10.0.0.x' netmask='255.255.255.0'/></config>")
```
Automatically assigns IPs: robot[0] = 10.0.0.1, robot[1] = 10.0.0.2, etc.

**Think of it as**: The "settings panel" where you configure everything without recompiling.

---

### 9. **external_simulator.py** (External Position Provider)

**Purpose**: Provides robot positions to OMNeT++ via UDP.

**Key Components**:

#### **Initialization**
```python
def __init__(self, host='localhost', port=9999):
    self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    self.sock.bind((host, port))  # Listen on UDP port 9999
    
    self.robots = {
        'robot[0]': {'x': 100, 'y': 100, 'z': 0, 'vx': 1, 'vy': 0.5},
        'robot[1]': {'x': 300, 'y': 150, 'z': 0, 'vx': -0.5, 'vy': 1},
        'robot[2]': {'x': 200, 'y': 250, 'z': 0, 'vx': 0.7, 'vy': -0.7},
    }
```
Creates UDP socket and initializes robot positions with velocities.

#### **Position Update**
```python
def update_positions(self, dt):
    for robot_id, pos in self.robots.items():
        # Simple motion model
        pos['x'] += pos['vx'] * dt + math.sin(self.time * 0.1) * 5
        pos['y'] += pos['vy'] * dt + math.cos(self.time * 0.1) * 5
        
        # Bounce off walls
        if pos['x'] < 0 or pos['x'] > 1000:
            pos['vx'] *= -1
        if pos['y'] < 0 or pos['y'] > 1000:
            pos['vy'] *= -1
```
Updates positions every frame—you can replace this with SUMO, Gazebo, or real sensors.

#### **Request Handling**
```python
def handle_request(self):
    data, addr = self.sock.recvfrom(1024)
    request = data.decode('utf-8')  # e.g., "GET_POS:robot[0]"
    
    if request.startswith('GET_POS:'):
        robot_id = request.split(':')[1]
        
        for rid, pos in self.robots.items():
            if rid in robot_id:
                response = f"POS:{pos['x']},{pos['y']},{pos['z']}"
                self.sock.sendto(response.encode(), addr)
                break
```
Receives position requests and sends back coordinates.

#### **Main Loop**
```python
def run(self):
    while True:
        # Update physics every 100ms
        if time_elapsed >= 0.1:
            self.update_positions(0.1)
        
        # Handle OMNeT++ requests
        self.handle_request()
```
Continuously updates positions and responds to OMNeT++ queries.

**Think of it as**: The "physics engine" or "external world" that tells OMNeT++ where robots are.

---

## Communication Flow

### Position Updates (OMNeT++ ← External Simulator)

```
Every 0.1s:

OMNeT++ Robot                       External Simulator
     |                                      |
     |  "GET_POS:robot[0]"                 |
     |------------------------------------->|
     |                                      | [Looks up robot[0] position]
     |                                      |
     |  "POS:123.45,67.89,0.0"             |
     |<-------------------------------------|
     |                                      |
[Updates position in simulation]           |
```

### Message Passing (Robot ↔ Robot via WiFi)

```
Robot[0]                    Radio Medium                    Robot[1]
    |                            |                               |
    | Create RobotMessage        |                               |
    | Type: "ALERT"              |                               |
    | Payload: "Obstacle ahead"  |                               |
    |                            |                               |
    | Send via UDP to 10.0.0.255 |                               |
    |--------------------------->|                               |
    |                            | [Simulates radio propagation] |
    |                            | [Checks signal strength]      |
    |                            |                               |
    |                            |------------------------------>|
    |                            |                               |
    |                            |              Receives message |
    |                            |              Process "ALERT"  |
```

---

## Setup Instructions

### 1. Prerequisites

Install required software:

```bash
# OMNeT++ 6.0+
wget https://github.com/omnetpp/omnetpp/releases/download/omnetpp-6.0.3/omnetpp-6.0.3-linux-x86_64.tgz
tar xf omnetpp-6.0.3-linux-x86_64.tgz
cd omnetpp-6.0.3
. setenv
./configure
make

# INET Framework 4.5+
git clone https://github.com/inet-framework/inet.git
cd inet
make makefiles
make
```

### 2. Create Project Structure

```bash
mkdir -p multirobot/{simulations,nodes,mobility,applications,external}
cd multirobot
```

### 3. Copy Files

Place each file in its respective directory as shown in the project structure.

### 4. Generate Message Class

```bash
cd applications
opp_msgc RobotMessage.msg  # Generates RobotMessage_m.h and RobotMessage_m.cc
```

### 5. Create Makefile

```bash
cd ..  # Back to multirobot/
opp_makemake -f --deep \
  -I/path/to/inet/src \
  -L/path/to/inet/src \
  -lINET \
  -KINET_PROJ=/path/to/inet
```

### 6. Compile

```bash
make MODE=release  # or MODE=debug for debugging
```

---

## Running the Simulation

### Step 1: Start External Simulator

```bash
cd external
python3 external_simulator.py
```

You should see:
```
External Simulator running on localhost:9999
```

### Step 2: Run OMNeT++ (GUI Mode)

```bash
cd ../simulations
./multirobot -u Qtenv -c General
```

This opens the graphical interface where you can:
- See robots moving based on external positions
- Watch wireless signals propagate
- View message logs
- Inspect packet details

### Step 3: Run OMNeT++ (Command Line Mode)

```bash
./multirobot -u Cmdenv -c General
```

For batch simulations without GUI.

---

## What You Should See

### In External Simulator Terminal:
```
External Simulator running on localhost:9999
Sent position for robot[0]: POS:105.234,102.456,0.0
Sent position for robot[1]: POS:298.123,151.789,0.0
Sent position for robot[2]: POS:207.891,248.345,0.0
...
```

### In OMNeT++ GUI:
- Three robots moving according to external positions
- Yellow circles showing wireless transmission range
- Lines between robots when they communicate
- Message animations flying between robots
- Trail lines showing movement paths

### In OMNeT++ Logs:
```
robot[0]: Received ALERT from robot[1]: Obstacle detected
robot[1]: Sent STATUS to broadcast: Robot operational
robot[2]: Position updated to (207.891, 248.345)
```

---

## Customization Examples

### Change Number of Robots

In `omnetpp.ini`:
```ini
*.numRobots = 5  # Now have 5 robots
```

Add corresponding entries in `external_simulator.py`:
```python
self.robots = {
    'robot[0]': {'x': 100, 'y': 100, 'z': 0, 'vx': 1, 'vy': 0.5},
    'robot[1]': {'x': 300, 'y': 150, 'z': 0, 'vx': -0.5, 'vy': 1},
    'robot[2]': {'x': 200, 'y': 250, 'z': 0, 'vx': 0.7, 'vy': -0.7},
    'robot[3]': {'x': 400, 'y': 100, 'z': 0, 'vx': -1, 'vy': 1},
    'robot[4]': {'x': 500, 'y': 300, 'z': 0, 'vx': 0.5, 'vy': -0.5},
}
```

### Integrate with SUMO (Traffic Simulator)

Replace `external_simulator.py` with SUMO TraCI interface:

```python
import traci

traci.start(["sumo", "-c", "simulation.sumocfg"])

while traci.simulation.getMinExpectedNumber() > 0:
    traci.simulationStep()
    
    for veh_id in traci.vehicle.getIDList():
        x, y = traci.vehicle.getPosition(veh_id)
        # Send position to OMNeT++
        response = f"POS:{x},{y},0"
        sock.sendto(response.encode(), addr)
```

### Add Custom Message Types

In `RobotMessage.msg`:
```msg
class RobotMessage extends inet::FieldsChunk {
    // ... existing fields ...
    double batteryLevel;
    string sensorData;
    int taskPriority;
}
```

Recompile with `opp_msgc RobotMessage.msg` and `make`.

### Change Wireless Range

In `omnetpp.ini`:
```ini
*.robot[*].wlan[0].radio.transmitter.power = 10mW  # Longer range
*.robot[*].wlan[0].radio.receiver.sensitivity = -90dBm  # More sensitive
```

---

## Troubleshooting

### "Cannot connect to external simulator"
- Check external_simulator.py is running
- Verify port 9999 is not blocked by firewall
- Check `externalHost` in omnetpp.ini matches simulator location

### "Robots not moving"
- Verify external_simulator.py is sending position updates
- Check update_positions() is being called
- Look for socket errors in both terminals

### "No messages being received"
- Check all robots have same port (5000)
- Verify wireless transmission power is sufficient
- Check IP configuration in omnetpp.ini

### "Compilation errors"
- Run `opp_msgc RobotMessage.msg` first
- Verify INET path in makefile
- Check all .h files are included

---

## Summary

This setup creates a powerful hybrid simulation where:

1. **OMNeT++/INET** handles:
    - Realistic wireless communication
    - Network protocols (IP, UDP)
    - Radio propagation and interference
    - Message routing and delivery

2. **External Simulator** handles:
    - Robot physics and kinematics
    - Environment interaction
    - Sensor simulation
    - Path planning

3. **Together** they provide:
    - Realistic multi-robot communication
    - External position control
    - Custom message passing
    - Full network simulation

You can now replace the Python simulator with any external system (SUMO, Gazebo, ROS, real robots) while maintaining realistic network simulation in OMNeT++!