#ifndef UTIL_H
#define UTIL_H

#include <string>

std::string ReplaceString(std::string subject, const std::string& search,
                          const std::string& replace);

void ReplaceStringInPlace(std::string& subject, const std::string& search,
                          const std::string& replace);

std::string file2sha2(const std::string filename);

#endif
