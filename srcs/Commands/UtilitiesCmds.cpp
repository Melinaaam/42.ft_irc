#include "Server.hpp"
#include "Replies.hpp"
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <fstream>

void Server::handleChannelMessage(const std::string& channelName, const std::string& message) {
    std::map<std::string, Channel>::iterator it = _channels.find(channelName);
    if (it == _channels.end()) {
        sendReply(ERR_NOSUCHCHANNEL, _client, channelName, "", "No such channel");
        return;
    }
    Channel& channel = it->second;
    if (!channel.isMember(_client)) {
        sendReply(ERR_CANNOTSENDTOCHAN, _client, channelName, "", "Cannot send to channel");
        return;
    }
    if (message.find("!") == 0) {
        std::string botResponse = processIRCBotCommand(message, channelName);
        if (!botResponse.empty()) {
            std::string botMsg = ":IRCBot!bot@" + std::string(SERVER_NAME) + " PRIVMSG " + channelName + " :" + botResponse;
            const std::set<Client*>& members = channel.getMembers();
            for (std::set<Client*>::const_iterator it = members.begin(); it != members.end(); ++it) {
                sendToClient((*it)->getFd(), botMsg);
            }
            return;
        }
    }
    if (_botEnabled) {
        if (message.find("hello") != std::string::npos ||
            message.find("hi") != std::string::npos ||
            message.find("time") != std::string::npos ||
            message.find("help") != std::string::npos ||
            message.find("joke") != std::string::npos ||
            message.find("bye") != std::string::npos ||
            message.find("goodbye") != std::string::npos ||
            (message.find("bot") != std::string::npos && message.find("online") != std::string::npos)) {

            std::string botResponse = "";
            if (message.find("hello") != std::string::npos || message.find("hi") != std::string::npos) {
                botResponse = "👋 Hello! How can I assist you?";
            } else if (message.find("time") != std::string::npos) {
                time_t now = time(0);
                char* dt = ctime(&now);
                std::string timeStr = dt;
                if (!timeStr.empty() && timeStr[timeStr.length()-1] == '\n') {
                    timeStr.erase(timeStr.length()-1);
                }
                botResponse = "⏰ Current time: " + timeStr;
            } else if (message.find("joke") != std::string::npos) {
                updateBotStats("joke", _client->getNickname(), channelName);
                const char* jokes[] = {
                    "😄 Why do programmers prefer dark mode? Because light attracts bugs! 🐛",
                    "😄 Why did the IRC user go broke? Too many /quit commands! 💸",
                    "😄 What's a computer's favorite snack? Microchips! 🍟"
                };
                botResponse = jokes[rand() % 3];
            } else if (message.find("bye") != std::string::npos || message.find("goodbye") != std::string::npos) {
                botResponse = "👋 Goodbye! Have a great day!";
            } else if (message.find("help") != std::string::npos) {
                botResponse = "📋 I understand: hello, time, help, joke, bye, goodbye";
            } else if (message.find("bot") != std::string::npos && message.find("online") != std::string::npos) {
                botResponse = "🤖 Yes, I'm online and ready to help!";
            }

            if (!botResponse.empty()) {
                std::string botMsg = ":IRCBot!bot@" + std::string(SERVER_NAME) + " PRIVMSG " + channelName + " :" + botResponse;
                const std::set<Client*>& members = channel.getMembers();
                for (std::set<Client*>::const_iterator it = members.begin(); it != members.end(); ++it) {
                    sendToClient((*it)->getFd(), botMsg);
                }
            }
        }
    }
	updateBotStats("message", _client->getNickname(), channelName);
    moderateMessage(message, channelName);
    if (!channel.isMember(_client)) {
        return;
    }
    std::string fullMsg = ":" + _client->getPrefix() + " PRIVMSG " + channelName + " :" + message;
    const std::set<Client*>& members = channel.getMembers();
    int sentCount = 0;
    for (std::set<Client*>::const_iterator it = members.begin(); it != members.end(); ++it) {
        if (*it != _client) {
            sendToClient((*it)->getFd(), fullMsg);
            sentCount++;
        }
    }
}

void Server::handlePrivateMessage(const std::string& targetNick, const std::string& message) {
    if (targetNick == "IRCBot" && message.find("!") == 0) {
        std::string botResponse = processIRCBotCommand(message, "");
        if (!botResponse.empty()) {
            std::string botMsg = ":IRCBot!bot@" + std::string(SERVER_NAME) + " PRIVMSG " + _client->getNickname() + " :" + botResponse;
            sendToClient(_client->getFd(), botMsg);
            return;
        }
    }
    Client* target = getClientByNick(targetNick);
    if (!target) {
        sendReply(ERR_NOSUCHNICK, _client, targetNick, "", "No such nick/channel");
        return;
    }
    if (!target->isRegistered()) {
        return;
    }
    std::string fullMsg = ":" + _client->getPrefix() + " PRIVMSG " + targetNick + " :" + message;
    sendToClient(target->getFd(), fullMsg);
}

