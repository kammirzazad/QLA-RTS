#ifndef SCHEDSTREAM_NET_INFO_H
#define SCHEDSTREAM_NET_INFO_H

#include <cstdlib>
#include <inet/networklayer/common/L3Address.h>
//#include <inet/networklayer/common/L3AddressResolver.h>

class   netInfo
{
        public:

        netInfo(std::string _actor, std::string _host, double _weight, uint _port, bool _hasInitialToken)
        : hasInitialToken(_hasInitialToken), port(_port), weight(_weight), actor(_actor), host(_host), addr(getIP(_host))
        {
                        /* nothing to do */
                        //std::cout << "created netInfo with port=" << port << " and host=" << host << std::endl;
        }

        static  inet::L3Address getIP(std::string hostName)
        {
                        std::string baseIP = "169.254.";
                        uint idx = std::stoi(hostName.substr(1)) + 1; // cannot use "169.254.0.0"
                        baseIP += std::to_string(idx) + '.' + std::to_string(idx); // 169.254.idx.idx
                        return inet::L3Address(baseIP.c_str());
        }

        bool            hasInitialToken;
        uint            port;
        double          weight;
        std::string     actor, host;
        inet::L3Address addr;
};

#endif
