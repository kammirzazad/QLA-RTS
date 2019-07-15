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

#include "LinearActor.h"
#include "../include/utility.h"

#include <cmath>
#include <cctype>
#include <fstream>
#include <inet/applications/base/ApplicationPacket_m.h>

Define_Module(LinearActor);

void    LinearActor::sendVal()
{
        if(isOutput)
        {
            assert(!hasInput);

            emulateSDFIteration();

            pSignal += pow(actorVals[name],2);
            pNoise += pow((actorVals[name]-outVal),2);

        }
        else
        {
            //std::cout << "actor " << name << " @ iter " << iterCnt << " is sending " << outVal << std::endl;

            for(uint i=0; i<consumers.size(); i++)
            {
                inet::ApplicationPacket *msg = new inet::ApplicationPacket("token");
                msg->setByteLength(sizeof(double)+sizeof(uint));
                msg->setSequenceNumber(iterCnt);
                msg->addPar("data") = outVal;

                outSockets[i]->sendTo(msg, consumers[i]->addr, consumers[i]->port);
            }
        }

        iterCnt++;

        if(iterCnt < ((uint)par("numIter")))
        {
            selfMsg->setKind(POP);
            scheduleAt(simTime()+(period-wcet), selfMsg);
        }
        else
        {
            //printLoss();

            if(isOutput) { std::cout << "pSignal:" << pSignal << ",pNoise:" << pNoise << std::endl; }

            if(isOutput) { std::cout << "output," << name << ",SNR," << pSignal/pNoise << ",weight," << snrWeight << std::endl; }
        }
}

void    LinearActor::setOutVal()
{
        double val = 0;

        /* in case actor is connected to input(s), calculate initial value */
        outVal = genVal(inputs,weights);

        for(uint i=0; i<producers.size(); i++)
        {
            if(producers[i]->hasInitialToken)
            {
                std::cout << "producer " << i << " of " << name << " has initial token" << std::endl;

                val = 0; // FIXME: initialTokens are assumed to be zero
                runningSums[i] += val;
                lastSeenVals[i] = val;

                producers[i]->hasInitialToken = false; // no more initial token
            }
            else // there is no initial token associated with the producer
            {
                buffers[i]->waitForToken();

                if(buffers[i]->readToken().isEmpty())
                {
                    switch(myPolicy)
                    {
                        case STATIC:
                        {
                            val = defaultVal; /* FIXME: need to know multiplicative weights as well */
                            break;
                        }

                        case LAST:
                        {
                            val = lastSeenVals[i]; // FIXME: what if no value has been received so far?
                            break;
                        }

                        case AVG:
                        {
                            uint numSamples = iterCnt-lostCount[i];

                            if(numSamples==0)
                            {
                                val = 0.0; // FIXME
                            }
                            else
                            {
                                val = runningSums[i] / numSamples;
                            }
                            break;
                        }

                        default:
                        {
                            std::cout << "something bad happened!" << std::endl;
                            exit(1);
                        }
                    }

                    lostCount[i]++;
                }
                else
                {
                    val = buffers[i]->readToken().getData();
                    runningSums[i] += val;
                    lastSeenVals[i] = val;
                }

                buffers[i]->popToken();
            }

            //std::cout << name << " received " << val << std::endl;

            outVal += producers[i]->weight * val;
        }

        selfMsg->setKind(PUSH);
        scheduleAt(simTime()+wcet, selfMsg);
}

double  LinearActor::genVal(arr<str2> inArr, arr<double> weightArr)
{
        double val = 0;

        for(uint i=0; i<inArr.size(); i++)
            val += weightArr[i] * sampleInput(inArr[i]);

        return val;
}

double  LinearActor::sampleInput(str2 inputName)
{
        uint index = atoi(inputName.substr(1).c_str());

        return sineBase + (sineAmplitude * sin((2*M_PI*(iterCnt+index))/sinePeriod));
}

void    LinearActor::printLoss()
{
        std::cout << name << ": ";

        for(uint i=0; i<lostCount.size(); i++)
        {
            std::cout << 1.0-((double)(lostCount[i])/iterCnt) << " ";
        }

        std::cout << std::endl;
}

