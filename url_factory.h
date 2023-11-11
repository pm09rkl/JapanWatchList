#pragma once

#include <string>

namespace watchList
{
    template <typename F>
    class CUrlFactory
    {
    public:
        template <typename... Args>
        static std::string createUrl(Args&&... args)
        {
            return std::string(F::createProtocol()).append("://").append(F::createHost()).append(F::createTarget(std::forward<Args>(args)...));
        }
    };
    
    template <typename F>
    class CSecureUrlFactory : public CUrlFactory<F>
    {
    public:
        static std::string_view createProtocol()
            { return "https"; }
        
        static std::string_view createPort()
            { return "443"; }
    };
}
