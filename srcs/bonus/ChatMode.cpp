#include"Server.hpp"

bool Server::isInChatMode(int clientFd) {
    return _clientChatMode.find(clientFd) != _clientChatMode.end();
}
void Server::setChatMode(int clientFd, const std::string& channel) {
    _clientChatMode[clientFd] = channel;
}

void Server::exitChatMode(int clientFd) {
    std::map<int, std::string>::iterator it = _clientChatMode.find(clientFd);
    if (it != _clientChatMode.end()) {
        _clientChatMode.erase(it);
    }
}

void Server::promptChatMode(int clientFd, const std::string& failedCommand) {
    _clientChatModePrompt[clientFd] = true;

    std::string promptMsg = ":IRCBot!bot@" + std::string(SERVER_NAME) +
                           " PRIVMSG " + _client->getNickname() +
                           " :I noticed you tried '" + failedCommand + "' - did you want to send a message?\n"
                           "Type 'yes' to enable Chat Mode and send messages directly without PRIVMSG!\n"
                           "Or type 'no' to continue with normal IRC commands.";
    sendToClient(clientFd, promptMsg);
}

bool Server::isAwaitingChatModeResponse(int clientFd) {
    return _clientChatModePrompt.find(clientFd) != _clientChatModePrompt.end() && _clientChatModePrompt[clientFd];
}

void Server::handleChatModeResponse(int clientFd, const std::string& response) {
    _clientChatModePrompt[clientFd] = false;
    if (response == "yes" || response == "YES" || response == "y" || response == "Y") {
        std::string targetChannel = "";
        for (std::map<std::string, Channel>::const_iterator it = _channels.begin(); it != _channels.end(); ++it) {
            if (it->second.isMember(_client)) {
                targetChannel = it->first;
                break;
            }
        }
        if (!targetChannel.empty()) {
            setChatMode(clientFd, targetChannel);
            std::string successMsg = ":IRCBot!bot@" + std::string(SERVER_NAME) +
                                   " PRIVMSG " + _client->getNickname() +
                                   " :Chat Mode ENABLED for " + targetChannel + "!\n"
                                   "Now type messages directly. Use '!chat exit' to disable.";
            sendToClient(clientFd, successMsg);
        }
		else {
            std::string errorMsg = ":IRCBot!bot@" + std::string(SERVER_NAME) +
                                 " PRIVMSG " + _client->getNickname() +
                                 " :Please join a channel first with: JOIN #channelname";
            sendToClient(clientFd, errorMsg);
        }
    }
	else {
        std::string cancelMsg = ":IRCBot!bot@" + std::string(SERVER_NAME) +
                              " PRIVMSG " + _client->getNickname() +
                              " :No problem! Continue using normal IRC commands.";
        sendToClient(clientFd, cancelMsg);
    }
}
