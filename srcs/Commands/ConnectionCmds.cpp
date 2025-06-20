#include"Server.hpp"

void Server::cap() {
	// CAP is used for capability negotiation in IRC
	// it allows clients to request or list capabilities
    std::string arg = _client->getArg();
    if (arg.empty())
        return;
    std::istringstream iss(arg);
    std::string subcmd;
    iss >> subcmd;
    if (subcmd == "LS") // capabilities list -> none in IRC
        sendToClient(_clientFd, ":localhost CAP * LS :");
    else if (subcmd == "REQ") { // request from client -> always answer none accepted
        std::string caps;
        std::getline(iss, caps);
        if (!caps.empty() && caps[0] == ':')
            caps = caps.substr(1);
        sendToClient(_clientFd, ":localhost CAP * NAK :" + caps);
    }
    else if (subcmd == "LIST") // capabilities list of those activated
        sendToClient(_clientFd, ":localhost CAP * LIST :");
    else if (subcmd == "END") // end of negotiation from client
        _client->setCapNegotiationDone(true);
}

void Server::pass() {
	// The PASS command is used to set the password for the client

    if (std::find(_clientsToRemove.begin(), _clientsToRemove.end(), _clientFd) != _clientsToRemove.end()) {
        return;
    }
    if (_client->isRegistered()) {
        sendReply(ERR_ALREADYREGISTRED, _client, "PASS", "", "You may not reregister");
        return;
    }
    if (_client->getArg().empty()) {
        sendReply(ERR_NEEDMOREPARAMS, _client, "PASS", "", "Not enough parameters");
        return;
    }
    std::string inputPass = _client->getArg();
    inputPass.erase(inputPass.find_last_not_of(" \r\n") + 1);
    if (inputPass == _password) {
        _client->setPasswordOk(true);
        _client->setPassErrorSent(false);
    }
    else {
        sendReply(ERR_PASSWDMISMATCH, _client, "", "", "Password incorrect");
        _client->setPasswordOk(false);
    }
}

void Server::nick() {
	// The NICK command is used to set the nickname for the client

    if (std::find(_clientsToRemove.begin(), _clientsToRemove.end(), _clientFd) != _clientsToRemove.end())
        return;
    if (_client->getArg().empty()) {
        sendReply(ERR_NONICKNAMEGIVEN, _client, "*", "", "No nickname given");
        return;
    }
    std::string newNick = _client->getArg();
    size_t firstSpace = newNick.find_first_of(" \t\r\n");
    if (firstSpace != std::string::npos)
        newNick = newNick.substr(0, firstSpace);
    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        if (it->second->getNickname() == newNick && it->first != _clientFd) {
            sendReply(ERR_NICKNAMEINUSE, _client, "*", newNick, "Nickname is already in use");
            return;
        }
    }
    std::string oldNick = _client->getNickname();
    _client->setNickname(newNick);
    std::string prefix;
    if (oldNick.empty())
        prefix = "*";
    else
        prefix = oldNick;
    std::string username = _client->getUsername();
    if (username.empty())
        username = "*";
    std::string nickMsg = ":" + prefix + "!" + username + "@localhost NICK :" + newNick;
    sendToClient(_clientFd, nickMsg);

    // Update members and operator status in all channels where the user is present
    for (std::map<std::string, Channel>::iterator it = _channels.begin(); it != _channels.end(); ++it) {
        Channel& chan = it->second;
        if (chan.isMember(_client)) {
            // Notify all members in the channel about the nickname change
            const std::set<Client*>& members = chan.getMembers();
            for (std::set<Client*>::const_iterator memberIt = members.begin(); memberIt != members.end(); ++memberIt) {
                if ((*memberIt) != _client) { // Don't send to the user themselves since they already got the message
                    sendToClient((*memberIt)->getFd(), nickMsg);
                }
            }
        }
    }
    checkRegistration();
}

void Server::user() {
	// The USER command is used to set the username and real name for the client

    if (std::find(_clientsToRemove.begin(), _clientsToRemove.end(), _clientFd) != _clientsToRemove.end())
        return;
    if (_client->isRegistered()) {
        sendReply(462, _client, "", "", "You may not reregister");
        return;
    }
    std::string args = _client->getArg();
    std::string& buffer = _client->getBuffer();
    size_t realNamePos = args.find(" :");
    if (realNamePos == std::string::npos) {
        realNamePos = buffer.find(":");
        if (realNamePos != std::string::npos)
            args += buffer.substr(realNamePos);
    }
    std::istringstream iss(args);
    std::string username, hostname, servername, realname;
    if (!(iss >> username >> hostname >> servername)) {
        sendReply(ERR_NEEDMOREPARAMS, _client, "USER", "", "Not enough parameters");
        return;
    }
    if (realNamePos != std::string::npos)
        realname = args.substr(realNamePos + 2);
    else {
        sendReply(ERR_NEEDMOREPARAMS, _client, "USER", "", "Not enough parameters");
        return;
    }
    _client->setUsername(username);
    _client->setRealname(realname);
    checkRegistration();
}