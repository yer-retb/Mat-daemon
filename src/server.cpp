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
#include <errno.h>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <iomanip>
#include <sstream>

void debug(const std::string &msg);

#define PORT 4242
#define MAX_CLIENTS 3
#define CLIENT_TIMEOUT_SECONDS 60

// Global statistics
static std::chrono::steady_clock::time_point server_start_time;
static int total_connections = 0;
static int total_messages = 0;

void check_inactive_clients(int client_socket[], std::chrono::steady_clock::time_point client_last_activity[], std::string client_nicknames[], Tintin_reporter &logger)
{
    auto now = std::chrono::steady_clock::now();
    const auto timeout_duration = std::chrono::seconds(CLIENT_TIMEOUT_SECONDS);

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (client_socket[i] > 0)
        {
            auto time_since_activity = now - client_last_activity[i];
            if (time_since_activity >= timeout_duration)
            {
                logger.info("Matt_daemon: Client timeout, disconnecting inactive client");
                close(client_socket[i]);
                client_socket[i] = 0;
                client_nicknames[i] = ""; // Clear nickname on timeout
            }
        }
    }
}

std::string generate_dashboard(int client_socket[], std::chrono::steady_clock::time_point client_last_activity[], std::string client_nicknames[])
{
    auto now = std::chrono::steady_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - server_start_time);

    // Count active clients
    int active_clients = 0;
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (client_socket[i] > 0)
            active_clients++;
    }

    // Calculate uptime in human readable format
    int hours = uptime.count() / 3600;
    int minutes = (uptime.count() % 3600) / 60;
    int seconds = uptime.count() % 60;

    // Get log file size
    std::string log_size = "Unknown";
    if (std::filesystem::exists("/var/log/matt_daemon/matt_daemon.log"))
    {
        auto size = std::filesystem::file_size("/var/log/matt_daemon/matt_daemon.log");
        if (size < 1024)
            log_size = std::to_string(size) + " bytes";
        else if (size < 1024 * 1024)
            log_size = std::to_string(size / 1024) + "." + std::to_string((size % 1024) / 100) + " KB";
        else
            log_size = std::to_string(size / (1024 * 1024)) + "." + std::to_string((size % (1024 * 1024)) / 100000) + " MB";
    }

    std::ostringstream dashboard;
    dashboard << "\n";
    dashboard << "╔═══════════════════════════════════╗\n";
    dashboard << "║        Matt_daemon Dashboard      ║\n";
    dashboard << "╠═══════════════════════════════════╣\n";
    dashboard << "║ Status: Running                   ║\n";
    dashboard << "║ Uptime: " << std::setfill(' ') << std::setw(2) << hours << "h "
              << std::setw(2) << minutes << "m " << std::setw(2) << seconds << "s" << std::setw(13) << "  ║\n";
    dashboard << "║ Active Clients: " << active_clients << "/" << MAX_CLIENTS << std::setw(15) << "  ║\n";
    dashboard << "║ Total Connections: " << std::setw(3) << total_connections << std::setw(10) << "  ║\n";
    dashboard << "║ Messages Received: " << std::setw(3) << total_messages << std::setw(10) << "  ║\n";
    dashboard << "║ Log File Size: " << std::left << std::setw(15) << log_size << std::right << "  ║\n";
    dashboard << "║ Port: " << PORT << std::setw(22) << "      ║\n";
    dashboard << "╚═══════════════════════════════════╝\n";

    if (active_clients > 0)
    {
        dashboard << "\nConnected Clients:\n";
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (client_socket[i] > 0)
            {
                auto connection_time = std::chrono::duration_cast<std::chrono::seconds>(now - client_last_activity[i]);
                std::string client_name = client_nicknames[i].empty() ? "Client " + std::to_string(i + 1) : client_nicknames[i];
                dashboard << "  " << client_name << ": Active (" << connection_time.count() << "s ago)\n";
            }
        }
    }

    dashboard << "\nCommands: help, dashboard, nick, quit\n\n";
    return dashboard.str();
}

