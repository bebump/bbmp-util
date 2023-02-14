#include "bbmp_ChildProcess.h"

#include <filesystem>
#include <iostream>

int main()
{
    for (int i = 0; i < 5; ++i)
    {
        static bbmp::ChildProcess cp ((std::filesystem::current_path() / "test_child_process.exe").string(),
                                      [] (const char* c, size_t n) {
                                          std::cout << std::string { c, n };
                                      });
        cp.IssueRead();

        if (bbmp::WindowsSleepEx (1100, true) == 0)
            std::cout << "Read timeout expired" << std::endl;

        if (cp.TryIssueWrite ("kadzsing\n"))
            if (bbmp::WindowsSleepEx (200, true) == 0);
    }
}
