#include "bbmp_ChildProcess.h"

#include <filesystem>
#include <iostream>

int main()
{
    for (int i = 0; i < 5; ++i)
    {
        static bbmp::ChildProcess cp ((std::filesystem::current_path() / "test_child_process.exe").string(),
                                      [] (const char* c, size_t n)
                                      {
                                          std::cout << std::string { c, n };
                                      });
        cp.IssueRead();
        cp.TryIssueWrite ("ati\n");
        while (bbmp::WindowsSleepEx (1000, true) != 0);
    }
}
