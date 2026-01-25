# =====================================================================
# OMNET++ SETUP WITH EXTERNAL ROBOT SIMULATOR CONTAINERS
# Each robot (or group of robots) runs in its own external container
# =====================================================================

# ---------------------------------------------------------------------
# 1. MULTI-CONTAINER docker-compose.yml
# ---------------------------------------------------------------------
# FILE: docker-compose.yml
---
version: '3.8'

services:
# =============================================
# OMNeT++ Simulation Container
# =============================================
omnetpp-sim:
build:
context: .
dockerfile: Dockerfile
args:
OMNET_VERSION: "6.0.1"
INET_VERSION: "4.5.2"
VARIANT: "22.04"
USER_UID: 1000
USER_GID: 1000

    container_name: omnetpp-sim
    hostname: omnetpp-sim
    
    deploy:
      resources:
        limits:
          cpus: '16'
          memory: 32G
        reservations:
          cpus: '8'
          memory: 16G
    
    shm_size: 4gb
    
    volumes:
      - .:/home/ubuntu/src:rw
      - omnetpp-results:/home/ubuntu/results:rw
    
    ports:
      - "6083:6083"           # noVNC web interface
      - "5900:5900"           # VNC direct access
      - "9999:9999/udp"       # Position aggregator service
    
    networks:
      robot-network:
        ipv4_address: 172.25.0.10
    
    environment:
      NOVNC_PORT: 6083
      DISPLAY: ":1"
      VNC_RESOLUTION: "1920x1080"
      OMP_NUM_THREADS: 16
      OMNETPP_THREAD_COUNT: 16
      
      # Service discovery
      POSITION_AGGREGATOR_HOST: position-aggregator
      POSITION_AGGREGATOR_PORT: 9999
      REDIS_HOST: redis
      
    depends_on:
      - redis
      - position-aggregator

# =============================================
# Position Aggregator Service
# Collects positions from all external robot containers
# =============================================
position-aggregator:
build:
context: ./services
dockerfile: Dockerfile.aggregator

    container_name: position-aggregator
    hostname: position-aggregator
    
    deploy:
      resources:
        limits:
          cpus: '4'
          memory: 4G
    
    ports:
      - "9999:9999/udp"       # OMNeT++ queries this
      - "10000:10000/udp"     # External robots send to this
      - "8080:8080"           # HTTP API for monitoring
    
    networks:
      robot-network:
        ipv4_address: 172.25.0.20
    
    environment:
      LISTEN_PORT: 9999
      EXTERNAL_PORT: 10000
      REDIS_HOST: redis
      REDIS_PORT: 6379
      NUM_ROBOTS: 150
    
    depends_on:
      - redis

# =============================================
# Redis for state synchronization
# =============================================
redis:
image: redis:7-alpine
container_name: robot-redis
hostname: redis

    ports:
      - "6379:6379"
    
    volumes:
      - redis-data:/data
    
    networks:
      robot-network:
        ipv4_address: 172.25.0.30
    
    command: >
      redis-server
      --save 60 1
      --loglevel warning
      --maxmemory 4gb
      --maxmemory-policy allkeys-lru
      --appendonly yes

# =============================================
# External Robot Simulators (Examples)
# You can scale these or run them separately
# =============================================

# Example: Gazebo simulator for robots 0-49
robot-sim-gazebo-1:
build:
context: ./external_simulators
dockerfile: Dockerfile.gazebo

    container_name: robot-sim-gazebo-1
    hostname: robot-sim-gazebo-1
    
    deploy:
      resources:
        limits:
          cpus: '4'
          memory: 8G
    
    volumes:
      - ./external_simulators/gazebo:/workspace:rw
    
    networks:
      robot-network:
        ipv4_address: 172.25.0.101
    
    environment:
      ROBOT_ID_START: 0
      ROBOT_ID_END: 49
      NUM_ROBOTS: 50
      AGGREGATOR_HOST: position-aggregator
      AGGREGATOR_PORT: 10000
      UPDATE_RATE_HZ: 10
      SIM_TYPE: "gazebo"
    
    depends_on:
      - position-aggregator

