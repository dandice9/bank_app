//
// Created by dandy on 21/10/2022.
//

#ifndef BANK_APP_BASEBANK_H
#define BANK_APP_BASEBANK_H
#include <string>
#include "HttpClient.h"
#include "CookieJar.h"
#include "UUIDGenerator.h"
#include <chrono>
#include <random>
#include <boost/algorithm/string.hpp>

namespace bank_app{
    union IP
    {
        unsigned char asBYTE[4];
        unsigned int asUINT;
    };

    enum BANK_PATHS{
        LOGIN_PAGE = 0,
        LOGIN = 1,
        LOGOUT,
        BALANCE_INQUIRY,
        MENU_PATH,
        STATEMENT,
        STATEMENT_VIEW,
        TRANSFER_FORM,
        TRANSFER_FUND
    };

    constexpr auto getBCAPath(BANK_PATHS path){
        switch(path){
            case LOGIN_PAGE:
                return "/login.jsp";
            case LOGIN:
                return "/authentication.do";
            case LOGOUT:
                return "/authentication.do?value(actions)=logout";
            case BALANCE_INQUIRY:
                return "/balanceinquiry.do";
            case MENU_PATH:
                return "/accountstmt.do?value(actions)=menu";
            case STATEMENT:
                return "/accountstmt.do?value(actions)=acct_stmt";
            case STATEMENT_VIEW:
                return "/accountstmt.do?value(actions)=acctstmtview";
            case TRANSFER_FORM:
                return "/fundtransfer.do?value(actions)=formentry";
            case TRANSFER_FUND:
                return "/fundtransfer.do";
        }

        throw std::invalid_argument("bca path not found");
    }

    class BaseBank{
    protected:
        IP addr;
        std::unordered_map<std::string, std::string> urlPaths;
        std::string host, port = "443", currentIp;
        std::unique_ptr<bank_app::CookieJar> cookieJarPtr;
        std::unique_ptr<bank_app::HttpClient> httpClientPtr;
        std::chrono::time_point<std::chrono::system_clock> lastActionTimestamp;
        std::chrono::time_point<std::chrono::system_clock> loginTimestamp;
    public:
        virtual bool login(std::string username, std::string password) = 0;
        virtual std::string getBalance() = 0;
        virtual std::shared_ptr<std::vector<std::string>> getStatements(std::string start, std::string end) = 0;
        virtual bool logout() = 0;
    };
}

#endif //BANK_APP_BASEBANK_H
