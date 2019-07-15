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

#include "Sensor.h"

#include <inet/applications/base/ApplicationPacket_m.h>

Define_Module(Sensor);

void    Sensor::sendVal()
{
        nnData  tiles[GRID_WIDTH*GRID_WIDTH];

        for(uint tileIdx=0; tileIdx<(GRID_WIDTH*GRID_WIDTH); tileIdx++)
        {
            // row first tiling
            uint tileX = tileIdx%GRID_WIDTH;
            uint tileY = tileIdx/GRID_WIDTH;

            uint minX = tileX*TILE_WIDTH;
            uint minY = tileY*TILE_WIDTH;

            // row first serialization
            for(uint offset=0; offset<(TILE_WIDTH*TILE_WIDTH); offset++)
            {
                uint x = minX + (offset%TILE_WIDTH);
                uint y = minY + (offset/TILE_WIDTH);

                tiles[tileIdx].array[offset] = d[x+1][y+1];
            }
        }

        for(uint tileIdx=0; tileIdx<(GRID_WIDTH*GRID_WIDTH); tileIdx++)
        {
            //std::cout << "@iteration " << iterCnt << " sending tile " << tileIdx << " to FCLayer1" << std::endl;

            for(uint id=0; id<8; id++)
            {
                nnPacket *msg = new nnPacket("token");
                msg->setByteLength(sizeof(nnData)+sizeof(uint));
                msg->setSequenceNumber((GRID_WIDTH*GRID_WIDTH*iterCnt)+tileIdx);
                msg->setPayload(tiles[tileIdx]);
                outSockets[id]->send(msg);
            }
        }

        iterCnt++;
        if(iterCnt < numSamples)
        {
            selfMsg->setKind(POP);
            scheduleAt(simTime()+(period-wcet), selfMsg);
        }
}

void    Sensor::setOutVal()
{
        char number;
        for (int j = 1; j <= IN_WIDTH; ++j)
        {
            for (int i = 1; i <= IN_WIDTH; ++i)
            {
                image.read(&number, sizeof(char));

                if (number == 0)
                {
                    d[i][j] = 0;
                }
                else
                {
                    d[i][j] = 1;
                }
            }
        }

        selfMsg->setKind(PUSH);
        scheduleAt(simTime()+wcet, selfMsg);
}

void    Sensor::initialize(int stage)
{
        ApplicationBase::initialize(stage);

        if(stage == inet::INITSTAGE_LOCAL)
        {
            ts = par("startTime");
            wcet = par("wcet");
            period = par("period");
            numSamples = par("numSamples");
            str2 path2image = par("path_to_image");
            image.open(path2image.c_str(), std::ifstream::in | std::ifstream::binary); // Binary image file

            // Reading file headers
            char number;
            for (int i = 1; i <= 16; ++i)
                image.read(&number, sizeof(char));

            for(uint id=0; id<8; id++)
            {
                auto sPtr = new sock();
                sPtr->setOutputGate(gate("udpOut"));
                sPtr->connect(getIP(id+1),get_L0_L1_portnum(id));

                outSockets.push_back(sPtr);
            }

            selfMsg = new cMessage("scheduler");
        }
}

void    Sensor::handleMessageWhenUp(cMessage* msg)
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
            throw cRuntimeError("Sensor received UDP packet");
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



bool    Sensor::handleNodeStart(inet::IDoneCallback *doneCallBack)
{
        std::cout << "Sensor started." << std::endl;

        selfMsg->setKind(POP);
        scheduleAt(ts,selfMsg);
        return true;
}

Sensor::~Sensor()
{
        if(selfMsg) { cancelEvent(selfMsg); }

        delete  selfMsg;

        image.close();
}

Sensor::Sensor()
:   iterCnt(0), numSamples(0), ts(0.0), wcet(0.0), period(0.0)
{}

void    Sensor::handleNodeCrash()
{
        std::cout << "Sensor crashed!" << std::endl;
}

bool    Sensor::handleNodeShutdown(inet::IDoneCallback *doneCallBack)
{
        std::cout << "Sensor shutdown." << std::endl;
        return true;
}

