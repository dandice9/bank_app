//
// Created by dandy on 21/10/2022.
//

#ifndef BANK_APP_BCABANK_H
#define BANK_APP_BCABANK_H

#include <iostream>
#include "BaseBank.h"
#include "HtmlParser.h"

namespace bank_app{
    struct BcaTransferForm{
        std::string randomCode;
        std::string sourceAccount;
        std::map<std::string, std::string> destinationList;
    };

    struct BcaTransferData{
        std::string sourceAccount;
        std::string destinationAccount;
        std::string destinationAccountName;
        int amount;
        std::string notes1; // max 18 characters
        std::string notes2;
        std::string appli1;
        std::string appli2;
    };

    class BcaBank : public BaseBank{
        // private properties
        const std::string _bcaEscapeToken = "&=_.+";
        net::io_context& ioc_;
        std::string username_, password_;

        // private methods
        std::string _getUrl(std::string path){
            return "https://" + host + path;
        }
        std::string _parseIp(unsigned char ipVal[]){
            std::vector<std::string> cleanValues;
            for (int i = 0; i < sizeof(IP::asBYTE); ++i) {
                cleanValues.push_back(std::to_string(ipVal[i]));
            }
            return boost::algorithm::join(cleanValues, ".");
        }
        void _generateIp(){
            std::random_device rd;
            auto ip = std::make_unique<IP>();
            ip->asUINT = rd();
            currentIp = _parseIp(ip->asBYTE);
        }
        std::string createBcaPayload(std::vector<std::pair<std::string, std::string>> listData){
            std::string result;
            for(auto it = listData.begin(); it != listData.end(); it++){
                const auto notLastItem = std::distance(it, listData.end()) > 1;
                result += it->first + "=" + it->second + (notLastItem ? "&" : "");
            }
            return bank_app::HttpClient::UrlEncode(result, _bcaEscapeToken);
        }
    public:
        BcaBank(net::io_context& ioc) : ioc_(ioc){
            _generateIp();

            host = "m.klikbca.com";
            cookieJarPtr = std::make_unique<bank_app::CookieJar>();
            httpClientPtr = std::make_unique<bank_app::HttpClient>(ioc, host, port, cookieJarPtr.get());

            httpClientPtr->get("/")->fillCookie();
        }
        ~BcaBank() {
            httpClientPtr->closeConnection();
        }

        void relogin() {
            httpClientPtr->openConnection();
            login(username_, password_);
        }

        bool login(std::string username, std::string password) override {
            auto uuidGen = std::make_unique<bank_app::UUIDGenerator>();
            std::string loginPayload = bank_app::HttpClient::UrlEncode("value(user_id)=" + username + "&" +
                                                                       "value(pswd)=" + password + "&" +
                                                                       "value(Submit)=LOGIN" + "&" +
                                                                       "value(actions)=login" + "&" +
                                                                       "value(user_ip)=" + currentIp + "&" +
                                                                       "user_ip=" + currentIp + "&" +
                                                                       "value(mobile)=true" + "&" +
                                                                       "value(browser_info)=" + bank_app::DEFAULT_USER_AGENT + "&" +
                                                                       "mobile=true&" +
                "as_fid=" + uuidGen->get() + std::string(uuidGen->get().data(), 8), _bcaEscapeToken);

            auto refererUrl = std::string(getBCAPath(BANK_PATHS::LOGIN_PAGE));
            auto loginUrl = std::string(getBCAPath(BANK_PATHS::LOGIN));

            httpClientPtr->get(refererUrl)->fillCookie();

            auto loginPostPtr = httpClientPtr->prepareRequest(loginUrl, http::verb::post)
                    ->setHeader(http::field::cookie, cookieJarPtr->toString())
                    ->setHeader(http::field::referer, refererUrl)
                    ->setPayload(loginPayload)
                    ->send();

            auto loginPostCookiesPtr = loginPostPtr->cookies();
            auto loginStatus = loginPostCookiesPtr->size() == 0;

            if(loginStatus){
                loginTimestamp = std::chrono::system_clock::now();

                username_ = username;
                password_ = password;
            }

            return loginStatus;
        }

