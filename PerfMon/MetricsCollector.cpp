#include "stdafx.h"
#include "MetricsCollector.h"

#include <dxgi.h>

#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "dxgi.lib")

namespace
{
    ULONGLONG FileTimeToU64(const FILETIME& ft)
    {
        return (((ULONGLONG)ft.dwHighDateTime) << 32) | (ULONGLONG)ft.dwLowDateTime;
    }

    double ClampPercent(double v)
    {
        if (v < 0.0)   v = 0.0;
        if (v > 100.0) v = 100.0;
        return v;
    }
}

MetricsSnapshot::MetricsSnapshot()
    : cpuPercent(0.0)
    , ramUsedBytes(0.0)
    , ramTotalBytes(0.0)
    , gpuValid(false)
    , gpuPercent(0.0)
    , vramValid(false)
    , vramUsedBytes(0)
    , vramTotalBytes(0)
    , fpsValid(false)
    , fps(0.0)
{
}

CMetricsCollector::CMetricsCollector()
    : m_prevIdle(0)
    , m_prevKernel(0)
    , m_prevUser(0)
    , m_cpuReady(false)
    , m_cpuPercent(0.0)
    , m_query(nullptr)
    , m_gpuCounter(nullptr)
    , m_vramCounter(nullptr)
    , m_pdhReady(false)
    , m_firstSample(true)
    , m_gpuPercent(0.0)
    , m_gpuValid(false)
    , m_vramTotal(0)
{
}

CMetricsCollector::~CMetricsCollector()
{
    Shutdown();
}

void CMetricsCollector::Initialize()
{
    m_prevIdle = m_prevKernel = m_prevUser = 0;
    m_cpuReady = false;

    FILETIME idle, kernel, user;
    if (GetSystemTimes(&idle, &kernel, &user))
    {
        m_prevIdle = FileTimeToU64(idle);
        m_prevKernel = FileTimeToU64(kernel);
        m_prevUser = FileTimeToU64(user);
        m_cpuReady = true;
    }

    if (PdhOpenQueryW(nullptr, 0, &m_query) == ERROR_SUCCESS)
    {
        PDH_STATUS stGpu = PdhAddEnglishCounterW(
            m_query, L"\\GPU Engine(*)\\Utilization Percentage", 0, &m_gpuCounter);
        if (stGpu != ERROR_SUCCESS)
        {
            m_gpuCounter = nullptr;
        }

        PDH_STATUS stVram = PdhAddEnglishCounterW(
            m_query, L"\\GPU Adapter Memory(*)\\Dedicated Usage", 0, &m_vramCounter);
        if (stVram != ERROR_SUCCESS)
        {
            m_vramCounter = nullptr;
        }

        m_pdhReady = (m_gpuCounter != nullptr) || (m_vramCounter != nullptr);
        m_firstSample = false;

        // Prime the counters so the first sample one second later is valid.
        if (m_pdhReady)
        {
            PdhCollectQueryData(m_query);
        }
    }

    QueryVramTotal();
}

void CMetricsCollector::Shutdown()
{
    if (m_query != nullptr)
    {
        PdhCloseQuery(m_query);
        m_query = nullptr;
    }
    m_gpuCounter = nullptr;
    m_vramCounter = nullptr;
    m_pdhReady = false;
}

MetricsSnapshot CMetricsCollector::Collect()
{
    MetricsSnapshot s;
    CollectCpu(s);
    CollectRam(s);

    if (m_pdhReady)
    {
        PdhCollectQueryData(m_query);
    }
    CollectGpu(s);
    CollectVram(s);

    m_firstSample = false;
    return s;
}

void CMetricsCollector::CollectCpu(MetricsSnapshot& s)
{
    FILETIME idle, kernel, user;
    if (!GetSystemTimes(&idle, &kernel, &user))
    {
        return;
    }

    ULONGLONG i = FileTimeToU64(idle);
    ULONGLONG k = FileTimeToU64(kernel);
    ULONGLONG u = FileTimeToU64(user);

    if (m_cpuReady)
    {
        ULONGLONG dIdle = i - m_prevIdle;
        ULONGLONG dKernel = k - m_prevKernel;
        ULONGLONG dUser = u - m_prevUser;

        // Kernel time already includes idle time.
        ULONGLONG total = dKernel + dUser;
        if (total > 0)
        {
            ULONGLONG busy = (total > dIdle) ? (total - dIdle) : 0;
            m_cpuPercent = ClampPercent(100.0 * (double)busy / (double)total);
        }
    }
    else
    {
        m_cpuReady = true;
        m_cpuPercent = 0.0;
    }

    m_prevIdle = i;
    m_prevKernel = k;
    m_prevUser = u;

    s.cpuPercent = m_cpuPercent;
}

