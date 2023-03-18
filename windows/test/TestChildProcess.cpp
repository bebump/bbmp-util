#include <iostream>
#include <sstream>
#include <thread>

int main()
{
    while (true)
    {
        std::string line;
        std::getline (std::cin, line);
        std::cout << "hello " << line << std::endl;
    }
}