void    LinearActor::processPacket(cPacket *msg)
{
        bool valid = false;
        auto ctrl  = check_and_cast<inet::UDPDataIndication*>(msg->removeControlInfo());
        auto srcAddr = ctrl->getSrcAddr();

        for(uint i=0; i<producers.size(); i++)
        {
            if(producers[i]->addr == srcAddr)
            {
                valid = true;

                if(producers[i]->port != (uint)(ctrl->getDestPort()))
                {
                    std::cout << "expected data from port " << producers[i]->port << " but got " << ctrl->getDestPort() << std::endl;
                    assert(false);
                }

                buffers[i]->addToken(msg);
                //std::cout << "actor " << name << " received data from " << producers[i].actor << std::endl;
                break;
            }
        }

        if(!valid)
            std::cout << "actor " << name << " received data from unknown source" << std::endl;

        delete msg;
        delete ctrl;
}

void    LinearActor::handleMessageWhenUp(cMessage *msg)
{
        if(idle)
        {
            std::cout << "Something bad happened, idle host received a message" << std::endl;
            return;
        }

        if( msg->isSelfMessage() )
        {
            ASSERT(msg == selfMsg);

            switch( msg->getKind() )
            {
                case    PUSH:   { sendVal(); break; }

                case    POP:    { setOutVal(); break; }

                default:
                {
                        std::cout << "unknown msg kind " << msg->getKind() << std::endl;
                        exit(1);
                }
            }
        }
        else if( msg->getKind() == inet::UDP_I_DATA )
        {
            processPacket(PK(msg));
        }
        else if( msg->getKind() == inet::UDP_I_ERROR )
        {
            EV_WARN << "Ignoring UDP error report\n";
            delete msg;
        }
        else
        {
            throw cRuntimeError("Unrecognized message (%s)%s", msg->getClassName(), msg->getName());
        }
}

bool    LinearActor::handleNodeStart(inet::IDoneCallback *doneCallback)
{
        printInfo();

        if(!idle)
        {
            selfMsg->setKind(POP);
            scheduleAt(ts, selfMsg);
        }

        return true;
}

void    LinearActor::printInfo()
{
        if(idle)
            return;

        std::cout << "actor:" << name << " (hasInput=" << hasInput << ", isOutput=" << isOutput << ", host=" << host << ", ts=" << ts << "s, wcet=" << wcet << "s";
        std::cout << ", producers: ";
        printActors(producers);
        std::cout << ", srcPorts: ";
        printPorts(producers);
        std::cout << ", consumers: ";
        printActors(consumers);
        std::cout << ", dstPorts: ";
        printPorts(consumers);
        std::cout << ")" << std::endl;
}

void    LinearActor::initialize(int stage)
{
        ApplicationBase::initialize(stage);

        if(stage == inet::INITSTAGE_LOCAL)
        {
            Json::Value obj;

            sineBase = par("sineBase");
            sinePeriod = par("sinePeriod");
            sineAmplitude = par("sineAmplitude");

            defaultVal = par("defaultVal");
            str2 rp = par("replacementPolicy");
            str2 aName = par("name");
            str2 path2graph = par("graph");

            name = aName;

            if( rp == "static" )
            {
                myPolicy = STATIC;
            }
            else if( rp == "lastSeen" )
            {
                myPolicy = LAST;
            }
            else if( rp == "runningAverage" )
            {
                myPolicy = AVG;
            }
            else
            {
                std::cout << "unknown replacement policy " << rp << ", using static policy" << std::endl;
                myPolicy = STATIC;
            }

            std::ifstream cfg(path2graph.c_str(), std::ifstream::binary);

            if(!cfg.is_open())
            {
                std::cout << "Unable to open file " << path2graph << std::endl;
                exit(3);
            }

            cfg >> obj;

            period = conv2sec(obj["period"].asString());
            channels = obj["channels"];

            parseActors(obj["actors"]);

            Json::Value exeOrderArr = obj["executionOrder"];
            for(uint i=0; i<exeOrderArr.size(); i++)
            {
                str2 actorName = exeOrderArr[i].asString();
                exeOrder.push_back(actorName);
                actorVals.insert(std::pair<str2,double>(actorName,0.0));
            }

            if(!idle)
            {
                parseChannels(obj["channels"]);

                selfMsg = new cMessage("scheduler");

                auto myAddr = netInfo::getIP(host);

                for(uint i=0; i<producers.size(); i++)
                {
                    auto sPtr = new sock();
                    sPtr->setOutputGate(gate("udpOut"));
                    sPtr->bind(myAddr, producers[i]->port);

                    inSockets.push_back(sPtr);
                    lostCount.push_back(0);
                    runningSums.push_back(0);
                    lastSeenVals.push_back(0);
                }

                for(uint i=0; i<consumers.size(); i++)
                {
                    auto sPtr = new sock();
                    sPtr->setOutputGate(gate("udpOut"));

                    outSockets.push_back(sPtr);
                }
            }

            if(isOutput)
            {
                std::cout << "execution order: ";
                for(const auto& str2:exeOrder)
                    std::cout << str2 << " ";
                std::cout << std::endl;
            }
        }
}

