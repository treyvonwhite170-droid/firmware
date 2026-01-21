# Meshtastic Python Module API Reference

A comprehensive guide for implementing custom modules using the Meshtastic Python CLI library.

---

## Table of Contents

1. [Installation & Setup](#installation--setup)
2. [Connection Interfaces](#connection-interfaces)
3. [Core Messaging Functions](#core-messaging-functions)
4. [Event System (Pub/Sub)](#event-system-pubsub)
5. [Node Management Functions](#node-management-functions)
6. [Port Numbers (PortNum)](#port-numbers-portnum)
7. [Packet Structure](#packet-structure)
8. [Custom Module Implementation](#custom-module-implementation)
9. [Complete API Reference](#complete-api-reference)
10. [CLI Device Configuration](#cli-device-configuration)
    - [Connection Options](#connection-options)
    - [Device Configuration](#device-configuration-device)
    - [LoRa Configuration](#lora-configuration-lora)
    - [Power Configuration](#power-configuration-power)
    - [Position/GPS Configuration](#positiongps-configuration-position)
    - [Network Configuration](#network-configuration-network)
    - [Bluetooth Configuration](#bluetooth-configuration-bluetooth)
    - [Display Configuration](#display-configuration-display)
    - [Channel Configuration](#channel-configuration)
    - [Module Configuration](#module-configuration)
    - [Python API Configuration Examples](#python-api-configuration-examples)

---

## Installation & Setup

```bash
pip3 install --upgrade "meshtastic[cli]"
```

**Required imports for custom modules:**

```python
import meshtastic
import meshtastic.serial_interface
import meshtastic.tcp_interface
import meshtastic.ble_interface
from meshtastic.protobuf import portnums_pb2
from pubsub import pub
```

---

## Connection Interfaces

### SerialInterface

Connects via USB/Serial to a Meshtastic device.

```python
import meshtastic.serial_interface

# Auto-detect device
interface = meshtastic.serial_interface.SerialInterface()

# Specify device path
interface = meshtastic.serial_interface.SerialInterface(devPath='/dev/ttyUSB0')
```

**Parameters:**

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `devPath` | str | None | Device path (e.g., `/dev/ttyUSB0`, `COM4`) |
| `debugOut` | stream | None | Stream for debug output |
| `noProto` | bool | False | Disable protocol (raw serial mode) |
| `connectNow` | bool | True | Connect immediately |
| `noNodes` | bool | False | Skip node initialization |

### TCPInterface

Connects via WiFi/Network to a Meshtastic device.

```python
import meshtastic.tcp_interface

interface = meshtastic.tcp_interface.TCPInterface(hostname='192.168.1.100')
```

**Parameters:**

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `hostname` | str | Required | IP address or hostname |
| `portNumber` | int | 4403 | TCP port number |
| `debugOut` | stream | None | Stream for debug output |
| `noProto` | bool | False | Disable protocol |
| `connectNow` | bool | True | Connect immediately |
| `noNodes` | bool | False | Skip node initialization |

### BLEInterface

Connects via Bluetooth Low Energy.

```python
import meshtastic.ble_interface

# Scan for devices
devices = meshtastic.ble_interface.BLEInterface.scan()

# Connect by address
interface = meshtastic.ble_interface.BLEInterface(address='AA:BB:CC:DD:EE:FF')
```

---

## Core Messaging Functions

### sendText()

Sends a UTF-8 text message to the mesh network.

```python
# Broadcast to all nodes
interface.sendText("Hello everyone!")

# Send to specific node
interface.sendText("Hello!", destinationId="!abcd1234")

# With acknowledgment
interface.sendText("Important message", wantAck=True)

# On specific channel
interface.sendText("Channel 2 message", channelIndex=2)

# With response callback
def on_response(packet):
    print(f"Got response: {packet}")

interface.sendText("Ping", wantResponse=True, onResponse=on_response)
```

**Parameters:**

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `text` | str | Required | UTF-8 message content |
| `destinationId` | int/str | BROADCAST_ADDR | Target node ID or `^all` |
| `wantAck` | bool | False | Request delivery acknowledgment |
| `wantResponse` | bool | False | Request application-layer response |
| `onResponse` | callable | None | Callback for response packets |
| `channelIndex` | int | 0 | Channel number to use |
| `portNum` | int | TEXT_MESSAGE_APP | Application port |

**Returns:** Sent packet with populated `id` field for tracking.

---

### sendData()

Sends binary data or protobuf messages.

```python
from meshtastic.protobuf import portnums_pb2

# Send raw bytes
interface.sendData(
    data=b'\x01\x02\x03\x04',
    destinationId="!abcd1234",
    portNum=portnums_pb2.PortNum.PRIVATE_APP
)

# Send with options
interface.sendData(
    data=my_protobuf_message,
    destinationId="!abcd1234",
    portNum=portnums_pb2.PortNum.PRIVATE_APP,
    wantAck=True,
    hopLimit=5,
    channelIndex=1
)
```

**Parameters:**

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `data` | bytes/protobuf | Required | Payload data |
| `destinationId` | int/str | BROADCAST_ADDR | Target node |
| `portNum` | PortNum | Required | Application port number |
| `wantAck` | bool | False | Reliable delivery mode |
| `wantResponse` | bool | False | Request response |
| `onResponse` | callable | None | Response callback |
| `onResponseAckPermitted` | bool | False | Include ACK in callbacks |
| `channelIndex` | int | 0 | Channel identifier |
| `hopLimit` | int | None | Maximum relay hops |
| `pkiEncrypted` | bool | False | Enable PKI encryption |
| `publicKey` | bytes | None | Recipient's public key |
| `priority` | int | None | Message priority level |

**Returns:** Mesh packet with assigned ID.

**Note:** Maximum payload size is defined by `mesh_pb2.Constants.DATA_PAYLOAD_LEN` (237 bytes).

---

### sendPosition()

Broadcasts geographic coordinates.

```python
interface.sendPosition(
    latitude=37.7749,
    longitude=-122.4194,
    altitude=10
)

# To specific node
interface.sendPosition(
    latitude=37.7749,
    longitude=-122.4194,
    altitude=10,
    destinationId="!abcd1234",
    wantAck=True
)
```

**Parameters:**

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `latitude` | float | Required | Geographic latitude |
| `longitude` | float | Required | Geographic longitude |
| `altitude` | int | 0 | Elevation in meters |
| `destinationId` | int/str | BROADCAST_ADDR | Target node |
| `wantAck` | bool | False | Reliable delivery |
| `wantResponse` | bool | False | Request response |
| `channelIndex` | int | 0 | Channel number |

**Returns:** Position packet with ID field.

**Note:** Latitude/longitude are converted to integers internally (multiplied by 1e7).

---

### sendTelemetry()

Transmits device telemetry data.

```python
# Send device metrics
interface.sendTelemetry(
    destinationId="!abcd1234",
    telemetryType="device_metrics"
)
```

**Parameters:**

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `destinationId` | int/str | BROADCAST_ADDR | Target node |
| `wantResponse` | bool | False | Request response |
| `channelIndex` | int | 0 | Channel identifier |
| `telemetryType` | str | Required | Type of telemetry |

**Telemetry Types:**

- `"device_metrics"` - Battery, voltage, channel utilization, uptime
- `"environment_metrics"` - Temperature, humidity, pressure
- `"air_quality_metrics"` - Air quality sensor data
- `"power_metrics"` - Power consumption data
- `"local_stats"` - Local device statistics

**Returns:** None (publishes asynchronously).

---

### sendWaypoint()

Broadcasts location markers.

```python
interface.sendWaypoint(
    name="Base Camp",
    description="Meeting point",
    latitude=37.7749,
    longitude=-122.4194,
    expire=int(time.time()) + 86400  # 24 hours
)
```

**Parameters:**

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `name` | str | Required | Waypoint identifier |
| `description` | str | "" | Location details |
| `expire` | int | Required | Expiration timestamp (Unix) |
| `waypoint_id` | int | None | Custom waypoint ID (auto-generated if None) |
| `latitude` | float | Required | Location latitude |
| `longitude` | float | Required | Location longitude |
| `destinationId` | int/str | BROADCAST_ADDR | Target node |
| `wantAck` | bool | True | Reliable delivery |
| `wantResponse` | bool | False | Request response |
| `channelIndex` | int | 0 | Channel number |

**Returns:** Waypoint packet with ID field.

---

### deleteWaypoint()

Removes a previously created waypoint.

```python
interface.deleteWaypoint(
    waypoint_id=123456,
    destinationId="!abcd1234"
)
```

**Parameters:**

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `waypoint_id` | int | Required | Waypoint ID to remove |
| `destinationId` | int/str | BROADCAST_ADDR | Target node |
| `wantAck` | bool | True | Reliable delivery |
| `wantResponse` | bool | False | Request response |
| `channelIndex` | int | 0 | Channel number |

**Returns:** Deletion packet with ID field.

---

### sendAlert()

Sends high-priority alert messages.

```python
interface.sendAlert(
    text="Emergency alert!",
    destinationId="!abcd1234"
)
```

**Parameters:**

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `text` | str | Required | Alert message |
| `destinationId` | int/str | BROADCAST_ADDR | Target node |
| `onResponse` | callable | None | Response handler |
| `channelIndex` | int | 0 | Channel number |

**Returns:** Sent packet with ID field.

---

### sendTraceRoute()

Discovers routing path to a destination node.

```python
interface.sendTraceRoute(
    dest="!abcd1234",
    hopLimit=7,
    channelIndex=0
)
```

**Parameters:**

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `dest` | int/str | Required | Target node |
| `hopLimit` | int | Required | Maximum relay hops |
| `channelIndex` | int | 0 | Channel identifier |

**Returns:** None (results published via events).

---

## Event System (Pub/Sub)

The library uses a publish-subscribe pattern for handling asynchronous events.

### Subscribing to Events

```python
from pubsub import pub

def on_receive(packet, interface):
    """Called when any packet is received"""
    print(f"Received: {packet}")

def on_text(packet, interface):
    """Called for text messages specifically"""
    print(f"Text: {packet['decoded']['text']}")

def on_connection(interface, topic=pub.AUTO_TOPIC):
    """Called when connected to device"""
    print("Connected!")

def on_disconnect(interface, topic=pub.AUTO_TOPIC):
    """Called when connection is lost"""
    print("Disconnected!")

# Subscribe to events
pub.subscribe(on_receive, "meshtastic.receive")
pub.subscribe(on_text, "meshtastic.receive.text")
pub.subscribe(on_connection, "meshtastic.connection.established")
pub.subscribe(on_disconnect, "meshtastic.connection.lost")
```

### Available Event Topics

| Topic | Description | Callback Args |
|-------|-------------|---------------|
| `meshtastic.connection.established` | Radio connection successful | `interface`, `topic` |
| `meshtastic.connection.lost` | Connection terminated | `interface`, `topic` |
| `meshtastic.receive` | Any packet received | `packet`, `interface` |
| `meshtastic.receive.text` | Text message received | `packet`, `interface` |
| `meshtastic.receive.position` | Position update received | `packet`, `interface` |
| `meshtastic.receive.user` | User info received | `packet`, `interface` |
| `meshtastic.receive.data.portnum` | Data on specific port | `packet`, `interface` |
| `meshtastic.node.updated` | Node database updated | `node` |
| `meshtastic.log.line` | Debug log line | `line` |

### Custom Port Event Handling

```python
# Listen for data on PRIVATE_APP port
def on_private_data(packet, interface):
    payload = packet['decoded']['data']['payload']
    print(f"Private data: {payload}")

pub.subscribe(on_private_data, "meshtastic.receive.data.PRIVATE_APP")
```

---

## Node Management Functions

### getNode()

Retrieves a node object with device settings.

```python
# Get local node
local_node = interface.getNode('^local')

# Get remote node
remote_node = interface.getNode('!abcd1234')

# With options
node = interface.getNode(
    nodeId='!abcd1234',
    requestChannels=True,
    requestChannelAttempts=3,
    timeout=300
)
```

**Parameters:**

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `nodeId` | str | Required | Node identifier |
| `requestChannels` | bool | True | Fetch channel configuration |
| `requestChannelAttempts` | int | 3 | Retry count |
| `timeout` | int | 300 | Operation timeout (seconds) |

**Returns:** Node object with settings and channels.

---

### showNodes()

Displays nodes in the mesh network.

```python
# Show all nodes
table = interface.showNodes()
print(table)

# Exclude self
table = interface.showNodes(includeSelf=False)

# Specific fields only
table = interface.showNodes(showFields=['user', 'position', 'snr'])
```

**Parameters:**

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `includeSelf` | bool | True | Include local node |
| `showFields` | list | None | Specific fields to display |

**Returns:** Formatted table string.

---

### getMyNodeInfo()

Gets local node information.

```python
my_info = interface.getMyNodeInfo()
print(f"Node ID: {my_info['user']['id']}")
print(f"Long Name: {my_info['user']['longName']}")
```

**Returns:** Dictionary containing local node information or None.

---

### Interface Properties

```python
# Node databases
interface.nodes        # Dict of nodes by ID (read-only)
interface.nodesByNum   # Dict of nodes by number

# Local device info
interface.myInfo       # MyNodeInfo protobuf
interface.metadata     # DeviceMetadata protobuf
interface.localNode    # Local node object

# Connection status
interface.isConnected  # Threading event
```

---

## Port Numbers (PortNum)

Port numbers identify the application type for data packets.

### Import

```python
from meshtastic.protobuf import portnums_pb2
```

### Core Port Numbers

| Port Name | Value | Description |
|-----------|-------|-------------|
| `UNKNOWN_APP` | 0 | Deprecated |
| `TEXT_MESSAGE_APP` | 1 | UTF-8 text messages |
| `REMOTE_HARDWARE_APP` | 2 | GPIO/remote hardware control |
| `POSITION_APP` | 3 | Position/location data |
| `NODEINFO_APP` | 4 | User/node information |
| `ROUTING_APP` | 5 | Mesh protocol control |
| `ADMIN_APP` | 6 | Administrative control |
| `TEXT_MESSAGE_COMPRESSED_APP` | 7 | Compressed text (Unishox2) |
| `WAYPOINT_APP` | 8 | Waypoint data |
| `AUDIO_APP` | 9 | Codec2 audio frames |
| `DETECTION_SENSOR_APP` | 10 | Detection sensor messages |
| `ALERT_APP` | 11 | Critical alerts |
| `KEY_VERIFICATION_APP` | 12 | Key verification requests |
| `REPLY_APP` | 32 | Ping/reply service |
| `IP_TUNNEL_APP` | 33 | IP tunneling |
| `PAXCOUNTER_APP` | 34 | Paxcounter data |

### Third-Party Port Numbers (64-127)

| Port Name | Value | Description |
|-----------|-------|-------------|
| `SERIAL_APP` | 64 | Hardware serial interface |
| `STORE_FORWARD_APP` | 65 | Store and forward module |
| `RANGE_TEST_APP` | 66 | Range testing |
| `TELEMETRY_APP` | 67 | Telemetry data |
| `ZPS_APP` | 68 | GPS-less position estimation |
| `SIMULATOR_APP` | 69 | Simulator communication |
| `TRACEROUTE_APP` | 70 | Packet route discovery |
| `NEIGHBORINFO_APP` | 71 | Network neighbor info |
| `ATAK_PLUGIN` | 72 | ATAK integration |
| `MAP_REPORT_APP` | 73 | Map reporting |
| `POWERSTRESS_APP` | 74 | Power consumption testing |
| `RETICULUM_TUNNEL_APP` | 76 | Reticulum network tunnel |
| `CAYENNE_APP` | 77 | Cayenne LPP support |

### Private Port Numbers (256-511)

| Port Name | Value | Description |
|-----------|-------|-------------|
| `PRIVATE_APP` | 256 | Private application base |
| `ATAK_FORWARDER` | 257 | ATAK forwarder |
| `MAX` | 511 | Maximum port number |

### Usage Example

```python
from meshtastic.protobuf import portnums_pb2

# Use in sendData
interface.sendData(
    data=b'\x01\x02\x03',
    portNum=portnums_pb2.PortNum.PRIVATE_APP
)

# Custom private port
MY_APP_PORT = 300  # Within 256-511 range
interface.sendData(
    data=my_data,
    portNum=MY_APP_PORT
)
```

### Port Number Allocation Rules

| Range | Usage |
|-------|-------|
| 0-63 | Core Meshtastic - **DO NOT USE** |
| 64-127 | Registered 3rd party apps (requires PR) |
| 256-511 | Private applications (no registration) |
| Other | Reserved |

---

## Packet Structure

Received packets are dictionaries with the following structure:

```python
{
    'from': 1234567890,          # Sender node number
    'to': 4294967295,            # Destination (0xFFFFFFFF = broadcast)
    'fromId': '!abcd1234',       # Sender ID string
    'toId': '^all',              # Destination ID string
    'rxTime': 1699123456,        # Reception timestamp (Unix)
    'rxSnr': 10.5,               # Signal-to-noise ratio
    'rxRssi': -80,               # RSSI value
    'hopLimit': 3,               # Remaining hops
    'hopStart': 3,               # Original hop limit
    'priority': 'DEFAULT',       # Message priority
    'channel': 0,                # Channel index
    'decoded': {
        'portnum': 'TEXT_MESSAGE_APP',
        'text': 'Hello!',        # For text messages
        'data': {
            'portnum': 1,
            'payload': b'\x01\x02'  # Raw bytes
        },
        'position': {            # For position packets
            'latitude': 37.7749,
            'longitude': -122.4194,
            'altitude': 10
        }
    }
}
```

---

## Custom Module Implementation

### Basic Module Template

```python
#!/usr/bin/env python3
"""
Custom Meshtastic Module Template
"""

import time
import meshtastic
import meshtastic.serial_interface
from meshtastic.protobuf import portnums_pb2
from pubsub import pub


class CustomMeshtasticModule:
    """Base class for custom Meshtastic modules"""

    # Define your private port number (256-511)
    PORT_NUM = 300

    def __init__(self, device_path=None):
        self.interface = None
        self.device_path = device_path
        self.running = False

    def on_receive(self, packet, interface):
        """Handle incoming packets"""
        decoded = packet.get('decoded', {})
        portnum = decoded.get('portnum')

        # Handle our custom port
        if portnum == self.PORT_NUM or portnum == 'PRIVATE_APP':
            self._handle_custom_data(packet)

        # Handle text messages
        elif portnum == 'TEXT_MESSAGE_APP':
            self._handle_text(packet)

    def _handle_custom_data(self, packet):
        """Process custom data packets"""
        payload = packet['decoded']['data']['payload']
        from_id = packet.get('fromId', 'unknown')
        print(f"Custom data from {from_id}: {payload.hex()}")

        # Process your custom protocol here
        self.process_data(payload, from_id)

    def _handle_text(self, packet):
        """Process text messages"""
        text = packet['decoded'].get('text', '')
        from_id = packet.get('fromId', 'unknown')
        print(f"Text from {from_id}: {text}")

        # Handle commands
        if text.startswith('/'):
            self.process_command(text, from_id)

    def process_data(self, payload: bytes, from_id: str):
        """Override this to handle custom data"""
        pass

    def process_command(self, command: str, from_id: str):
        """Override this to handle text commands"""
        pass

    def on_connection(self, interface, topic=pub.AUTO_TOPIC):
        """Called when connected to device"""
        print(f"Connected to {interface.getLongName()}")
        self.on_ready()

    def on_disconnect(self, interface, topic=pub.AUTO_TOPIC):
        """Called when connection is lost"""
        print("Disconnected from device")
        self.running = False

    def on_ready(self):
        """Override this for post-connection initialization"""
        pass

    def connect(self):
        """Establish connection to device"""
        # Subscribe to events
        pub.subscribe(self.on_receive, "meshtastic.receive")
        pub.subscribe(self.on_connection, "meshtastic.connection.established")
        pub.subscribe(self.on_disconnect, "meshtastic.connection.lost")

        # Connect
        if self.device_path:
            self.interface = meshtastic.serial_interface.SerialInterface(
                devPath=self.device_path
            )
        else:
            self.interface = meshtastic.serial_interface.SerialInterface()

        self.running = True

    def send_text(self, text: str, destination: str = None, channel: int = 0):
        """Send a text message"""
        self.interface.sendText(
            text,
            destinationId=destination,
            channelIndex=channel
        )

    def send_data(self, data: bytes, destination: str = None,
                  channel: int = 0, want_ack: bool = False):
        """Send custom binary data"""
        self.interface.sendData(
            data=data,
            destinationId=destination if destination else meshtastic.BROADCAST_ADDR,
            portNum=self.PORT_NUM,
            channelIndex=channel,
            wantAck=want_ack
        )

    def send_position(self, lat: float, lon: float, alt: int = 0):
        """Send position update"""
        self.interface.sendPosition(
            latitude=lat,
            longitude=lon,
            altitude=alt
        )

    def get_nodes(self) -> dict:
        """Get all known nodes"""
        return self.interface.nodes

    def run(self):
        """Main run loop"""
        try:
            while self.running:
                self.loop()
                time.sleep(0.1)
        except KeyboardInterrupt:
            print("\nShutting down...")
        finally:
            self.close()

    def loop(self):
        """Override for periodic tasks"""
        pass

    def close(self):
        """Clean up connection"""
        if self.interface:
            self.interface.close()
            self.interface = None


# Example: Sensor Data Module
class SensorModule(CustomMeshtasticModule):
    """Example module that sends sensor data"""

    PORT_NUM = 301

    def __init__(self, device_path=None):
        super().__init__(device_path)
        self.last_send = 0
        self.send_interval = 60  # seconds

    def on_ready(self):
        """Announce presence on connection"""
        self.send_text("Sensor module online!")

    def process_command(self, command: str, from_id: str):
        """Handle incoming commands"""
        cmd = command.lower().strip()

        if cmd == '/status':
            self.send_text(f"Sensor active. Last reading: {self.get_reading()}")
        elif cmd == '/read':
            reading = self.get_reading()
            self.send_data(reading.to_bytes(4, 'big'), destination=from_id)

    def get_reading(self) -> int:
        """Get sensor reading (override for real sensor)"""
        import random
        return random.randint(0, 1000)

    def loop(self):
        """Periodic sensor broadcast"""
        now = time.time()
        if now - self.last_send >= self.send_interval:
            reading = self.get_reading()
            # Broadcast sensor data
            self.send_data(reading.to_bytes(4, 'big'))
            print(f"Broadcast reading: {reading}")
            self.last_send = now


# Example: Command Relay Module
class CommandRelayModule(CustomMeshtasticModule):
    """Example module that relays commands between nodes"""

    def __init__(self, device_path=None):
        super().__init__(device_path)
        self.authorized_nodes = set()

    def process_command(self, command: str, from_id: str):
        """Process relay commands"""
        parts = command.split()
        if len(parts) < 1:
            return

        cmd = parts[0].lower()

        if cmd == '/ping':
            self.send_text(f"Pong!", destination=from_id)

        elif cmd == '/nodes':
            nodes = self.get_nodes()
            node_list = [n.get('user', {}).get('longName', 'Unknown')
                        for n in nodes.values()]
            self.send_text(f"Nodes: {', '.join(node_list)}", destination=from_id)

        elif cmd == '/relay' and len(parts) >= 3:
            target = parts[1]
            message = ' '.join(parts[2:])
            self.send_text(message, destination=target)


if __name__ == "__main__":
    # Run the sensor module
    module = SensorModule()
    module.connect()
    module.run()
```

---

## Complete API Reference

### MeshInterface Methods

| Method | Description |
|--------|-------------|
| `sendText(text, ...)` | Send UTF-8 text message |
| `sendData(data, ...)` | Send binary data packet |
| `sendPosition(lat, lon, alt, ...)` | Send position update |
| `sendTelemetry(...)` | Send telemetry data |
| `sendWaypoint(...)` | Create waypoint marker |
| `deleteWaypoint(...)` | Remove waypoint |
| `sendAlert(text, ...)` | Send high-priority alert |
| `sendTraceRoute(dest, ...)` | Discover route to node |
| `getNode(nodeId, ...)` | Get node object |
| `showNodes(...)` | Display node table |
| `getMyNodeInfo()` | Get local node info |
| `getMyUser()` | Get local user info |
| `getLongName()` | Get local long name |
| `getShortName()` | Get local short name |
| `getPublicKey()` | Get local public key |
| `waitForConfig()` | Wait for config download |
| `close()` | Close connection |

### Node Object Methods

| Method | Description |
|--------|-------------|
| `writeConfig(section)` | Write configuration section |
| `writeChannel(index)` | Write channel configuration |

### Node Object Properties

| Property | Description |
|----------|-------------|
| `localConfig` | Device configuration |
| `moduleConfig` | Module configuration |
| `channels` | Channel configurations |

### Special Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `BROADCAST_ADDR` | `"^all"` | Send to all nodes |
| `LOCAL_ADDR` | `"^local"` | Reference local device |
| `BROADCAST_NUM` | `0xFFFFFFFF` | Numeric broadcast ID |

---

## CLI Device Configuration

The Meshtastic Python CLI provides comprehensive device configuration capabilities. This section documents all configuration commands and options.

### Connection Options

```bash
# Auto-detect device
meshtastic --info

# Specify serial port
meshtastic --port /dev/ttyUSB0 --info
meshtastic --port COM4 --info

# Connect via TCP/WiFi
meshtastic --host 192.168.1.100 --info

# Connect via Bluetooth
meshtastic --ble-scan                    # Scan for BLE devices
meshtastic --ble Meshtastic_1234 --info  # Connect by name
meshtastic --ble AA:BB:CC:DD:EE:FF --info # Connect by address
```

### Device Information Commands

```bash
# Display device info
meshtastic --info

# List all nodes in mesh
meshtastic --nodes

# Get all preferences (list available options)
meshtastic --get all

# Get specific configuration section
meshtastic --get lora
meshtastic --get device
meshtastic --get position
meshtastic --get power
meshtastic --get network
meshtastic --get bluetooth
meshtastic --get display

# Get specific setting
meshtastic --get lora.region
meshtastic --get device.role
```

### Configuration Export/Import

```bash
# Export full device configuration to YAML
meshtastic --export-config > my_device_config.yaml

# Import configuration from YAML file
meshtastic --configure my_device_config.yaml

# Set configuration via URL (from QR code)
meshtastic --seturl "https://meshtastic.org/e/#..."
```

---

### Device Configuration (device.*)

Configure basic device behavior and identity.

```bash
# Set device owner name (long name)
meshtastic --set-owner "My Node Name"

# Set short name (max 4 characters)
meshtastic --set-owner-short "NODE"

# Set as ham radio operator (disables encryption)
meshtastic --set-ham "KD1ABC"
```

#### Device Roles

```bash
meshtastic --set device.role CLIENT
```

| Role | Description |
|------|-------------|
| `CLIENT` | Standard messaging with rebroadcasting (default) |
| `CLIENT_MUTE` | Receives but doesn't rebroadcast |
| `CLIENT_HIDDEN` | Hidden from node list, prioritizes GPS |
| `TRACKER` | Optimized for location broadcasting |
| `LOST_AND_FOUND` | Regularly broadcasts location for recovery |
| `SENSOR` | Prioritizes telemetry data |
| `TAK` | ATAK system integration |
| `TAK_TRACKER` | Automatic tactical PLI broadcasts |
| `REPEATER` | Infrastructure node (hidden from list) |
| `ROUTER` | Infrastructure node (visible) |
| `ROUTER_LATE` | Rebroadcasts after other nodes |

#### Rebroadcast Modes

```bash
meshtastic --set device.rebroadcast_mode ALL
```

| Mode | Description |
|------|-------------|
| `ALL` | Rebroadcasts all messages (default) |
| `ALL_SKIP_DECODING` | Rebroadcasts without decoding |
| `LOCAL_ONLY` | Ignores messages from other meshes |
| `KNOWN_ONLY` | Only rebroadcasts from known nodes |
| `NONE` | Disables rebroadcasting |
| `CORE_PORTNUMS_ONLY` | Only standard portnum packets |

#### Other Device Settings

```bash
# Node info broadcast interval (seconds, default: 10800 = 3 hours)
meshtastic --set device.node_info_broadcast_secs 10800

# Disable double-tap detection
meshtastic --set device.double_tap_as_button_press false

# Set timezone (TZ database format)
meshtastic --set device.tzdef "EST5EDT,M3.2.0,M11.1.0"

# Disable LED heartbeat
meshtastic --set device.led_heartbeat_disabled true
```

---

### LoRa Configuration (lora.*)

Configure radio parameters for your region and use case.

#### Region Setting (Required)

```bash
meshtastic --set lora.region US
```

| Region | Frequency | Notes |
|--------|-----------|-------|
| `US` | 902-928 MHz | North America |
| `EU_868` | 869.4-869.65 MHz | Europe |
| `EU_433` | 433-434 MHz | Europe (433 MHz) |
| `CN` | 470-510 MHz | China |
| `JP` | 920-925 MHz | Japan |
| `ANZ` | 915-928 MHz | Australia/New Zealand |
| `IN` | 865-867 MHz | India |
| `KR` | 920-923 MHz | Korea |
| `TW` | 920-925 MHz | Taiwan |
| `RU` | 868-869 MHz | Russia |
| `UA` | 868-869 MHz | Ukraine |
| `LORA_24` | 2.4 GHz | Worldwide (2.4 GHz) |
| `UNSET` | - | Not configured |

#### Modem Presets

```bash
meshtastic --set lora.modem_preset LONG_FAST
```

| Preset | Speed | Range | Use Case |
|--------|-------|-------|----------|
| `SHORT_TURBO` | Fastest | Shortest | High-speed, short range |
| `SHORT_FAST` | Very Fast | Short | Quick messaging nearby |
| `SHORT_SLOW` | Fast | Short | Balanced short range |
| `MEDIUM_FAST` | Medium | Medium | General use |
| `MEDIUM_SLOW` | Slower | Medium | Better range, medium speed |
| `LONG_FAST` | Default | Long | **Default** - good balance |
| `LONG_MODERATE` | Slow | Longer | Extended range |
| `LONG_SLOW` | Very Slow | Very Long | Maximum range |
| `VERY_LONG_SLOW` | Slowest | Maximum | Extreme range |

#### Advanced Radio Parameters

```bash
# Bandwidth (kHz): 31, 62, 125, 250, 500
meshtastic --set lora.bandwidth 250

# Spreading Factor: 7-12 (higher = longer range, slower)
meshtastic --set lora.spread_factor 12

# Coding Rate: 5-8 (higher = more error correction)
meshtastic --set lora.coding_rate 8

# Hop Limit: 1-7 (max relay hops, default: 3)
meshtastic --set lora.hop_limit 3

# TX Power: 0-30 dBm (0 = use legal max)
meshtastic --set lora.tx_power 0

# Enable/disable transmit
meshtastic --set lora.tx_enabled true

# Frequency offset (Hz)
meshtastic --set lora.frequency_offset 0
```

---

### Power Configuration (power.*)

Optimize power consumption for battery-powered deployments.

```bash
# Enable power saving mode (disables BT, Serial, WiFi, Screen)
meshtastic --set power.is_power_saving true

# Shutdown after losing external power (seconds, 0 = disabled)
meshtastic --set power.on_battery_shutdown_after_secs 0

# Light sleep interval - ESP32 only (seconds, default: 300)
meshtastic --set power.ls_secs 300

# Minimum wake time after receiving packet (seconds, default: 10)
meshtastic --set power.min_wake_secs 10

# Bluetooth timeout when inactive (seconds, default: 60)
meshtastic --set power.wait_bluetooth_secs 60

# Keep Bluetooth alive for 8 hours
meshtastic --set power.wait_bluetooth_secs 28800

# ADC multiplier for battery voltage (2.0-6.0)
meshtastic --set power.adc_multiplier_override 2.0
```

---

### Position/GPS Configuration (position.*)

Configure GPS and position broadcasting behavior.

```bash
# Set fixed position (disables GPS updates)
meshtastic --setlat 37.7749 --setlon -122.4194 --setalt 10

# GPS Mode: ENABLED, DISABLED, NOT_PRESENT
meshtastic --set position.gps_mode ENABLED

# GPS update interval (seconds, default: 120)
meshtastic --set position.gps_update_interval 120

# Position broadcast interval (seconds, default: 900 = 15 min)
meshtastic --set position.position_broadcast_secs 900

# Enable smart broadcast (sends more often when moving)
meshtastic --set position.position_broadcast_smart_enabled true

# Smart broadcast minimum distance (meters, default: 100)
meshtastic --set position.broadcast_smart_minimum_distance 100

# Smart broadcast minimum interval (seconds, default: 30)
meshtastic --set position.broadcast_smart_minimum_interval_secs 30

# Fixed position mode
meshtastic --set position.fixed_position true
```

#### Position Flags

Control what data is included in position broadcasts:

```bash
# Enable altitude
meshtastic --set position.position_flags 1

# Common flag combinations (bitfield):
# ALTITUDE = 1, ALTITUDE_MSL = 2, GEOIDAL_SEPARATION = 4
# DOP = 8, HVDOP = 16, SATINVIEW = 32, SEQ_NO = 64
# TIMESTAMP = 128, HEADING = 256, SPEED = 512
```

---

### Network Configuration (network.*)

Configure WiFi and Ethernet connectivity.

```bash
# Enable WiFi (Note: disables Bluetooth)
meshtastic --set network.wifi_enabled true
meshtastic --set network.wifi_ssid "MyNetwork"
meshtastic --set network.wifi_psk "MyPassword"

# Full WiFi setup in one command
meshtastic --set network.wifi_enabled 1 \
           --set network.wifi_ssid "MyNetwork" \
           --set network.wifi_psk "MyPassword"

# Enable Ethernet
meshtastic --set network.eth_enabled true

# IPv4 Mode: DHCP or STATIC
meshtastic --set network.address_mode DHCP

# Static IP configuration
meshtastic --set network.address_mode STATIC
meshtastic --set network.ipv4_config.ip 192.168.1.100
meshtastic --set network.ipv4_config.gateway 192.168.1.1
meshtastic --set network.ipv4_config.subnet 255.255.255.0
meshtastic --set network.ipv4_config.dns 8.8.8.8

# NTP server (default: meshtastic.pool.ntp.org)
meshtastic --set network.ntp_server "pool.ntp.org"

# Rsyslog server for remote logging
meshtastic --set network.rsyslog_server "192.168.1.50"
```

---

### Bluetooth Configuration (bluetooth.*)

Configure Bluetooth connectivity.

```bash
# Enable/disable Bluetooth
meshtastic --set bluetooth.enabled true

# Bluetooth pairing mode: RANDOM_PIN, FIXED_PIN, NO_PIN
meshtastic --set bluetooth.mode RANDOM_PIN

# Set fixed PIN (when using FIXED_PIN mode)
meshtastic --set bluetooth.fixed_pin 123456
```

---

### Display Configuration (display.*)

Configure screen and UI settings.

```bash
# Screen timeout (seconds, 0 = always on)
meshtastic --set display.screen_on_secs 60

# Auto-carousel interval (seconds)
meshtastic --set display.auto_screen_carousel_secs 10

# Compass north orientation: DEGREES_0, DEGREES_90, DEGREES_180, DEGREES_270
meshtastic --set display.compass_north_top true

# Flip screen
meshtastic --set display.flip_screen true

# Display units: METRIC, IMPERIAL
meshtastic --set display.units METRIC

# OLED type: OLED_AUTO, OLED_SSD1306, OLED_SH1106, OLED_SH1107
meshtastic --set display.oled OLED_AUTO
```

---

### Channel Configuration

Channels define encryption keys and communication groups.

```bash
# View channel info
meshtastic --info

# Set channel name (on channel index 1)
meshtastic --ch-set name "MyChannel" --ch-index 1

# Apply modem preset to channel
meshtastic --ch-longslow      # Long range, slow
meshtastic --ch-longfast      # Long range, fast (default)
meshtastic --ch-medslow       # Medium range, slow
meshtastic --ch-medfast       # Medium range, fast
meshtastic --ch-shortslow     # Short range, slow
meshtastic --ch-shortfast     # Short range, fast

# Add a secondary channel
meshtastic --ch-add "SecureChannel"

# Delete a channel
meshtastic --ch-del --ch-index 2
```

#### Channel Encryption (PSK)

```bash
# Set random AES256 key
meshtastic --ch-set psk random --ch-index 0

# Set default AES128 key
meshtastic --ch-set psk default --ch-index 0

# Disable encryption
meshtastic --ch-set psk none --ch-index 0

# Set custom key (base64)
meshtastic --ch-set psk base64:puavdd7vtYJh8NUVWgxbsoG2u9Sdqc54YvMLs+KNcMA= --ch-index 0

# Set custom key (hex)
meshtastic --ch-set psk 0x1a1a1a1a2b2b2b2b1a1a1a1a2b2b2b2b1a1a1a1a2b2b2b2b1a1a1a1a2b2b2b2b --ch-index 0
```

#### Channel Settings

```bash
# Uplink enabled (send to MQTT)
meshtastic --ch-set uplink_enabled true --ch-index 0

# Downlink enabled (receive from MQTT)
meshtastic --ch-set downlink_enabled true --ch-index 0
```

---

### Module Configuration

#### MQTT Module (mqtt.*)

```bash
# Enable MQTT
meshtastic --set mqtt.enabled true

# Server settings
meshtastic --set mqtt.address "mqtt.example.com"
meshtastic --set mqtt.username "user"
meshtastic --set mqtt.password "pass"

# Enable TLS
meshtastic --set mqtt.tls_enabled true

# Enable encryption for MQTT messages
meshtastic --set mqtt.encryption_enabled true

# Enable JSON output (not supported on nRF52)
meshtastic --set mqtt.json_enabled true

# Custom root topic
meshtastic --set mqtt.root "mymesh"

# Map reporting (v2.3.2+)
meshtastic --set mqtt.map_reporting_enabled true
meshtastic --set mqtt.map_publish_interval_secs 3600
```

#### Telemetry Module (telemetry.*)

```bash
# Device metrics interval (seconds, default: 1800)
meshtastic --set telemetry.device_update_interval 1800

# Enable environment telemetry
meshtastic --set telemetry.environment_measurement_enabled true
meshtastic --set telemetry.environment_update_interval 1800

# Display temperature in Fahrenheit
meshtastic --set telemetry.environment_display_fahrenheit true

# Enable air quality metrics
meshtastic --set telemetry.air_quality_enabled true
meshtastic --set telemetry.air_quality_interval 1800

# Enable power metrics
meshtastic --set telemetry.power_measurement_enabled true
meshtastic --set telemetry.power_update_interval 1800
```

#### Serial Module (serial.*)

```bash
# Enable serial module
meshtastic --set serial.enabled true

# Mode: SIMPLE, PROTO, TEXTMSG, NMEA, CALTOPO, WS85
meshtastic --set serial.mode TEXTMSG

# Baud rate
meshtastic --set serial.baud BAUD_115200

# GPIO pins (1-39 for RX, 1-33 for TX)
meshtastic --set serial.rxd 16
meshtastic --set serial.txd 17

# Timeout (milliseconds, 0 = 250ms default)
meshtastic --set serial.timeout 0

# Echo received packets
meshtastic --set serial.echo true
```

#### Range Test Module (range_test.*)

```bash
# Enable range test
meshtastic --set range_test.enabled true

# Sender interval (seconds)
meshtastic --set range_test.sender 30

# Save to file (ESP32 with SD card)
meshtastic --set range_test.save true
```

#### Store & Forward Module (store_forward.*)

```bash
# Enable store & forward
meshtastic --set store_forward.enabled true

# Heartbeat interval (seconds)
meshtastic --set store_forward.heartbeat true

# Number of records to store
meshtastic --set store_forward.records 100

# History return max messages
meshtastic --set store_forward.history_return_max 100

# History return window (seconds)
meshtastic --set store_forward.history_return_window 7200
```

#### External Notification Module (external_notification.*)

```bash
# Enable external notification
meshtastic --set external_notification.enabled true

# GPIO pin for output
meshtastic --set external_notification.output 13

# Output duration (milliseconds)
meshtastic --set external_notification.output_ms 1000

# Active high/low
meshtastic --set external_notification.active true

# Alert on message
meshtastic --set external_notification.alert_message true

# Alert on bell character
meshtastic --set external_notification.alert_bell true

# Use PWM buzzer
meshtastic --set external_notification.use_pwm true
```

#### Canned Message Module (canned_message.*)

```bash
# Enable canned messages
meshtastic --set canned_message.enabled true

# Set messages (pipe-separated)
meshtastic --set canned_message.messages "Help|OK|On my way|Be there soon"

# Rotary encoder settings
meshtastic --set canned_message.rotary1_enabled true
meshtastic --set canned_message.inputbroker_pin_a 12
meshtastic --set canned_message.inputbroker_pin_b 13
meshtastic --set canned_message.inputbroker_pin_press 14
```

---

### Device Management Commands

```bash
# Reboot device
meshtastic --reboot

# Shutdown device
meshtastic --shutdown

# Factory reset (erases all settings)
meshtastic --factory-reset

# Reset node database only
meshtastic --reset-nodedb

# Enter DFU mode (firmware update)
meshtastic --enter-dfu

# Set device to remote node
meshtastic --dest '!abcd1234' --set device.role ROUTER
```

---

### Sending Messages via CLI

```bash
# Send text message to all nodes
meshtastic --sendtext "Hello mesh!"

# Send to specific node
meshtastic --sendtext "Hello!" --dest "!abcd1234"

# Send on specific channel
meshtastic --sendtext "Private message" --ch-index 1

# Request position from node
meshtastic --request-position --dest "!abcd1234"

# Send trace route
meshtastic --traceroute "!abcd1234"
```

---

### Debugging & Troubleshooting

```bash
# Enable debug output
meshtastic --debug --info

# Serial terminal mode (no protocol)
meshtastic --noproto

# List serial ports
meshtastic --port list

# Test with specific timeout
meshtastic --timeout 30 --info
```

#### Linux Permission Fix

```bash
# Add user to dialout group
sudo usermod -a -G dialout $USER
# Log out and back in for changes to take effect
```

---

### Python API Configuration Examples

Configure devices programmatically using the Python API:

```python
import meshtastic
import meshtastic.serial_interface

# Connect to device
interface = meshtastic.serial_interface.SerialInterface()

# Get local node
ourNode = interface.getNode('^local')

# Read current configuration
print(f"Current region: {ourNode.localConfig.lora.region}")
print(f"Current role: {ourNode.localConfig.device.role}")

# Modify LoRa settings
ourNode.localConfig.lora.region = 1  # US
ourNode.localConfig.lora.hop_limit = 5
ourNode.writeConfig("lora")

# Modify device settings
ourNode.localConfig.device.role = 1  # CLIENT
ourNode.writeConfig("device")

# Modify power settings
ourNode.localConfig.power.is_power_saving = True
ourNode.localConfig.power.wait_bluetooth_secs = 3600
ourNode.writeConfig("power")

# Modify position settings
ourNode.localConfig.position.gps_update_interval = 60
ourNode.localConfig.position.position_broadcast_secs = 300
ourNode.writeConfig("position")

# Modify module settings
ourNode.moduleConfig.telemetry.device_update_interval = 900
ourNode.writeConfig("telemetry")

ourNode.moduleConfig.mqtt.enabled = True
ourNode.moduleConfig.mqtt.address = "mqtt.example.com"
ourNode.writeConfig("mqtt")

# Configure channel
channel = ourNode.channels[0]
channel.settings.name = "MyMesh"
ourNode.writeChannel(0)

# Close connection
interface.close()
```

### Complete Configuration Script Example

```python
#!/usr/bin/env python3
"""
Complete Meshtastic Device Configuration Script
Configures a device for optimal mesh network operation.
"""

import meshtastic
import meshtastic.serial_interface
import sys
import time


def configure_device(device_path=None):
    """Configure a Meshtastic device with optimal settings."""

    # Connect to device
    print("Connecting to device...")
    if device_path:
        interface = meshtastic.serial_interface.SerialInterface(devPath=device_path)
    else:
        interface = meshtastic.serial_interface.SerialInterface()

    # Wait for connection
    time.sleep(2)

    # Get local node
    ourNode = interface.getNode('^local')

    print(f"Connected to: {interface.getLongName()}")
    print(f"Node ID: {interface.getMyNodeInfo()['user']['id']}")

    # =====================
    # LoRa Configuration
    # =====================
    print("\nConfiguring LoRa settings...")
    ourNode.localConfig.lora.region = 1  # US
    ourNode.localConfig.lora.modem_preset = 5  # LONG_FAST
    ourNode.localConfig.lora.hop_limit = 3
    ourNode.localConfig.lora.tx_enabled = True
    ourNode.writeConfig("lora")
    print("  - Region: US")
    print("  - Modem Preset: LONG_FAST")
    print("  - Hop Limit: 3")

    # =====================
    # Device Configuration
    # =====================
    print("\nConfiguring device settings...")
    ourNode.localConfig.device.role = 1  # CLIENT
    ourNode.localConfig.device.node_info_broadcast_secs = 10800  # 3 hours
    ourNode.writeConfig("device")
    print("  - Role: CLIENT")
    print("  - Node Info Broadcast: 3 hours")

    # =====================
    # Position Configuration
    # =====================
    print("\nConfiguring position settings...")
    ourNode.localConfig.position.gps_update_interval = 120
    ourNode.localConfig.position.position_broadcast_secs = 900  # 15 min
    ourNode.localConfig.position.position_broadcast_smart_enabled = True
    ourNode.localConfig.position.broadcast_smart_minimum_distance = 100
    ourNode.localConfig.position.broadcast_smart_minimum_interval_secs = 30
    ourNode.writeConfig("position")
    print("  - GPS Update: 2 minutes")
    print("  - Position Broadcast: 15 minutes")
    print("  - Smart Broadcast: Enabled")

    # =====================
    # Power Configuration
    # =====================
    print("\nConfiguring power settings...")
    ourNode.localConfig.power.wait_bluetooth_secs = 3600  # 1 hour
    ourNode.localConfig.power.ls_secs = 300  # 5 min light sleep
    ourNode.localConfig.power.min_wake_secs = 10
    ourNode.writeConfig("power")
    print("  - Bluetooth Timeout: 1 hour")
    print("  - Light Sleep: 5 minutes")

    # =====================
    # Telemetry Configuration
    # =====================
    print("\nConfiguring telemetry...")
    ourNode.moduleConfig.telemetry.device_update_interval = 1800
    ourNode.moduleConfig.telemetry.environment_measurement_enabled = True
    ourNode.moduleConfig.telemetry.environment_update_interval = 1800
    ourNode.writeConfig("telemetry")
    print("  - Device Metrics: 30 minutes")
    print("  - Environment: Enabled (30 min)")

    # =====================
    # Channel Configuration
    # =====================
    print("\nConfiguring primary channel...")
    # Channel name
    ourNode.channels[0].settings.name = "MyMesh"
    ourNode.writeChannel(0)
    print("  - Channel Name: MyMesh")

    print("\n" + "="*50)
    print("Configuration complete!")
    print("Device will reboot to apply settings.")
    print("="*50)

    # Close connection
    interface.close()


if __name__ == "__main__":
    device = sys.argv[1] if len(sys.argv) > 1 else None
    configure_device(device)
```

---

## Sources

- [Meshtastic Python API Documentation](https://python.meshtastic.org/)
- [MeshInterface API](https://python.meshtastic.org/mesh_interface.html)
- [Meshtastic Python Library Usage](https://meshtastic.org/docs/development/python/library/)
- [Meshtastic Python CLI Guide](https://meshtastic.org/docs/software/python/cli/)
- [Meshtastic Python CLI Usage](https://meshtastic.org/docs/software/python/cli/usage/)
- [Device Configuration](https://meshtastic.org/docs/configuration/radio/device/)
- [LoRa Configuration](https://meshtastic.org/docs/configuration/radio/lora/)
- [Power Configuration](https://meshtastic.org/docs/configuration/radio/power/)
- [Position Configuration](https://meshtastic.org/docs/configuration/radio/position/)
- [Network Configuration](https://meshtastic.org/docs/configuration/radio/network/)
- [Module Configuration](https://meshtastic.org/docs/configuration/module/)
- [MQTT Module](https://meshtastic.org/docs/configuration/module/mqtt/)
- [Telemetry Module](https://meshtastic.org/docs/configuration/module/telemetry/)
- [Serial Module](https://meshtastic.org/docs/configuration/module/serial/)
- [Port Numbers Documentation](https://meshtastic.org/docs/development/firmware/portnum/)
- [Port Numbers Protobuf](https://github.com/meshtastic/protobufs/blob/master/meshtastic/portnums.proto)
- [Meshtastic Python GitHub](https://github.com/meshtastic/python)
