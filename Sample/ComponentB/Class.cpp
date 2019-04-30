#include "pch.h"
#include "Class.h"
#include "Class.g.cpp"

namespace winrt::ComponentB::implementation
{
    Windows::Foundation::IAsyncOperation<hstring> Class::GetName() const
    {
        co_return L"ComponentB";
    }
}
