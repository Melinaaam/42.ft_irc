#include"Server.hpp"

void Server::loadBotStats() {
    std::ifstream file("bot_stats.txt");
    if (!file.is_open()) {
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            if (key == "TOTAL_INTERACTIONS") {
                _totalBotInteractions = std::atoi(value.c_str());
            } else if (key == "TOTAL_JOKES") {
                _totalJokesShared = std::atoi(value.c_str());
            }
        }
    }
    file.close();
}

void Server::saveBotStats() {
    std::ofstream file("bot_stats.txt");
    if (!file.is_open()) return;

    file << "# Bot Statistics File\n";
    file << "# Format: KEY=VALUE\n";
    file << "TOTAL_MESSAGES=" << (_userMessageCount.size() > 0 ? _userMessageCount.size() : 0) << "\n";
    file << "TOTAL_JOKES=" << _totalJokesShared << "\n";
    file << "TOTAL_INTERACTIONS=" << _totalBotInteractions << "\n";
    file << "UPTIME_START=" << _serverStartTime << "\n";

    std::string mostActiveUser = "";
    int maxUserMessages = 0;
    for (std::map<std::string, int>::const_iterator it = _userMessageCount.begin(); it != _userMessageCount.end(); ++it) {
        if (it->second > maxUserMessages) {
            maxUserMessages = it->second;
            mostActiveUser = it->first;
        }
    }
    file << "MOST_ACTIVE_USER=" << mostActiveUser << "\n";

    std::string mostActiveChannel = "";
    int maxChannelMessages = 0;
    for (std::map<std::string, int>::const_iterator it = _channelMessageCount.begin(); it != _channelMessageCount.end(); ++it) {
        if (it->second > maxChannelMessages) {
            maxChannelMessages = it->second;
            mostActiveChannel = it->first;
        }
    }
    file << "MOST_ACTIVE_CHANNEL=" << mostActiveChannel << "\n";
    file << "LAST_RESTART=" << time(0) << "\n";

    file.close();
}

void Server::updateBotStats(const std::string& action, const std::string& user, const std::string& channel) {
    if (action == "interaction") {
        _totalBotInteractions++;
    } else if (action == "joke") {
        _totalJokesShared++;
    } else if (action == "message" && !user.empty()) {
        _userMessageCount[user]++;
        if (!channel.empty()) {
            _channelMessageCount[channel]++;
        }
    }
}

