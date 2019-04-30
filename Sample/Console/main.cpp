#include "pch.h"

using namespace std::literals;
using namespace winrt;
using namespace Windows::Foundation;

IAsyncAction RunAsync()
{
    co_return;
}

int main()
{
    RunAsync().get();
}