void CMetricsCollector::CollectRam(MetricsSnapshot& s)
{
    MEMORYSTATUSEX ms;
    ZeroMemory(&ms, sizeof(ms));
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms))
    {
        s.ramTotalBytes = (double)ms.ullTotalPhys;
        s.ramUsedBytes = (double)(ms.ullTotalPhys - ms.ullAvailPhys);
    }
}

void CMetricsCollector::CollectGpu(MetricsSnapshot& s)
{
    if (m_gpuCounter == nullptr || m_firstSample)
    {
        return;
    }

    DWORD size = 0;
    DWORD count = 0;
    PDH_STATUS st = PdhGetFormattedCounterArrayW(
        m_gpuCounter, PDH_FMT_DOUBLE, &size, &count, nullptr);

    if (st != PDH_MORE_DATA || size == 0 || count == 0)
    {
        return;
    }

    std::vector<BYTE> buffer(size);
    PPDH_FMT_COUNTERVALUE_ITEM_W items = (PPDH_FMT_COUNTERVALUE_ITEM_W)buffer.data();
    st = PdhGetFormattedCounterArrayW(m_gpuCounter, PDH_FMT_DOUBLE, &size, &count, items);
    if (st != ERROR_SUCCESS)
    {
        return;
    }

    double sum3d = 0.0;
    double sumAll = 0.0;
    bool   has3d = false;

    for (DWORD i = 0; i < count; ++i)
    {
        DWORD status = items[i].FmtValue.CStatus;
        if (status != PDH_CSTATUS_VALID_DATA && status != PDH_CSTATUS_NEW_DATA)
        {
            continue;
        }

        double value = items[i].FmtValue.doubleValue;
        if (value < 0.0)
        {
            continue;
        }

        sumAll += value;

        CStringW name(items[i].szName);
        if (name.Find(L"engtype_3D") >= 0)
        {
            sum3d += value;
            has3d = true;
        }
    }

    double result = has3d ? sum3d : sumAll;
    m_gpuPercent = ClampPercent(result);
    m_gpuValid = true;

    s.gpuValid = m_gpuValid;
    s.gpuPercent = m_gpuPercent;
}

void CMetricsCollector::CollectVram(MetricsSnapshot& s)
{
    if (m_vramCounter == nullptr || m_firstSample)
    {
        return;
    }

    DWORD size = 0;
    DWORD count = 0;
    PDH_STATUS st = PdhGetFormattedCounterArrayW(
        m_vramCounter, PDH_FMT_LARGE, &size, &count, nullptr);

    if (st != PDH_MORE_DATA || size == 0 || count == 0)
    {
        return;
    }

    std::vector<BYTE> buffer(size);
    PPDH_FMT_COUNTERVALUE_ITEM_W items = (PPDH_FMT_COUNTERVALUE_ITEM_W)buffer.data();
    st = PdhGetFormattedCounterArrayW(m_vramCounter, PDH_FMT_LARGE, &size, &count, items);
    if (st != ERROR_SUCCESS)
    {
        return;
    }

    unsigned long long used = 0;
    for (DWORD i = 0; i < count; ++i)
    {
        DWORD status = items[i].FmtValue.CStatus;
        if (status != PDH_CSTATUS_VALID_DATA && status != PDH_CSTATUS_NEW_DATA)
        {
            continue;
        }

        LONGLONG value = items[i].FmtValue.largeValue;
        if (value > 0)
        {
            used += (unsigned long long)value;
        }
    }

    s.vramValid = true;
    s.vramUsedBytes = used;
    s.vramTotalBytes = m_vramTotal;
}

void CMetricsCollector::QueryVramTotal()
{
    m_vramTotal = 0;

    IDXGIFactory1* factory = nullptr;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&factory)) || factory == nullptr)
    {
        return;
    }

    IDXGIAdapter1* adapter = nullptr;
    for (UINT i = 0; factory->EnumAdapters1(i, &adapter) == S_OK; ++i)
    {
        DXGI_ADAPTER_DESC1 desc;
        ZeroMemory(&desc, sizeof(desc));
        if (SUCCEEDED(adapter->GetDesc1(&desc)))
        {
            if ((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0)
            {
                m_vramTotal += desc.DedicatedVideoMemory;
            }
        }
        adapter->Release();
        adapter = nullptr;
    }

    factory->Release();
}
