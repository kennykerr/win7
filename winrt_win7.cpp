#include <restrictederrorinfo.h>
#include <winrt/base.h>
#include <filesystem>

// Note: this implementation intentionally avoids using any C++ standard library features like new/malloc/atomic
// to ensure that the runtime behavior is ABI stable.

namespace
{
    enum class hstring_flags
    {
        none,
        reference,
        preallocated,
    };

    DEFINE_ENUM_FLAG_OPERATORS(hstring_flags)

    struct hstring_reference
    {
        hstring_flags flags;
        uint32_t size;
        uint32_t padding1;
        uint32_t padding2;
        wchar_t const* value;
    };

    struct hstring_handle
    {
        hstring_flags flags{};
        uint32_t size{};
        uint32_t m_references{ 1 };

        void add_ref() noexcept
        {
            InterlockedIncrement(&m_references);
        }

        void release() noexcept
        {
            if (0 == InterlockedDecrement(&m_references))
            {
                VirtualFree(this, 0, MEM_RELEASE);
            }
        }

        static auto create(wchar_t const* value, uint32_t size)
        {
            auto handle = static_cast<hstring_handle*>(VirtualAlloc(
                nullptr,
                sizeof(hstring_handle) + (size + 1) * sizeof(wchar_t),
                MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));

            if (handle)
            {
                new (handle) hstring_handle();
                handle->size = size;
                memcpy(handle + 1, value, (size + 1) * sizeof(wchar_t));
            }

            return handle;
        }
    };

    struct __declspec(uuid("1CF2B120-547D-101B-8E65-08002B2BD119")) ILegacyErrorInfo : ::IUnknown
    {
        virtual HRESULT WINRT_CALL GetGUID(GUID* value) noexcept = 0;
        virtual HRESULT WINRT_CALL GetSource(BSTR* value) noexcept = 0;
        virtual HRESULT WINRT_CALL GetDescription(BSTR* value) noexcept = 0;
        virtual HRESULT WINRT_CALL GetHelpFile(BSTR* value) noexcept = 0;
        virtual HRESULT WINRT_CALL GetHelpContext(DWORD* value) noexcept = 0;
    };

    struct error_object : winrt::implements<error_object, ILegacyErrorInfo, IRestrictedErrorInfo>
    {
        error_object(HRESULT code, std::wstring_view const& message) :
            m_code(code),
            m_message(SysAllocString(message.data()))
        {
        }

        HRESULT WINRT_CALL GetGUID(GUID* value) noexcept final
        {
            *value = {};
            return S_OK;
        }

        HRESULT WINRT_CALL GetSource(BSTR* value) noexcept final
        {
            *value = nullptr;
            return S_OK;
        }

        HRESULT WINRT_CALL GetDescription(BSTR* value) noexcept final
        {
            *value = duplicate_bstr(m_message.get());
            return S_OK;

        }

        HRESULT WINRT_CALL GetHelpFile(BSTR* value) noexcept final
        {
            *value = nullptr;
            return S_OK;

        }

        HRESULT WINRT_CALL GetHelpContext(DWORD* value) noexcept final
        {
            *value = 0;
            return S_OK;
        }

        HRESULT WINRT_CALL GetErrorDetails(BSTR* description, HRESULT* error, BSTR* restricted, BSTR* capabilitySid) noexcept final
        {
            *description = nullptr;
            *error = m_code;
            *restricted = duplicate_bstr(m_message.get());
            *capabilitySid = nullptr;
            return S_OK;
        }

        HRESULT WINRT_CALL GetReference(BSTR* value) noexcept final
        {
            *value = nullptr;
            return S_OK;
        }

    private:

        BSTR duplicate_bstr(BSTR value) const noexcept
        {
            return SysAllocStringByteLen(reinterpret_cast<char*>(value), SysStringByteLen(value));
        }

        struct bstr_traits
        {
            using type = BSTR;

