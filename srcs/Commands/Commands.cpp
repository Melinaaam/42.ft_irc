#include "Server.hpp"
#include "Replies.hpp"
#include <sstream>
#include <cstdlib>

void Server::ping() {
	// The PING command is used to check if the server is reachable

    if (!_client || std::find(_clientsToRemove.begin(), _clientsToRemove.end(), _clientFd) != _clientsToRemove.end())
        return;

    const std::string& args = _client->getArg();
    if (args.empty()) {
        sendReply(ERR_NOORIGIN, _client, "", "", "No origin specified");
        return;
    }
    // Reply with PONG followed by the server and the original message
    std::string response = ":localhost PONG localhost :" + args;
    sendToClient(_clientFd, response);
}

void Server::pong() {
    // The PONG command is usually sent by the client in response to a PING from the server
    // In most cases, it can be silently ignored
    // since it's the server that initiates pings to test the connection

    if (!_client || std::find(_clientsToRemove.begin(), _clientsToRemove.end(), _clientFd) != _clientsToRemove.end())
        return;
    return;
}

void Server::privmsg() {
	// The PRIVMSG command is used to send a private message to a user or a channel

    if (!_client || std::find(_clientsToRemove.begin(), _clientsToRemove.end(), _clientFd) != _clientsToRemove.end())
        return;
    if (!_client->isRegistered()) {
        sendReply(ERR_NOTREGISTERED, _client, "", "", "You have not registered");
        return;
    }
    std::string args = _client->getArg();
    if (args.empty()) {
        sendReply(ERR_NEEDMOREPARAMS, _client, "PRIVMSG", "", "Not enough parameters");
        return;
    }
    std::string target, message;
    size_t colonPos = args.find(" :");
    if (colonPos != std::string::npos) {
        target = args.substr(0, colonPos);
        message = args.substr(colonPos + 2); // +2 to skip " :"
    } else {
        std::istringstream iss(args);
        if (!(iss >> target)) {
            sendReply(ERR_NEEDMOREPARAMS, _client, "PRIVMSG", "", "Not enough parameters");
            return;
        }
        sendReply(ERR_NEEDMOREPARAMS, _client, "PRIVMSG", "", "No text to send");
        return;
    }
    target = trim(target);
    if (target.empty()) {
        sendReply(ERR_NEEDMOREPARAMS, _client, "PRIVMSG", "", "Not enough parameters");
        return;
    }
    if (message.empty()) {
        sendReply(ERR_NEEDMOREPARAMS, _client, "PRIVMSG", "", "No text to send");
        return;
    }
    if (message.find("\001DCC") == 0) { // get file transfer from irssi
        std::istringstream iss(message);
        std::string dcc, send, filename, dccMsg;
        if (!(iss >> dcc >> send >> filename)) {
            sendReply(ERR_NEEDMOREPARAMS, _client, "USER", "", "Not enough parameters");
            return;
        }
        if (send != "SEND") {
            sendReply(ERR_UNKNOWNCOMMAND, _client, send, "", "Unknown DCC command");
            return;
        }
        dccMsg = target + " " + filename;
        _client->setArg(dccMsg);
        sendfile();
        return;
    }
    if (target[0] == '#') {
        handleChannelMessage(target, message);
    }
    else {
        handlePrivateMessage(target, message);
    }
}

