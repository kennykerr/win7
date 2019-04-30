#pragma once
#include "Class.g.h"

namespace winrt::ComponentA::implementation
{
    struct Class : ClassT<Class>
    {
        Class() = default;

        Windows::Foundation::IAsyncOperation<hstring> GetNameAsync() const;
    };
}
namespace winrt::ComponentA::factory_implementation
{
    struct Class : ClassT<Class, implementation::Class>
    {
    };
}
