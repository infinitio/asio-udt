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

        void
        acceptor::async_accept(
          std::function<void (boost::system::error_code const&,
                              boost::asio::ip::udt::socket*)> const& handler)
        {
          auto udt_socket = UDT::accept(this->_socket._udt_socket,
                                        nullptr, nullptr);
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
            socket* res(new socket(_service, udt_socket));
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
