#include "pch.h"
#include "Class.h"
#include "Class.g.cpp"

using namespace std::literals;

namespace winrt::ComponentB::implementation
{
    Windows::Foundation::IAsyncOperation<hstring> Class::GetNameAsync() const
    {
        co_await 1000ms;

        co_return L"ComponentB";
    }
}
