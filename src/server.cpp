#include "../include/server.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include <arpa/inet.h>
#include <iostream>
#include <fstream>

void debug(const std::string& msg); // Reuse your debug logger

void run_server()
{
    int server_fd, client_fd;
    struct sockaddr_in address;
    char buffer[1024] = {0};
    int opt = 1;
    socklen_t addrlen = sizeof(address);

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        debug("Socket creation failed");
        return;
    }

    // Allow address reuse
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // 0.0.0.0
    address.sin_port = htons(4242);

    // Bind
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0)
    {
        debug("Bind failed");
        close(server_fd);
        return;
    }

    // Listen
    if (listen(server_fd, 1) < 0)
    {
        debug("Listen failed");
        close(server_fd);
        return;
    }

    debug("Server listening on port 4242");

    // Accept one client
    if ((client_fd = accept(server_fd, (struct sockaddr*)&address, &addrlen)) < 0)
    {
        debug("Accept failed");
        close(server_fd);
        return;
    }

    debug("Client connected");

    while (true)
    {
        memset(buffer, 0, sizeof(buffer));
        int bytes = read(client_fd, buffer, sizeof(buffer) - 1);
        if (bytes <= 0)
            break;

        std::string msg(buffer);
        msg.erase(msg.find_last_not_of(" \n\r\t") + 1);

        if (msg == "quit")
        {
            debug("Received quit command from client");
            break;
        }
        else
        {
            debug("User input: " + msg);
        }
    }

    close(client_fd);
    close(server_fd);
    debug("Server shutdown");
    exit(0); // Triggers atexit and removes lock file
}
