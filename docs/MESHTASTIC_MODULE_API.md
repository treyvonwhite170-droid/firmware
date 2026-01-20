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

## Sources

- [Meshtastic Python API Documentation](https://python.meshtastic.org/)
- [MeshInterface API](https://python.meshtastic.org/mesh_interface.html)
- [Meshtastic Python Library Usage](https://meshtastic.org/docs/development/python/library/)
- [Port Numbers Documentation](https://meshtastic.org/docs/development/firmware/portnum/)
- [Port Numbers Protobuf](https://github.com/meshtastic/protobufs/blob/master/meshtastic/portnums.proto)
- [Meshtastic Python GitHub](https://github.com/meshtastic/python)
