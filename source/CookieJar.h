#pragma once
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <cstdlib>
#include <cstring>
#include <boost/algorithm/string.hpp>

namespace bank_app {
	struct CookieData {
		std::string key;
		std::string value;
		std::string Expires;
		std::string MaxAge;
		std::string domain;
		std::string path;
		std::string sameSite = "Lax";
		bool secure = false;
		bool httpOnly = false;
	};

	class CookieJar
	{
		std::map<std::string, CookieData> cookies;
	public:
        auto set(std::string cookieParam)
        {
            char delim[] = ";", eqDelim[] = "=";
            std::vector<std::string> result;

            // first index
            auto data = std::strtok(const_cast<char*>(cookieParam.c_str()), delim);

            while (data) {
                auto cookiePart = std::string(data, std::strlen(data));

                boost::trim(cookiePart);

                result.push_back(cookiePart);

                data = std::strtok(NULL, delim);
            }

            CookieData cookie;

            for (auto it = result.begin(); it != result.end(); it++) {
                if (it == result.begin()) {
                    auto key = std::strtok(const_cast<char*>(it->c_str()), eqDelim);
                    auto value = std::strtok(NULL, eqDelim);

                    cookie.key = std::string(key, strlen(key));
                    cookie.value = std::string(value, strlen(value));
                }
                else {
                    auto found = it->find('=');
                    auto keyRaw = std::strtok(const_cast<char*>(it->c_str()), eqDelim);
                    auto valueRaw = found != std::string::npos ? std::strtok(NULL, eqDelim) : NULL;
                    auto key = std::string(keyRaw);
                    boost::to_lower(key);

                    if (valueRaw) {
                        auto value = std::string(valueRaw, std::strlen(valueRaw));
                        boost::to_lower(value);

                        if (key == "domain") {
                            cookie.domain = value;
                        }
                        else if (key == "path") {
                            cookie.path = value;
                        }
                        else if (key == "expires") {
                            cookie.Expires = value;
                        }
                        else if (key == "max-age") {
                            cookie.MaxAge = value;
                        }
                    }
                    else {
                        if (key == "secure") {
                            cookie.secure = true;
                        }
                        else if(key == "httponly") {
                            cookie.httpOnly = true;
                        }
                    }
                }
            }

            cookies[cookie.key] = cookie;

            return this;
        }

        auto all()
        {
            auto result = std::make_shared<std::vector<CookieData>>();

            if(!cookies.empty()){
                for (auto& cookie : cookies) {
                    result->push_back(cookie.second);
                }
            }

            return result;
        }

        auto toString(){
            std::string result = "";

            if(!cookies.empty()){
                for (auto& cookie : cookies) {
                    result += (result.size() > 0 ? "; " : "") + cookie.first + "=" + cookie.second.value;
                }
            }


            return result;
        }
	};
}

