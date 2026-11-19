#include "wmi_subscription.hpp"
#include <windows.h>
#include <wbemidl.h>
#include <comdef.h>

#pragma comment(lib, "wbemuuid.lib")

namespace l4g::persistence {

HRESULT WMISubscription::connect_wmi(IWbemServices** ppSvc) {
    HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hr)) return hr;
    
    hr = CoInitializeSecurity(nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE, nullptr);
    if (FAILED(hr)) { CoUninitialize(); return hr; }
    
    IWbemLocator* pLoc = nullptr;
    hr = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID*)&pLoc);
    if (FAILED(hr)) { CoUninitialize(); return hr; }
    
    hr = pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr, 0, nullptr, 0, 0, ppSvc);
    pLoc->Release();
    
    if (SUCCEEDED(hr)) {
        hr = CoSetProxyBlanket(*ppSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
    }
    
    return hr;
}

WMISubscriptionResult WMISubscription::create(const std::wstring& filter_name, const std::wstring& consumer_name,
                                              const std::wstring& query, const std::wstring& command) {
    WMISubscriptionResult result;
    
    IWbemServices* pSvc = nullptr;
    HRESULT hr = connect_wmi(&pSvc);
    if (FAILED(hr)) { result.error = "WMI connect failed"; return result; }
    
    // Create Event Filter
    IWbemClassObject* pFilterClass = nullptr;
    hr = pSvc->GetObject(_bstr_t(L"__EventFilter"), 0, nullptr, &pFilterClass, nullptr);
    if (SUCCEEDED(hr)) {
        IWbemClassObject* pFilter = nullptr;
        hr = pFilterClass->SpawnInstance(0, &pFilter);
        if (SUCCEEDED(hr)) {
            VARIANT v;
            VariantInit(&v);
            v.vt = VT_BSTR; v.bstrVal = _bstr_t(filter_name.c_str());
            pFilter->Put(L"Name", 0, &v, 0);
            
            v.bstrVal = _bstr_t(query.c_str());
            pFilter->Put(L"Query", 0, &v, 0);
            
            v.bstrVal = _bstr_t(L"WQL");
            pFilter->Put(L"QueryLanguage", 0, &v, 0);
            
            pSvc->PutInstance(pFilter, WBEM_FLAG_CREATE_ONLY, nullptr, nullptr);
            pFilter->Release();
        }
        pFilterClass->Release();
    }
    
    // Create Event Consumer
    IWbemClassObject* pConsumerClass = nullptr;
    hr = pSvc->GetObject(_bstr_t(L"CommandLineEventConsumer"), 0, nullptr, &pConsumerClass, nullptr);
    if (SUCCEEDED(hr)) {
        IWbemClassObject* pConsumer = nullptr;
        hr = pConsumerClass->SpawnInstance(0, &pConsumer);
        if (SUCCEEDED(hr)) {
            VARIANT v;
            VariantInit(&v);
            v.vt = VT_BSTR; v.bstrVal = _bstr_t(consumer_name.c_str());
            pConsumer->Put(L"Name", 0, &v, 0);
            
            v.bstrVal = _bstr_t(command.c_str());
            pConsumer->Put(L"CommandLineTemplate", 0, &v, 0);
            
            pSvc->PutInstance(pConsumer, WBEM_FLAG_CREATE_ONLY, nullptr, nullptr);
            pConsumer->Release();
        }
        pConsumerClass->Release();
    }
    
    // Create Binding
    IWbemClassObject* pBindingClass = nullptr;
    hr = pSvc->GetObject(_bstr_t(L"__FilterToConsumerBinding"), 0, nullptr, &pBindingClass, nullptr);
    if (SUCCEEDED(hr)) {
        IWbemClassObject* pBinding = nullptr;
        hr = pBindingClass->SpawnInstance(0, &pBinding);
        if (SUCCEEDED(hr)) {
            VARIANT v;
            VariantInit(&v);
            v.vt = VT_BSTR; v.bstrVal = _bstr_t((L"__EventFilter.Name=\"" + filter_name + L"\"").c_str());
            pBinding->Put(L"Filter", 0, &v, 0);
            
            v.bstrVal = _bstr_t((L"CommandLineEventConsumer.Name=\"" + consumer_name + L"\"").c_str());
            pBinding->Put(L"Consumer", 0, &v, 0);
            
            pSvc->PutInstance(pBinding, WBEM_FLAG_CREATE_ONLY, nullptr, nullptr);
            pBinding->Release();
        }
        pBindingClass->Release();
    }
    
    pSvc->Release();
    CoUninitialize();
    
    result.success = true;
    return result;
}

