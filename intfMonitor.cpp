#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <sstream>

using namespace std;

#define SOCKET_PATH "/tmp/assignment2"  // Path for Unix domain socket
#define BUFFER_SIZE 1024               // Size of buffer for messages

volatile sig_atomic_t isRunning = false;  // Flag to control monitoring loop
int sock_fd = -1;                       // Socket file descriptor for communication

// Utility function to handle errors with a message
void handleError(const string& message) {
    cerr << message << ": " << strerror(errno) << endl;
    if (sock_fd != -1) close(sock_fd);  // Close socket before exiting
    exit(1);
}

// Signal handler for graceful shutdown
void signalHandler(int sig) {
    if (sig == SIGINT) {
        cout << "Interface Monitor: Received SIGINT, shutting down..." << endl;
        isRunning = false;
        if (sock_fd != -1) {
            write(sock_fd, "Done", 5);
            close(sock_fd);
        }
        exit(0);
    }
}

// Function to set up the signal handler
void setupSignalHandler() {
    struct sigaction sa;
    sa.sa_handler = signalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) == -1) handleError("Error setting up signal handler");
}

// Function to create and connect to the Unix socket
void createSocket() {
    sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock_fd == -1) handleError("Error creating socket");

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (connect(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1)
        handleError("Error connecting to network monitor");
}

// Function to send a message to the network monitor
void sendMessage(const string& message) {
    if (write(sock_fd, message.c_str(), message.size() + 1) == -1)
        handleError("Error sending message");
}

// Function to read a specific statistic from the interface
string readInterfaceStat(const string& interface, const string& stat) {
    string path = "/sys/class/net/" + interface + "/" + stat;
    ifstream file(path);
    string value;
    if (file.is_open()) {
        getline(file, value);
        file.close();
    } else {
        cerr << "Warning: Could not read " << path << ": " << strerror(errno) << endl;
        value = "0";
    }
    return value;
}

// Function to set the interface up (bring it online)
void setInterfaceUp(const string& interface) {
    struct ifreq ifr;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) handleError("Error creating socket for ioctl");

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, interface.c_str(), IFNAMSIZ - 1);

    if (ioctl(sock, SIOCGIFFLAGS, &ifr) < 0) {
        handleError("Error getting interface flags");
    }

    if (ifr.ifr_flags & IFF_UP) {
        cout << "Interface " << interface << " is already up." << endl;
        close(sock);
        return;
    }

    ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
    if (ioctl(sock, SIOCSIFFLAGS, &ifr) < 0) {
        handleError("Error setting interface up");
    } else {
        cout << "Successfully set interface " << interface << " up" << endl;
    }

    close(sock);
}

// Function to handle the "Monitor" command and gather statistics
void monitorInterface(const string& interface) {
    while (isRunning) {
        string operstate = readInterfaceStat(interface, "operstate");
        string carrier_up = readInterfaceStat(interface, "carrier_up_count");
        string carrier_down = readInterfaceStat(interface, "carrier_down_count");
        string rx_bytes = readInterfaceStat(interface, "statistics/rx_bytes");
        string rx_dropped = readInterfaceStat(interface, "statistics/rx_dropped");
        string rx_errors = readInterfaceStat(interface, "statistics/rx_errors");
        string rx_packets = readInterfaceStat(interface, "statistics/rx_packets");
        string tx_bytes = readInterfaceStat(interface, "statistics/tx_bytes");
        string tx_dropped = readInterfaceStat(interface, "statistics/tx_dropped");
        string tx_errors = readInterfaceStat(interface, "statistics/tx_errors");
        string tx_packets = readInterfaceStat(interface, "statistics/tx_packets");

        // Print statistics
        cout << "Interface: " << interface << " state: " << operstate
             << " up_count: " << carrier_up << " down_count: " << carrier_down << endl
             << "rx_bytes: " << rx_bytes << " rx_dropped: " << rx_dropped
             << " rx_errors: " << rx_errors << " rx_packets: " << rx_packets << endl
             << "tx_bytes: " << tx_bytes << " tx_dropped: " << tx_dropped
             << " tx_errors: " << tx_errors << " tx_packets: " << tx_packets << endl << endl;

        // Check if interface is down and attempt to bring it up
        if (operstate == "down") {
            sendMessage("Link Down");
            cout << "Alert: Interface " << interface << " is down. Attempting to bring it up..." << endl;
            setInterfaceUp(interface);
        }

        // Sleep for 1 second before next read
        this_thread::sleep_for(chrono::seconds(1));
    }
}

// Main function for the interface monitor
int main(int argc, char *argv[]) {
    if (argc != 2) handleError("Usage: <interface_name>");

    string interface = argv[1];

    // Set up signal handler and socket connection
    setupSignalHandler();
    createSocket();

    // Send "Ready" message to the network monitor
    sendMessage("Ready");

    char buffer[BUFFER_SIZE];
    isRunning = true;

    while (isRunning) {
        int bytes_read = read(sock_fd, buffer, BUFFER_SIZE - 1);
        if (bytes_read <= 0) {
            if (bytes_read == 0) {
                cerr << "Connection closed by network monitor." << endl;
            } else {
                handleError("Error reading from socket");
            }
            break;
        }
        buffer[bytes_read] = '\0';

        if (strcmp(buffer, "Monitor") == 0) {
            sendMessage("Monitoring");
            monitorInterface(interface);
        }
        else if (strcmp(buffer, "Shut Down") == 0) {
            sendMessage("Done");
            break;
        }
    }

    close(sock_fd);
    return 0;
}
