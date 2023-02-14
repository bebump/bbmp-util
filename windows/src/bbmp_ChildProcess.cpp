#include "bbmp_ChildProcess.h"

#include "bbmp_WindowsHandles.h"

#include <array>
#include <cstring>
#include <functional>
#include <iostream>
#include <mutex>
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

static auto createSecurityDescriptor (ChildInheritsHandle inherit)
{
    SECURITY_ATTRIBUTES securityAttributes;
    ZeroMemory (&securityAttributes, sizeof (securityAttributes));
    securityAttributes.nLength              = sizeof (securityAttributes);
    securityAttributes.lpSecurityDescriptor = NULL;
    securityAttributes.bInheritHandle       = inherit == ChildInheritsHandle::yes ? TRUE : FALSE;

    return securityAttributes;
}

class ChildProcess::Impl
{
public:
    Impl (const std::string& path_to_exe, std::function<void (const char*, size_t)> read_callback)
        : read_callback_ (std::move (read_callback)), read_issued_ (false)
    {
        const auto pipe_name =
            std::string ("\\\\.\\pipe\\") + std::string (std::to_string (GetCurrentProcessId()));
        const auto pipe_name2 =
            std::string ("\\\\.\\pipe\\") + std::string (std::to_string (GetCurrentProcessId()) + "2");

        parent_read_handle_ = [&]
        {
            auto securityAttribs = createSecurityDescriptor (ChildInheritsHandle::no);

            return WindowsHandle<-1> (CreateNamedPipeA (pipe_name.c_str(),
                                                        PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
                                                        0,
                                                        1,
                                                        8192,
                                                        8192,
                                                        0,
                                                        &securityAttribs));
        }();

        child_write_handle_ = [&]
        {
            auto securityAttribs = createSecurityDescriptor (ChildInheritsHandle::yes);

            return WindowsHandle<-1> (CreateFileA (pipe_name.c_str(),
                                                   GENERIC_WRITE,
                                                   0,
                                                   &securityAttribs,
                                                   OPEN_EXISTING,
                                                   FILE_FLAG_OVERLAPPED,
                                                   NULL));
        }();

        parent_write_handle_ = [&]
        {
            auto securityAttribs = createSecurityDescriptor (ChildInheritsHandle::no);

            return WindowsHandle<-1> (CreateNamedPipeA (pipe_name2.c_str(),
                                                        PIPE_ACCESS_OUTBOUND | FILE_FLAG_OVERLAPPED,
                                                        0,
                                                        1,
                                                        8192,
                                                        8192,
                                                        0,
                                                        &securityAttribs));
        }();

        child_read_handle_ = [&]
        {
            auto securityAttribs = createSecurityDescriptor (ChildInheritsHandle::yes);

            return WindowsHandle<-1> (CreateFileA (pipe_name2.c_str(),
                                                   GENERIC_READ,
                                                   0,
                                                   &securityAttribs,
                                                   OPEN_EXISTING,
                                                   FILE_FLAG_OVERLAPPED,
                                                   NULL));
        }();

        //****************
        // Read handle for the parent process
        //        parent_write_handle_ =
        //            WindowsHandle<-1> (CreateNamedPipeA (pipe_name.c_str(),
        //                                                 PIPE_ACCESS_OUTBOUND | FILE_FLAG_OVERLAPPED,
        //                                                 0,
        //                                                 1,
        //                                                 8192,
        //                                                 8192,
        //                                                 0,
        //                                                 &security_attributes));

        // Event handle for ConnectNamedPipe
        ZeroMemory (&overlapped_, sizeof (overlapped_));
        overlapped_.hEvent = CreateEvent (NULL, TRUE, FALSE, NULL);
        if (overlapped_.hEvent == NULL)
        {
            throw std::runtime_error ("CreateEvent failed");
        }

        ScopeGuard overlapped_hEvent_guard;
        overlapped_hEvent_guard.add ([this]() { CloseHandle (overlapped_.hEvent); });

        // Connect to the pipe
        BOOL success = ConnectNamedPipe (parent_read_handle_.Get(), &overlapped_);
        if (! success)
        {
            if (GetLastError() == ERROR_IO_PENDING)
            {
                if (WaitForSingleObject (overlapped_.hEvent, INFINITE) == WAIT_FAILED)
                {
                    CloseHandle (overlapped_.hEvent);
                    throw std::runtime_error ("failed WSFO for ConnectNamedPipe");
                }
            }
            else if (GetLastError() != ERROR_PIPE_CONNECTED)
            {
                CloseHandle (overlapped_.hEvent);
                throw std::runtime_error ("failed to connect to named pipe");
            }
        }
        CloseHandle (overlapped_.hEvent);
        overlapped_hEvent_guard.cancelAll();

        // Spawn the new process
        PROCESS_INFORMATION process_information;
        STARTUPINFO startupinfo;
        ZeroMemory (&process_information, sizeof (PROCESS_INFORMATION));
        ZeroMemory (&startupinfo, sizeof (STARTUPINFO));
        startupinfo.cb = sizeof (STARTUPINFO);

        startupinfo.hStdInput  = child_read_handle_.Get();
        startupinfo.hStdError  = child_write_handle_.Get();
        startupinfo.hStdOutput = child_write_handle_.Get();
        startupinfo.dwFlags    = STARTF_USESTDHANDLES;

        const int command_buffer_size = 2048;
        char command_buffer[command_buffer_size];
        strncpy (command_buffer, path_to_exe.c_str(), command_buffer_size);
        command_buffer[command_buffer_size - 1] = '\n';

        success = CreateProcess (NULL,
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
        {
            throw std::runtime_error (std::string ("CreateProcess failed with: ") + command_buffer);
        }

        process_handle_ = WindowsHandle<-1> (process_information.hProcess);
        thread_handle_  = WindowsHandle<-1> (process_information.hThread);

        // We reuse the overlapped_ structure for read() calls
        ZeroMemory (&overlapped_, sizeof (decltype (overlapped_)));
        overlapped_.hEvent = reinterpret_cast<HANDLE> (this);
    }

    void IssueRead()
    {
        auto lock = std::lock_guard (read_mutex_);
        if (read_issued_)
        {
            return;
        }
        auto success = ReadFileEx (
            parent_read_handle_.Get(), read_buffer_.data(), read_buffer_.size(), &overlapped_, ReadCallback);
        if (! success)
        {
            throw std::runtime_error ("ReadFileEx failed");
        }

        read_issued_ = true;
    }

    void Write (const std::string& msg)
    {
    }

    ~Impl()
    {
        int result = CancelIoEx (parent_read_handle_.Get(), &overlapped_);
        if (result != 0 || GetLastError() != ERROR_NOT_FOUND)
        {
            read_issued_ = true;
            while (read_issued_)
            {
                SleepEx (50, true);
            }
        }
    }

private:
    std::function<void (const char*, size_t)> read_callback_;
    std::array<char, 1024> read_buffer_ {};
    OVERLAPPED overlapped_;
    WindowsHandle<-1> parent_read_handle_;
    WindowsHandle<-1> child_write_handle_;
    WindowsHandle<-1> parent_write_handle_;
    WindowsHandle<-1> child_read_handle_;
    WindowsHandle<-1> process_handle_;
    WindowsHandle<-1> thread_handle_;
    bool read_issued_;
    std::mutex read_mutex_;

    static void ReadCallback (DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped)
    {
        auto p_this = reinterpret_cast<Impl*> (lpOverlapped->hEvent);
        auto lock   = std::lock_guard (p_this->read_mutex_);
        p_this->read_callback_ (p_this->read_buffer_.data(), dwNumberOfBytesTransfered);
        p_this->read_issued_ = false;
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

void ChildProcess::Write (const std::string& msg)
{
    impl->Write (msg);
}

int WindowsSleepEx (uint32_t timeoutMilliseconds, bool alertable)
{
    return SleepEx (timeoutMilliseconds, alertable);
}

} // namespace bbmp
