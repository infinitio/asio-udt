#include <udt/udt.h>

#include <asio-udt/error-category.hh>
#include <asio-udt/service.hh>
#include <asio-udt/socket.hh>


static boost::mutex _debug_mutex;

#define ASIO_UDT_DEBUG(Fmt)
//   do
//   {
//     boost::lock_guard<boost::mutex> lock(_debug_mutex);
//     std::cerr << Fmt << std::endl;
//   }
//   while (false)


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
          , _thread(nullptr)
          , _stop(false)
        {
          // Create the thread after all members have been initialized.
          this->_thread.reset(
            new boost::thread(std::bind(&service::_run, this)));
        }

        service::~service()
        {
          shutdown_service();
        }

        io_service::id id;

        void
        service::shutdown_service()
        {
          {
            boost::unique_lock<boost::mutex> lock(_lock);
            if (_stop)
              return;
            UDT::epoll_release(_epoll);
            _barrier.notify_one();
            _stop = true;
          }
          this->_thread->join();
        }

        void
        service::_run()
        {
          while (true)
          {
            std::set<UDTSOCKET> readfds;
            std::set<UDTSOCKET> writefds;
            while (true)
              {
                ASIO_UDT_DEBUG("epoll_wait");
                {
                  boost::unique_lock<boost::mutex> lock(_lock);
                  for (auto r: _read_map)
                    ASIO_UDT_DEBUG ("  read: " << r.first);
                  for (auto w: _write_map)
                    ASIO_UDT_DEBUG ("  write: " << w.first);
                }
                if (UDT::epoll_wait(this->_epoll, &readfds, &writefds, -1) < 0)
                  {
                    if (_stop)
                      {
                        ASIO_UDT_DEBUG("stopping UDT Asio service");
                        return;
                      }
                    if (UDT::getlasterror().getErrorCode() ==
                        udt_category::EINVPARAM)
                      {
                        ASIO_UDT_DEBUG("no socket to wait upon, waiting");
                        boost::unique_lock<boost::mutex> lock(_lock);
                        while (_read_map.empty() && _write_map.empty())
                          {
                            _barrier.wait(lock);
                            if (_stop)
                              {
                                ASIO_UDT_DEBUG("stopping UDT Asio service");
                                return;
                              }
                          }
                      }
                    else
                      throw_udt();
                  }
                else
                  if (_stop)
                    {
                      ASIO_UDT_DEBUG("stopping UDT Asio service");
                      return;
                    }
                  else
                    break;
              }
            ASIO_UDT_DEBUG("epoll woke: " << readfds.size() << " " << writefds.size());
            boost::unique_lock<boost::mutex> lock(_lock);
            for (auto read: readfds)
            {
              auto it = _read_map.find(read);
              if (it != _read_map.end())
              {
                ASIO_UDT_DEBUG("READ " << read);
                // static int const flags = UDT_EPOLL_IN;
                UDT::epoll_remove_usock(_epoll, read);
                this->get_io_service().post(it->second.action);
                _read_map.erase(it);
              }
              // else
              //   ASIO_UDT_DEBUG("LOST READ " << read);
            }
            for (auto write: writefds)
            {
              auto it = _write_map.find(write);
              if (it != _write_map.end())
              {
                ASIO_UDT_DEBUG("WRITE " << write);
                // static int const flags = UDT_EPOLL_OUT;
                UDT::epoll_remove_usock(_epoll, write);
                this->get_io_service().post(it->second.action);
                _write_map.erase(it);
              }
              // else
              //   ASIO_UDT_DEBUG("LOST WRITE " << write);
            }
          }
        }

        service::work::work(io_service& service,
                            std::function<void ()> const& action,
                            std::function<void ()> const& cancel)
          : action(action)
          , cancel(cancel)
          , _work(service)
        {}

        void
        service::register_read(socket* sock,
                               std::function<void ()> const& action,
                               std::function<void ()> const& cancel)
        {
          boost::unique_lock<boost::mutex> lock(_lock);
          ASIO_UDT_DEBUG("WAIT read " << sock->_udt_socket);
          static int const flags = UDT_EPOLL_IN | UDT_EPOLL_ERR;
          UDT::epoll_add_usock(_epoll, sock->_udt_socket, &flags);
          this->_read_map.insert(std::make_pair
                                 (sock->_udt_socket,
                                  work(this->get_io_service(),
                                       action, cancel)));
          _barrier.notify_one();
        }

        void
        service::cancel_read(socket* sock)
        {
          boost::unique_lock<boost::mutex> lock(_lock);
          ASIO_UDT_DEBUG("CANCEL read " << sock->_udt_socket);
          UDT::epoll_remove_usock(_epoll, sock->_udt_socket);
          auto work = this->_read_map.find(sock->_udt_socket);
          if (work != this->_read_map.end())
            {
              work->second.cancel();
              this->_read_map.erase(work);
            }
          _barrier.notify_one();
        }

        void
        service::register_write(socket* sock,
                               std::function<void ()> const& action,
                               std::function<void ()> const& cancel)
        {
          boost::unique_lock<boost::mutex> lock(_lock);
          ASIO_UDT_DEBUG("WAIT write " << sock->_udt_socket);
          static int const flags = UDT_EPOLL_OUT | UDT_EPOLL_ERR;
          UDT::epoll_add_usock(_epoll, sock->_udt_socket, &flags);
          this->_write_map.insert(std::make_pair
                                 (sock->_udt_socket,
                                  work(this->get_io_service(),
                                       action, cancel)));
          _barrier.notify_one();
        }

        void
        service::cancel_write(socket* sock)
        {
          boost::unique_lock<boost::mutex> lock(_lock);
          ASIO_UDT_DEBUG("CANCEL write " << sock->_udt_socket);
          UDT::epoll_remove_usock(_epoll, sock->_udt_socket);
          auto work = this->_write_map.find(sock->_udt_socket);
          if (work != this->_write_map.end())
            {
              work->second.cancel();
              this->_write_map.erase(work);
            }
        }
      }
    }
  }
}