void Server::join() {
	// The JOIN command is used to join a channel

    if (!_client || std::find(_clientsToRemove.begin(), _clientsToRemove.end(), _clientFd) != _clientsToRemove.end())
        return;
    std::string args = _client->getArg();
    if (args.empty()) {
        sendReply(ERR_NEEDMOREPARAMS, _client, "JOIN", "", "Not enough parameters");
        return;
    }
    std::istringstream iss(args);
    std::string channelName, key;
    iss >> channelName >> key;
    if (!channelName.empty() && channelName[0] == ':')
    channelName = channelName.substr(1);
    if (channelName == ":") {
            return;
    }
    if (!_client->isPasswordOk()) {
        if (!_client->hasSentPassError()) {
            sendReply(ERR_PASSWDMISMATCH, _client, "*", "", "Password required");
            _client->setPassErrorSent(true);
        }
        return;
    }
    if (!_client->isRegistered()) {
        sendReply(ERR_NOTREGISTERED, _client, "", "", "You have not registered");
        return;
    }

    if (channelName.empty() || channelName[0] != '#') {
        sendReply(ERR_NOSUCHCHANNEL, _client, channelName.empty() ? "*" : channelName, "", "No such channel");
        return;
    }
    if (isUserBannedFromChannel(_client->getNickname(), channelName)) {
        std::string banMsg = ":IRCBot!bot@" + std::string(SERVER_NAME) +
                            " PRIVMSG " + _client->getNickname() +
                            " :You are BANNED from " + channelName +
                            " for inappropriate language. Access denied 🚫 .";
        sendToClient(_client->getFd(), banMsg);
        sendReply(ERR_BANNEDFROMCHAN, _client, channelName, "", "Cannot join channel (+b) - you are banned 🚫 ");
        return;
    }
    std::pair<std::map<std::string, Channel>::iterator, bool> result =
        _channels.insert(std::make_pair(channelName, Channel(channelName)));
    Channel& chan = result.first->second;
    if (result.second) {
        chan.setCreator(_client->getNickname());
        chan.addOperator(_client);
        std::string modeMsg = ":" + _client->getPrefix() + " MODE " + channelName + " +o " + _client->getNickname();
        sendToClient(_clientFd, modeMsg);
    }
    else if (chan.isCreator(_client) && !chan.isOperator(_client)) {
        chan.addOperator(_client);
        std::string modeMsg = ":" + _client->getPrefix() + " MODE " + channelName + " +o " + _client->getNickname();
        sendToClient(_clientFd, modeMsg);
    }
    if (!result.second && chan.isInviteOnly() && !chan.isInvited(_client) && !chan.isOperator(_client)) {
        sendReply(ERR_INVITEONLYCHAN, _client, channelName, "", "Cannot join channel (+i)");
        return;
    }
    if (!result.second && chan.hasKey() && !chan.isOperator(_client)) {
        if (key.empty() || key != chan.getKey()) {
            sendReply(ERR_BADCHANNELKEY, _client, channelName, "", "Cannot join channel (+k)");
            return;
        }
    }
    if (!result.second && chan.hasUserLimit() && !chan.isOperator(_client)) {
        if ((int)chan.getMemberCount() >= chan.getUserLimit()) {
            sendReply(ERR_CHANNELISFULL, _client, channelName, "", "Cannot join channel (+l)");
            return;
        }
    }
    if (!chan.isMember(_client)) {
        chan.join(_client);
        std::string joinMsg = ":" + _client->getPrefix() + " JOIN :" + channelName;
        const std::set<Client*>& members = chan.getMembers();
        for (std::set<Client*>::const_iterator it = members.begin(); it != members.end(); ++it)
            sendToClient((*it)->getFd(), joinMsg);
        if (!chan.getTopic().empty()) {
            sendReply(RPL_TOPIC, _client, channelName, "", chan.getTopic());
            std::ostringstream timeStr;
            timeStr << chan.getTopicSetTime();
            sendReply(RPL_TOPICWHOTIME, _client, channelName, chan.getTopicSetBy(), timeStr.str());
        }

        std::string nickList;
        for (std::set<Client*>::const_iterator it = members.begin(); it != members.end(); ++it)
            nickList += (*it)->getNickname() + " ";
        sendReply(RPL_NAMREPLY, _client, "=", channelName, nickList);
        sendReply(RPL_ENDOFNAMES, _client, channelName, "", "End of /NAMES list.");
    }
}

