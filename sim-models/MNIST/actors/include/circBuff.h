#ifndef MNIST_CIRC_BUFF_H
#define MNIST_CIRC_BUFF_H

#include "token.h"

class   circBuff
{
        public :

        ~circBuff() { destroy(); }

        circBuff(uint _capacity)
        {
                    head = 0;
                    tail = 0;
                    cap = _capacity;

                    array = (token*) ::operator new(_capacity*sizeof(token)) ;
        }

        uint        size()      const   { return norm2(tail-head); }
        uint        capacity()  const   { return (cap-1); }

        bool        isFull()    const   { return (norm(tail+1) == head); }
        bool        isEmpty()   const   { return (head == tail); }

        token&      peek(uint index)    { return operator[](index); }

        void        pop(uint n)         { for(uint i=0; i<n; i++) {advanceHead();} }

        void        push(const token& item)
        {
                    new (&array[tail]) token(item);

                    advanceTail();
        }

        template<typename... Args>
        void        push(Args&&... args)
        {
                    new (&array[tail]) token(args...);

                    advanceTail();
        }

        token&      operator[](int index)
        {
                    uint realIndex = ( (index>=0)? norm(head+index) : norm2(tail+index) );

    /*
                    bool range = inRange(realIndex);

                    if(!range)
                    {
                        std::cout << "% " << index << "->" << realIndex << " ? in (" << head << "," << tail << ")" << std::endl;
                        assert(false);
                    }
    */

                    //assert(inRange(realIndex));

                    return array[realIndex];
        }

        private :

        uint        norm(uint x)    const   { return (x%cap); }
        uint        norm2(int x)    const   { return (x+cap)%cap; }

        void        advanceTail()       { assert(!isFull()); tail = norm(tail+1); }
        void        advanceHead()       { assert(!isEmpty()); array[head].~token();  head = norm(head+1); }

        void        destroy()
        {
                    // call destructor for all items
                    while(!isEmpty()) { advanceHead(); }
                    // deallocate storage
                    ::operator delete(array);
        }

        bool        inRange(uint index)
        {
                    if(head <= tail)
                    {
                        std::cout << "+ " << index << " in (" << head << ", " << tail << ")" << std::endl;

                        return (head <= index) && (index < tail);
                    }
                    else
                    {
                        std::cout << "- " << index << " in (" << head << ", " << tail << ")" << std::endl;

                        return (head <= index) || (index < tail);
                    }
        }

        uint        cap;
        uint        head;
        uint        tail;
        token*      array;
};

#endif
