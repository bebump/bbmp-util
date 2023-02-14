#pragma once

#define NOMINMAX
#define NOGDI

#include <strsafe.h>
#include <windows.h>

#undef NOMINMAX
#undef NOGDI

#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

std::string GetLastErrorAsString();

/* The (sad) reason for the templated approach:

   Some Win32 API functions return INVALID_HANDLE_VALUE (-1) to signal a failed attempt. I actually
   have seen 0 returned by them as a valid, working handle.

   Others return NULL.
*/
template <int V_INVALID>
class WindowsHandle final
{
public:
    WindowsHandle() = default;

    WindowsHandle (HANDLE raw_handle) : handle (raw_handle)
    {
        throwIfHandleIsInvalid();
    }

    WindowsHandle (const WindowsHandle<V_INVALID>& other) = delete;

    WindowsHandle (WindowsHandle<V_INVALID>&& other)
    {
        if (handle != reinterpret_cast<HANDLE> (V_INVALID))
            if (CloseHandle (handle) == TRUE)
                throw std::runtime_error ("Failed to close WindowsHandle");

        handle       = other.get();
        other.handle = reinterpret_cast<HANDLE> (V_INVALID);
    }

    WindowsHandle& operator= (WindowsHandle<V_INVALID>&& other)
    {
        if (handle != reinterpret_cast<HANDLE> (V_INVALID))
            if(! CloseHandle (handle))
                throw std::runtime_error ("Failed to close WindowsHandle");

        handle       = other.get();
        other.handle = reinterpret_cast<HANDLE> (V_INVALID);
        return *this;
    }

    ~WindowsHandle() noexcept
    {
        // The handle can become invalid after a move operation
        if (handle != reinterpret_cast<HANDLE> (V_INVALID))
            const auto success = CloseHandle (handle);
    }

    const HANDLE& get() const
    {
        throwIfHandleIsInvalid();
        return handle;
    }

private:
    HANDLE handle = reinterpret_cast<HANDLE> (V_INVALID);

    void throwIfHandleIsInvalid() const
    {
        if (handle == reinterpret_cast<HANDLE> (V_INVALID))
        {
            throw std::runtime_error ("Invalid handle encountered in WindowsHandle");
        }
    }
};

class ScopeGuard final
{
public:
    void add (std::function<void()>&& deferred_function) noexcept
    {
        deferredFunctions.push_back (std::move (deferred_function));
    }

    void cancelAll() noexcept
    {
        deferredFunctions.clear();
    }

    ~ScopeGuard() noexcept
    {
        try
        {
            for (auto& f : deferredFunctions)
            {
                f();
            }
        }
        catch (...)
        {
        }
    }

private:
    std::vector<std::function<void()>> deferredFunctions;
};
