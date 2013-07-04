#ifndef ASIO_UDT_SERVICE_HH
# define ASIO_UDT_SERVICE_HH

# include <functional>
# include <unordered_map>

# include <boost/asio.hpp>
# include <boost/thread.hpp>

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
        class service: public io_service::service
        {
        public:
          service(io_service& io_service);
          ~service();

            static io_service::id id;

            virtual
            void
            shutdown_service();
            void
            register_read(socket* sock,
                          std::function<void ()> const& action,
                          std::function<void ()> const& cancel);
            void
            cancel_read(socket* sock);
            void
            register_write(socket* sock,
                           std::function<void ()> const& action,
                           std::function<void ()> const& cancel);
            void
            cancel_write(socket* sock);
        private:
          std::set<UDTSOCKET> _wait_read;
          std::set<UDTSOCKET> _wait_write;
          void
          _wait_refresh(UDTSOCKET sock);

          private:
            int _epoll;

            std::unique_ptr<boost::thread> _thread;
            void
            _run();

            class work
            {
              public:
                work(io_service& service,
                     std::function<void ()> const& action,
                     std::function<void ()> const& cancel);
                std::function<void ()> action;
                std::function<void ()> cancel;
              private:
                io_service::work _work;
            };

            typedef std::unordered_map<int, work> Map;
            Map _read_map;
            Map _write_map;
            boost::mutex _lock;
            boost::condition_variable _barrier;
            bool _stop;
        };
      }
    }
  }
}

#endif
