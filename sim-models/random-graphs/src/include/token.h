#ifndef SCHEDSTREAM_TOKEN_H
#define SCHEDSTREAM_TOKEN_H

class   token
{
        public:

        bool        isEmpty() const { return !isInitialized; }

        double      getData()
        {
                    assert(!isEmpty());

                    isRead = true;

                    return data;
        }

        void        copy(double _data)
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

        token(uint _seqN, double _data)
        : seqN(_seqN), isRead(false), isInitialized(true), data(_data)
        {}

        token(uint _seqN)
        : seqN(_seqN), isRead(false), isInitialized(false)
        {}

        const uint  seqN;

        private:

        bool        isRead, isInitialized;
        double      data;
};

#endif
