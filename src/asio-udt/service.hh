#ifndef ASIO_UDT_SERVICE_HH
# define ASIO_UDT_SERVICE_HH

# include <functional>
# include <unordered_map>

# include <boost/asio.hpp>
# include <boost/thread.hpp>

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

            static io_service::id id;

            virtual
            void
            shutdown_service();
            void
            register_read(socket* sock, std::function<void ()> const& action);
            void
            register_write(socket* sock, std::function<void ()> const& action);

          private:
            int _epoll;

            boost::thread _thread;
            void
            _run();

            class work
            {
              public:
                work(io_service& service, std::function<void ()> const& action);
                std::function<void ()> action;
              private:
                io_service::work _work;
            };

            typedef std::unordered_map<int, work> Map;
            Map _read_map;
            Map _write_map;
            boost::mutex _lock;
        };
      }
    }
  }
}

#endif
