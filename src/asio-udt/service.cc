#include <asio-udt/error-category.hh>
#include <asio-udt/service.hh>
#include <asio-udt/socket.hh>

#include <elle/log.hh>

static boost::mutex _debug_mutex;

#define ASIO_UDT_DEBUG(Fmt)
//   do
//   {
//     boost::lock_guard<boost::mutex> lock(_debug_mutex);
//     std::cerr << Fmt << std::endl;
//   }
//   while (false)

ELLE_LOG_COMPONENT("boost.asio.ip.udt.service");

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
              ELLE_TRACE("%s: wait for socket event", *this);
              {
                boost::unique_lock<boost::mutex> lock(_lock);
                for (auto r: _read_map)
                  ELLE_DUMP("%s: monitor %s for read", *this, r.first);
                for (auto w: _write_map)
                  ELLE_DUMP("%s: monitor %s for write", *this, w.first);
              }
              if (UDT::epoll_wait(this->_epoll, &readfds, &writefds, -1) < 0)
              {
                if (_stop)
                {
                  ELLE_TRACE("%s: stop service", *this);
                  return;
                }
                if (UDT::getlasterror().getErrorCode() ==
                    udt_category::EINVPARAM)
                {
                  ELLE_DEBUG("%s: no socket to wait upon, waiting", *this);
                  boost::unique_lock<boost::mutex> lock(_lock);
                  while (_read_map.empty() && _write_map.empty())
                  {
                    _barrier.wait(lock);
                    if (_stop)
                    {
                      ELLE_TRACE("%s: stop service", *this);
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
                  ELLE_TRACE("%s: stop service", *this);
                  return;
                }
                else
                  break;
            }
            ELLE_DEBUG("%s: got %s read events and %s write events",
                       *this, readfds.size(), writefds.size());
            boost::unique_lock<boost::mutex> lock(_lock);
            for (auto read: readfds)
            {
              auto it = _read_map.find(read);
              if (it != _read_map.end())
              {
                ELLE_DEBUG("%s: execute read action for %s", *this, read);
                // static int const flags = UDT_EPOLL_IN;
                this->_wait_read.erase(read);
                this->_wait_refresh(read);
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
                ELLE_DEBUG("%s: execute read action for %s", *this, write);
                // static int const flags = UDT_EPOLL_OUT;
                this->_wait_write.erase(write);
                this->_wait_refresh(write);
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
        service::_wait_refresh(UDTSOCKET sock)
        {
          auto read = this->_wait_read.find(sock);
          auto write = this->_wait_write.find(sock);
          int flags = UDT_EPOLL_ERR;
          bool wait = false;
          if (read != this->_wait_read.end())
          {
            flags |= UDT_EPOLL_IN;
            wait = true;
          }
          if (write != this->_wait_write.end())
          {
            flags |= UDT_EPOLL_OUT;
            wait = true;
          }
          UDT::epoll_remove_usock(_epoll, sock);
          if (wait)
          {
            ELLE_DEBUG("%s: reregister %s", *this, sock);
            UDT::epoll_add_usock(_epoll, sock, &flags);
          }
          else
            ELLE_DEBUG("%s: unregister %s", *this, sock);
        }

        void
        service::register_read(socket* sock,
                               std::function<void ()> const& action,
                               std::function<void ()> const& cancel)
        {
          boost::unique_lock<boost::mutex> lock(_lock);
          ELLE_TRACE_SCOPE("%s: register read action on %s", *this, *sock);
          this->_wait_read.insert(sock->_udt_socket);
          this->_wait_refresh(sock->_udt_socket);
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
          ELLE_TRACE_SCOPE("%s: cancel read action on %s", *this, *sock);
          this->_wait_read.erase(sock->_udt_socket);
          this->_wait_refresh(sock->_udt_socket);
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
          ELLE_TRACE_SCOPE("%s: register write action on %s", *this, *sock);
          this->_wait_write.insert(sock->_udt_socket);
          this->_wait_refresh(sock->_udt_socket);
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
          ELLE_TRACE_SCOPE("%s: cancel write action on %s", *this, *sock);
          this->_wait_write.erase(sock->_udt_socket);
          this->_wait_refresh(sock->_udt_socket);
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