        bool isLoginTimeout(){
            auto currentTimestamp = std::chrono::system_clock::now();
            auto timeDiff = std::chrono::duration_cast<std::chrono::minutes>(currentTimestamp - loginTimestamp);

            return timeDiff.count() >= 5;
        }

        bool logout() override {
            auto refererUrl = std::string(getBCAPath(BANK_PATHS::LOGIN_PAGE));
            auto logoutUrl = std::string(getBCAPath(BANK_PATHS::LOGOUT));

            try{
                httpClientPtr->prepareRequest(logoutUrl, http::verb::get)
                        ->setHeader(http::field::cookie, cookieJarPtr->toString())
                        ->setHeader(http::field::referer, refererUrl)
                        ->send();

                return true;
            }
            catch(beast::system_error& err){
                std::cerr << err.what() << std::endl;

                return false;
            }
        }

        std::shared_ptr<std::vector<std::string>> getStatements(std::string start, std::string end) override {
            if (isLoginTimeout())
                relogin();

            time_t startTimeParam = std::stoll(start) / 1000, endTimeParam = std::stoll(end) / 1000;
            auto startt = *std::localtime(&startTimeParam);
            auto endt = *std::localtime(&endTimeParam);

            auto stmtPayload = createBcaPayload({
                 {"value(r1)", "1"},
                 {"value(D1)", "0"},
                 {"value(startDt)", std::to_string(startt.tm_mday)},
                 {"value(startMt)", std::to_string(startt.tm_mon+1)},
                 {"value(startYr)", std::to_string(startt.tm_year+1900)},
                 {"value(endDt)", std::to_string(endt.tm_mday)},
                 {"value(endMt)", std::to_string(endt.tm_mon+1)},
                 {"value(endYr)", std::to_string(endt.tm_year+1900)}
            });

            auto refererUrl = std::string(getBCAPath(BANK_PATHS::STATEMENT));
            auto statementUrl = std::string(getBCAPath(BANK_PATHS::STATEMENT_VIEW));

            auto response = httpClientPtr->prepareRequest(statementUrl, http::verb::post)
                    ->setHeader(http::field::cookie, cookieJarPtr->toString())
                    ->setHeader(http::field::referer, refererUrl)
                    ->setPayload(stmtPayload)
                    ->send()->response();

            auto responseHtml = boost::beast::buffers_to_string(response->body().data());

            lxb_char_t trNeedle[] = "table[width=\"100%\"][class=\"blue\"]:not([border]) tr[bgcolor]";
            auto htmlParser = std::make_unique<HtmlParser>(lxbFromString(responseHtml));

            auto resultList = htmlParser->css(trNeedle)->toArrayStdString();
            return resultList;
        }

        std::shared_ptr<BcaTransferForm> getTransferForm(){
            if (isLoginTimeout())
                relogin();

            auto response = httpClientPtr->post(getBCAPath(BANK_PATHS::TRANSFER_FORM))->response();
            auto responseHtml = boost::beast::buffers_to_string(response->body().data());

            auto htmlParser = std::make_unique<HtmlParser>(lxbFromString(responseHtml));

            std::string attr_name = "value";
            lxb_char_t needle[] = "select[name=\"value(acc_from)\"]>option[value=\"0\"],input[name=\"value(rndNum)\"],select[name=\"value(acc_to3)\"]>option";

            auto foundNodeList = htmlParser->css(needle)->toArray();

            auto formResult = std::make_shared<BcaTransferForm>();
            for(auto& node : foundNodeList){
                auto attrValue = bank_app::lxbGetNodeAttr(node, bank_app::lxbFromString(attr_name));

                // two digit random code
                if(attrValue.length() == 2){
                    formResult->randomCode = attrValue;
                }
                else if(attrValue.length() > 2){
                    auto destLine = bank_app::lxbGetInnerHtml(node);
                    std::vector<std::string> resultTexts;
                    boost::split(resultTexts, destLine, boost::is_any_of("-"));
                    boost::algorithm::trim(resultTexts[1]);
                    formResult->destinationList[attrValue] = resultTexts[1];
                }
                else if(attrValue.length() == 1){
                    formResult->sourceAccount = bank_app::lxbGetInnerHtml(node);
                    boost::algorithm::trim(formResult->sourceAccount);
                }
            }

            return formResult;
        }

