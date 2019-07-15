#include "typedefs.h"
#include <cstdlib>
#include <inet/networklayer/common/L3Address.h>

#define L0_L1_BASE_PORTNUM  10000
#define L1_L2_BASE_PORTNUM  10010

double  sigmoid(double x)
{
        return 1.0 / (1.0 + exp(-x));
}

inet::L3Address getIP(uint idx)
{
        std::string baseIP = "169.254.";
        idx++; // cannot use "169.254.0.0"
        baseIP += std::to_string(idx) + '.' + std::to_string(idx); // 169.254.idx.idx
        return inet::L3Address(baseIP.c_str());
}

uint    get_L0_L1_portnum(uint id)  { return L0_L1_BASE_PORTNUM+id; }
uint    get_L1_L2_portnum(uint id)  { return L1_L2_BASE_PORTNUM+id; }

int     getL1Id(uint l1_l2_portnum) { return (l1_l2_portnum-L1_L2_BASE_PORTNUM); }
