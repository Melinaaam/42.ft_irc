#ifndef CHANNEL_HPP
# define CHANNEL_HPP

# include <string>
# include <set>
# include <sys/socket.h>
# include <ctime>  // Pour time_t

class Client;

class Channel {
private:
	std::string _name;
	std::string _topic;
	std::string _topicSetBy;     // Who define the topic
	time_t _topicSetTime;        // When the topic was set
	std::string _creator;        // Creator of the channel

	std::set<Client*> _members;
	std::set<Client*> _operators;// +o
	std::set<Client*> _invited;

	bool _inviteOnly;				// +i
    bool _topicProtected;			// +t
    std::string _key;				// +k
    bool _hasKey;
    int _userLimit;					// +l
    bool _limitActive;
    std::set<std::string> _banList;  // +b ban masks

public:
	Channel(const std::string& name);
	~Channel();

	const std::string& getName() const;
	const std::set<Client*>& getMembers() const;
	size_t getMemberCount() const;

	void addMember(Client* client);
	void join(Client* client);
	void part(Client* client);
	bool isMember(Client* client) const;
	void sendToAll(const std::string& msg) const;

	void addOperator(Client* client);
	void removeOperator(Client* client);
	bool isOperator(Client* client) const;
	const std::set<Client*>& getOperators() const;

	void invite(Client* client);
	bool isInvited(Client* client) const;

	// Gestion du créateur
	void setCreator(const std::string& nickname);
	bool isCreator(Client* client) const;

	// Modes +i
	void setInviteOnly(bool value);
	bool isInviteOnly() const;

	// Modes +t
	void setTopicProtected(bool value);
	bool isTopicProtected() const;

	// Modes +k
	void setKey(const std::string& key);
	void removeKey();
	bool hasKey() const;
	const std::string& getKey() const;

	// Modes +l
	void setUserLimit(int limit);
	void removeUserLimit();
	bool hasUserLimit() const;
	int getUserLimit() const;


	// Gestion du topic
	const std::string& getTopic() const { return _topic; }
	const std::string& getTopicSetBy() const { return _topicSetBy; }
	void setTopic(const std::string& topic, const std::string& setBy);
	time_t getTopicSetTime() const { return _topicSetTime; }
	bool hasTopic() const { return !_topic.empty(); }
	void clearTopic();

};

#endif

