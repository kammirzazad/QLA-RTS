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

#ifndef MNIST_FCLAYER1_H
#define MNIST_FCLAYER1_H

#include "../include/typedefs.h"


class INET_API FCLayer1 : public inet::ApplicationBase
{
    private:

    enum            SelfMsgKinds { POP=1, PUSH };

    uint            id, lossCnt, iterCnt, numSamples;
    sock            *inSock, *outSock;
    nnData          partialIn2;
    double          ts, wcet, period;
    double          out1[N1];
    cMessage*       selfMsg;
    udpBuffer*      buffer;
    double          *w1[N1 + 1];        // From layer 1 to layer 2. Or: Input layer - Hidden layer

    void            sendVal();
    void            setOutVal();
    void            loadWeights(str2 path_to_model);

    protected:

    virtual void    initialize(int stage) override;
    virtual int     numInitStages() const override { return inet::NUM_INIT_STAGES; }
    virtual void    handleNodeCrash() override;
    virtual void    handleMessageWhenUp(cMessage *msg) override;
    virtual bool    handleNodeStart(inet::IDoneCallback *doneCallback) override;
    virtual bool    handleNodeShutdown(inet::IDoneCallback *doneCallback) override;

    public:

    FCLayer1();
    virtual ~FCLayer1();
};

#endif /* ACTORS_FCLAYER1_FCLAYER1_H_ */
