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

#ifndef SCHEDSTREAM_LINEAR_ACTOR_H
#define SCHEDSTREAM_LINEAR_ACTOR_H

#include "../include/typedefs.h"

class   INET_API LinearActor : public inet::ApplicationBase
{
        private:

        enum        SelfMsgKinds { POP = 1, PUSH };
        enum        ReplacementPolicy { AVG, LAST, STATIC };

        void        sendVal();
        void        setOutVal();
        void        printInfo();
        void        printLoss();
        void        processStart();
        void        processPacket(cPacket *msg);
        void        parseActors(Json::Value actorArr);
        void        parseChannels(Json::Value chArr);
        void        emulateSDFIteration();
        double      genVal(arr<str2> inArr, arr<double> weightArr);
        double      sampleInput(str2 inputName);

        uint                    sinePeriod;
        double                  sineBase, sineAmplitude;

        uint                    iterCnt;
        str2                    name, host;
        bool                    idle, hasInput, isOutput;
        double                  snrWeight; // for output actors
        double                  pSignal, pNoise;
        double                  ts, wcet, period;
        double                  outVal, defaultVal;
        strMap                  actor2host;
        cMessage*               selfMsg;
        arr<uint>               lostCount;
        arr<str2>               inputs, exeOrder;
        arr<sock*>              inSockets, outSockets;
        Json::Value             channels;               /* used for emulating SDF execution */
        arr<double>             weights;
        arr<double>             lastSeenVals, runningSums;
        arr<netInfo*>           producers, consumers;
        arr<udpBuffer*>         buffers;
        ReplacementPolicy       myPolicy;
        std::map<str2,double>   actorVals;              /* used for emulating SDF execution */

        protected:

        virtual void    initialize(int stage) override;
        virtual int     numInitStages() const override { return inet::NUM_INIT_STAGES; }
        virtual void    handleNodeCrash() override;
        virtual void    handleMessageWhenUp(cMessage *msg) override;
        virtual bool    handleNodeStart(inet::IDoneCallback *doneCallback) override;
        virtual bool    handleNodeShutdown(inet::IDoneCallback *doneCallback) override;

        public:

        LinearActor();
        ~LinearActor();
};

#endif /* SCHEDSTREAM_LINEAR_ACTOR_H */