# Example: SUMO simulator for robots 50-99
robot-sim-sumo-1:
build:
context: ./external_simulators
dockerfile: Dockerfile.sumo

    container_name: robot-sim-sumo-1
    hostname: robot-sim-sumo-1
    
    deploy:
      resources:
        limits:
          cpus: '4'
          memory: 6G
    
    volumes:
      - ./external_simulators/sumo:/workspace:rw
    
    networks:
      robot-network:
        ipv4_address: 172.25.0.102
    
    environment:
      ROBOT_ID_START: 50
      ROBOT_ID_END: 99
      NUM_ROBOTS: 50
      AGGREGATOR_HOST: position-aggregator
      AGGREGATOR_PORT: 10000
      UPDATE_RATE_HZ: 10
      SIM_TYPE: "sumo"
    
    depends_on:
      - position-aggregator

# Example: Custom Python simulator for robots 100-149
robot-sim-custom-1:
build:
context: ./external_simulators
dockerfile: Dockerfile.custom

    container_name: robot-sim-custom-1
    hostname: robot-sim-custom-1
    
    deploy:
      resources:
        limits:
          cpus: '2'
          memory: 2G
    
    volumes:
      - ./external_simulators/custom:/workspace:rw
    
    networks:
      robot-network:
        ipv4_address: 172.25.0.103
    
    environment:
      ROBOT_ID_START: 100
      ROBOT_ID_END: 149
      NUM_ROBOTS: 50
      AGGREGATOR_HOST: position-aggregator
      AGGREGATOR_PORT: 10000
      UPDATE_RATE_HZ: 10
      SIM_TYPE: "custom"
    
    depends_on:
      - position-aggregator

# =============================================
# Monitoring & Visualization
# =============================================
grafana:
image: grafana/grafana:latest
container_name: robot-monitor
hostname: grafana

    ports:
      - "3000:3000"
    
    volumes:
      - grafana-data:/var/lib/grafana
      - ./monitoring/dashboards:/etc/grafana/provisioning/dashboards:ro
      - ./monitoring/datasources:/etc/grafana/provisioning/datasources:ro
    
    networks:
      robot-network:
        ipv4_address: 172.25.0.40
    
    environment:
      GF_SECURITY_ADMIN_PASSWORD: admin
      GF_INSTALL_PLUGINS: redis-datasource
    
    depends_on:
      - redis

# Web-based control panel
control-panel:
build:
context: ./services
dockerfile: Dockerfile.control

    container_name: control-panel
    hostname: control-panel
    
    ports:
      - "8888:8888"
    
    networks:
      robot-network:
        ipv4_address: 172.25.0.50
    
    environment:
      REDIS_HOST: redis
      AGGREGATOR_HOST: position-aggregator
      OMNETPP_HOST: omnetpp-sim
    
    depends_on:
      - redis
      - position-aggregator

volumes:
omnetpp-results:
redis-data:
grafana-data:

networks:
robot-network:
driver: bridge
ipam:
config:
- subnet: 172.25.0.0/16
gateway: 172.25.0.1

# ---------------------------------------------------------------------
# 2. Position Aggregator Service (Collects from external containers)
# ---------------------------------------------------------------------
# FILE: services/Dockerfile.aggregator
---
FROM python:3.11-slim

WORKDIR /app

RUN pip install --no-cache-dir \
redis \
aiohttp \
asyncio \
msgpack \
numpy

COPY aggregator.py .
COPY common.py .

CMD ["python3", "aggregator.py"]

# FILE: services/aggregator.py
---
import asyncio
import socket
import json
import time
import redis
from typing import Dict, Tuple
from dataclasses import dataclass, asdict
import os

@dataclass
class RobotPosition:
robot_id: str
x: float
y: float
z: float
timestamp: float
source_container: str

