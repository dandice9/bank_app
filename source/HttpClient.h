//
// Created by dandy on 04/10/2022.
//

#ifndef BANK_APP_HTTPCLIENT_H
#define BANK_APP_HTTPCLIENT_H

#include <boost/asio/ssl.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <cstdlib>
#include <string>
#include <memory>
#include <unordered_map>
#include <optional>
#include <iomanip>
#include <atomic>
#include "CookieJar.h"

namespace beast = boost::beast; // from <boost/beast.hpp>
namespace http = beast::http;   // from <boost/beast/http.hpp>
namespace net = boost::asio;    // from <boost/asio.hpp>
namespace ssl = net::ssl;       // from <boost/asio/ssl.hpp>
using tcp = net::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

#if BOOST_OS_WINDOWS
#include <wincrypt.h>

void add_windows_root_certs(ssl::context &ctx)
{
    HCERTSTORE hStore = CertOpenSystemStore(0, "ROOT");
    if (hStore == NULL) {
        return;
    }

    X509_STORE *store = X509_STORE_new();
    PCCERT_CONTEXT pContext = NULL;
    while ((pContext = CertEnumCertificatesInStore(hStore, pContext)) != NULL) {
        X509 *x509 = d2i_X509(NULL,
                              (const unsigned char **)&pContext->pbCertEncoded,
                              pContext->cbCertEncoded);
        if(x509 != NULL) {
            X509_STORE_add_cert(store, x509);
            X509_free(x509);
        }
    }

    CertFreeCertificateContext(pContext);
    CertCloseStore(hStore, 0);

    SSL_CTX_set_cert_store(ctx.native_handle(), store);
}
#endif

namespace bank_app {
    const std::string DEFAULT_USER_AGENT = "Mozilla/5.0 (Linux; Android 6.0; Nexus 5 Build/MRA58N) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/46.0.2490.76 Mobile Safari/537.36";

    class HttpClient {
        std::string host, port;
        const int version = 11;
        std::atomic<bool> connectionStarted = false;
        net::io_context& ioc_;
        std::unique_ptr<ssl::context> ctx;
        std::unique_ptr<tcp::resolver> resolver;
        beast::flat_buffer buffer;
        std::shared_ptr<http::response<http::dynamic_body>> resPtr;
        std::unique_ptr<beast::ssl_stream<beast::tcp_stream>> beastStream;
        std::unique_ptr<http::request<http::string_body>> reqPtr;
        CookieJar* cookieJar = nullptr;

    public:
        HttpClient(net::io_context& ioc,
                   std::string host, std::string port,
                   const std::optional<CookieJar*>& cookieJarParam = std::nullopt) : ioc_(ioc), host(host), port(port){

            if(cookieJarParam){
                this->cookieJar = cookieJarParam.value();
            }

	#if BOOST_OS_WINDOWS
            ctx = std::make_unique<ssl::context>(ssl::context::sslv23);
	#else
            ctx = std::make_unique<ssl::context>(ssl::context::tls);
	#endif

            ctx->set_verify_mode(ssl::verify_peer);

#if BOOST_OS_WINDOWS
            add_windows_root_certs(*ctx);
#else
            ctx->set_default_verify_paths();
#endif
        }

        std::shared_ptr<http::response<http::dynamic_body>> response(){
            return resPtr;
        }

        auto headers(){
            auto headers = std::make_shared<std::unordered_map<http::field, std::string>>();

            for(auto& header : resPtr->base()){
                auto key = header.name();
                auto value = std::string(header.value());

                auto hdata = *headers;

                hdata[key] = value;
            }

            return headers;
        }

        auto cookies(){
            auto cookies = std::make_shared<std::vector<std::string>>();

            for(auto& header : resPtr->base()){
                if(header.name() == http::field::set_cookie){
                    auto value = std::string(header.value());
                    cookies->push_back(value);
                }
            }

            return cookies;
        }

        void fillCookie(const std::optional<bank_app::CookieJar*>& cookieJarParam = std::nullopt){
            if(cookieJarParam){
                cookieJar = cookieJarParam.value();
            }

            if(cookieJar){
                auto cookiesPtr = cookies();
                for(auto& cookie : *cookiesPtr){
                    cookieJar->set(cookie);
                }
            }
        }

