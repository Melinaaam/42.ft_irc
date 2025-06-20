#include "Utils.hpp"
#include <sstream>

std::vector<std::string> ft_split(const std::string& s, char delimiter) {
	std::vector<std::string> result;
	std::istringstream iss(s);
	std::string token;

	while (std::getline(iss, token, delimiter)) {
		if (!token.empty())
			result.push_back(token);
	}
	return result;
}

std::string trim(const std::string& str) {
	size_t first = str.find_first_not_of(" \r\n\t");
	size_t last = str.find_last_not_of(" \r\n\t");
	if (first == std::string::npos || last == std::string::npos)
		return "";
	return str.substr(first, last - first + 1);
}

bool isValidChannelName(const std::string& name) {
	return !name.empty() && name[0] == '#';
}

std::vector<std::string> parseParameters(const std::string& input) {
	std::vector<std::string> params;
	std::istringstream iss(input);
	std::string param;

	while (iss >> param) {
		params.push_back(param);
	}
	return params;
}