class PositionAggregator:
"""
Aggregates position data from multiple external robot simulator containers
and serves it to OMNeT++
"""
def __init__(self):
self.listen_port = int(os.getenv('LISTEN_PORT', 9999))
self.external_port = int(os.getenv('EXTERNAL_PORT', 10000))
self.num_robots = int(os.getenv('NUM_ROBOTS', 150))

        # Position cache: robot_id -> RobotPosition
        self.positions: Dict[str, RobotPosition] = {}
        
        # Redis for persistence and monitoring
        self.redis = redis.Redis(
            host=os.getenv('REDIS_HOST', 'redis'),
            port=int(os.getenv('REDIS_PORT', 6379)),
            decode_responses=False
        )
        
        # Statistics
        self.stats = {
            'omnetpp_requests': 0,
            'external_updates': 0,
            'last_stats_time': time.time()
        }
        
        # Sockets
        self.omnetpp_sock = None      # Responds to OMNeT++ queries
        self.external_sock = None     # Receives from external simulators
        
        print(f"Position Aggregator initialized")
        print(f"  OMNeT++ query port: {self.listen_port}")
        print(f"  External update port: {self.external_port}")
        print(f"  Expected robots: {self.num_robots}")
    
    async def handle_omnetpp_requests(self):
        """
        Handle position queries from OMNeT++
        Protocol: GET_POS:robot[0] -> POS:x,y,z
        """
        loop = asyncio.get_event_loop()
        
        self.omnetpp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.omnetpp_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.omnetpp_sock.setblocking(False)
        self.omnetpp_sock.bind(('0.0.0.0', self.listen_port))
        
        print(f"Listening for OMNeT++ requests on port {self.listen_port}")
        
        while True:
            try:
                data, addr = await loop.sock_recvfrom(self.omnetpp_sock, 1024)
                request = data.decode('utf-8').strip()
                
                if request.startswith('GET_POS:'):
                    robot_id = request.split(':', 1)[1]
                    await self.send_position_to_omnetpp(robot_id, addr, loop)
                    self.stats['omnetpp_requests'] += 1
                    
                elif request == 'GET_ALL':
                    await self.send_all_positions(addr, loop)
                    
            except Exception as e:
                if "Resource temporarily unavailable" not in str(e):
                    print(f"Error handling OMNeT++ request: {e}")
                await asyncio.sleep(0.0001)
    
    async def send_position_to_omnetpp(self, robot_id: str, addr: Tuple, loop):
        """Send single robot position to OMNeT++"""
        # Match robot ID (flexible matching)
        for rid, pos in self.positions.items():
            if rid in robot_id or robot_id in rid:
                response = f"POS:{pos.x:.3f},{pos.y:.3f},{pos.z:.3f}"
                await loop.sock_sendto(
                    self.omnetpp_sock,
                    response.encode(),
                    addr
                )
                return
        
        # Robot not found - send last known or default
        response = "POS:0.0,0.0,0.0"
        await loop.sock_sendto(self.omnetpp_sock, response.encode(), addr)
    
    async def send_all_positions(self, addr: Tuple, loop):
        """Send all positions as JSON"""
        positions_dict = {rid: asdict(pos) for rid, pos in self.positions.items()}
        response = json.dumps(positions_dict)
        await loop.sock_sendto(self.omnetpp_sock, response.encode(), addr)
    
    async def handle_external_updates(self):
        """
        Receive position updates from external simulator containers
        Protocol: UPDATE:robot[0]:x,y,z:container_name
        """
        loop = asyncio.get_event_loop()
        
        self.external_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.external_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.external_sock.setblocking(False)
        self.external_sock.bind(('0.0.0.0', self.external_port))
        
        print(f"Listening for external updates on port {self.external_port}")
        
        while True:
            try:
                data, addr = await loop.sock_recvfrom(self.external_sock, 2048)
                message = data.decode('utf-8').strip()
                
                if message.startswith('UPDATE:'):
                    await self.process_position_update(message, addr[0])
                    self.stats['external_updates'] += 1
                    
                elif message.startswith('BATCH:'):
                    await self.process_batch_update(message, addr[0])
                    
            except Exception as e:
                if "Resource temporarily unavailable" not in str(e):
                    print(f"Error handling external update: {e}")
                await asyncio.sleep(0.0001)
    
    async def process_position_update(self, message: str, source_ip: str):
        """
        Process single position update
        Format: UPDATE:robot[0]:100.5,200.3,0.0:gazebo-sim-1
        """
        try:
            parts = message.split(':')
            if len(parts) >= 4:
                robot_id = parts[1]
                coords = parts[2].split(',')
                container = parts[3] if len(parts) > 3 else source_ip
                
                x, y, z = float(coords[0]), float(coords[1]), float(coords[2])
                
                self.positions[robot_id] = RobotPosition(
                    robot_id=robot_id,
                    x=x, y=y, z=z,
                    timestamp=time.time(),
                    source_container=container
                )
                
                # Update Redis for monitoring
                self.redis.hset(
                    f"robot:{robot_id}",
                    mapping={
                        'x': x, 'y': y, 'z': z,
                        'timestamp': time.time(),
                        'source': container
                    }
                )
                
        except Exception as e:
            print(f"Error processing update: {e}")
    
    async def process_batch_update(self, message: str, source_ip: str):
        """
        Process batch update (more efficient)
        Format: BATCH:container_name:robot[0]:x,y,z:robot[1]:x,y,z:...
        """
        try:
            parts = message.split(':')
            container = parts[1]
            
            # Process pairs of robot_id:coords
            for i in range(2, len(parts), 2):
                if i + 1 < len(parts):
                    robot_id = parts[i]
                    coords = parts[i + 1].split(',')
                    
                    x, y, z = float(coords[0]), float(coords[1]), float(coords[2])
                    
                    self.positions[robot_id] = RobotPosition(
                        robot_id=robot_id,
                        x=x, y=y, z=z,
                        timestamp=time.time(),
                        source_container=container
                    )
        except Exception as e:
            print(f"Error processing batch: {e}")
    
    async def stats_reporter(self):
        """Report statistics periodically"""
        while True:
            await asyncio.sleep(5.0)
            
            current_time = time.time()
            elapsed = current_time - self.stats['last_stats_time']
            
            omnetpp_rate = self.stats['omnetpp_requests'] / elapsed
            external_rate = self.stats['external_updates'] / elapsed
            
            print(f"Stats: {len(self.positions)}/{self.num_robots} robots | "
                  f"OMNeT++: {omnetpp_rate:.1f} req/s | "
                  f"External: {external_rate:.1f} upd/s")
            
            # Update Redis metrics
            self.redis.set('metrics:active_robots', len(self.positions))
            self.redis.set('metrics:omnetpp_rate', f"{omnetpp_rate:.2f}")
            self.redis.set('metrics:external_rate', f"{external_rate:.2f}")
            
            # Reset counters
            self.stats['omnetpp_requests'] = 0
            self.stats['external_updates'] = 0
            self.stats['last_stats_time'] = current_time
    
    async def cleanup_stale_positions(self):
        """Remove positions that haven't been updated recently"""
        while True:
            await asyncio.sleep(10.0)
            
            current_time = time.time()
            stale_timeout = 5.0  # 5 seconds
            
            stale_robots = [
                rid for rid, pos in self.positions.items()
                if current_time - pos.timestamp > stale_timeout
            ]
            
            for rid in stale_robots:
                print(f"Removing stale position for {rid}")
                del self.positions[rid]
                self.redis.delete(f"robot:{rid}")
    
    async def run(self):
        """Run all services concurrently"""
        await asyncio.gather(
            self.handle_omnetpp_requests(),
            self.handle_external_updates(),
            self.stats_reporter(),
            self.cleanup_stale_positions()
        )

