#include <boost/lexical_cast.hpp>

#include <asio-udt/acceptor.hh>
#include <asio-udt/service.hh>
#include <asio-udt/socket.hh>

static const size_t buffer_size = 128;


boost::asio::ip::udt::socket* s;

class EchoServer
{
  public:
    EchoServer(boost::asio::io_service& io_service, int port)
      : _io_service(io_service)
      , _acceptor(io_service, port)
    {
      _acceptor.async_accept(handle_accept);
    }


  private:
    static
    void
    handle_accept(boost::system::error_code const& error,
                  boost::asio::ip::udt::socket* socket)
    {
      if (error)
      {
        std::cerr << "error: " << error.message() << std::endl;
        std::abort();
      }
      s = socket;
      char* buffer = new char[buffer_size];
      socket->async_read_some(boost::asio::buffer(buffer, buffer_size),
                              std::bind(&handle_read,
                                        std::ref(*socket),
                                        buffer,
                                        std::placeholders::_1,
                                        std::placeholders::_2));
    }

    static
    void
    handle_read(boost::asio::ip::udt::socket& socket,
                char* buffer,
                boost::system::error_code const& error,
                std::size_t bytes_transferred)
    {
      if (error == boost::asio::error::eof)
      {
        delete &socket;
        delete[] buffer;
        return;
      }
      else if (error)
      {
        std::cerr << "error server read: " << error.message() << std::endl;
        std::abort();
      }
      socket.async_write_some(boost::asio::buffer(buffer, bytes_transferred),
                              std::bind(&handle_write,
                                        std::ref(socket),
                                        buffer,
                                        std::placeholders::_1,
                                        std::placeholders::_2));
    }

    static
    void
    handle_write(boost::asio::ip::udt::socket& socket,
                 char* buffer,
                 boost::system::error_code const& error,
                 std::size_t bytes_transferred)
    {
      if (error)
      {
        std::cerr << "error: " << error.message() << std::endl;
        std::abort();
      }
      socket.async_read_some(boost::asio::buffer(buffer, bytes_transferred),
                             std::bind(&handle_read, std::ref(socket),
                                       buffer,
                                       std::placeholders::_1,
                                       std::placeholders::_2));
    }

    boost::asio::io_service& _io_service;
    boost::asio::ip::udt::acceptor _acceptor;
};

class EchoClient
{
  public:
    EchoClient(boost::asio::io_service& io_service, int port)
      : _socket(io_service)
    {
      unsigned long ip = (127 << 24) + 1;
      _socket.async_connect(boost::asio::ip::udp::endpoint(
                              boost::asio::ip::address_v4(ip), port),
                            std::bind(&EchoClient::handle_connected,
                                      this,
                                      std::placeholders::_1));
    }


    void
    send(std::string const& msg)
    {
      _pending += msg;
      _socket.async_write_some(boost::asio::buffer(msg.c_str(), msg.size()),
                               std::bind(&EchoClient::handle_sent,
                                         this,
                                         std::placeholders::_1,
                                         std::placeholders::_2));
    }

  private:

    void
    handle_sent(boost::system::error_code const& error,
                std::size_t bytes_transferred)
    {
      if (error)
        std::cerr << error.message() << std::endl;
      char* buffer = reinterpret_cast<char*>(malloc(buffer_size));
      _socket.async_read_some(boost::asio::buffer(buffer, buffer_size),
                              std::bind(&EchoClient::handle_receive,
                                        this,
                                        buffer,
                                        std::placeholders::_1,
                                        std::placeholders::_2));
    }

    void
    handle_receive(char* buffer,
                   boost::system::error_code const& error,
                   std::size_t bytes_transferred)
    {
      if (error)
        std::cerr << error.message() << std::endl;
      std::string received(buffer, bytes_transferred);
      assert(_pending.find(received) == 0);
      _pending = _pending.substr(received.size(), std::string::npos);
      if (!_pending.empty())
        _socket.async_read_some(boost::asio::buffer(buffer, buffer_size),
                                std::bind(&EchoClient::handle_receive,
                                          this,
                                          buffer,
                                          std::placeholders::_1,
                                          std::placeholders::_2));
      else
      {
        free(buffer);
        _socket.close();
      }
    }

    void
    handle_connected(boost::system::error_code const& error)
    {
      if (error)
        {
          std::cerr << "connection error: " << error.message() << std::endl;
          std::abort();
        }
      send("The bind method is usually to assign a UDT socket a local address, "
           "including IP address and port number. If INADDR_ANY is used, "
           "a proper IP address will be used once the UDT connection is set up."
           " If 0 is used for the port, a randomly available port number will "
           "be used. The method getsockname can be used to retrieve this "
           "port number."
           "The second form of bind allows UDT to bind directly on an existing "
           "UDP socket. This is usefule for firewall traversing in certain "
           "situations: 1) a UDP socket is created and its address is learned "
           "from a name server, there is no need to close the UDP socket and "
           "open a UDT socket on the same address again; 2) for certain "
           "firewall, especially some on local system, the port mapping "
           "maybe changed or the \"hole\" may be closed when a UDP socket is "
           "closed and reopened, thus it is necessary to use the UDP socket "
           "directly in UDT."
           "Use the second form of bind with caution, as it violates certain "
           "programming rules regarding code robustness. Once the UDP socket "
           "descriptor is passed to UDT, it MUST NOT be touched again. DO NOT "
           "use this unless you clearly understand how the related systems "
           "work."
           "The bind call is necessary in all cases except for a socket to "
           "listen. If bind is not called, UDT will automatically bind a "
           "socket to a randomly available address when a connection is set up."
           "By default, UDT allows to reuse existing UDP port for new UDT "
           "sockets, unless UDT_REUSEADDR is set to false. When UDT_REUSEADDR "
           "is false, UDT will create an exclusive UDP port for this UDT "
           "socket. UDT_REUSEADDR must be called before bind. To reuse an "
           "existing UDT/UDP port, the new UDT socket must explicitly bind to "
           "the port. If the port is already used by a UDT socket with "
           "UDT_REUSEADDR as false, the new bind will return error. If 0 is "
           "passed as the port number, bind always creates a new port, no "
           "matter what value the UDT_REUSEADDR sets.");
    }

    boost::asio::ip::udt::socket _socket;
    std::string _pending;
};

// void fuck(boost::system::error_code const&)
// {
//   std::cerr << "FUCK THIS " << s->_udt_socket << std::endl;
//   char buf[512];
//   s->async_read_some(boost::asio::buffer(buf, sizeof(buf)), ignore);
// }

int main(int, char** argv)
{
  try
  {
    boost::asio::io_service io_service;
    boost::asio::add_service(io_service,
                             new boost::asio::ip::udt::service(io_service));
    EchoServer server(io_service, 4242);
    EchoClient client(io_service, 4242);

    // boost::asio::deadline_timer t(io_service, boost::posix_time::seconds(5));
    // t.async_wait(fuck);

    io_service.run();
  }
  catch (std::exception const& e)
  {
    std::cerr << argv[0] << ": error: " << e.what() << std::endl;
    return 1;
  }
}
