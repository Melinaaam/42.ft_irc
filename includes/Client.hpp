#ifndef CLIENT_HPP
# define CLIENT_HPP

# include <iostream>
# include <unistd.h>
# include <sstream>
# include "Replies.hpp"
# include "Server.hpp"

class Client
{
private:
	int _fd;         // File descriptor of the client socket
	std::string _ip; // IP address of the client

	bool _isRegistered;
	bool _passOk;
	bool _hasUser;
	bool _hasNick;
	bool _passErrorSent;

	std::string _username;
	std::string _nickname;
	std::string _realName;

	std::string _readBuf;
	std::string _command;
	std::string _arg;

	// Client spécial ? (e.g. netcat sans \r)
	bool _clientType;
	bool _capNegotiationDone;

	// Client Type Detection
	bool _isNetcatLike;

public:
	Client(int fd, const std::string& ip);
	~Client();

	int getFd() const;
	std::string getIp() const;
	bool isRegistered() const;
	bool isPasswordOk() const;
	bool hasNick() const;
	bool hasUser() const;
	std::string getPrefix() const; // nickname!username@localhost

	const std::string& getUsername() const;
	const std::string& getNickname() const;
	const std::string& getRealname() const;
	std::string getCmd() const;
	std::string getArg() const;
	std::string& getBuffer();
	bool getClientType() const;

	void setUsername(const std::string& user);
	void setNickname(const std::string& nick);
	void setRealname(const std::string& real);
	void setPasswordOk(bool ok);
	void setCommand(const std::string& cmd);
	void setArg(const std::string& arg);
	void setCapNegotiationDone(bool done);
	void setClientType(bool isNetcat) { _isNetcatLike = isNetcat; }
	bool isNetcatLike() const { return _isNetcatLike; }

	void eraseBuf();
	void setPassErrorSent(bool v);
	bool hasSentPassError() const;

	void registerUser(const std::string& nick, const std::string& user, const std::string& real);
	void parseLine(const std::string& line);
	void appendToBuffer(const std::string& data);
};

#endif