            static void close(type value) noexcept
            {
                SysFreeString(value);
            }

            static constexpr type invalid() noexcept
            {
                return nullptr;
            }
        };

        using bstr_handle = winrt::handle_type<bstr_traits>;

        HRESULT m_code{};
        bstr_handle m_message;
    };

    hstring_reference* get_hstring_reference(void* string) noexcept
    {
        auto reference = static_cast<hstring_reference*>(string);

        if ((reference->flags & hstring_flags::reference) == hstring_flags::reference)
        {
            return reference;
        }

        return nullptr;
    }

    bool starts_with(std::wstring_view const& value, std::wstring_view const& match) noexcept
    {
        return 0 == value.compare(0, match.size(), match);
    }

    std::filesystem::path get_module_path()
    {
        std::wstring path(100, L'?');
        DWORD actual_size{};

        while (true)
        {
            actual_size = GetModuleFileNameW(nullptr, path.data(), 1 + static_cast<uint32_t>(path.size()));

            if (actual_size < 1 + path.size())
            {
                path.resize(actual_size);
                break;
            }
            else
            {
                path.resize(path.size() * 2, L'?');
            }
        }

        return path;
    }
}

extern "C"
{
    int32_t WINRT_CALL WINRT_GetErrorInfo(ULONG, ILegacyErrorInfo** info) noexcept;
    int32_t WINRT_CALL WINRT_SetErrorInfo(ULONG, ILegacyErrorInfo* info) noexcept;
    HMODULE WINRT_CALL WINRT_LoadLibraryW(LPCWSTR name) noexcept;
}

WINRT_LINK(GetErrorInfo, 8)
WINRT_LINK(SetErrorInfo, 8)
WINRT_LINK(LoadLibraryW, 4)

int32_t WINRT_CALL WINRT_GetRestrictedErrorInfo(void** info) noexcept
{
    winrt::com_ptr<ILegacyErrorInfo> error_info;
    WINRT_GetErrorInfo(0, error_info.put());

    *info = error_info.try_as<IRestrictedErrorInfo>().detach();
    return *info ? S_OK : S_FALSE;
}

int32_t WINRT_CALL WINRT_RoOriginateLanguageException(int32_t error, void* hstring, void*) noexcept
{
    uint32_t length{};
    wchar_t const* const buffer = WINRT_WindowsGetStringRawBuffer(hstring, &length);
    std::wstring_view const message{ buffer, length };

    return S_OK == WINRT_SetErrorInfo(0, winrt::make<error_object>(error, message).get());
}

int32_t WINRT_CALL WINRT_SetRestrictedErrorInfo(void* info) noexcept
{
    winrt::com_ptr<ILegacyErrorInfo> error_info;
    HRESULT const hr = static_cast<IRestrictedErrorInfo*>(info)->QueryInterface(IID_PPV_ARGS(&error_info));

    if (hr != S_OK)
    {
        return hr;
    }

    return WINRT_SetErrorInfo(0, error_info.get());
}

int32_t WINRT_CALL WINRT_RoGetActivationFactory(void* classId, winrt::guid const& iid, void** factory) noexcept
{
    *factory = nullptr;

    uint32_t length{};
    wchar_t const* const buffer = WINRT_WindowsGetStringRawBuffer(classId, &length);
    std::wstring_view const name{ buffer, length };

    std::size_t const position = name.rfind('.');

    if (std::wstring_view::npos == position)
    {
        return winrt::hresult_invalid_argument(L"classId").to_abi();
    }

    auto library_name = get_module_path();
    library_name.remove_filename();
    library_name /= name.substr(0, position);
    library_name += L".dll";
    HMODULE library = WINRT_LoadLibraryW(library_name.c_str());

    if (!library)
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    using DllGetActivationFactory = HRESULT __stdcall(void* classId, void** factory);
    auto call = reinterpret_cast<DllGetActivationFactory*>(GetProcAddress(library, "DllGetActivationFactory"));

    if (!call)
    {
        HRESULT const hr = HRESULT_FROM_WIN32(GetLastError());
        WINRT_VERIFY(FreeLibrary(library));
        return hr;
    }

    winrt::com_ptr<winrt::Windows::Foundation::IActivationFactory> activation_factory;
    HRESULT const hr = call(classId, activation_factory.put_void());

    if (hr != S_OK)
    {
        WINRT_VERIFY(FreeLibrary(library));
        return hr;
    }

    if (iid != winrt::guid_of<winrt::Windows::Foundation::IActivationFactory>())
    {
        return activation_factory.as(iid, factory);
    }

    *factory = activation_factory.detach();
    return S_OK;
}

