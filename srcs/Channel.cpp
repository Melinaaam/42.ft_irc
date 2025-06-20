#include "Channel.hpp"
#include "Client.hpp"
#include "colors.hpp"
#include <iostream>
#include <sys/socket.h>
#include <ctime>

Channel::Channel(const std::string& name)
	: _name(name), _topic(""), _topicSetBy(""), _topicSetTime(0), _creator(""),
	  _members(), _operators(), _invited(),
	  _inviteOnly(false), _topicProtected(false),
	  _key(""), _hasKey(false), _userLimit(0), _limitActive(false) {}

Channel::~Channel() {}

// ========== GETTERS ==========
const std::string& Channel::getName() const { return _name; }

size_t Channel::getMemberCount() const { return _members.size(); }

const std::set<Client*>& Channel::getMembers() const { return _members; }

// ========== MEMBERS MANAGEMENT ==========

void Channel::addMember(Client* client) { _members.insert(client); }

bool Channel::isMember(Client* client) const { return _members.find(client) != _members.end(); }

void Channel::join(Client* client) {
	_members.insert(client);
	std::cout << GREEN << client->getNickname() << " joined the channel " << getName() << RESET << std::endl;
	std::cout << "There are currently " << getMemberCount() << " members in the channel : " << getName() << "." << std::endl;
}

void Channel::part(Client* client) {
	_operators.erase(client);  // Remove from operators first
	_members.erase(client);    // Then remove from members
	_invited.erase(client);    // Clean up invited list as well
	std::cout << RED << client->getNickname() << " left the channel " << getName() << RESET << std::endl;
	std::cout << "There are currently " << getMemberCount() << " members in the channel : " << getName() << "." << std::endl;
}

// ========== OPERATORS ==========

void Channel::addOperator(Client* client) { _operators.insert(client); }

void Channel::removeOperator(Client* client) { _operators.erase(client); }

bool Channel::isOperator(Client* client) const { return _operators.find(client) != _operators.end(); }

const std::set<Client*>& Channel::getOperators() const { return _operators; }

void Channel::invite(Client* client) { _invited.insert(client); }

bool Channel::isInvited(Client* client) const { return _invited.find(client) != _invited.end(); }

void Channel::setCreator(const std::string& nickname) { _creator = nickname; }

bool Channel::isCreator(Client* client) const { return client && client->getNickname() == _creator; }

// ========== COMMUNICATION ==========

void Channel::sendToAll(const std::string& msg) const {
	for (std::set<Client*>::const_iterator it = _members.begin(); it != _members.end(); ++it) {
		send((*it)->getFd(), msg.c_str(), msg.length(), 0);
	}
}

// ========== TOPIC ==========
void Channel::setTopic(const std::string& topic, const std::string& setBy) {
	_topic = topic;
	_topicSetBy = setBy;
	_topicSetTime = time(NULL);
}

void Channel::clearTopic() {
	_topic.clear();
	_topicSetBy.clear();
	_topicSetTime = 0;
}


// ========== MODE +i (INVITE ONLY) ==========
void Channel::setInviteOnly(bool value) { _inviteOnly = value;}

bool Channel::isInviteOnly() const { return _inviteOnly;}

// ========== MODE +t (TOPIC PROTECTED) ==========
void Channel::setTopicProtected(bool value) { _topicProtected = value;}

bool Channel::isTopicProtected() const { return _topicProtected;}

// ========== MODE +k (KEY/PASSWORD) ==========
void Channel::setKey(const std::string& key) {
	_key = key;
	_hasKey = true;
}

void Channel::removeKey() {
	_key.clear();
	_hasKey = false;
}

bool Channel::hasKey() const { return _hasKey; }

const std::string& Channel::getKey() const { return _key; }

// ========== MODE +l (USER LIMIT) ==========
void Channel::setUserLimit(int limit) {
	_userLimit = limit;
	_limitActive = true;
}

void Channel::removeUserLimit() {
	_userLimit = 0;
	_limitActive = false;
}

bool Channel::hasUserLimit() const { return _limitActive; }

int Channel::getUserLimit() const { return _userLimit; }
