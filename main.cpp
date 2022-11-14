#include <thread>
#include <string>
#include <memory>
#include "source/BcaBank.h"
#include "source/HttpServer.h"
#include "source/Utility.h"
#include "source/UUIDGenerator.h"

int main() {

    auto uuidGen = std::make_unique<bank_app::UUIDGenerator>();
    auto clientIoc = std::make_unique<boost::asio::io_context>(1);
    auto serverIoc = std::make_unique<boost::asio::io_context>(std::thread::hardware_concurrency());
    const unsigned short port = 80;
    const std::string defaultSeparator = ";;";

    auto bcaInst = std::make_unique<bank_app::BcaBank>(*clientIoc);
    auto serv = std::make_unique<bank_app::HttpServer>(*serverIoc, port);


    serv->setEvent("/ping", [&](std::string payload) -> std::string {
        return "ok";
    });

    serv->setEvent("/login", [&](std::string payload) -> std::string {
        auto cred = bank_app::Utility::split(payload, defaultSeparator);
        std::string loginResult = "0";

        if (cred->size() == 2) {
            auto& credobj = *cred;
            loginResult = bcaInst->login(credobj[0], credobj[1]) ? "1" : "0";
        }

        return loginResult;
    });

    serv->setEvent("/balance", [&](std::string payload) -> std::string {
        return bcaInst->getBalance();
    });


    serv->setEvent("/statement", [&](std::string payload) -> std::string {
        auto dateRanges = bank_app::Utility::split(payload, defaultSeparator);

        if (dateRanges->size() == 2) {
            auto& dr = *dateRanges;
            auto statements = bcaInst->getStatements(dr[0], dr[1]);

            return bank_app::Utility::join(*statements, defaultSeparator);
        }

        return "0";
    });

    serv->setEvent("/logout", [&](std::string payload) -> std::string {
        auto logoutResult = bcaInst->logout();

        return logoutResult ? "1" : "0";
    });
	
	std::cout << "Server Running at port: " << port << std::endl;


    serv->run();

    return 0;
}