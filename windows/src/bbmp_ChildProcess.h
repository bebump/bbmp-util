#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace bbmp
{

class ChildProcess
{
public:
    ChildProcess (const std::string& pathToExe, std::function<void (const char*, size_t)> read_callback);

    ~ChildProcess();

    void IssueRead();

    void Write (const std::string& msg);

private:
    class Impl;

    std::unique_ptr<Impl> impl;
};

int WindowsSleepEx (uint32_t timeoutMilliseconds, bool alertable);

}
