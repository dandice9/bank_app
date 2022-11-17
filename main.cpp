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

    std::unordered_map<std::string, std::shared_ptr<bank_app::BcaBank>> bcaInsts;
    auto serv = std::make_unique<bank_app::HttpServer>(*serverIoc, port);

    serv->setEvent("/ping", [&](std::string payload) -> std::string {
        return "ok";
    });

    serv->setEvent("/login", [&](std::string payload) -> std::string {
        auto cred = bank_app::Utility::split(payload, defaultSeparator);
        std::string loginResult = "-1";

        if (cred->size() == 2) {
            auto& credobj = *cred;
            auto bcaInst = std::make_shared< bank_app::BcaBank>(*clientIoc);
            loginResult = bcaInst->login(credobj[0], credobj[1]) ? "1" : loginResult;

            if (loginResult == "1") {
                auto uuid = uuidGen->get();
                bcaInsts.insert({uuid, bcaInst });

                loginResult = uuid;
            }
        }

        return loginResult;
    });

    serv->setEvent("/balance", [&](std::string payload) -> std::string {

        if (bcaInsts.contains(payload)) {
            auto bcaInst = bcaInsts[payload];
            return bcaInst->getBalance();
        }

        return "-1";
    });


    serv->setEvent("/statement", [&](std::string payload) -> std::string {
        auto dateRanges = bank_app::Utility::split(payload, defaultSeparator);

        if (dateRanges->size() == 3) {
            auto& dr = *dateRanges;

            if (bcaInsts.contains(dr[0])) {
                auto bcaInst = bcaInsts[dr[0]];
                auto statements = bcaInst->getStatements(dr[1], dr[2]);

                return bank_app::Utility::join(*statements, defaultSeparator);
            }
        }

        return "-1";
    });

    serv->setEvent("/transfer_form", [&](std::string payload) -> std::string {
        std::string defaultRes = "-1";

        if (bcaInsts.contains(payload)) {
            auto bcaInst = bcaInsts[payload];
            auto tf = bcaInst->getTransferForm();
            std::vector<std::string> valueList;

            for (const auto& dest : tf->destinationList) {
                valueList.push_back(dest.first + ":" + dest.second);
            }

            return tf->randomCode + defaultSeparator + tf->sourceAccount + defaultSeparator + bank_app::Utility::join(valueList, defaultSeparator);
        }

        return defaultRes;
    });

    serv->setEvent("/transfer_action", [&](std::string payload) -> std::string {


        return "-1";
    });

    serv->setEvent("/logout", [&](std::string payload) -> std::string {
        std::string defaultRes = "-1";

        if (bcaInsts.contains(payload)) {
            auto bcaInst = bcaInsts[payload];
            auto logoutResult = bcaInst->logout();

            bcaInsts.erase(payload);

            return logoutResult ? "1" : defaultRes;
        }

        return defaultRes;
    });
	
	std::cout << "Server Running at port: " << port << std::endl;


    serv->run();

    return 0;
}