std::string Server::processIRCBotCommand(const std::string& command, const std::string& channelName) {
    (void)channelName;
    std::string cmd = command;
    if (cmd == "enable" || cmd == "bot enable" || cmd == "enable bot") {
        if (!_botEnabled) {
            _botEnabled = true;
            return "🤖 Bot is now ENABLED! ✅";
        }
		else {
            return "🤖 I'm already ENABLED! ✅";
        }
    }
    if (cmd == "disable" || cmd == "bot disable" || cmd == "disable bot") {
        if (_botEnabled) {
            _botEnabled = false;
            return "🤖 Bot is now DISABLED. Use 'bot enable' to reactivate me.";
        }
		else {
            return "🤖 I'm already ENABLED! ✅";
        }
    }
    if (cmd == "status" || cmd == "bot status") {
        return _botEnabled ? "🤖 Bot status: ENABLED ✅" : "🤖 Bot status: DISABLED ❌";
    }
    if (cmd == "stats") {
        updateBotStats("interaction", _client->getNickname());

        std::ostringstream oss;
        oss << "DETAILED SERVER STATISTICS:\n";
        oss << "┌─ CONNECTION STATS ─────────────────┐\n";
        oss << "│ Connected Users: " << _clients.size() << " clients        │\n";
        oss << "│ Active Channels: " << _channels.size() << " channels       │\n";

        int totalMembers = 0;
        int maxChannelSize = 0;
        std::string largestChannel = "N/A";

        for (std::map<std::string, Channel>::const_iterator it = _channels.begin(); it != _channels.end(); ++it) {
            int members = it->second.getMemberCount();
            totalMembers += members;
            if (members > maxChannelSize) {
                maxChannelSize = members;
                largestChannel = it->first;
            }
        }
        oss << "│ Total Memberships: " << totalMembers << "            │\n";
        oss << "│ Largest Channel: " << largestChannel;
        for (int i = largestChannel.length(); i < 10; i++) oss << " ";
        oss << "│\n";
        oss << "│ Bot Interactions: " << _totalBotInteractions << "             │\n";
        oss << "│ Jokes Shared: " << _totalJokesShared << "                 │\n";
        oss << "│ Server Uptime: ";
        std::string uptimeStr = getUptimeString();
        oss << uptimeStr;

		int paddingNeeded = 12 - uptimeStr.length();
        for (int i = 0; i < paddingNeeded; i++) oss << " ";
        oss << "│\n";
        oss << "└────────────────────────────────────┘\n";
        oss << "Status: OPERATIONAL & STABLE";

        saveBotStats();
        return oss.str();
    }
    if (cmd == "uptime") {
        updateBotStats("interaction", _client->getNickname());
        return "Server uptime: " + getUptimeString() + "\nStarted: " + std::string(ctime(&_serverStartTime)).substr(0, 24);
    }
    if (cmd == "users") {
        updateBotStats("interaction", _client->getNickname());

        std::ostringstream oss;
        oss << "CONNECTED USERS (" << _clients.size() << " total):\n";

        int count = 0;
        for (std::map<int, Client*>::const_iterator it = _clients.begin(); it != _clients.end(); ++it) {
            if (it->second->isRegistered()) {
                oss << "• " << it->second->getNickname();
                if (count % 3 == 2) oss << "\n";
                else oss << "  ";
                count++;
            }
        }
        if (count % 3 != 0) oss << "\n";
        oss << "All users are welcomed by IRCBot!";
        return oss.str();
    }
    if (cmd == "channels") {
        updateBotStats("interaction", _client->getNickname());

        std::ostringstream oss;
        oss << "ACTIVE CHANNELS (" << _channels.size() << " total):\n";

        for (std::map<std::string, Channel>::const_iterator it = _channels.begin(); it != _channels.end(); ++it) {
            oss << "• " << it->first << " (" << it->second.getMemberCount() << " users)";

            std::string modes = "";
            if (it->second.isInviteOnly()) modes += "i";
            if (it->second.isTopicProtected()) modes += "t";
            if (it->second.hasKey()) modes += "k";
            if (it->second.hasUserLimit()) modes += "l";

            if (!modes.empty()) {
                oss << " [+" << modes << "]";
            }
            if (!it->second.getTopic().empty()) {
                std::string topic = it->second.getTopic();
                if (topic.length() > 30) {
                    topic = topic.substr(0, 27) + "...";
                }
                oss << " - \"" << topic << "\"";
            }

            oss << "\n";
        }

        if (_channels.empty()) {
            oss << "No active channels. Create one with JOIN #channel!";
        } else {
            oss << "Join any channel to start chatting!\n";
            oss << "Use JOIN #channelname to join a channel";
        }

        return oss.str();
    }

    if (cmd == "joke") {
        updateBotStats("joke", _client->getNickname());
        const char* jokes[] = {
            "😄 Why do programmers prefer dark mode? Because light attracts bugs!",
            "😄 Why did the IRC user go broke? Too many /quit commands!",
            "😄 What's a computer's favorite snack? Microchips!",
            "😄 How do you comfort a JavaScript bug? You console it!",
            "😄 Why do Java developers wear glasses? Because they can't C# !",
            "😄 What's a programmer's favorite hangout place? Foo Bar!"
        };
        return jokes[rand() % 6];
    }

    if (cmd == "hello") {
        updateBotStats("interaction", _client->getNickname());
        return "Hello! How can I assist you today?";
    }
    if (cmd == "time") {
        updateBotStats("interaction", _client->getNickname());
        time_t now = time(0);
        char* dt = ctime(&now);
        std::string timeStr = dt;
        if (!timeStr.empty() && timeStr[timeStr.length()-1] == '\n') {
            timeStr.erase(timeStr.length()-1);
        }
        return "Current time: " + timeStr;
    }
    if (cmd == "bye") {
        updateBotStats("interaction", _client->getNickname());
        return "Goodbye! Have a great day!";
    }
    if (cmd == "bot" || cmd == "help") {
        return "🤖 Bot Control Commands:\n"
               "• BOT enable/disable - Activate/deactivate bot\n"
               "• BOT status - Check bot status\n"
               "• BOT stats - Server statistics\n"
               "• BOT uptime - Server uptime\n"
               "• BOT users - Connected users\n"
               "• BOT channels - Active channels\n"
               "• BOT chat #channel - Enable chat mode\n"
               "• BOT chat exit - Disable chat mode\n"
               "• BOT joke - Tell a random joke\n"
               "💬 Conversational: hello, time, help, joke, bye in channels";
    }
    return "";
}

