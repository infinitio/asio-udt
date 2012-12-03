#ifndef ASIO_UDT_ACCEPTOR_HH
# define ASIO_UDT_ACCEPTOR_HH

# include <boost/asio.hpp>

# include <asio-udt/fwd.hh>
# include <asio-udt/socket.hh>

namespace boost
{
  namespace asio
  {
    namespace ip
    {
      namespace udt
      {
        class acceptor
        {
          public:
            acceptor(io_service& io_service, int port);
            void
            async_accept(std::function<void (boost::system::error_code const&,
                                             boost::asio::ip::udt::socket*)>
                         const& handler);
            void
            cancel();
            int
            port() const;

          private:
            io_service& _service;
            service& _udt_service;
            int _port;
            socket _socket;
            std::function<void ()> _read_action;
        };
      }
    }
  }
}

#endif