bool Server::validateModeCommand(const std::string& target, Channel*& chan) {
	if (!isValidChannelName(target)) {
		sendReply(ERR_NOSUCHCHANNEL, _client, target, "", "No such channel");
		return false;
	}
	std::map<std::string, Channel>::iterator it = _channels.find(target);
	if (it == _channels.end()) {
		sendReply(ERR_NOSUCHCHANNEL, _client, target, "", "No such channel");
		return false;
	}
	chan = &(it->second);
	return true;
}

void Server::showCurrentModes(const std::string& channelName, const Channel& chan) {
	std::string currentModes = "+";
	if (chan.isInviteOnly()) currentModes += "i";
	if (chan.isTopicProtected()) currentModes += "t";
	if (chan.hasKey()) currentModes += "k";
	if (chan.hasUserLimit()) currentModes += "l";
	sendToClient(_clientFd, std::string(":") + SERVER_NAME + " 324 " + _client->getNickname() + " " + channelName + " " + currentModes + "\r\n");
}

bool Server::processSingleMode(char flag, bool adding, const std::vector<std::string>& params,
							  size_t& paramIndex, Channel& chan, const std::string& channelName,
							  std::string& appliedModes, std::string& appliedParams) {
	switch (flag) {
		case 'i':
			chan.setInviteOnly(adding);
			appliedModes += (adding ? "+" : "-");
			appliedModes += flag;
			break;
		case 't':
			chan.setTopicProtected(adding);
			appliedModes += (adding ? "+" : "-");
			appliedModes += flag;
			break;
		case 'k':
			if (adding) {
				if (paramIndex >= params.size()) {
					sendReply(ERR_NEEDMOREPARAMS, _client, "MODE", "", "Need key parameter for +k");
					return false;
				}
				chan.setKey(params[paramIndex]);
				appliedModes += "+k";
				if (!appliedParams.empty()) appliedParams += " ";
				appliedParams += params[paramIndex];
				paramIndex++;
			}
			else {
				chan.removeKey();
				appliedModes += "-k";
			}
			break;
		case 'l':
			if (adding) {
				if (paramIndex >= params.size()) {
					sendReply(ERR_NEEDMOREPARAMS, _client, "MODE", "", "Need limit parameter for +l");
					return false;
				}
				int limit = std::atoi(params[paramIndex].c_str());
				if (limit <= 0) {
					sendReply(ERR_NEEDMOREPARAMS, _client, "MODE", "", "Invalid limit parameter for +l");
					return false;
				}
				chan.setUserLimit(limit);
				appliedModes += "+l";
				if (!appliedParams.empty()) appliedParams += " ";
				appliedParams += params[paramIndex];
				paramIndex++;
			}
			else {
				chan.removeUserLimit();
				appliedModes += "-l";
			}
			break;
		case 'o':
			return handleOperatorMode(adding, params, paramIndex, chan, channelName, appliedModes, appliedParams);
		case 'b':
			if (adding && paramIndex < params.size()){
				paramIndex++;
			}
			break;
		default:
			sendReply(ERR_UNKNOWNMODE, _client, std::string(1, flag), "", "is unknown mode char to me");
			return false;
	}
	return true;
}

bool Server::handleOperatorMode(bool adding, const std::vector<std::string>& params, size_t& paramIndex,
							   Channel& chan, const std::string& channelName,
							   std::string& appliedModes, std::string& appliedParams) {
	if (paramIndex >= params.size()) {
		sendReply(ERR_NEEDMOREPARAMS, _client, "MODE", "", "Need nickname parameter for +o/-o");
		return false;
	}
	std::string targetNick = params[paramIndex];
	Client* targetClient = getClientByNick(targetNick);
	if (!targetClient) {
		sendReply(ERR_NOSUCHNICK, _client, targetNick, "", "No such nick/channel");
		return false;
	}
	if (!chan.isMember(targetClient)) {
		sendReply(ERR_USERNOTINCHANNEL, _client, targetNick, channelName, "They aren't on that channel");
		return false;
	}
	if (adding) {
		if (!chan.isOperator(targetClient)) {
			chan.addOperator(targetClient);
			appliedModes += "+o";
			if (!appliedParams.empty()) appliedParams += " ";
			appliedParams += targetNick;
		}
	}
	else {
		if (chan.isOperator(targetClient)) {
			chan.removeOperator(targetClient);
			appliedModes += "-o";
			if (!appliedParams.empty()) appliedParams += " ";
			appliedParams += targetNick;
		}
	}
	paramIndex++;
	return true;
}

std::string Server::getUptimeString() {
    time_t currentTime = time(0);
    time_t uptime = currentTime - _serverStartTime;

    int days = uptime / 86400;
    int hours = (uptime % 86400) / 3600;
    int minutes = (uptime % 3600) / 60;
    int seconds = uptime % 60;

    std::ostringstream oss;
    if (days > 0) oss << days << "d ";
    if (hours > 0) oss << hours << "h ";
    if (minutes > 0) oss << minutes << "m ";
    oss << seconds << "s";

    return oss.str();
}

std::string Server::getCurrentChannel(int clientFd) {
    std::map<int, std::string>::const_iterator it = _clientChatMode.find(clientFd);
    return (it != _clientChatMode.end()) ? it->second : "";
}