void broadcast_message_to_others(int client_socket[], int sender_index, const std::string &message, std::string client_nicknames[], Tintin_reporter &logger)
{
    std::string sender_name = client_nicknames[sender_index].empty() ? "Client " + std::to_string(sender_index + 1) : client_nicknames[sender_index];

    std::string broadcast_msg = "[" + sender_name + "]: " + message + "\n";

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        // Send to all clients except the sender
        if (i != sender_index && client_socket[i] > 0)
        {
            ssize_t sent = send(client_socket[i], broadcast_msg.c_str(), broadcast_msg.size(), MSG_NOSIGNAL);
            if (sent <= 0)
            {
                // Client disconnected during send, clean up
                logger.info("Matt_daemon: Client disconnected during broadcast");
                close(client_socket[i]);
                client_socket[i] = 0;
                client_nicknames[i] = ""; // Clear nickname
            }
        }
    }

    logger.log("LOG", "Matt_daemon: Message broadcast from " + sender_name);
}

std::string generate_welcome_message(int client_number)
{
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm *ptm = std::localtime(&now_time);

    std::ostringstream welcome;
    welcome << "\n";
    welcome << "╔═══════════════════════════════════╗\n";
    welcome << "║      Welcome to Matt_daemon!      ║\n";
    welcome << "╠═══════════════════════════════════╣\n";
    welcome << "║ You are Client " << client_number << "                  ║\n";
    welcome << "║ Connected: " << std::setfill('0') << std::setw(2) << ptm->tm_mday << "/"
            << std::setw(2) << ptm->tm_mon + 1 << "/" << ptm->tm_year + 1900
            << " - " << std::setw(2) << ptm->tm_hour << ":"
            << std::setw(2) << ptm->tm_min << ":" << std::setw(2) << ptm->tm_sec << "  ║\n";
    welcome << "║                                   ║\n";
    welcome << "║ Commands:                         ║\n";
    welcome << "║   help      - Show this help      ║\n";
    welcome << "║   dashboard - Show server stats   ║\n";
    welcome << "║   nick <name> - Set nickname      ║\n";
    welcome << "║   quit      - Disconnect          ║\n";
    welcome << "║                                   ║\n";
    welcome << "║ Your messages will be broadcast   ║\n";
    welcome << "║ to all other connected clients!   ║\n";
    welcome << "╚═══════════════════════════════════╝\n\n";

    return welcome.str();
}

std::string generate_help_message()
{
    std::ostringstream help;
    help << "\n";
    help << "╔═══════════════════════════════════╗\n";
    help << "║         Matt_daemon Help          ║\n";
    help << "╠═══════════════════════════════════╣\n";
    help << "║                                   ║\n";
    help << "║ Available Commands:               ║\n";
    help << "║                                   ║\n";
    help << "║ help      - Show this help        ║\n";
    help << "║ dashboard - Show server stats     ║\n";
    help << "║ nick <name> - Set your nickname   ║\n";
    help << "║ quit      - Disconnect & exit     ║\n";
    help << "║                                   ║\n";
    help << "║ Message Broadcasting:             ║\n";
    help << "║ Any other text you type will be   ║\n";
    help << "║ broadcast to all other clients    ║\n";
    help << "║ currently connected to the server ║\n";
    help << "║                                   ║\n";
    help << "╚═══════════════════════════════════╝\n\n";

    return help.str();
}

