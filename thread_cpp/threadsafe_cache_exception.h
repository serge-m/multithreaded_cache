#include <exception>
#include <string>

namespace threadsafe_cache
{
    class threadsafe_cache_exception : public std::exception
    {
        std::string message_;
    public:
        threadsafe_cache_exception(std::string const & message)
            : message_(message)
        {}

        const char * what() const
        {
            return message_.c_str();
        }


    };
}