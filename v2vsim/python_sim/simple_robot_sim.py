#!/usr/bin/env python3
"""
Docker container simulator for testing V2V simulation
Simulates a robot sending position updates and receiving V2V messages
"""

import socket
import struct
import time
import sys
import threading
import math

class RobotSimulator:
    def __init__(self, robot_id, omnetpp_port, listen_port):
        self.robot_id = robot_id
        self.omnetpp_host = "127.0.0.1"
        self.omnetpp_port = omnetpp_port  # Port where OMNeT++ listens
        self.listen_port = listen_port     # Port where we listen for OMNeT++

        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(("0.0.0.0", listen_port))
        self.sock.settimeout(0.5)

        self.position = [0.0, 0.0, 0.0]
        self.running = True

        print(f"Robot {robot_id} simulator started")
        print(f"  Sending to OMNeT++: {self.omnetpp_host}:{self.omnetpp_port}")
        print(f"  Listening on: {listen_port}")

    def send_position_update(self):
        """Send position update to OMNeT++ mobility module"""
        # Simple circular motion for testing
        t = time.time()
        radius = 50.0
        self.position[0] = radius * math.cos(t * 0.5 + self.robot_id * math.pi)
        self.position[1] = radius * math.sin(t * 0.5 + self.robot_id * math.pi)
        self.position[2] = 0.0

        # Pack as 3 floats (12 bytes)
        data = struct.pack('fff', *self.position)

        # Send to OMNeT++ mobility module (port 8000 + robot_id)
        mobility_port = 8000 + self.robot_id
        self.sock.sendto(data, (self.omnetpp_host, mobility_port))

        print(f"Robot {self.robot_id} sent position: ({self.position[0]:.2f}, "
              f"{self.position[1]:.2f}, {self.position[2]:.2f})")

    def send_v2v_message(self, message):
        """Send V2V message to OMNeT++ app module"""
        # Format: [MSG_TYPE=0x02, ...message...]
        data = bytes([0x02]) + message.encode('utf-8')

        # Send to OMNeT++ app module (port 5000 + robot_id)
        app_port = 5000 + self.robot_id
        self.sock.sendto(data, (self.omnetpp_host, app_port))

        print(f"Robot {self.robot_id} sent V2V message: {message}")

    def receive_loop(self):
        """Listen for messages from OMNeT++"""
        while self.running:
            try:
                data, addr = self.sock.recvfrom(1024)
                self.handle_message(data, addr)
            except socket.timeout:
                continue
            except Exception as e:
                print(f"Robot {self.robot_id} receive error: {e}")

    def handle_message(self, data, addr):
        """Handle incoming message from OMNeT++"""
        if len(data) == 0:
            return

        msg_type = data[0]

        if msg_type == 0x01:  # Poll request
            print(f"Robot {self.robot_id} received poll request")

        elif msg_type == 0x04:  # V2V message from another robot
            message = data[1:].decode('utf-8', errors='ignore')
            print(f"Robot {self.robot_id} received V2V from network: {message}")

        elif msg_type == 0xFF:  # Heartbeat
            print(f"Robot {self.robot_id} received heartbeat")

        else:
            print(f"Robot {self.robot_id} received unknown message type: {msg_type}")

    def run(self):
        """Main loop"""
        # Start receive thread
        recv_thread = threading.Thread(target=self.receive_loop, daemon=True)
        recv_thread.start()

        print(f"\nRobot {self.robot_id} running. Press Ctrl+C to stop.\n")

        try:
            message_counter = 0
            while True:
                # Send position update every 100ms
                self.send_position_update()

                # Send V2V message every 5 seconds
                if message_counter % 50 == 0:
                    msg = f"Hello from Robot {self.robot_id} at t={time.time():.1f}"
                    self.send_v2v_message(msg)

                message_counter += 1
                time.sleep(0.1)

        except KeyboardInterrupt:
            print(f"\nRobot {self.robot_id} stopping...")
            self.running = False
            self.sock.close()

def main():
    if len(sys.argv) != 2:
        print("Usage: python3 docker_robot_simulator.py <robot_id>")
        print("Example: python3 docker_robot_simulator.py 0")
        sys.exit(1)

    robot_id = int(sys.argv[1])

    # OMNeT++ ports configuration
    # App listens on: 5000 + robot_id
    # Mobility listens on: 8000 + robot_id
    # This simulator listens on: 9000 + robot_id (for receiving from OMNeT++)

    omnetpp_app_port = 5000 + robot_id
    listen_port = 9000 + robot_id

    simulator = RobotSimulator(robot_id, omnetpp_app_port, listen_port)
    simulator.run()

if __name__ == "__main__":
    main()