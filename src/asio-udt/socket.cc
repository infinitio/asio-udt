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
        rendezvous::rendezvous(bool value):
          basic_option{value, option::rendezvous}
        {}

        reuseaddr::reuseaddr(bool value):
          basic_option{value, option::reuseaddr}
        {}

        non_blocking::non_blocking(bool value):
          basic_option{value, option::non_blocking}
        {}

        socket::socket(io_service& io_service)
          : socket(io_service, UDT::socket(AF_INET, SOCK_STREAM, 0),
                   endpoint_type())
        {
          this->set_option(non_blocking{true});
        }

        socket::~socket()
        {
          if (this->_udt_socket != -1)
            this->close();
        }

        void
        socket::set_option(basic_option const& opt,
                           boost::system::error_code &code)
        {
          int err = UDT::ERROR;
          switch (opt.opt)
          {
            case basic_option::rendezvous:
              err = UDT::setsockopt(this->_udt_socket, 0,
                                    UDT_RENDEZVOUS,
                                    new bool(opt.value), sizeof(bool));
              break;
            case basic_option::non_blocking:
              // UDT_SNDSYN and UDT_RCVSYN means "make it blocking". So we
              // negate the value of non-blocking to have the right result.
              err = UDT::setsockopt(this->_udt_socket, 0, UDT_SNDSYN,
                                    new bool(!opt.value), sizeof(bool));
              if (err == UDT::ERROR)
                break;
              err = UDT::setsockopt(this->_udt_socket, 0, UDT_RCVSYN,
                                    new bool(!opt.value), sizeof(bool));
              if (err == UDT::ERROR)
                break;
              break;
            case basic_option::reuseaddr:
              err = UDT::setsockopt(this->_udt_socket, 0, UDT_REUSEADDR,
                                    new bool(opt.value), sizeof(bool));
              break;
            default:
              break;
          }
          if (err == UDT::ERROR)
          {
            auto error = UDT::getlasterror();
            code.assign(error.getErrorCode(), udt_category::get());
          }
        }

        void
        socket::set_option(basic_option const& opt)
        {
          boost::system::error_code error;
          this->set_option(opt, error);
          if (error)
          {
            throw_udt();
          }
        }

        socket::socket(io_service& io_service, int fd,
                       endpoint_type const& endpoint)
          : _service(io_service)
          , _udt_service(use_service<service>(_service))
          , _udt_socket(fd)
          , _ready_read(false)
          , _ready_write(false)
          , _peer(endpoint)
          , _connecting(false)
        {
          if (this->_udt_socket == -1)
            throw_errno();
          this->set_option(non_blocking{true});
        }

        void
        socket::_handle_connect(std::function<void (system::error_code const&)>
                                const& handler)
        {
          this->_connecting = false;
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
          _peer = peer;
          // std::cerr << "IP from asio: "
          //           << inet_ntoa(addr.sin_addr) << std::endl;
          if (UDT::connect(this->_udt_socket, peer.data(),
                           peer.size()) == UDT::ERROR)
          {
            std::stringstream ss;

            ss << "connect(" << this->_udt_socket << ", " << peer << ")";
            throw_udt(ss.str());
          }
          this->_connecting = true;
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
            this->_service.post(std::bind(handler, error, read));
            return;
          }
          if (read > 0)
          {
            this->_service.post(std::bind(handler, system::error_code(), read));
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
            this->_service.post(std::bind(handler, error, sent));
            return;
          }
          if (sent > 0)
          {
            this->_service.post(std::bind(handler, system::error_code(), sent));
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
        socket::bind(endpoint_type const& endpoint)
        {
          if (UDT::bind(this->_udt_socket,
                        endpoint.data(), endpoint.size()) == UDT::ERROR)
              throw_udt();
        }

        void
        socket::bind(unsigned short port)
        {
          endpoint_type local_endpoint{boost::asio::ip::udp::v4(), port};

          this->bind(local_endpoint);
        }

        void
        socket::_bind_fd(int fd)
        {
          if (UDT::bind2(this->_udt_socket, fd) == UDT::ERROR)
            throw_udt();
        }

        void
        socket::shutdown(shutdown_type, system::error_code&)
        {
          // FIXME: nothing ?
        }

        void
        socket::close()
        {
          if (UDT::close(this->_udt_socket) == UDT::ERROR)
            throw_udt();
          else
            this->_udt_socket = -1;
        }

        void
        socket::cancel()
        {
          this->_udt_service.cancel_read(this);
          this->_udt_service.cancel_write(this);
          if (this->_connecting)
            {
              this->_connecting = false;
              // FIXME: close socket to cancel out UDT connection
              close();
            }
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
