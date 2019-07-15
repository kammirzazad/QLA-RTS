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

#include "FCLayer2.h"
#include <inet/applications/base/ApplicationPacket_m.h>

Define_Module(FCLayer2);

void    FCLayer2::sendVal()
{
        int predict = predictLabel();
        int currentLabel = setExpected();

        // Write down the classification result and the squared error
        double error = square_error();
        printf("Error: %0.6lf\n", error);

        if (currentLabel == predict)
        {
            ++nCorrect;
            std::cout << "Classification: YES. Label = " << currentLabel << ". Predict = " << predict << std::endl << std::endl;
            report << "Sample " << iterCnt << ": YES. Label = " << currentLabel << ". Predict = " << predict << ". Error = " << error << std::endl;
        }
        else
        {
            std::cout << "Classification: NO.  Label = " << currentLabel << ". Predict = " << predict << std::endl;

            //showImage();

            report << "Sample " << iterCnt << ": NO.  Label = " << currentLabel << ". Predict = " << predict << ". Error = " << error << std::endl;
        }

        iterCnt++;
        if(iterCnt < numSamples)
        {
            selfMsg->setKind(POP);
            scheduleAt(simTime()+(period-wcet), selfMsg);
        }
        else
        {
            std::cout << "loss rate: ";
            for(uint id=0; id<8; id++)
                std::cout << lossCnts[id]/(double)numSamples << " ";
            std::cout << std::endl;

            uint  numSamples = par("numSamples");
            double accuracy = (double)(nCorrect) / numSamples * 100.0;
            std::cout << "Number of correct samples: " << nCorrect << " / " << numSamples << std::endl;
            printf("Accuracy: %0.2lf\n", accuracy);

            report << "Number of correct samples: " << nCorrect << " / " << numSamples << std::endl;
            report << "Accuracy: " << accuracy << endl;
        }
}

void    FCLayer2::setOutVal()
{
        for(uint id=0; id<8; id++)
            buffers[id]->waitForToken(1);

        for(int j=0; j<N3; j++)
        {
            in3[j+1] = 0.0;

            // break n2 (128) to 16*8
            for(uint id=0; id<8; id++)
            {
                token& nnToken = buffers[id]->readToken(0);

                /* empty token does not add anything to sum */
                if(nnToken.isEmpty())
                {
                    #ifdef EMPTY
                    std::cout << "FCLayer2 received an empty token from producer " << id << std::endl;
                    #endif

                    lossCnts[id]++;
                    continue;
                }

                auto parIn2 = nnToken.getData();

                for(int i=0; i<16; i++)
                {
                    in3[j+1] += parIn2.array[i] * w2[(16*id)+i+1][j+1];
                }
            }

            out3[j+1] = sigmoid(in3[j+1]);
        }

        for(uint id=0; id<8; id++)
            buffers[id]->popToken(1);

        selfMsg->setKind(PUSH);
        scheduleAt(simTime()+wcet, selfMsg);
}

/* don't use */
void    FCLayer2::showImage()
{
        std::cout << "Image:" << endl;
        for (int j = 1; j <= IN_WIDTH; ++j)
        {
            for (int i = 1; i <= IN_WIDTH; ++i)
            {
                //std::cout << d[i][j];
            }
            std::cout << std::endl;
        }
        std::cout << std::endl;
}

int     FCLayer2::predictLabel()
{
        // Prediction
        int predict = 1;
        for (int i = 2; i <= N3; ++i)
        {
            if (out3[i] > out3[predict])
            {
                predict = i;
            }
        }
        --predict;

        return predict;
}

int     FCLayer2::setExpected()
{
        char number;
        // Reading label
        label.read(&number, sizeof(char));
        for (int i = 1; i <= N3; ++i)
            expected[i] = 0.0;
        expected[number + 1] = 1.0;

        return (int) number;
}