void Server::part() {
	// The PART command is used to leave a channel

    if (!_client || std::find(_clientsToRemove.begin(), _clientsToRemove.end(), _clientFd) != _clientsToRemove.end())
        return;
    if (!_client->isRegistered()) {
        sendReply(ERR_NOTREGISTERED, _client, "", "", "You have not registered");
        return;
    }
    std::string args = _client->getArg();
    if (args.empty()) {
        sendReply(ERR_NEEDMOREPARAMS, _client, "PART", "", "Not enough parameters");
        return;
    }
    std::string channelName, partMessage;
    size_t colonPos = args.find(" :");
    if (colonPos != std::string::npos) {
        channelName = args.substr(0, colonPos);
        partMessage = args.substr(colonPos + 2); // +2 to skip " :"
    } else {
        std::istringstream iss(args);
        iss >> channelName;
    }
    if (channelName.empty() || channelName[0] != '#') {
        sendReply(ERR_NOSUCHCHANNEL, _client, channelName.empty() ? "*" : channelName, "", "No such channel");
        return;
    }
    std::map<std::string, Channel>::iterator it = _channels.find(channelName);
    if (it == _channels.end()) {
        sendReply(ERR_NOSUCHCHANNEL, _client, channelName, "", "No such channel");
        return;
    }
    Channel& chan = it->second;
    if (!chan.isMember(_client)) {
        sendReply(ERR_NOTONCHANNEL, _client, channelName, "", "You're not on that channel");
        return;
    }
    std::string partMsg = ":" + _client->getPrefix() + " PART " + channelName;
    if (!partMessage.empty()) {
        partMsg += " :" + partMessage;
    }
    const std::set<Client*>& members = chan.getMembers();
    for (std::set<Client*>::const_iterator it = members.begin(); it != members.end(); ++it)
        sendToClient((*it)->getFd(), partMsg);
    if (chan.isOperator(_client)) {
        chan.removeOperator(_client);
    }
    chan.part(_client);
    if (chan.getMemberCount() == 0) {
        _channels.erase(channelName);
    }
}

void Server::quit() {
	// The QUIT command is used to disconnect the client from the server

    if (!_client)
        return;
    std::string quitMessage = _client->getArg().empty() ? "Client Quit" : _client->getArg();
    std::string quitMsg = ":" + _client->getPrefix() + " QUIT :" + quitMessage;
    for (std::map<std::string, Channel>::iterator it = _channels.begin(); it != _channels.end(); ++it) {
        Channel& chan = it->second;
        if (!chan.isMember(_client))
            continue;
        const std::set<Client*>& members = chan.getMembers();
        for (std::set<Client*>::const_iterator mit = members.begin(); mit != members.end(); ++mit) {
            if (*mit != _client)
                sendToClient((*mit)->getFd(), quitMsg);
        }
        chan.part(_client);
    }
    _clientsToRemove.push_back(_clientFd);
}

void Server::mode() {
    // The MODE command is used to set or retrieve channel modes
    if (!_client || std::find(_clientsToRemove.begin(), _clientsToRemove.end(), _clientFd) != _clientsToRemove.end())
        return;

    if (!_client->isRegistered()) {
        sendReply(ERR_NOTREGISTERED, _client, "", "", "You have not registered");
        return;
    }

    std::string args = _client->getArg();
    if (args.empty()) {
        sendReply(ERR_NEEDMOREPARAMS, _client, "MODE", "", "Not enough parameters");
        return;
    }

    std::vector<std::string> params = ft_split(args, ' ');
    if (params.empty()) {
        sendReply(ERR_NEEDMOREPARAMS, _client, "MODE", "", "Not enough parameters");
        return;
    }

    std::string target = params[0];
    Channel* chan = NULL;
    if (!validateModeCommand(target, chan)) {
        return;
    }

    if (params.size() == 1) {
        showCurrentModes(target, *chan);
        return;
    }

    if (!chan->isOperator(_client)) {
        sendReply(ERR_CHANOPRIVSNEEDED, _client, target, "", "You're not channel operator");
        return;
    }

    std::string modeString = params[1];
    std::vector<std::string> modeParams(params.begin() + 2, params.end());

    std::string appliedModes;
    std::string appliedParams;
    size_t paramIndex = 0;
    bool adding = true;

    for (size_t i = 0; i < modeString.length(); i++) {
        char c = modeString[i];
        if (c == '+') {
            adding = true;
        } else if (c == '-') {
            adding = false;
        } else {
            if (!processSingleMode(c, adding, modeParams, paramIndex, *chan, target, appliedModes, appliedParams)) {
                continue;
            }
        }
    }

    if (!appliedModes.empty()) {
        std::string modeMsg = ":" + _client->getPrefix() + " MODE " + target + " " + appliedModes;
        if (!appliedParams.empty()) {
            modeMsg += " " + appliedParams;
        }

        const std::set<Client*>& members = chan->getMembers();
        for (std::set<Client*>::const_iterator it = members.begin(); it != members.end(); ++it) {
            sendToClient((*it)->getFd(), modeMsg);
        }
    }
}

