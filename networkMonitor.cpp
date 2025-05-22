#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <iostream>
#include <vector>
#include <string>
#include <sys/wait.h>
#include <array>

using namespace std;

#define SOCKET_PATH "/tmp/assignment2"
#define BUFFER_SIZE 1024

std::vector<int> intf_sock_fds;
std::vector<pid_t> intf_pids;
bool isRunning = true;

// Signal handler for graceful shutdown
void signalHandler(int sig) {
    if (sig == SIGINT) {
        cout << "\nNetwork Monitor: Received SIGINT, initiating shutdown..." << endl;
        isRunning = false;
    }
}

// Set up the signal handler for SIGINT
void setupSignalHandler() {
    struct sigaction sa;
    sa.sa_handler = signalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
}

// Create and bind a Unix domain socket for communication
int createUnixSocket() {
    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket");
        exit(1);
    }
    unlink(SOCKET_PATH);
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind");
        close(server_fd);
        exit(1);
    }
    if (listen(server_fd, SOMAXCONN) == -1) {
        perror("listen");
        close(server_fd);
        exit(1);
    }
    return server_fd;
}

// Clean up resources and close sockets before exiting
void cleanup() {
    cout << "Network Monitor: Starting cleanup..." << endl;
    for (size_t i = 0; i < intf_sock_fds.size(); i++) {
        if (intf_sock_fds[i] != -1) {
            write(intf_sock_fds[i], "Shut Down", 10);
            close(intf_sock_fds[i]);
        }
        if (intf_pids[i] > 0) {
            kill(intf_pids[i], SIGINT);
            waitpid(intf_pids[i], NULL, 0);
        }
    }
    unlink(SOCKET_PATH);
    cout << "Network Monitor: Cleanup complete" << endl;
}

// Spawn interface monitor processes for the specified number of interfaces
void spawnMonitorProcesses(int num_interfaces) {
    intf_sock_fds.resize(num_interfaces, -1);
    intf_pids.resize(num_interfaces);
    for (int i = 0; i < num_interfaces; i++) {
        string name;
        cout << "Enter interface name " << (i + 1) << ": ";
        cin >> name;
        pid_t pid = fork();
        if (pid == 0) {
            execl("./intfMonitor", "./intfMonitor", name.c_str(), NULL);
            perror("execl failed");
            exit(1);
        } else if (pid > 0) {
            intf_pids[i] = pid;
            cout << "Spawned interface monitor for " << name << " (PID: " << pid << ")" << endl;
        } else {
            perror("fork failed");
            cleanup();
            exit(1);
        }
    }
}

// Handle incoming connections from interface monitors
void handleIncomingConnections(int server_fd, fd_set &master_fds, int &max_fd) {
    int client_fd = accept(server_fd, NULL, NULL);
    if (client_fd == -1) {
        perror("accept");
        return;
    }
    char buffer[BUFFER_SIZE];
    int bytes_read = read(client_fd, buffer, BUFFER_SIZE - 1);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        if (strcmp(buffer, "Ready") == 0) {
            for (int &sock : intf_sock_fds) {
                if (sock == -1) {
                    sock = client_fd;  // Assign the client socket file descriptor to the first available slot
                    FD_SET(client_fd, &master_fds);  // Add the client socket to the master file descriptor set
                    max_fd = max(max_fd, client_fd);  // Update the maximum file descriptor value if necessary
                    write(sock, "Monitor", 8);  // Send a "Monitor" command to the newly connected interface monitor
                    break;  // Exit the loop after handling the connection
                }
            }
        }
    }
}

// Process messages from the interface monitors
void processInterfaceMessages(fd_set &read_fds) {
    for (size_t i = 0; i < intf_sock_fds.size(); i++) {
        if (intf_sock_fds[i] != -1 && FD_ISSET(intf_sock_fds[i], &read_fds)) {
            char buffer[BUFFER_SIZE];
            int bytes_read = read(intf_sock_fds[i], buffer, BUFFER_SIZE - 1);
            if (bytes_read <= 0) {
                close(intf_sock_fds[i]);
                intf_sock_fds[i] = -1;
                continue;
            }
            buffer[bytes_read] = '\0';
            if (strcmp(buffer, "Link Down") == 0) {
                write(intf_sock_fds[i], "Set Link Up", 11);
            }
        }
    }
}

// Main function to set up the network monitor and handle connections
int main() {
    setupSignalHandler();
    int server_fd = createUnixSocket();
    int num_interfaces;
    cout << "Enter number of interfaces to monitor: ";
    cin >> num_interfaces;
    if (num_interfaces <= 0) {
        cerr << "Invalid number of interfaces" << endl;
        cleanup();
        return 1;
    }
    spawnMonitorProcesses(num_interfaces);
    fd_set master_fds, read_fds;  // Declare file descriptor sets for monitoring
    FD_ZERO(&master_fds);          // Initialize the master file descriptor set
    FD_SET(server_fd, &master_fds); // Add the server socket to the master set
    int max_fd = server_fd;        // Initialize the maximum file descriptor value

    while (isRunning) {            // Main loop that runs while the monitor is active
        read_fds = master_fds;    // Copy the master set to read_fds for use in select()
        
        // Wait for activity on any of the monitored file descriptors
        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) == -1) { 
            if (errno == EINTR) continue; // If interrupted by a signal, continue the loop
            perror("select"); // Print error message if select fails
            cleanup();        // Clean up resources before exiting
            return 1;        // Exit the program with an error code
        }
        
        // Check if there's activity on the server socket (indicating a new connection)
        if (FD_ISSET(server_fd, &read_fds)) { 
            handleIncomingConnections(server_fd, master_fds, max_fd); // Handle new connections
        }
        
        // Process messages from the interface monitors that are ready to be read
        processInterfaceMessages(read_fds); 
    }
    cleanup();
    close(server_fd);
    return 0;
}
