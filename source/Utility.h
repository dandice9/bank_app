#ifndef BANK_APP_UTILITY_H
#define BANK_APP_UTILITY_H

#include <vector>
#include <string>
#include <sstream>
#include <memory>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string_regex.hpp>
#include "UUIDGenerator.h"

namespace bank_app {
	using namespace std;
	class Utility {
	public:
		static shared_ptr<vector<string>> split(string text, string separator) {
			auto result = make_shared<vector<string>>();

			boost::split_regex(*result, text, boost::regex(separator));

			return result;
		}

		static string join(vector<string>& textList, string separator) {
			stringstream ss;
			for (const auto& text : textList) {
				ss << text;

				if (&text != &textList.back()) {
					ss << separator;
				}
			}

			return ss.str();
		}
	};
}
#endif