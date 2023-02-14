#include <iostream>
#include <thread>

int main()
{
    while (true)
    {
        static int i = 0;
        std::cout << "hello " << i++ << std::endl;
        std::this_thread::sleep_for (std::chrono::seconds (1));
    }
}
