#include "crpropa/Version.h"
#include "kiss/logger.h"

#include <cstring>
#include <sstream>
#include <vector>
#include <iostream>

#define GIT_SHA1 "bc17418444ad4b8fb6ece32a5249065bd46b7859"
#define GIT_REFSPEC "refs/heads/master"
#define GIT_DESC "3.2.1-22-gbc174184"
const char g_GIT_SHA1[] = GIT_SHA1;
const char g_GIT_REFSPEC[] = GIT_REFSPEC;
const char g_GIT_DESC[] = GIT_DESC;

std::vector<std::string> split_version(std::string input) {
	std::istringstream ss(input);
	std::string part;
	std::vector<std::string> parts;

	while(std::getline(ss, part, '-'))
		parts.push_back(part);

	return parts;
}
		
void declare_version(const std::string input_version) {
	std::string git_version(GIT_DESC);
	std::vector<std::string> git_parts, input_parts;
	
	git_parts = split_version(git_version);
	input_parts = split_version(input_version);

	for(size_t i = 0; i < input_parts.size(); ++i) {
		if (git_parts[i].compare(input_parts[i]) != 0) {
			KISS_LOG_WARNING <<
		"Version mismatch! To clear this warning \n"
		"review the python code for potential incompatibilities and update \n"
		"its version declaration or install the declared version of CRPropa. \n"
		"- CRPropa version: " << git_version << "\n"
		"- Python code version: " << input_version << "\n"
		"Use git diff to inspect the differences: \n"
		"  git diff " << input_version << " " << git_version << "\n";
			break;
		}
	}
}
