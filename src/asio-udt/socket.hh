#ifndef ASIO_UDT_SOCKET_HH
# define ASIO_UDT_SOCKET_HH

# include <boost/asio.hpp>
# include <boost/noncopyable.hpp>

# include <udt/udt.h>

# include <asio-udt/fwd.hh>

namespace boost
{
  namespace asio
  {
    namespace ip
    {
      namespace udt
      {
        class socket: public boost::noncopyable
        {
          public:
            explicit
            socket(io_service& io_service);
            ~socket();

          private:
            socket(io_service& io_service, int fd);

          public:
            void
            async_connect(std::string const& address, int port,
                          std::function<void (boost::system::error_code const&)>
                          const& handler);
            io_service&
            get_io_service();
            void
            async_read_some(mutable_buffer buffer,
                            std::function<void (system::error_code const&,
                                                std::size_t)> const& handler);
            void
            async_write_some(const_buffer buffer,
                             std::function<void (system::error_code const&,
                                                 std::size_t)> const& handler);
            void
            close();

            //private:
            friend class acceptor;
            friend class service;
            void _bind(int port);
            io_service& _service;
            service& _udt_service;
            UDTSOCKET _udt_socket;
            bool _ready_read;
            bool _ready_write;
        };
      }
    }
  }
}

#endif
