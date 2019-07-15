#ifndef MNIST_UDP_BUFFER_H
#define MNIST_UDP_BUFFER_H

#include "circBuff.h"
#include "../../nnPacket_m.h"

//#define DEBUG_TOKEN_DROP

class   udpBuffer
{
        public:

        udpBuffer(uint mem) : minSeqN(0), maxSeqN(0), buffer(mem+1)
        {}

        token&      readToken(uint index) { return buffer[index]; }

        void        popToken(uint count)
        {
                    minSeqN += count;
                    buffer.pop(count);
        }

        void        waitForToken(uint count)
        {
                    while(buffer.size() < count)
                        buffer.push(maxSeqN++);
        }

        void        addToken(cPacket* msg)
        {
                    nnPacket* aMsg = check_and_cast<nnPacket*> (msg);

                    uint seqN = aMsg->getSequenceNumber();
                    nnData data = aMsg->getPayload();

                    //std::cout << " seqN=" << seqN << std::endl;

                    placeToken(seqN, data);
        }

        private:

        void        placeToken(uint seqNum, nnData data)
        {
                    if( seqNum >= maxSeqN )
                    {
                        pushToken(seqNum, data);
                    }
                    else if( seqNum >= minSeqN )
                    {
                        updateToken(seqNum, data);
                    }
                    #if defined(DEBUG_TOKEN) || defined(DEBUG_TOKEN_DROP)
                    else
                    {
                        std::cout << "Token with seqN=" << seqNum << " arrived too late (minSeqN=" << minSeqN << ")" << std::endl;
                    }
                    #endif
        }

        void        pushToken(uint seqNum, nnData data)
        {
                    while(true)
                    {
                        if( buffer.isFull() )
                        {
                            std::cout << "Buffer overflow, dropping token with seqN=" << seqNum << " (maxSeqN=" << maxSeqN << ")" << std::endl;
                            return;
                        }

                        if( seqNum == maxSeqN )
                        {
                            buffer.push(seqNum, data);
                            #if defined(DEBUG_TOKEN) || defined(DEBUG_TOKEN_PUSH)
                            std::cout << "Pushed token with seqN=" << seqNum << std::endl;
                            #endif
                            maxSeqN++;
                            return;
                        }

                        /* there are missing packets, just insert empty ones */
                        assert(seqNum > maxSeqN);
                        buffer.push(maxSeqN);
                        #if defined(DEBUG_TOKEN) || defined(DEBUG_TOKEN_PUSH)
                        std::cout << "Pushed empty token with seqN=" << maxSeqN << std::endl;
                        #endif
                        maxSeqN++;
                    }
        }

        void        updateToken(uint seqNum, nnData data)
        {
                    assert(!buffer.isEmpty());

                    /*
                        There is an empty token in the buffer with given seqNum, update it.

                        Note that,
                          - "tokenBuffer" will make sure that index exists
                          - "udpToken" will make sure that data was not received before
                    */

                    uint firstSeqNum = buffer[0].seqN;
                    buffer[seqNum-firstSeqNum].copy(data);
                    #if defined(DEBUG_TOKEN) || defined(DEBUG_TOKEN_UPDATE)
                    std::cout << "Updated token with seqN=" << seqNum << std::endl;
                    #endif
                    return;
        }

        uint        minSeqN, maxSeqN;
        circBuff    buffer;
};

#endif