int32_t WINRT_CALL WINRT_RoInitialize(uint32_t const type) noexcept
{
    return CoInitializeEx(0, type == 0 ? COINIT_APARTMENTTHREADED : COINIT_MULTITHREADED);
}

void WINRT_CALL WINRT_RoUninitialize() noexcept
{
    CoUninitialize();
}

int32_t WINRT_CALL WINRT_CoIncrementMTAUsage(void** cookie) noexcept
{
    *cookie = nullptr;
    return E_NOTIMPL;
}

int32_t WINRT_CALL WINRT_RoGetAgileReference(uint32_t options, winrt::guid const& iid, void* object, void** reference) noexcept
{
    *reference = nullptr;
    return E_NOTIMPL;
}

int32_t WINRT_CALL WINRT_WindowsCreateString(wchar_t const* const value, uint32_t const size, void** result) noexcept
{
    if (size == 0)
    {
        *result = nullptr;
        return S_OK;
    }

    *result = hstring_handle::create(value, size);
    return *result ? S_OK : E_OUTOFMEMORY;
}

int32_t WINRT_CALL WINRT_WindowsDuplicateString(void* value, void** result) noexcept
{
    if (!value)
    {
        *result = nullptr;
        return S_OK;
    }

    if (auto reference = get_hstring_reference(value))
    {
        return WINRT_WindowsCreateString(reference->value, reference->size, result);
    }

    *result = value;
    static_cast<hstring_handle*>(value)->add_ref();
    return S_OK;
}

int32_t WINRT_CALL WINRT_WindowsCreateStringReference(wchar_t const* const value, uint32_t const size, void* header, void** result) noexcept
{
    if (size == 0)
    {
        *result = nullptr;
        return S_OK;
    }

    auto reference = static_cast<hstring_reference*>(header);
    reference->flags = hstring_flags::reference;
    reference->size = size;
    reference->value = value;
    *result = reference;
    return S_OK;
}

int32_t WINRT_CALL WINRT_WindowsDeleteString(void* string) noexcept
{
    if (string)
    {
        static_cast<hstring_handle*>(string)->release();
    }

    return S_OK;
}

wchar_t const* WINRT_CALL WINRT_WindowsGetStringRawBuffer(void* string, uint32_t* size) noexcept
{
    if (!string)
    {
        if (size)
        {
            *size = 0;
        }

        return L"";
    }

    auto handle = static_cast<hstring_handle*>(string);

    if (size)
    {
        *size = handle->size;
    }

    if (auto reference = get_hstring_reference(string))
    {
        return reference->value;
    }

    return reinterpret_cast<wchar_t const*>(handle + 1);
}

int32_t WINRT_CALL WINRT_WindowsPreallocateStringBuffer(uint32_t length, wchar_t** charBuffer, void** bufferHandle) noexcept
{
    *charBuffer = nullptr;
    *bufferHandle = nullptr;

    return S_OK;
}

int32_t WINRT_CALL WINRT_WindowsDeleteStringBuffer(void* bufferHandle) noexcept
{
    return S_OK;
}

int32_t WINRT_CALL WINRT_WindowsPromoteStringBuffer(void* bufferHandle, void** string) noexcept
{
    return S_OK;

}

