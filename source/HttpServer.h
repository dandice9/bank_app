
#ifndef BANK_APP_HTTPSERVER_H
#define BANK_APP_HTTPSERVER_H

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <boost/config.hpp>
#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <functional>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

namespace bank_app{
    class HttpSession : public std::enable_shared_from_this<HttpSession>{
        void
        fail(beast::error_code ec, char const* what)
        {
            std::cerr << what << ": " << ec.message() << "\n";
        }

        // This is the C++11 equivalent of a generic lambda.
        // The function object is used to send an HTTP message.
        struct send_lambda
        {
            HttpSession& self_;

            explicit
            send_lambda(HttpSession& self)
                    : self_(self)
            {
            }

            template<bool isRequest, class Body, class Fields>
            void
            operator()(http::message<isRequest, Body, Fields>&& msg) const
            {
                // The lifetime of the message has to extend
                // for the duration of the async operation so
                // we use a shared_ptr to manage it.
                auto sp = std::make_shared<
                        http::message<isRequest, Body, Fields>>(std::move(msg));

                // Store a type-erased version of the shared
                // pointer in the class to keep it alive.
                self_.res_ = sp;

                // Write the response
                http::async_write(
                        self_.stream_,
                        *sp,
                        beast::bind_front_handler(
                                &HttpSession::on_write,
                                self_.shared_from_this(),
                                sp->need_eof()));
            }
        };

        beast::tcp_stream stream_;
        beast::flat_buffer buffer_;
        std::shared_ptr<std::string const> doc_root_;
        http::request<http::string_body> req_;
        std::shared_ptr<void> res_;
        send_lambda lambda_;
        std::unordered_map<std::string, std::function<std::string(std::string)>>& eventList_;

    public:
        // Take ownership of the stream
        HttpSession(
            tcp::socket&& socket,
            std::shared_ptr<std::string const> const& doc_root,
            std::unordered_map<std::string, std::function<std::string(std::string)>>& eventList)
        : stream_(std::move(socket))
        , doc_root_(doc_root)
        , lambda_(*this)
        , eventList_(eventList)
        {
        }

        // Start the asynchronous operation
        void
        run()
        {
            // We need to be executing within a strand to perform async operations
            // on the I/O objects in this session. Although not strictly necessary
            // for single-threaded contexts, this example code is written to be
            // thread-safe by default.
            net::dispatch(stream_.get_executor(),
                          beast::bind_front_handler(
                                  &HttpSession::do_read,
                                  shared_from_this()));
        }

        void
        do_read()
        {
            // Make the request empty before reading,
            // otherwise the operation behavior is undefined.
            req_ = {};

            // Set the timeout.
            stream_.expires_after(std::chrono::seconds(30));

            // Read a request
            http::async_read(stream_, buffer_, req_,
                             beast::bind_front_handler(
                                     &HttpSession::on_read,
                                     shared_from_this()));
        }

        void
        on_read(
                beast::error_code ec,
                std::size_t bytes_transferred)
        {
            boost::ignore_unused(bytes_transferred);

            // This means they closed the connection
            if(ec == http::error::end_of_stream)
                return do_close();

            if(ec)
                return fail(ec, "read");

            // Send the response
            handle_request(*doc_root_, std::move(req_), lambda_);
        }

        void
        on_write(
                bool close,
                beast::error_code ec,
                std::size_t bytes_transferred)
        {
            boost::ignore_unused(bytes_transferred);

            if(ec)
                return fail(ec, "write");

            if(close)
            {
                // This means we should close the connection, usually because
                // the response indicated the "Connection: close" semantic.
                return do_close();
            }

            // We're done with the response so delete it
            res_ = nullptr;

            // Read another request
            do_read();
        }

        void
        do_close()
        {
            // Send a TCP shutdown
            beast::error_code ec;
            stream_.socket().shutdown(tcp::socket::shutdown_send, ec);

            // At this point the connection is closed gracefully
        }