if __name__ == '__main__':
aggregator = PositionAggregator()
print("Starting Position Aggregator Service...")
asyncio.run(aggregator.run())

# ---------------------------------------------------------------------
# 3. External Robot Simulator Base Class
# ---------------------------------------------------------------------
# FILE: external_simulators/robot_sim_base.py
---
import asyncio
import socket
import time
import os
from abc import ABC, abstractmethod
from typing import List, Tuple

class RobotSimulatorBase(ABC):
"""
Base class for external robot simulators
Each external container inherits from this
"""
def __init__(self):
self.robot_id_start = int(os.getenv('ROBOT_ID_START', 0))
self.robot_id_end = int(os.getenv('ROBOT_ID_END', 49))
self.num_robots = self.robot_id_end - self.robot_id_start + 1

        self.aggregator_host = os.getenv('AGGREGATOR_HOST', 'position-aggregator')
        self.aggregator_port = int(os.getenv('AGGREGATOR_PORT', 10000))
        self.update_rate_hz = int(os.getenv('UPDATE_RATE_HZ', 10))
        
        self.sim_type = os.getenv('SIM_TYPE', 'unknown')
        self.container_name = os.getenv('HOSTNAME', 'unknown')
        
        # UDP socket for sending positions
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.aggregator_addr = (self.aggregator_host, self.aggregator_port)
        
        print(f"Robot Simulator initialized:")
        print(f"  Type: {self.sim_type}")
        print(f"  Container: {self.container_name}")
        print(f"  Managing robots: {self.robot_id_start}-{self.robot_id_end} ({self.num_robots} total)")
        print(f"  Aggregator: {self.aggregator_host}:{self.aggregator_port}")
        print(f"  Update rate: {self.update_rate_hz} Hz")
    
    @abstractmethod
    async def initialize_simulation(self):
        """Initialize the specific simulator (Gazebo, SUMO, etc.)"""
        pass
    
    @abstractmethod
    async def get_robot_positions(self) -> List[Tuple[str, float, float, float]]:
        """
        Get current positions of all robots
        Returns: List of (robot_id, x, y, z)
        """
        pass
    
    @abstractmethod
    async def step_simulation(self, dt: float):
        """Advance simulation by dt seconds"""
        pass
    
    async def send_position_update(self, robot_id: str, x: float, y: float, z: float):
        """Send single position update to aggregator"""
        message = f"UPDATE:{robot_id}:{x:.3f},{y:.3f},{z:.3f}:{self.container_name}"
        self.sock.sendto(message.encode(), self.aggregator_addr)
    
    async def send_batch_update(self, positions: List[Tuple[str, float, float, float]]):
        """Send batch update (more efficient for many robots)"""
        message_parts = [f"BATCH:{self.container_name}"]
        
        for robot_id, x, y, z in positions:
            message_parts.append(f"{robot_id}:{x:.3f},{y:.3f},{z:.3f}")
        
        message = ':'.join(message_parts)
        self.sock.sendto(message.encode(), self.aggregator_addr)
    
    async def simulation_loop(self):
        """Main simulation loop"""
        update_interval = 1.0 / self.update_rate_hz
        last_update = time.time()
        
        await self.initialize_simulation()
        
        print(f"Starting simulation loop at {self.update_rate_hz} Hz")
        
        while True:
            current_time = time.time()
            dt = current_time - last_update
            
            if dt >= update_interval:
                # Step simulation
                await self.step_simulation(dt)
                
                # Get positions
                positions = await self.get_robot_positions()
                
                # Send to aggregator (batch is more efficient)
                if len(positions) > 5:
                    await self.send_batch_update(positions)
                else:
                    for robot_id, x, y, z in positions:
                        await self.send_position_update(robot_id, x, y, z)
                
                last_update = current_time
            
            await asyncio.sleep(0.001)
    
    def run(self):
        """Run the simulator"""
        asyncio.run(self.simulation_loop())