void Server::topic() {
	// The TOPIC command is used to set or retrieve the topic of a channel

    if (!_client || std::find(_clientsToRemove.begin(), _clientsToRemove.end(), _clientFd) != _clientsToRemove.end())
        return;

    if (!_client->isRegistered()) {
        sendReply(ERR_NOTREGISTERED, _client, "", "", "You have not registered");
        return;
    }
    std::string args = _client->getArg();
    if (args.empty()) {
        sendReply(ERR_NEEDMOREPARAMS, _client, "TOPIC", "", "Not enough parameters");
        return;
    }
    std::istringstream iss(args);
    std::string channelName;
    iss >> channelName;
    std::map<std::string, Channel>::iterator it = _channels.find(channelName);
    if (it == _channels.end()) {
        sendReply(ERR_NOSUCHCHANNEL, _client, channelName, "", "No such channel");
        return;
    }
    Channel& chan = it->second;
    if (!chan.isMember(_client)) {
        sendReply(ERR_NOTONCHANNEL, _client, channelName, "", "You're not on that channel");
        return;
    }
    size_t colonPos = args.find(" :");
    if (colonPos != std::string::npos) {
        std::string newTopic = args.substr(colonPos + 2); // Skip " :"
        if (chan.isTopicProtected() && !chan.isOperator(_client)) {
            sendReply(ERR_CHANOPRIVSNEEDED, _client, channelName, "", "You're not channel operator");
            return;
        }
        chan.setTopic(newTopic, _client->getNickname());
        std::string topicMsg = ":" + _client->getPrefix() + " TOPIC " + channelName + " :" + newTopic;
        const std::set<Client*>& members = chan.getMembers();
        for (std::set<Client*>::const_iterator it = members.begin(); it != members.end(); ++it)
            sendToClient((*it)->getFd(), topicMsg);
        sendReply(RPL_TOPIC, _client, channelName, "", chan.getTopic());
        std::ostringstream timeStr;
        timeStr << chan.getTopicSetTime();
        sendReply(RPL_TOPICWHOTIME, _client, channelName, chan.getTopicSetBy(), timeStr.str());
    }
    else {
        if (chan.getTopic().empty())
            sendReply(RPL_NOTOPIC, _client, channelName, "", "No topic is set");
        else {
            sendReply(RPL_TOPIC, _client, channelName, "", chan.getTopic());
            std::ostringstream timeStr;
            timeStr << chan.getTopicSetTime();
            sendReply(RPL_TOPICWHOTIME, _client, channelName, chan.getTopicSetBy(), timeStr.str());
        }
    }
}

void Server::invite() {
	// The INVITE command is used to invite a user to a channel

    if (!_client || std::find(_clientsToRemove.begin(), _clientsToRemove.end(), _clientFd) != _clientsToRemove.end())
        return;

    if (!_client->isRegistered()) {
        sendReply(ERR_NOTREGISTERED, _client, "", "", "You have not registered");
        return;
    }
    std::string args = _client->getArg();
    if (args.empty()) {
        sendReply(ERR_NEEDMOREPARAMS, _client, "INVITE", "", "Not enough parameters");
        return;
    }
    std::istringstream iss(args);
    std::string targetNick, channelName;

    if (!(iss >> targetNick >> channelName)) {
        sendReply(ERR_NEEDMOREPARAMS, _client, "INVITE", "", "Not enough parameters");
        return;
    }
    Client* target = getClientByNick(targetNick);
    if (!target) {
        sendReply(ERR_NOSUCHNICK, _client, targetNick, "", "No such nick/channel");
        return;
    }
    std::map<std::string, Channel>::iterator it = _channels.find(channelName);
    if (it == _channels.end()) {
        sendReply(ERR_NOSUCHCHANNEL, _client, channelName, "", "No such channel");
        return;
    }
    Channel& chan = it->second;
    if (!chan.isMember(_client)) {
        sendReply(ERR_NOTONCHANNEL, _client, channelName, "", "You're not on that channel");
        return;
    }
    if (chan.isInviteOnly() && !chan.isOperator(_client)) {
        sendReply(ERR_CHANOPRIVSNEEDED, _client, channelName, "", "You're not channel operator");
        return;
    }
    if (chan.isMember(target)) {
        sendReply(ERR_USERONCHANNEL, _client, targetNick, channelName, "is already on channel");
        return;
    }
    chan.invite(target);
    std::string inviteMsg = ":" + _client->getPrefix() + " INVITE " + targetNick + " :" + channelName;
    sendToClient(target->getFd(), inviteMsg);
    sendReply(RPL_INVITING, _client, targetNick, channelName, "");
}