void    FCLayer2::handleMessageWhenUp(cMessage* msg)
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
        else if(msg->getKind() == inet::UDP_I_DATA)
        {
            auto ctrl = check_and_cast<inet::UDPDataIndication*>(msg->removeControlInfo());

            int port = (uint)ctrl->getDestPort();
            int id = getL1Id(port);

            if((id<0) || (id>7)) // id should be in [0,7]
            {
                std::cout << "Received data from unexpected port number" << std::endl;
                exit(1);
            }

            if(ctrl->getSrcAddr() != getIP(id+1))
            {
                std::cout << "Received data from unexpected IP address" << std::endl;
                exit(1);
            }

            //std::cout << "@iteration " << iterCnt << " (t=" << simTime() << ") FCLayer2 received data from id=" << id;
            buffers[id]->addToken(PK(msg));

            delete msg;
            delete ctrl;
        }
        else if(msg->getKind() == inet::UDP_I_ERROR)
        {
            EV_WARN << "Ignoring UDP error report" << std::endl;
            delete msg;
        }
        else
        {
            throw cRuntimeError("Unrecognized message (%s)%s", msg->getClassName(), msg->getName());
        }
}


void    FCLayer2::initialize(int stage)
{
        ApplicationBase::initialize(stage);

        if(stage == inet::INITSTAGE_LOCAL)
        {
            ts = par("startTime");
            wcet = par("wcet");
            period = par("period");
            numSamples = par("numSamples");
            loadWeights(par("path_to_model"));

            str2 path2label = par("path_to_label");
            str2 path2report = par("path_to_report");

            const char *mem_str = par("mem").stringValue();
            arr<int> mems = cStringTokenizer(mem_str).asIntVector();

            report.open(path2report.c_str(), std::ofstream::out);
            label.open(path2label.c_str(), std::ifstream::in | std::ifstream::binary ); // Binary label file

            //std::cout << "mem=" << mem_str << std::endl;

            // Reading file headers
            char number;
            for (int i = 1; i <= 8; ++i)
                label.read(&number, sizeof(char));

            for(uint id=0; id<8; id++)
            {
                auto sPtr = new sock();
                sPtr->setOutputGate(gate("udpOut"));
                sPtr->bind(getIP(9),get_L1_L2_portnum(id));

                inSockets.push_back(sPtr);
                buffers.push_back(new udpBuffer(mems[id]));
            }

            selfMsg = new cMessage("scheduler");
        }
}

bool    FCLayer2::handleNodeStart(inet::IDoneCallback *doneCallBack)
{
        std::cout << "FCLayer2 started." << std::endl;

        selfMsg->setKind(POP);
        scheduleAt(ts,selfMsg);
        return true;
}

FCLayer2::~FCLayer2()
{
        if(selfMsg) { cancelEvent(selfMsg); }

        for(sock* socket:inSockets) delete socket;
        for(udpBuffer* buff:buffers) delete buff;

        delete  selfMsg;

        label.close();
        report.close();
}

FCLayer2::FCLayer2()
:   mem(1), iterCnt(0), nCorrect(0), numSamples(0), ts(0.0), wcet(0.0), period(0.0)
{
        for(uint i=0; i<8; i++)
            lossCnts[i] = 0;

        for (int i = 1; i <= N2; ++i)
        {
            w2[i] = new double [N3 + 1];
        }

        in3 = new double [N3 + 1];
        out3 = new double [N3 + 1];
}

void    FCLayer2::handleNodeCrash()
{
        std::cout << "FCLayer2 crashed!" << std::endl;
}

bool    FCLayer2::handleNodeShutdown(inet::IDoneCallback *doneCallBack)
{
        std::cout << "FCLayer2 shutdown." << std::endl;
        return true;
}

void    FCLayer2::loadWeights(str2 file_name)
{
        double temp;
        std::ifstream file(file_name.c_str(), std::ifstream::in);

        // Input layer - Hidden layer
        for (int i = 1; i <= N1; ++i)
        {
            for (int j = 1; j <= N2; ++j)
            {
                file >> temp; //w1[i][j];
            }
        }

        // Hidden layer - Output layer
        for (int i = 1; i <= N2; ++i)
        {
            for (int j = 1; j <= N3; ++j)
            {
                file >> w2[i][j];
            }
        }

        file.close();
}

double  FCLayer2::square_error()
{
        double res = 0.0;
        for (int i = 1; i <= N3; ++i)
        {
            res += (out3[i] - expected[i]) * (out3[i] - expected[i]);
        }
        res *= 0.5;
        return res;
}
