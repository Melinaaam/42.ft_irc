#include"Server.hpp"

void Server::bot() {
    // Commande BOT - Contrôle complet du bot IRC
    if (!_client || std::find(_clientsToRemove.begin(), _clientsToRemove.end(), _clientFd) != _clientsToRemove.end())
        return;

    if (!_client->isRegistered()) {
        sendReply(ERR_NOTREGISTERED, _client, "", "", "You have not registered");
        return;
    }

    std::string args = _client->getArg();
    if (args.empty()) {
        std::string helpMsg = ":IRCBot!bot@" + std::string(SERVER_NAME) +
                             " PRIVMSG " + _client->getNickname() +
                             " :🤖 BOT COMMAND USAGE:\n"
                             "• BOT enable/disable - Control bot activation\n"
                             "• BOT status - Check bot status\n"
                             "• BOT stats - Server statistics\n"
                             "• BOT uptime - Server uptime\n"
                             "• BOT users - Connected users\n"
                             "• BOT channels - Active channels\n"
                             "• BOT chat #channel - Enable chat mode\n"
                             "• BOT chat exit - Disable chat mode\n"
                             "• BOT help - Show this help";
        sendToClient(_clientFd, helpMsg);
        return;
    }
    std::string botCommand = args;
    if (args.find("chat ") == 0) {
        std::string chatArgs = args.substr(5);
        if (chatArgs == "exit") {
            if (isInChatMode(_clientFd)) {
                std::string currentChannel = getCurrentChannel(_clientFd);
                exitChatMode(_clientFd);
                std::string exitMsg = ":IRCBot!bot@" + std::string(SERVER_NAME) +
                                     " PRIVMSG " + _client->getNickname() +
                                     " :Chat mode DISABLED for " + currentChannel + ". Back to normal IRC mode.";
                sendToClient(_clientFd, exitMsg);
            } else {
                std::string errorMsg = ":IRCBot!bot@" + std::string(SERVER_NAME) +
                                      " PRIVMSG " + _client->getNickname() +
                                      " :You are not in chat mode.";
                sendToClient(_clientFd, errorMsg);
            }
            return;
        }
        else if (chatArgs == "status") {
            if (isInChatMode(_clientFd)) {
                std::string currentChannel = getCurrentChannel(_clientFd);
                std::string statusMsg = ":IRCBot!bot@" + std::string(SERVER_NAME) +
                                       " PRIVMSG " + _client->getNickname() +
                                       " :Chat mode ACTIVE for " + currentChannel + ". Type directly to send messages!";
                sendToClient(_clientFd, statusMsg);
            } else {
                std::string statusMsg = ":IRCBot!bot@" + std::string(SERVER_NAME) +
                                       " PRIVMSG " + _client->getNickname() +
                                       " :Chat mode DISABLED. Use BOT chat #channel to enable.";
                sendToClient(_clientFd, statusMsg);
            }
            return;
        }
        else if (chatArgs[0] == '#') {
            std::string channelName = chatArgs;
            std::map<std::string, Channel>::iterator it = _channels.find(channelName);
            if (it == _channels.end()) {
                std::string errorMsg = ":IRCBot!bot@" + std::string(SERVER_NAME) +
                                      " PRIVMSG " + _client->getNickname() +
                                      " :Channel " + channelName + " does not exist. Join it first with: JOIN " + channelName;
                sendToClient(_clientFd, errorMsg);
                return;
            }
            Channel& chan = it->second;
            if (!chan.isMember(_client)) {
                std::string errorMsg = ":IRCBot!bot@" + std::string(SERVER_NAME) +
                                      " PRIVMSG " + _client->getNickname() +
                                      " :You are not a member of " + channelName + ". Join it first with: JOIN " + channelName;
                sendToClient(_clientFd, errorMsg);
                return;
            }
            setChatMode(_clientFd, channelName);
            std::string successMsg = ":IRCBot!bot@" + std::string(SERVER_NAME) +
                                   " PRIVMSG " + _client->getNickname() +
                                   " :Chat mode ENABLED for " + channelName + "! \n"
                                   "Now type messages directly (no need for PRIVMSG)\n"
                                   "Use BOT chat exit to return to normal IRC mode";
            sendToClient(_clientFd, successMsg);
            return;
        }
    }
    std::string botResponse = processIRCBotCommand(args, "");
    if (!botResponse.empty()) {
        if (_client->isNetcatLike()) {
            std::string botMsg = ":IRCBot!bot@" + std::string(SERVER_NAME) +
                               " PRIVMSG " + _client->getNickname() + " :" + botResponse;
            sendToClient(_clientFd, botMsg);
        } else {
            if (_ircBotCreated && _ircBotClient) {
                std::string botMsg = ":" + _ircBotClient->getPrefix() +
                                   " PRIVMSG " + _client->getNickname() + " :" + botResponse;
                sendToClient(_clientFd, botMsg);
            } else {
                std::string botMsg = ":IRCBot!bot@" + std::string(SERVER_NAME) +
                                   " PRIVMSG " + _client->getNickname() + " :" + botResponse;
                sendToClient(_clientFd, botMsg);
            }
        }
    } else {
        std::string errorMsg = ":IRCBot!bot@" + std::string(SERVER_NAME) +
                              " PRIVMSG " + _client->getNickname() +
                              " :❌ Unknown BOT command. Use BOT help for available commands.";
        sendToClient(_clientFd, errorMsg);
    }
}

