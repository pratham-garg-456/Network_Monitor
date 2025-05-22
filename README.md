# A Network Monitor

## Overview

This project implements a **Network Monitor** system for Linux, capable of monitoring and controlling multiple network interfaces. The monitor gathers a wide range of statistics for each interface and ensures that interfaces stay operational, restarting them if they go down. It is designed with scalability and modularity in mind, using inter-process communication between a central controller (**Network Monitor**) and per-interface monitoring processes (**Interface Monitors**).

---

## Features & Main Components

### 1. Network Monitor (Central Controller)

- **User Interaction**: Prompts the user for network interfaces to monitor.
- **Process Management**: Forks and manages one **Interface Monitor** process per interface.
- **IPC**: Communicates with each interface monitor via UNIX domain sockets in `/tmp/`.
- **Control Logic**:
  - Instructs interface monitors to begin monitoring.
  - Receives status updates (e.g., when an interface link goes down).
  - Commands interface monitors to bring interfaces back up if they go down.
- **Graceful Shutdown**: Intercepts `SIGINT` (Ctrl-C), sends shutdown commands to all interface monitors, and cleans up resources.

### 2. Interface Monitor (Per-interface Worker)

- **Statistics Gathering**: Reads statistics from `/sys/class/net/<interface-name>/`:
  - `operstate`, `carrier_up_count`, `carrier_down_count`
  - `rx_bytes`, `rx_dropped`, `rx_errors`, `rx_packets`
  - `tx_bytes`, `tx_dropped`, `tx_errors`, `tx_packets`
- **Reporting**: Prints statistics every second, as instructed.
- **Link Management**: Detects if the interface goes down and reports it; can bring the interface back up via IOCTL as directed by the network monitor.
- **IPC**: Communicates status and control messages with the Network Monitor.
- **Graceful Shutdown**: Responds to `SIGINT` or shutdown messages, closes connections, and exits cleanly.

### 3. Communication Protocol

- Uses UNIX domain sockets (socket files in `/tmp/`) for reliable local IPC.
- Implements a simple handshake and command/response protocol:
  - `"Ready"` / `"Monitor"` / `"Monitoring"` for setup
  - `"Link Down"` / `"Set Link Up"` for fault recovery
  - `"Shut Down"` / `"Done"` for cleanup

### 4. Polling & Display

- Each interface monitor polls statistics every second (interval can be adjusted).
- Output format per interface:
  ```
  Interface:<interface-name> state:<state> up_count:<up-count> down_count:<down-count>
  rx_bytes:<rx-bytes> rx_dropped:<rx-dropped> rx_errors:<rx-errors> rx_packets:<rx-packets>
  tx_bytes:<tx-bytes> tx_dropped:<tx-dropped> tx_errors:<tx-errors> tx_packets:<tx-packets>
  ```

---

## Directory Structure

- `networkMonitor.cpp` — Central controller source code.
- `intfMonitor.cpp` — Interface monitor source code.
- `Makefile` — Build system for both components.
- `README.md` — This documentation.

---

## How To Use

1. **Build the project**:
   ```sh
   make
   ```
2. **Run the network monitor as superuser** (required for interface control):
   ```sh
   sudo ./networkMonitor
   ```
3. **Follow prompts** to enter the interfaces to monitor (e.g., `ens33`, `lo`).
4. **Observe output**: Statistics for each interface will be printed every second.
5. **Test link-down recovery**:
   - Bring an interface down:
     ```sh
     sudo ip link set lo down
     ```
   - The monitor should detect and recover the interface automatically.
6. **Shutdown**: Use `Ctrl-C` to gracefully terminate all processes.

---

## Testing

- Compare program output with `/sys/class/net/<interface>/` files and `ifconfig`.
- Test automatic recovery by bringing interfaces up/down.
- Check for clean shutdown and resource deallocation.

---

## Notes

- **Debugging**: Use the `DEBUG` flag in your Makefile and code for additional debug output.
- **Permissions**: Superuser privileges are required for controlling interfaces.
- **Scalability**: The architecture is designed to support many interfaces (dozens or more).

## Authors

- UNX511 Assignment 2
- Contributors: Pratham Garg