# ---------------------------------------------------------------------
# 4. Example: Gazebo Simulator Implementation
# ---------------------------------------------------------------------
# FILE: external_simulators/Dockerfile.gazebo
---
FROM osrf/ros:noetic-desktop-full

WORKDIR /workspace

# Install Python dependencies
RUN apt-get update && apt-get install -y \
python3-pip \
ros-noetic-gazebo-ros-pkgs \
ros-noetic-gazebo-ros-control

RUN pip3 install asyncio numpy

COPY robot_sim_base.py .
COPY gazebo_simulator.py .
COPY gazebo_world.world .

CMD ["python3", "gazebo_simulator.py"]

# FILE: external_simulators/gazebo_simulator.py
---
import asyncio
import math
import numpy as np
from robot_sim_base import RobotSimulatorBase
from typing import List, Tuple

# Note: In real implementation, use rospy and gazebo_msgs
# This is a simplified example

class GazeboSimulator(RobotSimulatorBase):
"""Gazebo-based robot simulator"""

    def __init__(self):
        super().__init__()
        self.robot_states = {}
    
    async def initialize_simulation(self):
        """Initialize Gazebo simulation"""
        print("Initializing Gazebo simulation...")
        
        # In real implementation:
        # - Launch Gazebo with roslaunch
        # - Spawn robot models
        # - Set up ROS subscribers/publishers
        
        # For this example, initialize simple state
        for i in range(self.robot_id_start, self.robot_id_end + 1):
            robot_id = f"robot[{i}]"
            row = (i - self.robot_id_start) // 10
            col = (i - self.robot_id_start) % 10
            
            self.robot_states[robot_id] = {
                'x': col * 50.0 + np.random.uniform(-5, 5),
                'y': row * 50.0 + np.random.uniform(-5, 5),
                'z': 0.0,
                'vx': np.random.uniform(-1, 1),
                'vy': np.random.uniform(-1, 1)
            }
        
        print(f"Initialized {len(self.robot_states)} robots in Gazebo")
    
    async def get_robot_positions(self) -> List[Tuple[str, float, float, float]]:
        """Get positions from Gazebo (via ROS topics in real implementation)"""
        positions = []
        
        for robot_id, state in self.robot_states.items():
            positions.append((robot_id, state['x'], state['y'], state['z']))
        
        return positions
    
    async def step_simulation(self, dt: float):
        """Step Gazebo simulation"""
        # In real implementation: Gazebo steps automatically
        # Here we simulate simple physics
        
        for robot_id, state in self.robot_states.items():
            # Update position based on velocity
            state['x'] += state['vx'] * dt
            state['y'] += state['vy'] * dt
            
            # Boundary checks
            if state['x'] < 0 or state['x'] > 500:
                state['vx'] *= -1
            if state['y'] < 0 or state['y'] > 500:
                state['vy'] *= -1
            
            # Add some noise
            state['vx'] += np.random.normal(0, 0.05)
            state['vy'] += np.random.normal(0, 0.05)
            
            # Limit velocity
            speed = math.sqrt(state['vx']**2 + state['vy']**2)
            if speed > 3:
                state['vx'] = (state['vx'] / speed) * 3
                state['vy'] = (state['vy'] / speed) * 3