        HttpClient* openConnection(){
            resolver = std::make_unique<tcp::resolver>(ioc_);
			
            beastStream = std::make_unique<beast::ssl_stream<beast::tcp_stream>>(ioc_, *ctx);

            auto const results = resolver->resolve(host.c_str(), port.c_str());

            beast::get_lowest_layer(*beastStream).connect(results);

            // Set SNI Hostname (many hosts need this to handshake successfully)
            if(! (SSL_set_tlsext_host_name(beastStream->native_handle(), host.c_str())))
            {
                beast::error_code ec{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
                throw beast::system_error{ec};
            }

            beastStream->handshake(ssl::stream_base::client);

            connectionStarted = true;

            return this;
        }

        HttpClient* closeConnection(){
            if(connectionStarted){
                connectionStarted = false;

                beast::error_code ec;
                beastStream->shutdown(ec);
                if(ec == net::error::eof)
                {
                    // Rationale:
                    // http://stackoverflow.com/questions/25587403/boost-asio-ssl-async-shutdown-always-finishes-with-an-error
                    ec = {};
                }
                if(ec)
                    throw beast::system_error{ec};
            }

            return this;
        }

        HttpClient* prepareRequest(std::string target, http::verb method){
            resPtr = std::make_shared<http::response<http::dynamic_body>>();
            reqPtr = std::make_unique<http::request<http::string_body>>();
			
            reqPtr->method(method);
            reqPtr->target(target);
            reqPtr->version(version);
            reqPtr->keep_alive(true);

            reqPtr->set(http::field::host, host);
            reqPtr->set(http::field::cache_control, "max-age=0");
            reqPtr->set("Upgrade-Insecure-Requests", "1");
            reqPtr->set(http::field::user_agent, DEFAULT_USER_AGENT);
            reqPtr->set(http::field::accept, "text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8");
            reqPtr->set(http::field::accept_encoding, "gzip, deflate, sdch, br");
            reqPtr->set(http::field::accept_language, "en-US,en;q=0.8,id;q=0.6,fr;q=0.4");
            reqPtr->set(http::field::content_type, "application/x-www-form-urlencoded");

            return this;
        }

        HttpClient* setPayload(std::string body){
            reqPtr->body() = body;
            reqPtr->prepare_payload();
            reqPtr->set(http::field::content_length, std::to_string(body.size()));

            return this;
        }

        HttpClient* setHeader(http::field fieldType, std::string value){
            reqPtr->set(fieldType, value);

            return this;
        }

        HttpClient* send(){
            http::write(*beastStream, *reqPtr);
            http::read(*beastStream, buffer, *resPtr);

            buffer.clear();
            return this;
        }

        HttpClient* get(std::string target, const std::optional<std::string>& cookie = std::nullopt){
            if(!connectionStarted){
                openConnection();
            }

            prepareRequest(target, http::verb::get);

             if(cookie){
                 setHeader(http::field::cookie, cookie.value());
             }
             else if(cookieJar){
                 setHeader(http::field::cookie, cookieJar->toString());
             }

             return send();
        }
        HttpClient* post(std::string target, const std::optional<std::string>& cookie = std::nullopt, const std::optional<std::string>& body = std::nullopt){
            if(!connectionStarted){
                openConnection();
            }

            prepareRequest(target, http::verb::post);

            if(cookie){
                setHeader(http::field::cookie, cookie.value());
            }
            else if(cookieJar){
                setHeader(http::field::cookie, cookieJar->toString());
            }

            if(body){
                setPayload(body.value());
            }

            return send();
        }

        static std::string UrlEncode(const std::string& value, const std::optional<std::string>& excludedChars = std::nullopt)
        {
            static auto hex_digt = "0123456789ABCDEF";

            std::string result;
            result.reserve(value.size() << 1);

            for (auto ch : value)
            {
                if(
                    (excludedChars && (excludedChars.value().find(ch) != std::string::npos))
                    || (ch >= '0' && ch <= '9')
                    || (ch >= 'A' && ch <= 'Z')
                    || (ch >= 'a' && ch <= 'z')
                )
                    result.push_back(ch);
                else
                {
                    result += std::string("%") +
                              hex_digt[static_cast<unsigned char>(ch) >> 4]
                              +  hex_digt[static_cast<unsigned char>(ch) & 15];
                }
            }

            return result;
        }

        static std::string UrlDecode(const std::string& value)
        {
            std::string result;
            result.reserve(value.size());

            for (std::size_t i = 0; i < value.size(); ++i)
            {
                auto ch = value[i];

                if (ch == '%' && (i + 2) < value.size())
                {
                    auto hex = value.substr(i + 1, 2);
                    auto dec = static_cast<char>(std::strtol(hex.c_str(), nullptr, 16));
                    result.push_back(dec);
                    i += 2;
                }
                else if (ch == '+')
                {
                    result.push_back(' ');
                }
                else
                {
                    result.push_back(ch);
                }
            }

            return result;
        }

    };

}


#endif //BANK_APP_HTTPCLIENT_H
