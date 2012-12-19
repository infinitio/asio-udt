#include <asio-udt/acceptor.hh>
#include <asio-udt/error-category.hh>
#include <asio-udt/service.hh>

namespace boost
{
  namespace asio
  {
    namespace ip
    {
      namespace udt
      {
        acceptor::acceptor(io_service& io_service, int port)
          : _service(io_service)
          , _udt_service(use_service<service>(_service))
          , _port(port)
          , _socket(io_service)
        {
          this->_socket._bind(port);
        }

        acceptor::acceptor(io_service& io_service, int port, int fd)
          : _service(io_service)
          , _udt_service(use_service<service>(_service))
          , _port(port)
          , _socket(io_service)
        {
          this->_socket._bind_fd(fd);
        }

        void
        acceptor::async_accept(
          std::function<void (boost::system::error_code const&,
                              boost::asio::ip::udt::socket*)> const& handler)
        {
          sockaddr peer;
          int len;
          auto udt_socket = UDT::accept(this->_socket._udt_socket,
                                        &peer, &len);
          if (udt_socket == UDT::ERROR)
          {
            if (UDT::getlasterror().getErrorCode() ==
                udt_category::EASYNCRCV)
            {
              std::function<void ()> action =
                std::bind(&acceptor::async_accept, this, handler);
              socket* nullsock = nullptr;
              system::error_code canceled(system::errc::operation_canceled,
                                          system::system_category());
              std::function<void ()> cancel =
                std::bind(handler,
                          canceled,
                          nullsock);
              _udt_service.register_read(&this->_socket, action, cancel);
            }
            else
              throw_udt();
          }
          else
          {
            socket::endpoint_type endpoint;
            if (peer.sa_family == AF_INET)
              // IP v4
              {
                namespace ip = boost::asio::ip;

                sockaddr_in& peer_v4 = (sockaddr_in&)peer;
                ip::address_v4::bytes_type ip_bin;
                std::copy(&reinterpret_cast<unsigned char&>(peer_v4.sin_addr.s_addr),
                          &reinterpret_cast<unsigned char&>(peer_v4.sin_addr.s_addr) + 4,
                          ip_bin.begin());
                boost::asio::ip::address_v4 v4(ip_bin);
                auto port = ntohs(peer_v4.sin_port);
                endpoint = socket::endpoint_type(v4, port);
              }
            else if (peer.sa_family == AF_INET6)
              // IP v6
              {
                namespace ip = boost::asio::ip;

                sockaddr_in6& peer_v6 = (sockaddr_in6&)peer;
                ip::address_v6::bytes_type ip_bin;
                std::copy(&reinterpret_cast<unsigned char&>(peer_v6.sin6_addr.s6_addr),
                          &reinterpret_cast<unsigned char&>(peer_v6.sin6_addr.s6_addr) + 16,
                          ip_bin.begin());
                boost::asio::ip::address_v6 v6(ip_bin);
                auto port = ntohs(peer_v6.sin6_port);
                endpoint = socket::endpoint_type(v6, port);
              }
            socket* res(new socket(_service, udt_socket, endpoint));
            this->_service.post
              (std::bind(handler, system::error_code(), res));
          }
        }

        void
        acceptor::cancel()
        {
          _udt_service.cancel_read(&_socket);
        }

        int
        acceptor::port() const
        {
          return _port;
        }
      }
    }
  }
}
