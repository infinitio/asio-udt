#include <boost/lexical_cast.hpp>

#include <asio-udt/error-category.hh>
#include <asio-udt/service.hh>
#include <asio-udt/socket.hh>

namespace boost
{
  namespace asio
  {
    namespace ip
    {
      namespace udt
      {
        socket::socket(io_service& io_service)
          : socket(io_service, UDT::socket(AF_INET, SOCK_STREAM, 0))
        {}

        socket::~socket()
        {
          if (this->_udt_socket != -1)
            UDT::close(this->_udt_socket);
        }

        socket::socket(io_service& io_service, int fd)
          : _service(io_service)
          , _udt_service(use_service<service>(_service))
          , _udt_socket(fd)
          , _ready_read(false)
          , _ready_write(false)
        {
          if (this->_udt_socket == -1)
            throw_errno();
          // UDT::setsockopt(this->_udt_socket, 0,
          //                 UDT_RENDEZVOUS, new bool(true), sizeof(bool));
          UDT::setsockopt(this->_udt_socket, 0, UDT_SNDSYN,
                          new bool(false), sizeof(bool));
          UDT::setsockopt(this->_udt_socket, 0, UDT_RCVSYN,
                          new bool(false), sizeof(bool));
        }

        void
        socket::_handle_connect(std::function<void (system::error_code const&)>
                                const& handler)
        {
          system::error_code err;
          if (!UDT::connected(this->_udt_socket))
            // FIXME: actual error code is lost by UDT
            err = system::error_code(udt_category::ENOSERVER,
                                     udt_category::get());
          this->get_io_service().post(boost::bind(handler, err));
        }

        void
        socket::async_connect(endpoint_type const& peer,
                              std::function<void (system::error_code const&)>
                              const& handler)
        {
          // FIXME: handle ipv6
          _peer = peer;
          sockaddr_in addr;
          addr.sin_family = AF_INET;
          addr.sin_port = htons(peer.port());
          auto ip_bin = peer.address().to_v4().to_bytes();
          // std::cerr << "IP from asio: "
          //           << inet_ntoa((in_addr&)ip_bin) << std::endl;
          addr.sin_addr.s_addr = (unsigned int&)ip_bin;
          //addr.sin_addr.s_addr = inet_ntoa();
          if (UDT::connect(this->_udt_socket,
                           (sockaddr*)&addr,
                           sizeof(sockaddr_in)) == UDT::ERROR)
            throw_udt();
          system::error_code canceled(system::errc::operation_canceled,
                                      system::system_category());
          this->_udt_service.register_write
            (this,
             std::bind(&socket::_handle_connect, this, handler),
             std::bind(handler, canceled));
        }

        io_service&
        socket::get_io_service()
        {
          return _service;
        }

        void
        socket::async_read_some(
          mutable_buffer buffer,
          std::function<void (system::error_code const&,
                              std::size_t)> const& handler)
        {
          auto buf = buffer_cast<char*>(buffer);
          int size = buffer_size(buffer);
          int read = UDT::recv(_udt_socket, buf, size, 0);
          if (read == -1
              && UDT::getlasterror().getErrorCode() != udt_category::EASYNCRCV)
          {
            system::error_code error;
            if (UDT::getlasterror().getErrorCode() == udt_category::ECONNLOST)
              error = boost::asio::error::eof;
            else
              error = system::error_code(UDT::getlasterror().getErrorCode(),
                                         udt_category::get());
            this->_service.post(bind(handler, error, read));
            return;
          }
          if (read > 0)
          {
            this->_service.post(bind(handler, system::error_code(), read));
          }
          else
          {
            auto action =
              std::bind(&socket::async_read_some, this, buffer, handler);
            system::error_code canceled(system::errc::operation_canceled,
                                        system::system_category());
            auto cancel = std::bind(handler, canceled, 0);
            this->_udt_service.register_read(this, action, cancel);
          }
        }

        void
        socket::async_write_some(const_buffer buffer,
                                 std::function<void (system::error_code const&,
                                                     std::size_t)> const& handler)
        {
          auto buf = buffer_cast<char const*>(buffer);
          int size = buffer_size(buffer);
          int sent = UDT::send(_udt_socket, buf, size, 0);
          if (sent == -1
              && UDT::getlasterror().getErrorCode() != udt_category::EASYNCSND)
          {
            system::error_code error(UDT::getlasterror().getErrorCode(),
                                     udt_category::get());
            this->_service.post(bind(handler, error, sent));
            return;
          }
          if (sent > 0)
          {
            this->_service.post(bind(handler, system::error_code(), sent));
          }
          else
          {
            system::error_code canceled(system::errc::operation_canceled,
                                        system::system_category());
            this->_udt_service.register_write
              (this,
               std::bind(&socket::async_write_some, this, buffer, handler),
               std::bind(handler, canceled, 0));
          }
        }

        void
        socket::_bind(int port)
        {
          // Build the listening endpoint.
          addrinfo* local;
          {
            addrinfo hints;
            memset(&hints, 0, sizeof(struct addrinfo));
            hints.ai_flags = AI_PASSIVE;
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;
            if (getaddrinfo(NULL, lexical_cast<std::string>(port).c_str(),
                            &hints, &local) != 0)
              throw_errno();
          }
          // Listen.
          if (UDT::bind(this->_udt_socket,
                        local->ai_addr, local->ai_addrlen) == UDT::ERROR)
            throw_udt();
          static const int queue_size = 1024;
          if (UDT::listen(this->_udt_socket, queue_size) == UDT::ERROR)
            throw_udt();
          freeaddrinfo(local);
        }

        void
        socket::shutdown(shutdown_type, system::error_code&)
        {
          // FIXME: nothing ?
        }

        void
        socket::close()
        {
          std::cerr << "CLOSE " << _udt_socket << std::endl;
          if (UDT::close(_udt_socket) == UDT::ERROR)
            throw_udt();
          std::cerr << "/CLOSE " << _udt_socket << std::endl;
        }

        void
        socket::cancel()
        {
          std::cerr << "CANCEL" << std::endl;
          this->_udt_service.cancel_read(this);
          this->_udt_service.cancel_write(this);
        }

        socket::endpoint_type
        socket::local_endpoint() const
        {
          return _local;
        }

        socket::endpoint_type
        socket::remote_endpoint() const
        {
          return _peer;
        }
      }
    }
  }
}
