#ifndef MNIST_TYPEDEF_H
#define MNIST_TYPEDEF_H

#include <map>
#include <set>
#include <vector>
#include <fstream>
#include <iterator>
#include <algorithm>
#include <inet/applications/base/ApplicationBase.h>
#include <inet/transportlayer/contract/udp/UDPSocket.h>
#include "udpBuffer.h"

template<class T>
using   arr = std::vector<T>;

using   str2 = std::string;
using   uint = unsigned int;
using   sock = inet::UDPSocket;
using   strMap = std::map<str2,str2>;
using   addrMap = std::map<str2,inet::L3Address>;

int     getL1Id(uint);
uint    get_L0_L1_portnum(uint);
uint    get_L1_L2_portnum(uint);
double  sigmoid(double);
inet::L3Address getIP(uint);

#define IN_WIDTH    28
#define TILE_WIDTH  4
#define GRID_WIDTH  (IN_WIDTH/TILE_WIDTH)

// n1 = Number of input neurons
#define N1  (IN_WIDTH*IN_WIDTH)

// n2 = Number of hidden neurons
#define N2  128

// n3 = Number of output neurons
#define N3  10

#endif
