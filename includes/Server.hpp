#ifndef SERVER_HPP
# define SERVER_HPP

# include <iostream>
# include <sstream>
# include <fstream>
# include <unistd.h>
# include <map>
# include <vector>
# include <string>
# include <fcntl.h>
# include <cstring>
# include <stdexcept>
# include <netinet/in.h>
# include <sys/epoll.h>
# include <arpa/inet.h>
# include <algorithm>
# include <cerrno>
# include <vector>
# include "Utils.hpp"
# include "Client.hpp"
# include "Channel.hpp"
# include "colors.hpp"
# include "Replies.hpp"

# define MAX_EVENTS 64

struct PendingTransfer {
    std::string sender;
    std::string content;
    std::string filename;
};

class Server {
public:
    Server(int port, const std::string& password);
    ~Server();

	void checkRegistration();
    void run();
    typedef void (Server::*CommandFunc)();
	void disconnectClient(int fd);

private:
    int _port;
    int _serverSocket;
    int _epollFd;
    int _clientFd;
    std::string _password;

    std::map<std::string, Channel> _channels;
    std::map<int, Client*> _clients;
    Client* _client;
	std::vector<int> _clientsToRemove;

    bool _botEnabled;

    std::map<int, std::string> _clientChatMode; // fd -> canal actuel
    std::map<int, bool> _clientChatModePrompt; // fd -> en attente de réponse yes/no

    bool isInChatMode(int clientFd);
    std::string getCurrentChannel(int clientFd);
    void setChatMode(int clientFd, const std::string& channel);
    void exitChatMode(int clientFd);
    void promptChatMode(int clientFd, const std::string& failedCommand);
    bool isAwaitingChatModeResponse(int clientFd);
    void handleChatModeResponse(int clientFd, const std::string& response);

    time_t _serverStartTime;
    std::map<std::string, int> _userMessageCount;
    std::map<std::string, int> _channelMessageCount;
    int _totalBotInteractions;
    int _totalJokesShared;

    void loadBotStats();
    void saveBotStats();
    void updateBotStats(const std::string& action, const std::string& user = "", const std::string& channel = "");
    std::string getUptimeString();

    std::vector<std::string> _bannedWords;
    std::map<std::string, std::vector<std::string> > _channelBannedUsers; // canal -> liste users bannis

    void loadBannedWords();
    bool containsBannedWord(const std::string& message);
    void moderateMessage(const std::string& message, const std::string& channelName);
    bool isUserBannedFromChannel(const std::string& nickname, const std::string& channelName);
    void banUserFromChannel(const std::string& nickname, const std::string& channelName);

    std::string processIRCBotCommand(const std::string& command, const std::string& channelName);

    static const std::string _type[20];
    static CommandFunc _function[20];

    std::map<std::string, PendingTransfer> _pendingTransfers; // key: receiver_filename

    void initServerSocket();
    void handleNewConnection();
    void setNonBlocking(int fd);

    Client* getClientByNick(const std::string& nickname);
    void handleCommand(int clientFd);

    void sendToClient(int fd, const std::string& msg);
	void sendReply(int code, Client* client, const std::string& arg1 = "", const std::string& arg2 = "", const std::string& trailing = "");

	void cleanupClients();

    void execCommand();
    void cap();
    void pass();
    void nick();
    void user();
    void privmsg();
    void join();
    void part();
    void quit();
    void mode();
    void topic();
    void invite();
    void kick();
    void ping();
    void pong();
    void userhost();
    void whois();
    void sendfile();
    void acceptFile();
    void refuseFile();
    void bot();
	void who();
	void names();

    // ModeCommand utility methods
    bool validateModeCommand(const std::string& target, Channel*& chan);
    void showCurrentModes(const std::string& channelName, const Channel& chan);
    bool processSingleMode(char flag, bool adding, const std::vector<std::string>& params,
                          size_t& paramIndex, Channel& chan, const std::string& channelName,
                          std::string& appliedModes, std::string& appliedParams);
    bool handleOperatorMode(bool adding, const std::vector<std::string>& params, size_t& paramIndex,
                           Channel& chan, const std::string& channelName,
                           std::string& appliedModes, std::string& appliedParams);
    void handleChannelMessage(const std::string& channelName, const std::string& message);
    void handlePrivateMessage(const std::string& targetNick, const std::string& message);

	Client* _ircBotClient;
    bool _ircBotCreated;

    void createIRCBotGhost();
    void removeIRCBotGhost();
    bool isRealIRCClient(int clientFd);
};

#endif
