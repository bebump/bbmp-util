#include "bbmp_ChildProcess.h"

#include "bbmp_WindowsHandles.h"

#include <array>
#include <cassert>
#include <cstring>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>

#define NOMINMAX
#define NOGDI

#include <strsafe.h>
#include <windows.h>

#undef NOMINMAX
#undef NOGDI

namespace bbmp
{

enum class ChildInheritsHandle
{
    no,
    yes
};

enum class PipeDirection
{
    inbound,
    outbound
};

static auto createSecurityDescriptor (ChildInheritsHandle inherit)
{
    SECURITY_ATTRIBUTES securityAttributes;
    ZeroMemory (&securityAttributes, sizeof (securityAttributes));
    securityAttributes.nLength              = sizeof (securityAttributes);
    securityAttributes.lpSecurityDescriptor = NULL;
    securityAttributes.bInheritHandle       = inherit == ChildInheritsHandle::yes ? TRUE : FALSE;

    return securityAttributes;
}

struct WindowsPipe
{
    PipeDirection pipeDirection;
    std::string pipeName;
    WindowsHandle<-1> pipe;

    auto get() const
    {
        return pipe.get();
    }
};

static auto createPipe (PipeDirection dir)
{
    static int processPipeId = 0;

    const auto pipeName = std::string ("\\\\.\\pipe\\") + std::to_string (GetCurrentProcessId())
                                                        + std::to_string (processPipeId++);

    auto securityAttribs = createSecurityDescriptor (dir == PipeDirection::inbound ? ChildInheritsHandle::no
                                                                                   : ChildInheritsHandle::no);

    auto pipe = WindowsHandle<-1> (CreateNamedPipeA (pipeName.c_str(),
                                                     (dir == PipeDirection::inbound ? PIPE_ACCESS_INBOUND
                                                                                    : PIPE_ACCESS_OUTBOUND)
                                                         | FILE_FLAG_OVERLAPPED,
                                                     0,
                                                     1,
                                                     8192,
                                                     8192,
                                                     0,
                                                     &securityAttribs));

    return WindowsPipe { dir, pipeName, std::move (pipe) };
}

static auto createOverlappedOppositeHandle (const WindowsPipe& pipe)
{
    auto securityAttribs = createSecurityDescriptor (ChildInheritsHandle::yes);

    return WindowsHandle<-1> (CreateFileA (pipe.pipeName.c_str(),
                                           pipe.pipeDirection == PipeDirection::inbound ? GENERIC_WRITE : GENERIC_READ,
                                           0,
                                           &securityAttribs,
                                           OPEN_EXISTING,
                                           FILE_FLAG_OVERLAPPED,
                                           NULL));
}

class OverlappedWithEvent
{
public:
    OverlappedWithEvent()
    {
        ZeroMemory (&overlapped, sizeof (overlapped));
        overlapped.hEvent = CreateEvent (NULL, TRUE, FALSE, NULL);

        if (overlapped.hEvent == NULL)
            throw std::runtime_error ("OverlappedWithEvent: CreateEvent failed");
    }

    ~OverlappedWithEvent()
    {
        CloseHandle (overlapped.hEvent);
    }

    auto& get() { return overlapped; }

private:
    OVERLAPPED overlapped;
};

class OverlappedWithPointer
{
public:
    OverlappedWithPointer (void* ptr)
    {
        ZeroMemory (&overlapped, sizeof (overlapped));
        overlapped.hEvent = ptr;
    }

    auto& get() { return overlapped; }

private:
    OVERLAPPED overlapped;
};

class ChildProcess::Impl
{
public:
    Impl (const std::string& path_to_exe, std::function<void (const char*, size_t)> read_callback)
        : read_callback_ (std::move (read_callback)), readIssued (false)
    {
        const auto connectPipe = [&] (auto& pipe)
        {
            OverlappedWithEvent overlapped;
            auto success = ConnectNamedPipe (pipe.get(), &overlapped.get());

            if (! success)
            {
                if (GetLastError() == ERROR_IO_PENDING)
                {
                    if (WaitForSingleObject (overlapped.get().hEvent, INFINITE) == WAIT_FAILED)
                        throw std::runtime_error ("Failed WaitForSingleObject for ConnectNamedPipe");
                }
                else if (GetLastError() != ERROR_PIPE_CONNECTED)
                {
                    throw std::runtime_error ("Failed to connect to named pipe");
                }
            }
        };

        for (auto* pipe : std::initializer_list<WindowsPipe*> { &parentReadHandle, &parentWriteHandle })
            connectPipe (*pipe);

        // Spawn the new process
        PROCESS_INFORMATION process_information;
        STARTUPINFO startupinfo;
        ZeroMemory (&process_information, sizeof (PROCESS_INFORMATION));
        ZeroMemory (&startupinfo, sizeof (STARTUPINFO));
        startupinfo.cb = sizeof (STARTUPINFO);

        startupinfo.hStdInput  = childReadHandle.get();
        startupinfo.hStdError  = childWriteHandle.get();
        startupinfo.hStdOutput = childWriteHandle.get();
        startupinfo.dwFlags    = STARTF_USESTDHANDLES;

        const int command_buffer_size = 2048;
        char command_buffer[command_buffer_size];
        strncpy (command_buffer, path_to_exe.c_str(), command_buffer_size);
        command_buffer[command_buffer_size - 1] = '\n';

        const auto success = CreateProcess (NULL,
                                            command_buffer,
                                            NULL,
                                            NULL,
                                            TRUE,
                                            CREATE_NO_WINDOW,
                                            0,
                                            NULL,
                                            &startupinfo,
                                            &process_information);
        if (! success)
            throw std::runtime_error (std::string ("CreateProcess failed with: ") + command_buffer);

        processHandle = WindowsHandle<-1> (process_information.hProcess);
        threadHandle  = WindowsHandle<-1> (process_information.hThread);
    }

