#pragma once
#include "Class.g.h"

namespace winrt::ComponentB::implementation
{
    struct Class : ClassT<Class>
    {
        Class() = default;

        Windows::Foundation::IAsyncOperation<hstring> GetName() const;
    };
}
namespace winrt::ComponentB::factory_implementation
{
    struct Class : ClassT<Class, implementation::Class>
    {
    };
}