if __name__ == '__main__':
sim = SUMOSimulator()
sim.run()

# ---------------------------------------------------------------------
# 6. Example: Custom Python Simulator
# ---------------------------------------------------------------------
# FILE: external_simulators/Dockerfile.custom
---
FROM python:3.11-slim

WORKDIR /workspace

RUN pip install --no-cache-dir asyncio numpy scipy

COPY robot_sim_base.py .
COPY custom_simulator.py .

CMD ["python3", "custom_simulator.py"]

# FILE: external_simulators/custom_simulator.py
---
import asyncio
import math
import numpy as np
from robot_sim_base import RobotSimulatorBase
from typing import List, Tuple

class CustomSimulator(RobotSimulatorBase):
"""Simple custom physics simulator"""

    def __init__(self):
        super().__init__()
        self.robots = {}
    
    async def initialize_simulation(self):
        """Initialize custom simulation"""
        print("Initializing custom simulator...")
        
        # Create robots in formation
        for i in range(self.robot_id_start, self.robot_id_end + 1):
            robot_id = f"robot[{i}]"
            idx = i - self.robot_id_start
            
            # Circular formation
            angle = (2 * math.pi * idx) / self.num_robots
            radius = 200.0
            
            self.robots[robot_id] = {
                'x': 500 + radius * math.cos(angle),
                'y': 500 + radius * math.sin(angle),
                'z': 0.0,
                'vx': -math.sin(angle) * 2,
                'vy': math.cos(angle) * 2,
                'angle': angle
            }
        
        print(f"Initialized {len(self.robots)} robots")
    
    async def get_robot_positions(self) -> List[Tuple[str, float, float, float]]:
        """Get current positions"""
        return [(rid, r['x'], r['y'], r['z']) for rid, r in self.robots.items()]
    
    async def step_simulation(self, dt: float):
        """Update physics"""
        for robot_id, robot in self.robots.items():
            # Circular motion with some randomness
            robot['angle'] += 0.5 * dt
            radius = 200.0 + 20 * math.sin(robot['angle'] * 3)
            
            robot['x'] = 500 + radius * math.cos(robot['angle'])
            robot['y'] = 500 + radius * math.sin(robot['angle'])
            
            # Add noise
            robot['x'] += np.random.normal(0, 0.5)
            robot['y'] += np.random.normal(0, 0.5)

if __name__ == '__main__':
sim = CustomSimulator()
sim.run()

# ---------------------------------------------------------------------
# 7. Updated OMNeT++ Configuration
# ---------------------------------------------------------------------
# FILE: omnetpp.ini
---
[General]
network = multirobot.simulations.MultiRobotNetwork
sim-time-limit = 600s
cmdenv-express-mode = true
cmdenv-performance-display = true

# External mobility connects to aggregator service
*.robot[*].mobility.typename = "ExternalMobility"
*.robot[*].mobility.externalHost = "position-aggregator"
*.robot[*].mobility.externalPort = 9999
*.robot[*].mobility.updateInterval = 0.1s

# Robot count must match total across all external simulators
*.numRobots = 150

# Network configuration
*.configurator.config = xml("<config><interface hosts='**' address='10.0.x.x' netmask='255.255.0.0'/></config>")

# Wireless settings
*.robot[*].wlan[0].radio.transmitter.power = 5mW
*.robot[*].wlan[0].radio.receiver.sensitivity = -90dBm

# Application
*.robot[*].app[0].typename = "RobotApplication"
*.robot[*].app[0].messageInterval = exponential(2s)

# ---------------------------------------------------------------------
# 8. Startup Script
# ---------------------------------------------------------------------
# FILE: start.sh
---
#!/bin/bash

echo "================================================"
echo "Starting OMNeT++ Multi-Robot Simulation System"
echo "================================================"

# Start core services
echo "Starting Redis and Position Aggregator..."
docker-compose up -d redis position-aggregator

# Wait for services
echo "Waiting for services to be ready..."
sleep 5

# Start external robot simulators
echo "Starting external robot simulators..."
docker-compose up -d robot-sim-gazebo-1 robot-sim-sumo-1 robot-sim-custom-1