bool WMISubscription::remove(const std::wstring& filter_name, const std::wstring& consumer_name) {
    IWbemServices* pSvc = nullptr;
    HRESULT hr = connect_wmi(&pSvc);
    if (FAILED(hr)) return false;
    
    // Delete binding
    std::wstring binding_query = L"SELECT * FROM __FilterToConsumerBinding WHERE Filter=\"__EventFilter.Name=\\\"" + filter_name + L"\\\"\" AND Consumer=\"CommandLineEventConsumer.Name=\\\"" + consumer_name + L"\\\"\"";
    IEnumWbemClassObject* pEnum = nullptr;
    hr = pSvc->ExecQuery(_bstr_t(L"WQL"), _bstr_t(binding_query.c_str()), WBEM_FLAG_FORWARD_ONLY, nullptr, &pEnum);
    if (SUCCEEDED(hr)) {
        IWbemClassObject* pObj = nullptr;
        ULONG returned = 0;
        while (pEnum->Next(WBEM_INFINITE, 1, &pObj, &returned) == WBEM_S_NO_ERROR) {
            pSvc->DeleteInstance(_bstr_t(L"__FilterToConsumerBinding"), WBEM_FLAG_FORWARD_ONLY, pObj, nullptr, nullptr);
            pObj->Release();
        }
        pEnum->Release();
    }
    
    // Delete filter
    std::wstring filter_query = L"SELECT * FROM __EventFilter WHERE Name=\"" + filter_name + L"\"";
    hr = pSvc->ExecQuery(_bstr_t(L"WQL"), _bstr_t(filter_query.c_str()), WBEM_FLAG_FORWARD_ONLY, nullptr, &pEnum);
    if (SUCCEEDED(hr)) {
        IWbemClassObject* pObj = nullptr;
        ULONG returned = 0;
        while (pEnum->Next(WBEM_INFINITE, 1, &pObj, &returned) == WBEM_S_NO_ERROR) {
            pSvc->DeleteInstance(_bstr_t(L"__EventFilter"), WBEM_FLAG_FORWARD_ONLY, pObj, nullptr, nullptr);
            pObj->Release();
        }
        pEnum->Release();
    }
    
    // Delete consumer
    std::wstring consumer_query = L"SELECT * FROM CommandLineEventConsumer WHERE Name=\"" + consumer_name + L"\"";
    hr = pSvc->ExecQuery(_bstr_t(L"WQL"), _bstr_t(consumer_query.c_str()), WBEM_FLAG_FORWARD_ONLY, nullptr, &pEnum);
    if (SUCCEEDED(hr)) {
        IWbemClassObject* pObj = nullptr;
        ULONG returned = 0;
        while (pEnum->Next(WBEM_INFINITE, 1, &pObj, &returned) == WBEM_S_NO_ERROR) {
            pSvc->DeleteInstance(_bstr_t(L"CommandLineEventConsumer"), WBEM_FLAG_FORWARD_ONLY, pObj, nullptr, nullptr);
            pObj->Release();
        }
        pEnum->Release();
    }
    
    pSvc->Release();
    CoUninitialize();
    return true;
}

bool WMISubscription::exists(const std::wstring& filter_name) {
    IWbemServices* pSvc = nullptr;
    HRESULT hr = connect_wmi(&pSvc);
    if (FAILED(hr)) return false;
    
    std::wstring query = L"SELECT * FROM __EventFilter WHERE Name=\"" + filter_name + L"\"";
    IEnumWbemClassObject* pEnum = nullptr;
    hr = pSvc->ExecQuery(_bstr_t(L"WQL"), _bstr_t(query.c_str()), WBEM_FLAG_FORWARD_ONLY, nullptr, &pEnum);
    bool found = false;
    if (SUCCEEDED(hr)) {
        IWbemClassObject* pObj = nullptr;
        ULONG returned = 0;
        if (pEnum->Next(WBEM_INFINITE, 1, &pObj, &returned) == WBEM_S_NO_ERROR) {
            found = true;
            pObj->Release();
        }
        pEnum->Release();
    }
    
    pSvc->Release();
    CoUninitialize();
    return found;
}

WMISubscriptionResult WMISubscription::create_process_monitor(const std::wstring& target_process, const std::wstring& payload) {
    std::wstring query = L"SELECT * FROM __InstanceCreationEvent WITHIN 1 WHERE TargetInstance ISA 'Win32_Process' AND TargetInstance.Name = '" + target_process + L"'";
    return create(L"L4G_ProcessFilter", L"L4G_ProcessConsumer", query, payload);
}

WMISubscriptionResult WMISubscription::create_logon_trigger(const std::wstring& payload) {
    std::wstring query = L"SELECT * FROM __InstanceCreationEvent WITHIN 5 WHERE TargetInstance ISA 'Win32_LogonSession'";
    return create(L"L4G_LogonFilter", L"L4G_LogonConsumer", query, payload);
}

WMISubscriptionResult WMISubscription::create_timer_trigger(const std::wstring& payload, int interval_seconds) {
    std::wstring query = L"SELECT * FROM __TimerEvent WHERE TimerID=\"L4G_Timer\"";
    // Note: __TimerEvent requires creating a timer first
    return create(L"L4G_TimerFilter", L"L4G_TimerConsumer", query, payload);
}

} // namespace l4g::persistence