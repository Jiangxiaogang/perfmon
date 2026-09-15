#pragma once

#include <pdh.h>
#include <pdhmsg.h>

struct MetricsSnapshot
{
    double             cpuPercent;
    double             ramUsedBytes;
    double             ramTotalBytes;

    bool               gpuValid;
    double             gpuPercent;

    bool               vramValid;
    unsigned long long vramUsedBytes;
    unsigned long long vramTotalBytes;

    bool               fpsValid;
    double             fps;

    MetricsSnapshot();
};

class CMetricsCollector
{
public:
    CMetricsCollector();
    ~CMetricsCollector();

    void Initialize();
    void Shutdown();

    MetricsSnapshot Collect();

    double CpuPercent() const { return m_cpuPercent; }
    double GpuPercent() const { return m_gpuPercent; }
    bool   GpuValid()   const { return m_gpuValid; }

private:
    void CollectCpu(MetricsSnapshot& s);
    void CollectRam(MetricsSnapshot& s);
    void CollectGpu(MetricsSnapshot& s);
    void CollectVram(MetricsSnapshot& s);
    void QueryVramTotal();

    ULONGLONG m_prevIdle;
    ULONGLONG m_prevKernel;
    ULONGLONG m_prevUser;
    bool      m_cpuReady;
    double    m_cpuPercent;

    PDH_HQUERY   m_query;
    PDH_HCOUNTER m_gpuCounter;
    PDH_HCOUNTER m_vramCounter;
    bool         m_pdhReady;
    bool         m_firstSample;
    double       m_gpuPercent;
    bool         m_gpuValid;

    unsigned long long m_vramTotal;
};
