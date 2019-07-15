#ifndef SCHEDSTREAM_TYPEDEF_H
#define SCHEDSTREAM_TYPEDEF_H

#include <map>
#include <vector>
#include "netInfo.h"
#include "udpBuffer.h"
#include <jsoncpp/json/json.h>
#include <inet/applications/base/ApplicationBase.h>
#include <inet/transportlayer/contract/udp/UDPSocket.h>

template<class T>
using   arr = std::vector<T>;

using   str2 = std::string;
using   uint = unsigned int;
using   sock = inet::UDPSocket;
using   strMap = std::map<str2,str2>;
using   addrMap = std::map<str2,inet::L3Address>;

#endif
