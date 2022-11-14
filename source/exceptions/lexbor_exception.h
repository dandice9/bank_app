//
// Created by dandy on 02/10/2022.
//

#ifndef BANK_API_LEXBOR_EXCEPTION_H
#define BANK_API_LEXBOR_EXCEPTION_H

#include <exception>
#include <string>
#include <lexbor/core/types.h>

namespace bank_app{
    class lexbor_exception : public std::exception{
        std::string reason;
        std::string source;
        lxb_status_t status_code;

    public:
        lexbor_exception(std::string reason, std::string source, lxb_status_t status_code) : reason(reason), source(source), status_code(status_code){

        }

        ~lexbor_exception() /*_GLIBCXX_TXN_SAFE_DYN _GLIBCXX_NOTHROW*/ /*override*/ {

        }

        const char *what() /*const _GLIBCXX_TXN_SAFE_DYN _GLIBCXX_NOTHROW*/ /*override*/ {
            std::string message = "what: " + reason + ", where: " + source + ", code: " + std::to_string(status_code);
            return message.c_str();
        }

    };
}

#endif //BANK_API_LEXBOR_EXCEPTION_H
