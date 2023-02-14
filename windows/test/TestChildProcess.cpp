#include <iostream>
#include <sstream>
#include <thread>

int main()
{
    while (true)
    {
        static int i = 0;
        std::cout << "hello " << i++ << std::endl;

        std::string line;
        std::getline (std::cin, line);
        std::cout << "hello " << line << std::endl;

        std::this_thread::sleep_for (std::chrono::seconds (1));
    }
}