void Server::kick() {
	// The KICK command is used to remove a user from a channel

    if (!_client || std::find(_clientsToRemove.begin(), _clientsToRemove.end(), _clientFd) != _clientsToRemove.end())
    return;
    if (!_client->isRegistered()) {
        sendReply(ERR_NOTREGISTERED, _client, "", "", "You have not registered");
        return;
    }
    std::vector<std::string> args = ft_split(_client->getArg(), ' ');
    if (args.size() < 2) {
        sendReply(ERR_NEEDMOREPARAMS, _client, "KICK", "", "Not enough parameters");
        return;
    }
    std::string channelName = args[0];
    std::string targetNick = args[1];
    if (channelName.empty() || channelName[0] != '#') {
        sendReply(ERR_NOSUCHCHANNEL, _client, channelName, "", "No such channel");
        return;
    }
    std::string comment = "Kicked";
    size_t colon = _client->getArg().find(':');
    if (colon != std::string::npos)
        comment = _client->getArg().substr(colon + 1);
    std::map<std::string, Channel>::iterator it = _channels.find(channelName);
    if (it == _channels.end()) {
        sendReply(ERR_NOSUCHCHANNEL, _client, channelName, "", "No such channel");
        return;
    }
    Channel& channel = it->second;
    if (!channel.isOperator(_client)) {
        sendReply(ERR_CHANOPRIVSNEEDED, _client, channelName, "", "You're not channel operator");
        return;
    }
    Client* target = getClientByNick(targetNick);
    if (!target || !channel.isMember(target)) {
        sendReply(ERR_USERNOTINCHANNEL, _client, targetNick, channelName, "They aren't on that channel");
        return;
    }
    std::string kickMsg = ":" + _client->getPrefix() + " KICK " + channelName + " " + targetNick + " :" + comment + "\r\n";
    channel.sendToAll(kickMsg);
    channel.part(target);
}

void Server::userhost() {
	// The USERHOST command returns information about a user
	// Format: USERHOST <nick1>  ...
    if (!_client || std::find(_clientsToRemove.begin(), _clientsToRemove.end(), _clientFd) != _clientsToRemove.end())
    return;
    if (!_client->isRegistered()) {
        sendReply(ERR_NOTREGISTERED, _client, "", "", "You have not registered");
        return;
    }
    std::string args = _client->getArg();
    if (args.empty()) {
        sendReply(ERR_NEEDMOREPARAMS, _client, "USERHOST", "", "Not enough parameters");
        return;
    }
    std::istringstream iss(args);
    std::string nickname;
    std::string response;

    while (iss >> nickname) {
        Client* target = getClientByNick(nickname);
        if (target && target->isRegistered()) {
            if (!response.empty())
            response += " ";
            response += target->getNickname() + "=+" + target->getUsername() + "@localhost";
        }
    }
    sendReply(302, _client, "", "", response);
}

void Server::whois() {
    // The WHOIS command returns detailed information about a user
    // Format: WHOIS nick

    if (!_client || std::find(_clientsToRemove.begin(), _clientsToRemove.end(), _clientFd) != _clientsToRemove.end())
    return;
    if (!_client->isRegistered()) {
        sendReply(ERR_NOTREGISTERED, _client, "", "", "You have not registered");
        return;
    }
    std::string args = _client->getArg();
    if (args.empty()) {
        sendReply(ERR_NEEDMOREPARAMS, _client, "WHOIS", "", "Not enough parameters");
        return;
    }
    std::istringstream iss(args);
    std::string nickname;
    iss >> nickname;
    Client* target = getClientByNick(nickname);
    if (!target) {
        sendReply(ERR_NOSUCHNICK, _client, nickname, "", "No such nick/channel");
        return;
    }
    if (!target->isRegistered()) {
        sendReply(ERR_NOSUCHNICK, _client, nickname, "", "No such nick/channel");
        return;
    }
    sendReply(311, _client, nickname, target->getUsername() + " localhost *", target->getRealname());
    sendReply(312, _client, nickname, SERVER_NAME, "IRC Server");
    sendReply(318, _client, nickname, "", "End of /WHOIS list");
}

