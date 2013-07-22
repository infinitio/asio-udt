#ifndef ASIO_UDT_ERROR_CATEGORY_HH
# define ASIO_UDT_ERROR_CATEGORY_HH

# include <boost/system/error_code.hpp>
# include <string>

namespace boost
{
  namespace asio
  {
    namespace ip
    {
      namespace udt
      {
        void
        throw_errno();

        void
        throw_udt(std::string const& what = "");

        class udt_category : public system::error_category
        {
          public:
            static
            udt_category&
            get();

            enum ErrorCode
            {
              SUCCESS = 0,
              ECONNSETUP = 1000,
              ENOSERVER = 1001,
              ECONNREJ = 1002,
              ESOCKFAIL = 1003,
              ESECFAIL = 1004,
              ECONNFAIL = 2000,
              ECONNLOST = 2001,
              ENOCONN = 2002,
              ERESOURCE = 3000,
              ETHREAD = 3001,
              ENOBUF = 3002,
              EFILE = 4000,
              EINVRDOFF = 4001,
              ERDPERM = 4002,
              EINVWROFF = 4003,
              EWRPERM = 4004,
              EINVOP = 5000,
              EBOUNDSOCK = 5001,
              ECONNSOCK = 5002,
              EINVPARAM = 5003,
              EINVSOCK = 5004,
              EUNBOUNDSOCK = 5005,
              ENOLISTEN = 5006,
              ERDVNOSERV = 5007,
              ERDVUNBOUND = 5008,
              ESTREAMILL = 5009,
              EDGRAMILL = 5010,
              EDUPLISTEN = 5011,
              ELARGEMSG = 5012,
              EASYNCFAIL = 6000,
              EASYNCSND = 6001,
              EASYNCRCV = 6002,
              ETIMEOUT = 6003,
              EPEERERR = 7000,
            };

            virtual
            const char*
            name() const noexcept;

            virtual
            std::string
            message(int code) const;

          private:
            udt_category();
        };
      }
    }
  }
}

#endif
