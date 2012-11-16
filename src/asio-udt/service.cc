#include <udt/udt.h>

#include <asio-udt/service.hh>
#include <asio-udt/socket.hh>


static boost::mutex _debug_mutex;

#define DEBUG(Fmt)                                              \
  do                                                            \
  {                                                             \
    boost::lock_guard<boost::mutex> lock(_debug_mutex);         \
    std::cerr << Fmt << std::endl;                              \
  }                                                             \
  while (false)                                                 \


namespace boost
{
  namespace asio
  {
    namespace ip
    {
      namespace udt
      {
        io_service::id service::id;

        service::service(io_service& io_service)
          : io_service::service(io_service)
          , _epoll(UDT::epoll_create())
          , _thread(std::bind(&service::_run, this))
        {}

        io_service::id id;

        void
        service::shutdown_service()
        {
          UDT::epoll_release(_epoll);
        }

        void
        service::_run()
        {
          while (true)
          {
            std::set<UDTSOCKET> readfds;
            std::set<UDTSOCKET> writefds;
            DEBUG("epoll_wait");
            for (auto r: _read_map)
              DEBUG ("  read: " << r.first);
            for (auto w: _write_map)
              DEBUG ("  write: " << w.first);
            UDT::epoll_wait(this->_epoll, &readfds, &writefds, 1000);
            // std::cerr << "epoll" << std::endl;
            boost::lock_guard<boost::mutex> lock(_lock);
            for (auto read: readfds)
            {
              auto it = _read_map.find(read);
              if (it != _read_map.end())
              {
                DEBUG("READ " << read);
                // static int const flags = UDT_EPOLL_IN;
                UDT::epoll_remove_usock(_epoll, read);
                this->get_io_service().post(it->second.action);
                _read_map.erase(it);
              }
            }
            for (auto write: writefds)
            {
              auto it = _write_map.find(write);
              if (it != _write_map.end())
              {
                DEBUG("WRITE " << write);
                // static int const flags = UDT_EPOLL_OUT;
                UDT::epoll_remove_usock(_epoll, write);
                this->get_io_service().post(it->second.action);
                _write_map.erase(it);
              }
            }
          }
        }

        service::work::work(io_service& service,
                      std::function<void ()> const& action)
          : action(action)
          , _work(service)
        {}

        void
        service::register_read(socket* sock,
                               std::function<void ()> const& action)
        {
          boost::lock_guard<boost::mutex> lock(_lock);
          DEBUG("WAIT read " << sock->_udt_socket);
          static int const flags = UDT_EPOLL_IN | UDT_EPOLL_ERR;
          UDT::epoll_add_usock(_epoll, sock->_udt_socket, &flags);
          this->_read_map.insert(std::make_pair
                                 (sock->_udt_socket,
                                  work(this->get_io_service(), action)));
        }

        void
        service::register_write(socket* sock,
                               std::function<void ()> const& action)
        {
          boost::lock_guard<boost::mutex> lock(_lock);
          DEBUG("WAIT write " << sock->_udt_socket);
          static int const flags = UDT_EPOLL_OUT | UDT_EPOLL_ERR;
          UDT::epoll_add_usock(_epoll, sock->_udt_socket, &flags);
          this->_write_map.insert(std::make_pair
                                 (sock->_udt_socket,
                                  work(this->get_io_service(), action)));
        }
      }
    }
  }
}