void Server::sendfile() {
    std::istringstream iss(_client->getArg());
    std::string targetNick, filename;
    iss >> targetNick >> filename;
    if (targetNick.empty() || filename.empty()) {
        sendReply(ERR_NEEDMOREPARAMS, _client, "SEND", "", "Usage: SEND <nick> <filename>");
        return;
    }
    Client* target = getClientByNick(targetNick);
    if (!target) {
        sendReply(ERR_NOSUCHNICK, _client, targetNick, "", "No such nick");
        return;
    }
    std::ifstream file(filename.c_str());
    if (!file.is_open()) {
        sendReply(ERR_FILEERROR, _client, filename, "", "Cannot open file");
        return;
    }
    std::stringstream buffer;
    buffer << file.rdbuf(); // read file
    std::string content = buffer.str();
    file.close();
    std::string transferKey = targetNick + "_" + filename;
    PendingTransfer transfer; // keep track of the transfer
    transfer.sender = _client->getNickname();
    transfer.content = content;
    transfer.filename = filename;
    _pendingTransfers[transferKey] = transfer;
    // notif to target client
    std::string msg = ":" + _client->getPrefix() + " SEND " + filename + " :File transfer request from " + _client->getNickname() + "\r\n";
    sendToClient(target->getFd(), msg);
}

void Server::acceptFile() {
    std::istringstream iss(_client->getArg());
    std::string targetNick, filename;
    iss >> targetNick >> filename;
    if (targetNick.empty() || filename.empty()) {
        sendReply(ERR_NEEDMOREPARAMS, _client, "ACCEPT", "", "Usage: ACCEPT <nick> <filename>");
        return;
    }
    // check if the transfer exists
    std::string transferKey = _client->getNickname() + "_" + filename;
    std::map<std::string, PendingTransfer>::iterator it = _pendingTransfers.find(transferKey);
    if (it == _pendingTransfers.end()) {
        sendReply(ERR_FILEERROR, _client, filename, "", "No pending transfer for this file");
        return;
    }
    Client* sender = getClientByNick(targetNick);
    if (!sender) {
        sendReply(ERR_NOSUCHNICK, _client, targetNick, "", "No such nick");
        _pendingTransfers.erase(transferKey);
        return;
    }
    // Notif sender
    std::string acceptMsg = ":" + _client->getPrefix() + " ACCEPT " + filename + "\r\n";
    sendToClient(sender->getFd(), acceptMsg);
    // get file
    std::string contentMsg = ":" + _client->getPrefix() + " NOTICE " + _client->getNickname() 
                         + " :File content of " + filename + ":\r\n" + it->second.content + "\r\n";
    sendToClient(_clientFd, contentMsg);
    // delete the tracker
    _pendingTransfers.erase(transferKey);
}

void Server::refuseFile() {
    std::istringstream iss(_client->getArg());
    std::string targetNick, filename;
    iss >> targetNick >> filename;
    
    if (targetNick.empty() || filename.empty()) {
        sendReply(ERR_NEEDMOREPARAMS, _client, "REFUSE", "", "Usage: REFUSE <nick> <filename>");
        return;
    }
    // check if the transfer exists
    std::string transferKey = _client->getNickname() + "_" + filename;
    std::map<std::string, PendingTransfer>::iterator it = _pendingTransfers.find(transferKey);
    if (it == _pendingTransfers.end()) {
        sendReply(ERR_FILEERROR, _client, filename, "", "No pending transfer for this file");
        return;
    }
    Client* target = getClientByNick(targetNick);
    if (!target) {
        sendReply(ERR_NOSUCHNICK, _client, targetNick, "", "No such nick");
        return;
    }
    // Notif sender
    std::string msg = ":" + _client->getPrefix() + " REFUSE " + filename + "\r\n";
    sendToClient(target->getFd(), msg);
    // delete the tracker
    _pendingTransfers.erase(transferKey);
}