void Server::who() {
    // WHO command returns information about users in a channel or matching a pattern
    if (!_client || std::find(_clientsToRemove.begin(), _clientsToRemove.end(), _clientFd) != _clientsToRemove.end())
        return;
    if (!_client->isRegistered()) {
        sendReply(ERR_NOTREGISTERED, _client, "", "", "You have not registered");
        return;
    }
    std::string args = _client->getArg();
    if (args.empty()) {
        for (std::map<int, Client*>::const_iterator it = _clients.begin(); it != _clients.end(); ++it) {
            if (it->second->isRegistered()) {
                sendReply(352, _client, "*", it->second->getUsername() + " localhost " + SERVER_NAME + " " + it->second->getNickname(), "H :0 " + it->second->getRealname());
            }
        }
        sendReply(315, _client, "*", "", "End of /WHO list");
        return;
    }
    std::istringstream iss(args);
    std::string target;
    iss >> target;
    if (target[0] == '#') {
        std::map<std::string, Channel>::iterator chanIt = _channels.find(target);
        if (chanIt != _channels.end()) {
            const std::set<Client*>& members = chanIt->second.getMembers();
            for (std::set<Client*>::const_iterator it = members.begin(); it != members.end(); ++it) {
                std::string status = chanIt->second.isOperator(*it) ? "@" : "";
                sendReply(352, _client, target, (*it)->getUsername() + " localhost " + SERVER_NAME + " " + (*it)->getNickname(), "H" + status + " :0 " + (*it)->getRealname());
            }
        }
        sendReply(315, _client, target, "", "End of /WHO list");
    } else {
        Client* targetClient = getClientByNick(target);
        if (targetClient && targetClient->isRegistered()) {
            sendReply(352, _client, "*", targetClient->getUsername() + " localhost " + SERVER_NAME + " " + targetClient->getNickname(), "H :0 " + targetClient->getRealname());
        }
        sendReply(315, _client, target, "", "End of /WHO list");
    }
}

void Server::names() {
    // NAMES command returns the list of users in a channel
    if (!_client || std::find(_clientsToRemove.begin(), _clientsToRemove.end(), _clientFd) != _clientsToRemove.end())
        return;
    
    if (!_client->isRegistered()) {
        sendReply(ERR_NOTREGISTERED, _client, "", "", "You have not registered");
        return;
    }
    
    std::string args = _client->getArg();
    if (args.empty()) {
        for (std::map<std::string, Channel>::const_iterator it = _channels.begin(); it != _channels.end(); ++it) {
            std::string names = "";
            const std::set<Client*>& members = it->second.getMembers();
            for (std::set<Client*>::const_iterator memberIt = members.begin(); memberIt != members.end(); ++memberIt) {
                if (!names.empty()) names += " ";
                if (it->second.isOperator(*memberIt)) names += "@";
                names += (*memberIt)->getNickname();
            }
            sendReply(353, _client, "=", it->first, names);
        }
        sendReply(366, _client, "*", "", "End of /NAMES list");
        return;
    }
    std::istringstream iss(args);
    std::string channelName;
    iss >> channelName;
    if (channelName[0] == '#') {
        std::map<std::string, Channel>::iterator it = _channels.find(channelName);
        if (it != _channels.end()) {
            std::string names = "";
            const std::set<Client*>& members = it->second.getMembers();
            for (std::set<Client*>::const_iterator memberIt = members.begin(); memberIt != members.end(); ++memberIt) {
                if (!names.empty()) names += " ";
                if (it->second.isOperator(*memberIt)) names += "@";
                names += (*memberIt)->getNickname();
            }
            sendReply(353, _client, "=", channelName, names);
        }
        sendReply(366, _client, channelName, "", "End of /NAMES list");
    }
}

