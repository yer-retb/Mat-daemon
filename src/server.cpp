#include "../include/server.hpp"
#include "../include/Tintin_reporter.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include <arpa/inet.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <cstdlib>

void debug(const std::string& msg);

#define PORT 4242
#define MAX_CLIENTS 3

void run_server()
{
    Tintin_reporter logger;
    int server_fd, new_socket, activity, valread;
    int client_socket[MAX_CLIENTS] = {0};
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    char buffer[1025];

    fd_set readfds;

    // Create master socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        logger.error("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Allow address reuse
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0)
    {
        logger.error("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Listen
    if (listen(server_fd, 3) < 0)
    {
        logger.error("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    logger.info("Server listening on port 4242");

    while (true)
    {
        // Clear and set file descriptors
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        int max_sd = server_fd;

        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            int sd = client_socket[i];
            if (sd > 0)
                FD_SET(sd, &readfds);
            if (sd > max_sd)
                max_sd = sd;
        }

        // Wait for activity
        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if ((activity < 0) && (errno != EINTR))
        {
            logger.error("Select error");
            continue;
        }

        // Incoming connection
        if (FD_ISSET(server_fd, &readfds))
        {
            if ((new_socket = accept(server_fd, (struct sockaddr*)&address, &addrlen)) < 0)
            {
                logger.error("Accept failed");
                continue;
            }

            // Add to client list
            bool added = false;
            for (int i = 0; i < MAX_CLIENTS; i++)
            {
                if (client_socket[i] == 0)
                {
                    client_socket[i] = new_socket;
                    logger.info("New client connected");
                    added = true;
                    break;
                }
            }

            if (!added)
            {
                logger.error("Too many clients, rejecting connection");
                close(new_socket);
            }
        }

        // Check all clients for input
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            int sd = client_socket[i];
            if (FD_ISSET(sd, &readfds))
            {
                memset(buffer, 0, 1025);
                valread = read(sd, buffer, 1024);
                if (valread <= 0)
                {
                    logger.info("Client disconnected");
                    close(sd);
                    client_socket[i] = 0;
                }
                else
                {
                    std::string msg(buffer);
                    msg.erase(msg.find_last_not_of(" \n\r\t") + 1);
                    if (msg == "quit")
                    {
                        logger.info("Received 'quit' → shutting down daemon.");
                        for (int j = 0; j < MAX_CLIENTS; j++)
                        {
                            if (client_socket[j] != 0)
                                close(client_socket[j]);
                        }
                        close(server_fd);
                        exit(0); // Triggers atexit
                    }
                    else
                    {
                        logger.message("User input: " + msg);
                    }
                }
            }
        }
    }
}
