#include "pch.h"
#include <winrt/ComponentA.h>
#include <winrt/ComponentB.h>

using namespace std::literals;
using namespace winrt;
using namespace Windows::Foundation;

IAsyncAction RunAsync()
{
    puts("Running C++/WinRT on Windows 7");

    ComponentA::Class a;
    hstring name_a = co_await a.GetName();

    ComponentB::Class b;
    hstring name_b = co_await b.GetName();

    printf("\n%ls %ls\n", name_a.c_str(), name_b.c_str());
}

int main()
{
    init_apartment();
    RunAsync().get();
}
