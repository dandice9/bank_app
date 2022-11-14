//
// Created by dandy on 09/10/2022.
//

#ifndef BANK_APP_UUID_GEN_H
#define BANK_APP_UUID_GEN_H

#include <boost/uuid/uuid.hpp>            // uuid class
#include <boost/uuid/uuid_generators.hpp> // generators
#include <boost/uuid/uuid_io.hpp>         // streaming operators etc.

namespace bank_app{
    class UUIDGenerator{
        boost::uuids::random_generator generator;
    public:
        std::string get(){
            auto uuidStr = boost::uuids::to_string(generator());
            uuidStr.erase(std::remove(uuidStr.begin(), uuidStr.end(), '-'), uuidStr.end());
            return uuidStr;
        }
    };
}

#endif //BANK_APP_UUID_GEN_H