        // This function produces an HTTP response for the given
        // request. The type of the response object depends on the
        // contents of the request, so the interface requires the
        // caller to pass a generic lambda for receiving the response.
        template<
                class Body, class Allocator,
                class Send>
        void
        handle_request(
                beast::string_view doc_root,
                http::request<Body, http::basic_fields<Allocator>>&& req,
                Send&& send)
        {
            const std::string responseType = "text/plain";

            // Returns a bad request response
            auto const bad_request =
                    [&req](beast::string_view why)
                    {
                        http::response<http::string_body> res{http::status::bad_request, req.version()};
                        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
                        res.set(http::field::content_type, "text/html");
                        res.keep_alive(req.keep_alive());
                        res.body() = std::string(why);
                        res.prepare_payload();
                        return res;
                    };

            // Returns a not found response
            auto const not_found =
                    [&req](beast::string_view target)
                    {
                        http::response<http::string_body> res{http::status::not_found, req.version()};
                        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
                        res.set(http::field::content_type, "text/html");
                        res.keep_alive(req.keep_alive());
                        res.body() = "The resource '" + std::string(target) + "' was not found.";
                        res.prepare_payload();
                        return res;
                    };

            // Returns a server error response
            auto const server_error =
                    [&req](beast::string_view what)
                    {
                        http::response<http::string_body> res{http::status::internal_server_error, req.version()};
                        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
                        res.set(http::field::content_type, "text/html");
                        res.keep_alive(req.keep_alive());
                        res.body() = "An error occurred: '" + std::string(what) + "'";
                        res.prepare_payload();
                        return res;
                    };

            // Request path must be absolute and not contain "..".
            if( req.target().empty() ||
                req.target()[0] != '/' ||
                req.target().find("..") != beast::string_view::npos)
                return send(bad_request("Illegal request-target"));

            auto targetPath = static_cast<std::string>(req.target());
            if(!eventList_.contains(targetPath)){
                return send(not_found(targetPath));
            }

            beast::error_code ec;
            http::string_body::value_type body = "hello world";

            // Make sure we can handle the method
            if( req.method() != http::verb::get &&
                req.method() != http::verb::head &&
                req.method() != http::verb::post)
                return send(bad_request("Unknown HTTP-method"));


            std::string requestBody = static_cast<std::string>(req.body());

            body = eventList_[targetPath](requestBody);

            // Handle an unknown error
            if(ec)
                return send(server_error(ec.message()));

            // Cache the size since we need it after the move
            auto const size = body.size();

            // Respond to HEAD request
            if(req.method() == http::verb::head)
            {
                http::response<http::empty_body> res{http::status::ok, req.version()};
                res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
                res.set(http::field::content_type, responseType);
                res.content_length(size);
                res.keep_alive(req.keep_alive());
                return send(std::move(res));
            }

            http::response<http::string_body> res{
                    std::piecewise_construct,
                    std::make_tuple(std::move(body)),
                    std::make_tuple(http::status::ok, req.version())};

            res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
            res.set(http::field::content_type, responseType);
            res.set(http::field::access_control_allow_origin, "*");
            res.content_length(size);
            res.keep_alive(req.keep_alive());

            return send(std::move(res));
        }
    };

    class HttpListener : public std::enable_shared_from_this<HttpListener>{
        net::io_context& ioc_;
        std::unordered_map<std::string, std::function<std::string(std::string)>>& eventList_;
        tcp::acceptor acceptor_;
        std::shared_ptr<std::string const> doc_root_;

        void
        fail(beast::error_code ec, char const* what)
        {
            std::cerr << what << ": " << ec.message() << "\n";
        }

        void
        do_accept()
        {
            // The new connection gets its own strand
            acceptor_.async_accept(
                    net::make_strand(ioc_),
                    beast::bind_front_handler(
                            &HttpListener::on_accept,
                            shared_from_this()));
        }

        void
        on_accept(beast::error_code ec, tcp::socket socket)
        {
            if(ec)
            {
                fail(ec, "accept");
                return; // To avoid infinite loop
            }
            else
            {
                // Create the session and run it
                std::make_shared<HttpSession>(
                        std::move(socket),
                        doc_root_,
                        eventList_)->run();
            }

            // Accept another connection
            do_accept();
        }
    public:
        HttpListener(
                net::io_context& ioc,
                tcp::endpoint endpoint,
                std::shared_ptr<std::string const> const& doc_root,
                std::unordered_map<std::string, std::function<std::string(std::string)>>& eventList)
                : ioc_(ioc)
                , acceptor_(net::make_strand(ioc))
                , doc_root_(doc_root)
                , eventList_(eventList){
            beast::error_code ec;

            // Open the acceptor
            acceptor_.open(endpoint.protocol(), ec);
            if(ec)
            {
                fail(ec, "open");
                return;
            }

            // Allow address reuse
            acceptor_.set_option(net::socket_base::reuse_address(true), ec);
            if(ec)
            {
                fail(ec, "set_option");
                return;
            }

            // Bind to the server address
            acceptor_.bind(endpoint, ec);
            if(ec)
            {
                fail(ec, "bind");
                return;
            }

            // Start listening for connections
            acceptor_.listen(
                    net::socket_base::max_listen_connections, ec);
            if(ec)
            {
                fail(ec, "listen");
                return;
            }
        }

        // Start accepting incoming connections
        void
        run()
        {
            do_accept();
        }
    };

    class HttpServer{
        net::io_context& _ioc;
        std::unordered_map<std::string, std::function<std::string(std::string)>> eventList_;
        unsigned short _port;

    public:
        HttpServer(net::io_context& ioc, unsigned short port) : _ioc(ioc), _port(port) {
        }

        void setEvent(std::string key, std::function<std::string(std::string)> callback){
            eventList_[key] = callback;
        }

        void run(){
            auto threadCount = std::thread::hardware_concurrency();
            auto doc_root = std::make_shared<std::string>(".");
            auto const address = net::ip::make_address("0.0.0.0");

            std::vector<std::thread> threadPool;

            // Create and launch a listening port
            std::make_shared<HttpListener>(
                    _ioc,
                    tcp::endpoint{address, _port },
                    doc_root,
                    eventList_)->run();

            threadPool.reserve(threadCount);

            for (int i = 0; i < threadCount; ++i) {
                threadPool.emplace_back([this]{
                    _ioc.run();
                });
            }

            _ioc.run();
        }
    };
}

#endif //BANK_APP_HTTPSERVER_H