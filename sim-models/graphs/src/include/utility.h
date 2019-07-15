#ifndef SCHEDSTREAM_UTILITY_H
#define SCHEDSTREAM_UTILITY_H

double  conv2sec(std::string val)
{
        float number;
        std::string unit = "none";

        for(unsigned int i=0; i<val.length(); i++)
            if(std::isalpha(val[i]))
            {
                unit = val.substr(i);
                number = std::stof(val.substr(0,i));
                break;
            }

        if(unit == "ms")
        {
            return (number/1000.0);
        }
        else if(unit == "us")
        {
            return (number/1000000.0);
        }
        else if(unit == "s")
        {
            return number;
        }
        else
        {
            std::cout << "unknown unit " << unit << std::endl;
            exit(1);
        }
}

void    printActors(std::vector<netInfo*> arr)
{
        std::cout << "[";

        for(unsigned int i=0; i<arr.size(); i++)
        {
            std::cout << arr[i]->actor;

            if(i != arr.size()-1)
                std::cout << ",";
        }

        std::cout << "]";
}

void    printPorts(std::vector<netInfo*> arr)
{
        std::cout << "[";

        for(unsigned int i=0; i<arr.size(); i++)
        {
            std::cout << arr[i]->port;

            if(i != arr.size()-1)
                std::cout << ",";
        }

        std::cout << "]";
}



#endif