        bool transferFund(BcaTransferData& transferPayload){
            try{
                auto refererUrl = getBCAPath(BANK_PATHS::TRANSFER_FORM);
                auto transferUrl = getBCAPath(BANK_PATHS::TRANSFER_FUND);

                auto firstPayload = createBcaPayload({
                     {"value(actions)", "validate"},
                     {"value(StatusSend)", "notfirst"},
                     {"value(acc_from)", "0"},
                     {"value(acc_to2)", "0"},
                     {"value(acc_to_option)", "V3"},
                     {"value(acc_to3)", transferPayload.destinationAccount},
                     {"value(currency)", "Rp."},
                     {"value(amount)", std::to_string(transferPayload.amount)},
                     {"value(remarkLine1)", transferPayload.notes1},
                     {"value(remarkLine2)", transferPayload.notes2},
                     {"value(keyBCA)", transferPayload.appli2}
                });

                auto secondPayload = createBcaPayload({
                    {"value(actions)", "transfer"},
                    {"value(acc_from)", transferPayload.sourceAccount},
                    {"value(acc_to)", transferPayload.destinationAccount},
                    {"value(ref_no)", ""},
                    {"value(acctToNm)", transferPayload.destinationAccountName},
                    {"value(currency)", "IDR"},
                    {"value(amount)", std::to_string(transferPayload.amount)},
                    {"value(remarkLine1)", transferPayload.notes1},
                    {"value(remarkLine2)", transferPayload.notes2},
                    {"value(curToAcc)", "IDR"},
                    {"value(curFromAcc)", "IDR"},
                    {"value(acc_type_from)", "1"},
                    {"value(trans_type)", "0"},
                    {"value(post_txfer_dt)", ""},
                    {"value(recur_param)", ""},
                    {"value(recur_expire_dt)", ""},
                    {"value(StatusSend)", "notfirst"},
                    {"value(is_llg)", "0"},
                    {"value(respondAppli1)", transferPayload.appli1}
                });

                auto response1 = httpClientPtr->prepareRequest(transferUrl, http::verb::post)
                        ->setHeader(http::field::cookie, cookieJarPtr->toString())
                        ->setHeader(http::field::referer, refererUrl)
                        ->setPayload(firstPayload)
                        ->send()->response();

                auto response2 = httpClientPtr->prepareRequest(transferUrl, http::verb::post)
                        ->setHeader(http::field::cookie, cookieJarPtr->toString())
                        ->setHeader(http::field::referer, transferUrl)
                        ->setPayload(secondPayload)
                        ->send()->response();

                return true;
            }
            catch(...){
                std::cerr << "BCA Transfer Failed!" << std::endl;
                return false;
            }
        }

        std::string getBalance() override {
            if (isLoginTimeout())
                relogin();

            auto refererUrl = std::string(getBCAPath(BANK_PATHS::MENU_PATH));
            auto balanceInquiryUrl = std::string(getBCAPath(BANK_PATHS::BALANCE_INQUIRY));
            lxb_char_t cssNeedle[] = "td[align='right'] b";

            auto response = httpClientPtr->prepareRequest(balanceInquiryUrl, http::verb::post)
                    ->setHeader(http::field::cookie, cookieJarPtr->toString())
                    ->setHeader(http::field::referer, refererUrl)
                    ->send()->response();

            auto pageStr = boost::beast::buffers_to_string(response->body().data());
            auto pageParser = std::make_unique<HtmlParser>(lxb_string(reinterpret_cast<lxb_char_t*>(pageStr.data()), pageStr.size()));

            auto htmlResult = pageParser->css(cssNeedle)->toArrayString();
            auto lineStr = htmlResult->at(0);

            return std::string(reinterpret_cast<char*>(lineStr.data()), lineStr.size());
        }

    };
}

#endif //BANK_APP_BCABANK_H
