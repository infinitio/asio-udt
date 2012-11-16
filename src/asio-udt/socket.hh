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
            typedef udp::endpoint endpoint_type;

          public:
            explicit
            socket(io_service& io_service);
            ~socket();

          private:
            socket(io_service& io_service, int fd,
                   endpoint_type const& endpoint);

          public:
            void
            async_connect(endpoint_type const& endpoint,
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
            enum shutdown_type
            {
              shutdown_receive,
              shutdown_send,
              shutdown_both,
            };
            void
            shutdown(shutdown_type, system::error_code&);
            void
            cancel();
            endpoint_type
            local_endpoint() const;
            endpoint_type
            remote_endpoint() const;

          private:
            void
            _handle_connect(std::function<void (system::error_code const&)>
                            const& handler);

            friend class acceptor;
            friend class service;
            void _bind(int port);
            io_service& _service;
            service& _udt_service;
            UDTSOCKET _udt_socket;
            bool _ready_read;
            bool _ready_write;
            endpoint_type _local;
            endpoint_type _peer;
            bool _connecting;
        };
      }
    }
  }
}

#endif
