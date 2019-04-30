#include "pch.h"
#include <winrt/ComponentA.h>
#include <winrt/ComponentB.h>

using namespace std::literals;
using namespace winrt;
using namespace Windows::Foundation;

IAsyncAction RunAsync()
{
    puts("\nRunning C++/WinRT on Windows 7");

    ComponentA::Class a_object;
    hstring a_name = co_await a_object.GetNameAsync();
    printf("\nLoading %ls\n", a_name.c_str());

    ComponentB::Class b_object;
    hstring b_name = co_await b_object.GetNameAsync();
    printf("\nLoading %ls\n", b_name.c_str());
}

int main()
{
    init_apartment();
    RunAsync().get();
}
