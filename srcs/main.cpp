#include "Server.hpp"
#include "Client.hpp"
#include "Channel.hpp"
#include <cstdlib>
#include <sstream>
#include <colors.hpp>
#include <csignal>

bool isValidPort(const char* str, int& port) {
    std::istringstream iss(str);
    iss >> port;
    return !(iss.fail() || !iss.eof());
}

Server* globalServer = NULL;

void handleSigint(int signum) {
    std::cout << "\nSignal " << signum << " received. Cleaning up..." << std::endl;
    if (globalServer)
    {
        delete globalServer;
        globalServer = NULL;
    }
    std::exit(0);
}

int main(int ac, char** av)
{
    if (ac != 3) {
        std::cerr << RED << "Error: ./ircserv <port> <password>" << RESET << std::endl;
        return 1;
    }
    int port;
    if (!isValidPort(av[1], port) || port <= 0 || port > 65535) {
        std::cerr << RED << "Error: port should be a valid integer between 1 and 65535." << RESET << std::endl;
        return 1;
    }
    std::string password(av[2]);
    signal(SIGINT, handleSigint);
    try {
        globalServer = new Server(port, password);
        globalServer->run();
    } catch (const std::exception& e) {
        std::cerr << RED << "Error: " << e.what() << RESET << std::endl;
    }
    if (globalServer)
    {
        delete globalServer;
        globalServer = NULL;
    }
    return 0;
}
