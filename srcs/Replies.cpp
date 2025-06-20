#include "Replies.hpp"
#include "Server.hpp"
#include "Client.hpp"
#include <iostream>
#include <sstream>
#include <map>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <sys/socket.h>

void Server::sendToClient(int fd, const std::string& msg) {
    std::string formattedMsg = msg;

    std::map<int, Client*>::iterator it = _clients.find(fd);
    if (it != _clients.end()) {
        if (it->second->getClientType() == false)
            formattedMsg += "\r\n";
        else
            formattedMsg += "\n";
    } else {
        formattedMsg += "\r\n";
    }
    ssize_t bytesSent = send(fd, formattedMsg.c_str(), formattedMsg.length(), MSG_NOSIGNAL);
    if (bytesSent < 0) {
        if (errno == EPIPE || errno == ECONNRESET) {
            std::cout << "[ERROR] Client " << fd << " disconnected during send" << std::endl;
            if (std::find(_clientsToRemove.begin(), _clientsToRemove.end(), fd) == _clientsToRemove.end())
                _clientsToRemove.push_back(fd);
        } else {
            std::cerr << "[ERROR] send() failed: " << strerror(errno) << std::endl;
        }
    }
}

void Server::sendReply(int code, Client* client, const std::string& arg1, const std::string& arg2, const std::string& trailing) {
	std::ostringstream oss;
	std::string nickname = client->getNickname();
	std::ostringstream codeStr;
	codeStr.width(3);
	codeStr.fill('0');
	codeStr << code;
	oss << ":" << SERVER_NAME << " " << codeStr.str() << " " << nickname;
	if (!arg1.empty())
		oss << " " << arg1;
	if (!arg2.empty())
		oss << " " << arg2;
	if (!trailing.empty())
		oss << " :" << trailing;
	std::string msg = oss.str();
	if (_client->getClientType() == false)
		msg += "\r\n";
	else
		msg += "\n";
	sendToClient(client->getFd(), msg);
}