    void IssueRead()
    {
        if (readIssued)
            return;

        const auto success = ReadFileEx (parentReadHandle.get(),
                                         read_buffer_.data(),
                                         read_buffer_.size(),
                                         &readOverlapped.get(),
                                         ReadCallback);

        if (! success)
            throw std::runtime_error ("ReadFileEx failed");

        readIssued = true;
    }

    bool TryIssueWrite (std::string msg)
    {
        if (writeIssued)
            return false;

        outgoingMessage = msg;

        const auto success = WriteFileEx (parentWriteHandle.get(),
                                          outgoingMessage.c_str(),
                                          outgoingMessage.size(),
                                          &writeOverlapped.get(),
                                          WriteCallback);

        if (! success)
            throw std::runtime_error ("WriteFileEx failed");

        writeIssued = true;
        return writeIssued;
    }

    ~Impl()
    {
        int resultCancelRead  = CancelIoEx (parentReadHandle.get(), &readOverlapped.get());
        int resultCancelWrite = CancelIoEx (parentWriteHandle.get(), &writeOverlapped.get());

        // TODO: ati: what?
        if (resultCancelRead != 0 || resultCancelWrite || GetLastError() != ERROR_NOT_FOUND)
        {
            readIssued = true;

            while (readIssued)
            {
                SleepEx (50, true);
            }
        }

        std::cout << "Waiting for subprocess to exit..." << std::endl;

        if (WaitForSingleObject (processHandle.get(), 1000) == WAIT_TIMEOUT)
        {
            std::cout << "Killing subprocess" << std::endl;
            TerminateProcess (processHandle.get(), 1);
        }
    }

private:
    std::function<void (const char*, size_t)> read_callback_;
    std::array<char, 1024> read_buffer_ {};
    std::string outgoingMessage;
    WindowsPipe parentReadHandle = createPipe (PipeDirection::inbound);
    WindowsHandle<-1> childWriteHandle = createOverlappedOppositeHandle (parentReadHandle);
    WindowsPipe parentWriteHandle = createPipe (PipeDirection::outbound);
    WindowsHandle<-1> childReadHandle = createOverlappedOppositeHandle (parentWriteHandle);
    WindowsHandle<-1> processHandle;
    WindowsHandle<-1> threadHandle;
    OverlappedWithPointer writeOverlapped { this };
    OverlappedWithPointer readOverlapped  { this };
    bool readIssued = false;
    bool writeIssued  = false;

    static void ReadCallback (DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped)
    {
        auto p_this = reinterpret_cast<Impl*> (lpOverlapped->hEvent);
        p_this->read_callback_ (p_this->read_buffer_.data(), dwNumberOfBytesTransfered);
        p_this->readIssued = false;
    }

    static void WriteCallback (DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped)
    {
        auto p_this = reinterpret_cast<Impl*> (lpOverlapped->hEvent);
        p_this->writeIssued = false;
    }
};

ChildProcess::ChildProcess (const std::string& pathToExe,
                            std::function<void (const char*, size_t)> read_callback)
    : impl (std::make_unique<Impl> (pathToExe, std::move (read_callback)))
{
}

ChildProcess::~ChildProcess() = default;

void ChildProcess::IssueRead()
{
    impl->IssueRead();
}

bool ChildProcess::TryIssueWrite (const std::string& msg)
{
    return impl->TryIssueWrite (msg);
}

int WindowsSleepEx (uint32_t timeoutMilliseconds, bool alertable)
{
    return SleepEx (timeoutMilliseconds, alertable);
}

} // namespace bbmp
