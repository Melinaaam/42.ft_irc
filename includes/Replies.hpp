#ifndef REPLIES_HPP
#define REPLIES_HPP

#include <string>

#define SERVER_NAME "irc.ft_irc"

// === Numeric Replies ===
#define RPL_WELCOME           001
#define RPL_YOURHOST          002
#define RPL_CREATED           003
#define RPL_MYINFO            004

#define RPL_NOTOPIC           331
#define RPL_TOPIC             332
#define RPL_TOPICWHOTIME      333

#define RPL_INVITING          341
#define RPL_NAMREPLY          353
#define RPL_ENDOFNAMES        366

#define ERR_NOSUCHNICK        401
#define ERR_NOSUCHCHANNEL     403
#define ERR_CANNOTSENDTOCHAN  404

#define ERR_NORECIPIENT       411
#define ERR_NOTEXTTOSEND      412
#define ERR_NOORIGIN          409
#define ERR_UNKNOWNCOMMAND    421

#define ERR_NONICKNAMEGIVEN   431
#define ERR_ERRONEUSNICKNAME  432
#define ERR_NICKNAMEINUSE     433

#define ERR_USERNOTINCHANNEL  441
#define ERR_NOTONCHANNEL      442
#define ERR_USERONCHANNEL     443

#define ERR_NOTREGISTERED     451
#define ERR_NEEDMOREPARAMS    461
#define ERR_ALREADYREGISTRED  462
#define ERR_PASSWDMISMATCH    464

#define ERR_CHANNELISFULL     471
#define ERR_UNKNOWNMODE       472
#define ERR_INVITEONLYCHAN    473
#define ERR_BANNEDFROMCHAN    474
#define ERR_BADCHANNELKEY     475
#define ERR_BADCHANMASK       476

#define ERR_FILEERROR         481
#define ERR_CHANOPRIVSNEEDED  482

// === Simple Replies ===
#define MSG_ERR_NONICKNAMEGIVEN         "431 * :No nickname given"
#define MSG_ERR_PASSWDMISMATCH          "464 :Password required"

// === Inline Functions ===
inline std::string msgWelcome(const std::string& nick) {
	return ":irc.ft_irc 001 " + nick + " :Welcome to the IRC server!\r\n";
}

inline std::string errNickInUse(const std::string& nick) {
	return ":irc.ft_irc 433 " + nick + " :Nickname is already in use\r\n";
}

inline std::string errNeedMoreParams(const std::string& cmd) {
	return ":irc.ft_irc 461 " + cmd + " :Not enough parameters\r\n";
}

inline std::string errUnknownCommand(const std::string& cmd) {
	return ":irc.ft_irc 421 " + cmd + " :Unknown command\r\n";
}

inline std::string replyTopic(const std::string& source, const std::string& channel, const std::string& topic) {
	return ":" + source + " 332 " + channel + " :" + topic + "\r\n";
}

inline std::string replyNoTopic(const std::string& source, const std::string& channel) {
	return ":" + source + " 331 " + channel + " :No topic is set\r\n";
}

inline std::string errNotOnChannel(const std::string& source, const std::string& channel) {
	return ":" + source + " 442 " + channel + " :You're not on that channel\r\n";
}

inline std::string errChanOpNeeded(const std::string& source, const std::string& channel) {
	return ":" + source + " 482 " + channel + " :You're not channel operator\r\n";
}

#endif
