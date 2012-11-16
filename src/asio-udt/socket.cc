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
        socket::async_connect(std::string const& address, int port,
                              std::function<void (system::error_code const&)>
                              const& handler)
        {
          addrinfo hints;
          memset(&hints, 0, sizeof(struct addrinfo));
          hints.ai_flags = AI_PASSIVE;
          hints.ai_family = AF_INET;
          hints.ai_socktype = SOCK_DGRAM;
          addrinfo* peer;
          if (getaddrinfo(address.c_str(),
                          lexical_cast<std::string>(port).c_str(),
                          &hints, &peer))
            throw_errno();
          if (UDT::connect(this->_udt_socket,
                           peer->ai_addr,
                           peer->ai_addrlen) == UDT::ERROR)
            throw_udt();
          freeaddrinfo(peer);
          this->_udt_service.register_write
            (this, std::bind(&io_service::post<std::function<void ()>>,
                             std::ref(this->get_io_service()),
                             boost::bind(handler, system::error_code())));
        }

        io_service&
        socket::get_io_service()
        {
          return _service;
        }

        void
        socket::async_read_some(mutable_buffer buffer,
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
            this->_udt_service.register_read(this, action);
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
            this->_udt_service.register_write
              (this, std::bind(&socket::async_write_some,
                               this, buffer, handler));
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
        socket::close()
        {
          std::cerr << "CLOSE " << _udt_socket << std::endl;
          if (UDT::close(_udt_socket) == UDT::ERROR)
            throw_udt();
          std::cerr << "/CLOSE " << _udt_socket << std::endl;
        }
      }
    }
  }
}
