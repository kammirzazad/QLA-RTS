#ifndef MNIST_TOKEN_H
#define MNIST_TOKEN_H

struct  nnData      { double array[16]; };

class   token
{
        public:

        bool        isEmpty() const { return !isInitialized; }

        nnData      getData()
        {
                    assert(!isEmpty());

                    isRead = true;

                    return data;
        }

        void        copy(nnData _data)
        {
                    assert(isEmpty());

                    if(!isRead)
                    {
                        data = _data;

                        isInitialized = true;
                    }
                    #ifdef ALREADY
                    else
                    {
                        std::cout << "ignoring update to already consumed token" << std::endl;
                    }
                    #endif
        }

        token(uint _seqN, nnData _data)
        : seqN(_seqN), isRead(false), isInitialized(true), data(_data)
        {}

        token(uint _seqN)
        : seqN(_seqN), isRead(false), isInitialized(false)
        {}

        const uint  seqN;

        private:

        bool        isRead, isInitialized;
        nnData      data;
};

#endif
