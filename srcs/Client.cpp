#include "Client.hpp"
#include <algorithm>
#include <cctype>

Client::Client(int fd, const std::string& ip)
	: _fd(fd), _ip(ip), _isRegistered(false), _passOk(false),
	_hasUser(false), _hasNick(false), _passErrorSent(false),
	_username(""), _nickname(""), _realName(""),
	_readBuf(""), _command(""), _arg(""),
	_clientType(false), _capNegotiationDone(false), _isNetcatLike(false) {}

Client::~Client() {}

// ========== GETTERS ==========
int Client::getFd() const { return _fd; }

std::string Client::getIp() const { return _ip; }

bool Client::isRegistered() const { return _isRegistered; }

bool Client::isPasswordOk() const { return _passOk; }

bool Client::hasNick() const { return _hasNick; }

bool Client::hasUser() const { return _hasUser; }

bool Client::getClientType() const { return _clientType; }

bool Client::hasSentPassError() const { return _passErrorSent; }
// bool Client::isCapNegotiationDone() const { return _capNegotiationDone; }

const std::string& Client::getUsername() const { return _username; }

const std::string& Client::getNickname() const { return _nickname; }

const std::string& Client::getRealname() const { return _realName; }

std::string Client::getPrefix() const {
	return _nickname + "!" + _username + "@localhost";
}

std::string& Client::getBuffer() { return _readBuf; }

std::string Client::getCmd() const { return _command; }

std::string Client::getArg() const { return _arg; }

// ========== SETTERS ==========
void Client::setNickname(const std::string& nick) {
	if (!nick.empty() && nick.find(' ') == std::string::npos) {
		_nickname = nick;
		_hasNick = true;
	}
}

void Client::setUsername(const std::string& user) {
	_username = user;
	_hasUser = true;
}

void Client::setRealname(const std::string& real) { _realName = real; }

void Client::setPasswordOk(bool ok) { _passOk = ok; }

void Client::setPassErrorSent(bool v) { _passErrorSent = v; }

void Client::setCapNegotiationDone(bool done) { _capNegotiationDone = done; }

void Client::setCommand(const std::string& cmd) { _command = cmd; }

void Client::setArg(const std::string& arg) { _arg = arg; }

void Client::eraseBuf() { _readBuf.clear(); }

// ========== OTHER ==========
void Client::registerUser(const std::string& nick, const std::string& user, const std::string& real) {
	// std::cout << "[DEBUG] → registerUser called" << std::endl;

	setNickname(nick);
	setUsername(user);
	setRealname(real);
	_isRegistered = true;
}

void Client::parseLine(const std::string& line) {
	_command.clear();
	_arg.clear();

	std::istringstream iss(line);
	iss >> _command;
	std::transform(_command.begin(), _command.end(), _command.begin(), ::toupper);
	std::getline(iss, _arg);
	if (!_arg.empty() && _arg[0] == ' ')
		_arg.erase(0, 1);
}

void Client::appendToBuffer(const std::string& data) {
	_readBuf += data;
}
