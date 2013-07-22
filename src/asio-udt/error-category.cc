#include <string>

#include <udt/udt.h>

#include <boost/system/error_code.hpp>
#include <boost/system/system_error.hpp>

#include <asio-udt/error-category.hh>

namespace boost
{
  namespace asio
  {
    namespace ip
    {
      namespace udt
      {
        void
        throw_errno()
        {
          system::error_code ec(errno, udt_category::get());
          throw system::system_error(ec);
        }

        void
        throw_udt(std::string const& what)
        {
          system::error_code ec(UDT::getlasterror().getErrorCode(),
                                udt_category::get());
          throw system::system_error(ec, what);
        }

        udt_category&
        udt_category::get()
        {
          static udt_category res;
          return res;
        }

        char const*
        udt_category::name() const noexcept
        {
          return "UDT";
        }

        std::string
        udt_category::message(int code) const
        {
          switch (code)
          {
            case SUCCESS:
              return "success operation";
            case ECONNSETUP:
              return "connection setup failure";
            case ENOSERVER:
              return "server does not exist";
            case ECONNREJ:
              return "connection request was rejected by server";
            case ESOCKFAIL:
              return "could not create/configure UDP socket";
            case ESECFAIL:
              return "connection request was aborted due to security reasons";
            case ECONNFAIL:
              return "connection failure";
            case ECONNLOST:
              return "connection was broken";
            case ENOCONN:
              return "connection does not exist";
            case ERESOURCE:
              return "system resource failure";
            case ETHREAD:
              return "could not create new thread";
            case ENOBUF:
              return "no memory space";
            case EFILE:
              return "file access error";
            case EINVRDOFF:
              return "invalid read offset";
            case ERDPERM:
              return "no read permission";
            case EINVWROFF:
              return "invalid write offset";
            case EWRPERM:
              return "no write permission";
            case EINVOP:
              return "operation not supported";
            case EBOUNDSOCK:
              return "cannot execute the operation on a bound socket";
            case ECONNSOCK:
              return "cannot execute the operation on a connected socket";
            case EINVPARAM:
              return "bad parameters";
            case EINVSOCK:
              return "invalid UDT socket";
            case EUNBOUNDSOCK:
              return "cannot listen on unbound socket";
            case ENOLISTEN:
              return "accept: socket is not in listening state";
            case ERDVNOSERV:
              return "rendezvous connection process does not allow listen and accept call";
            case ERDVUNBOUND:
              return "rendezvous connection setup is enabled but bind has not been called before connect";
            case ESTREAMILL:
              return "operation not supported in SOCK_STREAM mode";
            case EDGRAMILL:
              return "operation not supported in SOCK_DGRAM mode";
            case EDUPLISTEN:
              return "another socket is already listening on the same UDP port";
            case ELARGEMSG:
              return "message is too large to be hold in the sending buffer";
            case EASYNCFAIL:
              return "non-blocking call failure";
            case EASYNCSND:
              return "no buffer available for sending";
            case EASYNCRCV:
              return "no data available for read";
            case ETIMEOUT:
              return "timeout before operation completes";
            case EPEERERR:
              return "Error has happened at the peer side";
            default:
              return "unkown UDT error";
          }
        }

        udt_category::udt_category()
        {}
      }
    }
  }
}
