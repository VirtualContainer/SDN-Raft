#pragma once
#include <asio/ip/tcp.hpp>

class mock_ssl_context {
public:
    enum type {
        sslv23 = 0,
    };

    mock_ssl_context(type t) {}
};

class mock_ssl_socket {
public:
    using lowest_layer_type = asio::ip::tcp::socket;
    using executor_type = lowest_layer_type::executor_type;

    mock_ssl_socket(asio::ip::tcp::socket& tcp_socket,
                    mock_ssl_context& context)
        : socket_(tcp_socket)
        , context_(context)
        {}

    lowest_layer_type& lowest_layer() { return socket_; }

    template<typename A, typename B>
    void async_read_some(A a, B b) {}

    template<typename A, typename B>
    void async_write_some(A a, B b) {}

    asio::ip::tcp::socket& socket_;
    mock_ssl_context& context_;
};

