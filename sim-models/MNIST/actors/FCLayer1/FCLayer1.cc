//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

#include "FCLayer1.h"

#include <cmath>
#include <cctype>
#include <fstream>
#include "nnPacket_m.h"

Define_Module(FCLayer1);

void    FCLayer1::sendVal()
{
        nnPacket *msg = new nnPacket("token");
        msg->setByteLength(sizeof(nnData)+sizeof(uint));
        msg->setSequenceNumber(iterCnt);
        msg->setPayload(partialIn2);
        outSock->send(msg);

        iterCnt++;
        if(iterCnt < numSamples)
        {
            selfMsg->setKind(POP);
            scheduleAt(simTime()+(period-wcet), selfMsg);
        }
        else
        {
            std::cout << "loss rate (id=" << id << "): " << lossCnt/(double)(49*numSamples) << std::endl;
        }
}

void    FCLayer1::setOutVal()
{
        buffer->waitForToken(GRID_WIDTH*GRID_WIDTH);

        // set "out1" by traversing tiles (tokens)
        for(uint tileIdx=0; tileIdx<(GRID_WIDTH*GRID_WIDTH); tileIdx++)
        {
            token& tile = buffer->readToken(tileIdx);

            if(tile.isEmpty())
            {
                lossCnt++;

                #ifdef EMPTY
                std::cout << "@iteration " << iterCnt << " FCLayer1 (id=" << id << ")'s tile#" << tileIdx << " was empty!" << std::endl;
                #endif
            }

            uint tileX = tileIdx%GRID_WIDTH;
            uint tileY = tileIdx/GRID_WIDTH;

            uint minX = (tileX*TILE_WIDTH);
            uint minY = (tileY*TILE_WIDTH);

            // traversal of elements inside each tile
            for(uint offset=0; offset<(TILE_WIDTH*TILE_WIDTH); offset++)
            {
                uint x = minX + (offset%TILE_WIDTH);
                uint y = minY + (offset/TILE_WIDTH);

                uint index = (IN_WIDTH*y) + x;

                if(tile.isEmpty())
                    out1[index] = 0.0;
                else
                    out1[index] = tile.getData().array[offset];
            }
        }

        buffer->popToken(GRID_WIDTH*GRID_WIDTH);

        for(int i=0; i<16; i++)
        {
            partialIn2.array[i] = 0.0;

            for(int j=0; j<N1; j++)
                partialIn2.array[i] += out1[j] * w1[j+1][(16*id)+i+1];

            partialIn2.array[i] = sigmoid(partialIn2.array[i]);
        }

        selfMsg->setKind(PUSH);
        scheduleAt(simTime()+wcet, selfMsg);
}

void    FCLayer1::handleMessageWhenUp(cMessage* msg)
{
        if(msg->isSelfMessage())
        {
            ASSERT(msg == selfMsg);

            switch( msg->getKind() )
            {
                case PUSH:  { sendVal(); break; }
                case POP:   { setOutVal(); break; }

                default:
                {
                    std::cout << "unknown message kind " << msg->getKind() << std::endl;
                    exit(1);
                }
            }
        }
        else if( msg->getKind() == inet::UDP_I_DATA)
        {
            auto ctrl = check_and_cast<inet::UDPDataIndication*>(msg->removeControlInfo());
            if(ctrl->getSrcAddr() != getIP(0)) // h0 hosts input
            {
                std::cout << "Received data from unexpected IP address" << std::endl;
                exit(1);
            }
            if((uint)ctrl->getDestPort() != get_L0_L1_portnum(id))
            {
                std::cout << "Received data from unexpected port number" << std::endl;
                exit(1);
            }

            //std::cout << "@iteration " << iterCnt << " (t=" << simTime() << ") FCLayer1 (id=" << id << ") received data";
            buffer->addToken(PK(msg));

            delete msg;
            delete ctrl;
        }
        else if( msg->getKind() == inet::UDP_I_ERROR)
        {
            EV_WARN << "Ignoring UDP error report" << std::endl;
            delete msg;
        }
        else
        {
            throw cRuntimeError("Unrecognized message (%s)%s", msg->getClassName(), msg->getName());
        }
}

void    FCLayer1::initialize(int stage)
{
        ApplicationBase::initialize(stage);

        if(stage == inet::INITSTAGE_LOCAL)
        {
            id = par("id");
            ts = par("startTime");
            wcet = par("wcet");
            period = par("period");
            int mem = par("mem");
            numSamples = par("numSamples");

            //std::cout << "mem=" << mem << std::endl;

            inSock = new sock();
            outSock = new sock();
            selfMsg = new cMessage("scheduler");
            buffer = new udpBuffer((GRID_WIDTH*GRID_WIDTH*mem));    // 28*28 = 16*49

            inSock->setOutputGate(gate("udpOut"));
            outSock->setOutputGate(gate("udpOut"));
            inSock->bind(getIP(id+1),get_L0_L1_portnum(id));    // id:0=>(h1,10000), id:1=>(h2,10001), ...
            outSock->connect(getIP(9),get_L1_L2_portnum(id));   // h9 hosts FC2

            loadWeights(par("path_to_model"));
        }
}

bool    FCLayer1::handleNodeStart(inet::IDoneCallback *doneCallBack)
{
        std::cout << "FCLayer1 (id=" << id << ") started." << std::endl;

        selfMsg->setKind(POP);
        scheduleAt(ts,selfMsg);
        return true;
}

FCLayer1::~FCLayer1()
{
        if(selfMsg) { cancelEvent(selfMsg); }

        delete  buffer;
        delete  inSock;
        delete  outSock;
        delete  selfMsg;
}

FCLayer1::FCLayer1()
:   id(0), lossCnt(0), iterCnt(0), numSamples(0), ts(0.0), wcet(0.0), period(0.0)
{
        for (int i = 1; i <= N1; ++i)
        {
            w1[i] = new double [N2 + 1];
        }

}

void    FCLayer1::handleNodeCrash()
{
        std::cout << "FCLayer1 (id=" << id << ") crashed!" << std::endl;
}

bool    FCLayer1::handleNodeShutdown(inet::IDoneCallback *doneCallBack)
{
        std::cout << "FCLayer1 (id=" << id << ") shutdown." << std::endl;
        return true;
}

void    FCLayer1::loadWeights(str2 file_name)
{
        double temp;
        std::ifstream file(file_name.c_str(), std::ifstream::in);

        // Input layer - Hidden layer
        for (int i = 1; i <= N1; ++i)
        {
            for (int j = 1; j <= N2; ++j)
            {
                file >> w1[i][j];
            }
        }

        // Hidden layer - Output layer
        for (int i = 1; i <= N2; ++i)
        {
            for (int j = 1; j <= N3; ++j)
            {
                file >> temp; //w2[i][j];
            }
        }

        file.close();
}