void    LinearActor::parseActors(Json::Value actorArr)
{
        for(const auto& actor:actorArr)
        {
            auto currName = actor["name"].asString();
            auto currHost = actor["host"].asString();

            actor2host[currName] = currHost;

            if(currName == name)
            {
                assert(idle);

                idle = false;
                host = currHost;
                ts = conv2sec(actor["ts"].asString());
                wcet = conv2sec(actor["wcet"].asString());
            }
        }
}

void    LinearActor::parseChannels(Json::Value chArr)
{
        for(uint i=0; i<chArr.size(); i++)
        {
            uint port = 11000+i;

            auto source = chArr[i]["source"].asString();
            auto weight = chArr[i]["weight"].asDouble();

            if(source == name)
            {
                if(chArr[i]["target"] == Json::Value::null)
                {
                    isOutput = true;
                    snrWeight = weight;
                }
                else
                {
                    auto target = chArr[i]["target"].asString();
                    //std::cout << "consumer " << target << ":" << port << std::endl;
                    consumers.push_back(new netInfo(target, actor2host[target], weight, port, false));
                }
            }
            else if(chArr[i]["target"].asString() == name)
            {
                if(source[0] == 'i')
                {
                    hasInput = true;
                    std::cout << "input " << source << " is connected to actor " << name <<std::endl;
                    inputs.push_back(source);
                    weights.push_back(weight);
                }
                else
                {
                    bool hasInitialToken = (chArr[i]["hasInitialToken"] != Json::Value::null);
                    //std::cout << "producer " << source << ":" << port << ":" << buffers.size() << std::endl;

                    buffers.push_back(new udpBuffer(chArr[i]["mem"].asUInt()));
                    producers.push_back(new netInfo(source, actor2host[source], weight, port, hasInitialToken));
                }
            }
        }
}

void    LinearActor::handleNodeCrash()
{
        std::cout << "actor " << name << " crashed!" << std::endl;
}

bool    LinearActor::handleNodeShutdown(inet::IDoneCallback *doneCallback)
{
        std::cout << "actor " << name << " shutdown." << std::endl;
        return true;
}

void    LinearActor::emulateSDFIteration()
{
        std::map<str2,double> oldVals = actorVals;

        for(const auto& actor:exeOrder)
        {
            double newVal = 0.0;

            // look for sources of "actor" amongst channels
            for(const auto& channel:channels)
            {
                auto target = channel["target"].asString();
                auto source = channel["source"].asString();
                auto weight = channel["weight"].asDouble();

                if(weight == 0.0)
                {
                    std::cout << "ERROR: found weight of zero" << std::endl;
                    exit(1);
                }

                bool hasTarget = (channel["target"] != Json::Value::null);                  // identify output channels
                bool hasInitialToken = (channel["hasInitialToken"] != Json::Value::null);   // identify backedges

                if(!hasTarget)
                    continue;

                if(target == actor)
                {
                    //std::cout << source << " -> " << target << std::endl;

                    double srcVal = 0.0;

                    if(source[0] == 'i')
                        srcVal = sampleInput(source);
                    else if(hasInitialToken)
                        srcVal = oldVals[source];        /* for backedges, use values from previous iteration */
                    else
                        srcVal = actorVals[source];

                    newVal += weight * srcVal;
                }
            }

            actorVals[actor] = newVal;
        }
}

LinearActor::LinearActor()
:       sinePeriod(1), sineBase(0.0), sineAmplitude(1.0),
        iterCnt(0),
        idle(true), hasInput(false), isOutput(false),
        snrWeight(1.0),
        pSignal(0.0), pNoise(0.0),
        ts(0.0), wcet(0.0), period(0.0),
        outVal(0.0), defaultVal(0.0)
{
        /* nothing to do */
}

LinearActor::~LinearActor()
{
        if(!idle)
        {
            if(selfMsg) { cancelEvent(selfMsg); }
            delete selfMsg;

            for(sock* socket:inSockets)     delete socket;
            for(sock* socket:outSockets)    delete socket;
            for(netInfo* prod:producers)    delete prod;
            for(netInfo* cons:consumers)    delete cons;
            for(udpBuffer* buff:buffers)    delete buff;
        }
}