# Wait for robots to initialize
echo "Waiting for robot simulators..."
sleep 10

# Start OMNeT++
echo "Starting OMNeT++ simulation..."
docker-compose up -d omnetpp-sim

# Start monitoring
echo "Starting monitoring services..."
docker-compose up -d grafana control-panel

echo ""
echo "================================================"
echo "System Status:"
echo "================================================"
docker-compose ps

echo ""
echo "Access Points:"
echo "  OMNeT++ GUI:     http://localhost:6083"
echo "  Grafana:         http://localhost:3000 (admin/admin)"
echo "  Control Panel:   http://localhost:8888"
echo ""
echo "To view logs:"
echo "  docker-compose logs -f position-aggregator"
echo "  docker-compose logs -f robot-sim-gazebo-1"
echo ""

# ---------------------------------------------------------------------
# 9. Stop Script
# ---------------------------------------------------------------------
# FILE: stop.sh
---
#!/bin/bash

echo "Stopping all services..."
docker-compose down

echo "Cleaning up volumes (optional)..."
read -p "Remove volumes? (y/N): " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
docker-compose down -v
fi

# ---------------------------------------------------------------------
# 10. README for External Container Setup
# ---------------------------------------------------------------------
# FILE: README.md
---
# OMNeT++ with External Robot Simulator Containers

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                     Docker Network (172.25.0.0/16)          │
│                                                              │
│  ┌──────────────┐         ┌────────────────────┐           │
│  │   OMNeT++    │◄────────│    Position        │           │
│  │  Simulation  │  Query  │   Aggregator       │           │
│  │  (Port 9999) │ Pos.    │  (Ports 9999/10000)│           │
│  └──────────────┘         └─────────┬──────────┘           │
│         │                           │                       │
│         │                           │ Updates               │
│         │                           │                       │
│         │              ┌────────────┴──────────────┐       │
│         │              │            │              │        │
│  ┌──────▼──────┐  ┌───▼─────┐ ┌───▼─────┐  ┌────▼────┐   │
│  │   Grafana   │  │ Gazebo  │ │  SUMO   │  │ Custom  │   │
│  │  Monitoring │  │Simulator│ │Simulator│  │   Sim   │   │
│  │             │  │(R 0-49) │ │(R 50-99)│  │(R100-149│   │
│  └─────────────┘  └─────────┘ └─────────┘  └─────────┘   │
│                                                              │
│  ┌──────────────────────────────────────────────────────┐  │
│  │              Redis (State & Metrics)                 │  │
│  └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

## Communication Protocol

### External Simulator → Position Aggregator
```
Single Update: UPDATE:robot[0]:100.5,200.3,0.0:container-name
Batch Update:  BATCH:container-name:robot[0]:100,200,0:robot[1]:150,250,0
```

### OMNeT++ → Position Aggregator
```
Request:  GET_POS:robot[0]
Response: POS:100.500,200.300,0.000
```

## Quick Start

### 1. Start System
```bash
chmod +x start.sh stop.sh
./start.sh
```

### 2. Check Status
```bash
# View all containers
docker-compose ps

# Check aggregator logs
docker-compose logs -f position-aggregator

# Check robot simulator logs
docker-compose logs -f robot-sim-gazebo-1
```

### 3. Run OMNeT++ Simulation
```bash
# Enter OMNeT++ container
docker exec -it omnetpp-sim bash

# Run simulation
cd /home/ubuntu/src/multirobot/simulations
./multirobot -u Qtenv -c General
```

### 4. Monitor
- **Grafana**: http://localhost:3000
- **OMNeT++ GUI**: http://localhost:6083

## Adding Your Own Simulator

### Step 1: Create Dockerfile
```dockerfile
# FILE: external_simulators/Dockerfile.mysim
FROM ubuntu:22.04

# Install your simulator
RUN apt-get update && apt-get install -y your-simulator

# Install Python
RUN apt-get install -y python3 python3-pip
RUN pip3 install asyncio numpy

WORKDIR /workspace
COPY robot_sim_base.py .
COPY my_simulator.py .

CMD ["python3", "my_simulator.py"]
```

### Step 2: Implement Simulator
```python
# FILE: external_simulators/my_simulator.py
from robot_sim_base import RobotSimulatorBase

class MySimulator(RobotSimulatorBase):
    async def initialize_simulation(self):
        # Initialize your simulator
        pass
    
    async def get_robot_positions(self):
        # Return [(robot_id, x, y, z), ...]
        pass
    
    async def step_simulation(self, dt):
        # Step your simulator
        pass

if __name__ == '__main__':
    MySimulator().run()
```

### Step 3: Add to docker-compose.yml
```yaml
robot-sim-mysim:
  build:
    context: ./external_simulators
    dockerfile: Dockerfile.mysim
  environment:
    ROBOT_ID_START: 150
    ROBOT_ID_END: 199
    NUM_ROBOTS: 50
    AGGREGATOR_HOST: position-aggregator
    AGGREGATOR_PORT: 10000
  networks:
    robot-network:
```

### Step 4: Update OMNeT++
```ini
# omnetpp.ini
*.numRobots = 200  # Now have 200 total robots
```

## Scaling

### Run Multiple Instances
```bash
# Scale Gazebo simulator to 3 instances
docker-compose up -d --scale robot-sim-gazebo-1=3
```

### Partition Robots
- Container 1: Robots 0-49 (Gazebo)
- Container 2: Robots 50-99 (SUMO)
- Container 3: Robots 100-149 (Custom)
- Add more as needed

## Performance Tips

1. **Batch Updates**: Use BATCH protocol for >10 robots
2. **Update Rate**: 10 Hz is optimal (set UPDATE_RATE_HZ=10)
3. **Network**: Use bridge network with fixed IPs
4. **Resources**: Allocate sufficient CPU/RAM per container

## Troubleshooting

### Positions not updating
```bash
# Check aggregator is receiving updates
docker-compose logs position-aggregator | grep "external_rate"

# Check external simulator is sending
docker-compose logs robot-sim-gazebo-1 | grep "Initialized"
```

### OMNeT++ can't connect
```bash
# Verify aggregator is listening
docker exec position-aggregator netstat -ulnp | grep 9999

# Test connection
docker exec omnetpp-sim nc -u position-aggregator 9999
```

### High CPU usage
- Reduce update rate: UPDATE_RATE_HZ=5
- Use headless mode in OMNeT++
- Disable visualization = GazeboSimulator()
  sim.run()

# ---------------------------------------------------------------------
# 5. Example: SUMO Traffic Simulator Implementation
# ---------------------------------------------------------------------
# FILE: external_simulators/Dockerfile.sumo
---
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
python3 \
python3-pip \
sumo \
sumo-tools

RUN pip3 install asyncio traci numpy

WORKDIR /workspace

COPY robot_sim_base.py .
COPY sumo_simulator.py .
COPY sumo_config.sumocfg .
COPY road_network.net.xml .

CMD ["python3", "sumo_simulator.py"]

# FILE: external_simulators/sumo_simulator.py
---
import asyncio
import traci
import numpy as np
from robot_sim_base import RobotSimulatorBase
from typing import List, Tuple

class SUMOSimulator(RobotSimulatorBase):
"""SUMO traffic simulator for robot vehicles"""

    def __init__(self):
        super().__init__()
        self.sumo_connection = None
    
    async def initialize_simulation(self):
        """Start SUMO and spawn vehicles"""
        print("Starting SUMO simulation...")
        
        # Start SUMO
        sumo_cmd = ["sumo", "-c", "sumo_config.sumocfg", "--start"]
        traci.start(sumo_cmd)
        self.sumo_connection = traci
        
        # Add vehicles (robots)
        for i in range(self.robot_id_start, self.robot_id_end + 1):
            vehicle_id = f"robot[{i}]"
            traci.vehicle.add(
                vehicle_id,
                routeID="route_0",
                typeID="robot_vehicle",
                departLane="best",
                departSpeed="max"
            )
        
        print(f"Spawned {self.num_robots} vehicles in SUMO")
    
    async def get_robot_positions(self) -> List[Tuple[str, float, float, float]]:
        """Get vehicle positions from SUMO"""
        positions = []
        
        for i in range(self.robot_id_start, self.robot_id_end + 1):
            vehicle_id = f"robot[{i}]"
            
            try:
                x, y = traci.vehicle.getPosition(vehicle_id)
                z = 0.0  # SUMO is 2D
                positions.append((vehicle_id, x, y, z))
            except traci.exceptions.TraCIException:
                # Vehicle not in network yet
                pass
        
        return positions
    
    async def step_simulation(self, dt: float):
        """Step SUMO simulation"""
        # SUMO steps in discrete time steps
        steps = max(1, int(dt * 10))  # SUMO default: 10 steps/second
        
        for _ in range(steps):
            traci.simulationStep()

if __name__ == '__main__':
sim