void run_server()
{
    server_start_time = std::chrono::steady_clock::now(); // Initialize server start time
    Tintin_reporter logger;
    int server_fd, new_socket, activity, valread;
    int client_socket[MAX_CLIENTS] = {0};
    std::chrono::steady_clock::time_point client_last_activity[MAX_CLIENTS];
    std::string client_nicknames[MAX_CLIENTS] = {"", "", ""}; // Client nicknames
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
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
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

        // Wait for activity with timeout
        struct timeval timeout;
        timeout.tv_sec = 10; // Check for inactive clients every 10 seconds
        timeout.tv_usec = 0;

        activity = select(max_sd + 1, &readfds, NULL, NULL, &timeout);

        if ((activity < 0) && (errno != EINTR))
        {
            logger.error("Select error");
            continue;
        }

        // Check for inactive clients on timeout or activity
        check_inactive_clients(client_socket, client_last_activity, client_nicknames, logger);

        // Incoming connection
        if (FD_ISSET(server_fd, &readfds))
        {
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen)) < 0)
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
                    client_last_activity[i] = std::chrono::steady_clock::now();
                    client_nicknames[i] = ""; // Initialize empty nickname
                    total_connections++;      // Update statistics
                    logger.info("Matt_daemon: New client connected");
                    // Send welcome message
                    std::string welcome_message = generate_welcome_message(i + 1);
                    send(new_socket, welcome_message.c_str(), welcome_message.size(), 0);
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
                    client_nicknames[i] = ""; // Clear nickname on disconnect
                }
                else
                {
                    client_last_activity[i] = std::chrono::steady_clock::now(); // Reset timer on activity
                    std::string msg(buffer);
                    msg.erase(msg.find_last_not_of(" \n\r\t") + 1);
                    if (msg == "quit")
                    {
                        logger.info("Matt_daemon: Request quit.");
                        logger.info("Matt_daemon: Quitting.");
                        for (int j = 0; j < MAX_CLIENTS; j++)
                        {
                            if (client_socket[j] != 0)
                                close(client_socket[j]);
                        }
                        close(server_fd);
                        exit(0); // Triggers atexit
                    }
                    else if (msg == "dashboard")
                    {
                        std::string dashboard = generate_dashboard(client_socket, client_last_activity, client_nicknames);
                        send(sd, dashboard.c_str(), dashboard.size(), 0);
                        logger.log("LOG", "Matt_daemon: Dashboard requested");
                    }
                    else if (msg == "help")
                    {
                        std::string help_message = generate_help_message();
                        send(sd, help_message.c_str(), help_message.size(), 0);
                        logger.log("LOG", "Matt_daemon: Help message requested");
                    }
                    else if (msg.substr(0, 5) == "nick ")
                    {
                        std::string new_nick = msg.substr(5);
                        if (new_nick.empty())
                        {
                            std::string response = "Usage: nick <nickname>\n";
                            send(sd, response.c_str(), response.size(), 0);
                        }
                        else if (new_nick.length() > 20)
                        {
                            std::string response = "Nickname too long (max 20 characters)\n";
                            send(sd, response.c_str(), response.size(), 0);
                        }
                        else
                        {
                            std::string old_name = client_nicknames[i].empty() ? "Client " + std::to_string(i + 1) : client_nicknames[i];
                            client_nicknames[i] = new_nick;
                            std::string response = "Nickname changed from '" + old_name + "' to '" + new_nick + "'\n";
                            send(sd, response.c_str(), response.size(), 0);
                            logger.log("LOG", "Matt_daemon: " + old_name + " changed nickname to " + new_nick);
                        }
                    }
                    else if (msg == "nick")
                    {
                        std::string current_nick = client_nicknames[i].empty() ? "Client " + std::to_string(i + 1) : client_nicknames[i];
                        std::string response = "Your current nickname: " + current_nick + "\n";
                        send(sd, response.c_str(), response.size(), 0);
                    }
                    else
                    {
                        total_messages++; // Update message count
                        logger.log("LOG", "Matt_daemon: User input: " + msg);
                        broadcast_message_to_others(client_socket, i, msg, client_nicknames, logger); // Broadcast to other clients
                    }
                }
            }
        }

        check_inactive_clients(client_socket, client_last_activity, client_nicknames, logger);
    }
}