//moderation
void Server::loadBannedWords() {
    std::ifstream file("banned_words.txt");
    if (!file.is_open()) {
        std::cout << YELLOW << "[WARNING] banned_words.txt file not found. Moderation disabled." << RESET << std::endl;
        return;
    }
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::string word = line;
        std::transform(word.begin(), word.end(), word.begin(), ::tolower);
        _bannedWords.push_back(word);
    }
    file.close();
}

bool Server::containsBannedWord(const std::string& message) {
    if (_bannedWords.empty()) return false;
    std::string lowerMessage = message;
    std::transform(lowerMessage.begin(), lowerMessage.end(), lowerMessage.begin(), ::tolower);
    for (std::vector<std::string>::const_iterator it = _bannedWords.begin(); it != _bannedWords.end(); ++it) {
        if (lowerMessage.find(*it) != std::string::npos) {
            return true;
        }
    }
    return false;
}

void Server::moderateMessage(const std::string& message, const std::string& channelName) {
    if (!containsBannedWord(message)) return;

    std::map<std::string, Channel>::iterator it = _channels.find(channelName);
    if (it == _channels.end()) return;

    Channel& channel = it->second;
    std::string warningMsg = ":IRCBot!bot@" + std::string(SERVER_NAME) +
                            " PRIVMSG " + channelName +
                            " :⚠️ MODERATION: User " + _client->getNickname() +
                            " has been kicked for inappropriate language!";

    const std::set<Client*>& members = channel.getMembers();
    for (std::set<Client*>::const_iterator mit = members.begin(); mit != members.end(); ++mit) {
        sendToClient((*mit)->getFd(), warningMsg);
    }
    channel.part(_client);
    banUserFromChannel(_client->getNickname(), channelName);
    std::string privateMsg = ":IRCBot!bot@" + std::string(SERVER_NAME) +
                            " PRIVMSG " + _client->getNickname() +
                            " :🚫 You have been kicked AND BANNED from " + channelName +
                            " for inappropriate language. You cannot rejoin this channel.";
    sendToClient(_client->getFd(), privateMsg);
    std::cout << RED << "[MODERATION] " << _client->getNickname() << " expelled from " << channelName << " for: " << message << RESET << std::endl;
}

bool Server::isUserBannedFromChannel(const std::string& nickname, const std::string& channelName) {
    std::map<std::string, std::vector<std::string> >::const_iterator it = _channelBannedUsers.find(channelName);
    if (it == _channelBannedUsers.end()) {
        return false;
    }
    const std::vector<std::string>& bannedUsers = it->second;
    return std::find(bannedUsers.begin(), bannedUsers.end(), nickname) != bannedUsers.end();
}

void Server::banUserFromChannel(const std::string& nickname, const std::string& channelName) {
    _channelBannedUsers[channelName].push_back(nickname);
    std::cout << RED << "[BAN] " << nickname << " banni du canal " << channelName << RESET << std::endl;
}

//bot for irc

void Server::createIRCBotGhost() {
    if (_ircBotCreated) return;
    int ghostFd = -999;
    _ircBotClient = new Client(ghostFd, "irc.ft_irc");
    _ircBotClient->setNickname("IRCBot");
    _ircBotClient->setUsername("bot");
    _ircBotClient->setRealname("IRC Server Bot");
    _ircBotClient->setPasswordOk(true);
    _ircBotClient->registerUser("IRCBot", "bot", "IRC Server Bot");
    _clients[ghostFd] = _ircBotClient;
    _ircBotCreated = true;
}

void Server::removeIRCBotGhost() {
    if (!_ircBotCreated || !_ircBotClient) return;

    int ghostFd = _ircBotClient->getFd();
    _clients.erase(ghostFd);
    delete _ircBotClient;
    _ircBotClient = NULL;
    _ircBotCreated = false;
}

bool Server::isRealIRCClient(int clientFd) {
    Client* client = _clients[clientFd];
    if (!client)
		return false;
    return !client->isNetcatLike();
}