#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include <vector>

std::vector<std::string> ft_split(const std::string& s, char delimiter);
std::string trim(const std::string& str);
bool isValidChannelName(const std::string& name);
std::vector<std::string> parseParameters(const std::string& input);

#endif
