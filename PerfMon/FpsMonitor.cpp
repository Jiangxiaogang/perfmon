#include "stdafx.h"
#include "FpsMonitor.h"

#pragma comment(lib, "advapi32.lib")

namespace
{
    const WCHAR kSessionName[] = L"PerfMonFpsTrace";

    // Microsoft-Windows-DXGI
    const GUID kDxgiProvider =
    { 0xca11c036, 0x0102, 0x4a2d, { 0xa6, 0xad, 0xf0, 0x3c, 0xfe, 0xd5, 0xd3, 0xc9 } };

    // Microsoft_Windows_DXGI_Analytic | Microsoft_Windows_DXGI_Events
    const ULONGLONG kDxgiKeyword = 0x8000000000000002ULL;

    // DXGI Present_Start event id.
    const USHORT kPresentStartId = 42;

    struct TraceSessionProps : public EVENT_TRACE_PROPERTIES
    {
        WCHAR LoggerName[128];
    };
}

CFpsMonitor::CFpsMonitor()
    : m_session(0)
    , m_trace(INVALID_PROCESSTRACE_HANDLE)
    , m_thread(nullptr)
    , m_freq(0)
    , m_running(false)
    , m_csReady(false)
    , m_fps(0.0)
    , m_fpsValid(false)
{
}

CFpsMonitor::~CFpsMonitor()
{
    Stop();
}

bool CFpsMonitor::Start()
{
    if (m_running)
    {
        return true;
    }

    LARGE_INTEGER freq;
    if (!QueryPerformanceFrequency(&freq) || freq.QuadPart == 0)
    {
        return false;
    }
    m_freq = freq.QuadPart;

    if (!m_csReady)
    {
        InitializeCriticalSection(&m_cs);
        m_csReady = true;
    }

    TraceSessionProps props;
    ZeroMemory(&props, sizeof(props));
    props.Wnode.BufferSize = sizeof(TraceSessionProps);
    props.Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    props.Wnode.ClientContext = 1; // QPC timestamps
    props.LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
    props.LoggerNameOffset = offsetof(TraceSessionProps, LoggerName);
    props.BufferSize = 64;
    props.MinimumBuffers = 24;
    props.MaximumBuffers = 64;
    props.FlushTimer = 1;
    wcscpy_s(props.LoggerName, kSessionName);

    ULONG status = StartTraceW(&m_session, kSessionName, &props);
    if (status == ERROR_ALREADY_EXISTS)
    {
        // A previous run crashed and left the session behind: stop it and retry.
        TraceSessionProps stopProps;
        ZeroMemory(&stopProps, sizeof(stopProps));
        stopProps.Wnode.BufferSize = sizeof(TraceSessionProps);
        stopProps.LoggerNameOffset = offsetof(TraceSessionProps, LoggerName);
        wcscpy_s(stopProps.LoggerName, kSessionName);
        ControlTraceW(0, kSessionName, &stopProps, EVENT_TRACE_CONTROL_STOP);

        status = StartTraceW(&m_session, kSessionName, &props);
    }

    if (status != ERROR_SUCCESS)
    {
        m_session = 0;
        return false;
    }

    ENABLE_TRACE_PARAMETERS params;
    ZeroMemory(&params, sizeof(params));
    params.Version = ENABLE_TRACE_PARAMETERS_VERSION_2;
    params.SourceId = props.Wnode.Guid;

    ULONG enabled = EnableTraceEx2(
        m_session,
        &kDxgiProvider,
        EVENT_CONTROL_CODE_ENABLE_PROVIDER,
        TRACE_LEVEL_VERBOSE,
        kDxgiKeyword,
        0,
        0,
        &params);

    if (enabled != ERROR_SUCCESS)
    {
        ReleaseSession();
        return false;
    }

    EVENT_TRACE_LOGFILEW logfile;
    ZeroMemory(&logfile, sizeof(logfile));
    logfile.LoggerName = (LPWSTR)kSessionName;
    logfile.ProcessTraceMode = PROCESS_TRACE_MODE_REAL_TIME
        | PROCESS_TRACE_MODE_EVENT_RECORD
        | PROCESS_TRACE_MODE_RAW_TIMESTAMP;
    logfile.EventRecordCallback = &CFpsMonitor::EventRecordCallback;
    logfile.Context = this;

    m_trace = OpenTraceW(&logfile);
    if (m_trace == INVALID_PROCESSTRACE_HANDLE)
    {
        m_trace = INVALID_PROCESSTRACE_HANDLE;
        ReleaseSession();
        return false;
    }

    m_running = true;
    m_thread = CreateThread(nullptr, 0, &CFpsMonitor::ThreadProc, this, 0, nullptr);
    if (m_thread == nullptr)
    {
        m_running = false;
        CloseTrace(m_trace);
        m_trace = INVALID_PROCESSTRACE_HANDLE;
        ReleaseSession();
        return false;
    }

    return true;
}

void CFpsMonitor::Stop()
{
    if (m_thread != nullptr)
    {
        ReleaseSession();
        WaitForSingleObject(m_thread, 5000);
        CloseHandle(m_thread);
        m_thread = nullptr;
    }

    if (m_trace != INVALID_PROCESSTRACE_HANDLE)
    {
        CloseTrace(m_trace);
        m_trace = INVALID_PROCESSTRACE_HANDLE;
    }

    ReleaseSession();
    m_running = false;

    if (m_csReady)
    {
        DeleteCriticalSection(&m_cs);
        m_csReady = false;
    }
}

void CFpsMonitor::ReleaseSession()
{
    if (m_session != 0 || m_trace != INVALID_PROCESSTRACE_HANDLE)
    {
        TraceSessionProps props;
        ZeroMemory(&props, sizeof(props));
        props.Wnode.BufferSize = sizeof(TraceSessionProps);
        props.LoggerNameOffset = offsetof(TraceSessionProps, LoggerName);
        wcscpy_s(props.LoggerName, kSessionName);
        ControlTraceW(m_session, kSessionName, &props, EVENT_TRACE_CONTROL_STOP);
    }
    m_session = 0;
}

DWORD WINAPI CFpsMonitor::ThreadProc(LPVOID param)
{
    CFpsMonitor* self = (CFpsMonitor*)param;
    if (self != nullptr)
    {
        self->Run();
    }
    return 0;
}

void CFpsMonitor::Run()
{
    if (m_trace != INVALID_PROCESSTRACE_HANDLE)
    {
        ProcessTrace(&m_trace, 1, nullptr, nullptr);
    }
}

VOID WINAPI CFpsMonitor::EventRecordCallback(PEVENT_RECORD record)
{
    if (record == nullptr || record->UserContext == nullptr)
    {
        return;
    }
    ((CFpsMonitor*)record->UserContext)->OnEvent(record);
}

void CFpsMonitor::OnEvent(PEVENT_RECORD record)
{
    if (!IsEqualGUID(record->EventHeader.ProviderId, kDxgiProvider))
    {
        return;
    }
    if (record->EventHeader.EventDescriptor.Id != kPresentStartId)
    {
        return;
    }

    DWORD pid = record->EventHeader.ProcessId;
    if (pid == GetCurrentProcessId())
    {
        return;
    }

    LONGLONG qpc = record->EventHeader.TimeStamp.QuadPart;

    EnterCriticalSection(&m_cs);
    std::deque<LONGLONG>& samples = m_presents[pid];
    samples.push_back(qpc);

    LONGLONG cutoff = qpc - (m_freq * 3);
    while (!samples.empty() && samples.front() < cutoff)
    {
        samples.pop_front();
    }
    LeaveCriticalSection(&m_cs);
}

void CFpsMonitor::Update()
{
    m_fps = 0.0;
    m_fpsValid = false;

    if (!m_running || !m_csReady)
    {
        return;
    }

    // Real-time ETW delivery can lag the wall clock by a second or more, so the
    // measurement window is anchored to the newest event we have received rather
    // than to the current time.
    LONGLONG latest = 0;
    EnterCriticalSection(&m_cs);
    for (std::map<DWORD, std::deque<LONGLONG> >::iterator it = m_presents.begin();
         it != m_presents.end(); ++it)
    {
        if (!it->second.empty() && it->second.back() > latest)
        {
            latest = it->second.back();
        }
    }
    LeaveCriticalSection(&m_cs);

    if (latest == 0)
    {
        return;
    }

    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    if (now.QuadPart - latest > m_freq * 4)
    {
        // Nothing has been presented for several seconds.
        return;
    }

    LONGLONG cutoff = latest - m_freq;

    DWORD foregroundPid = 0;
    HWND foreground = GetForegroundWindow();
    if (foreground != nullptr)
    {
        DWORD pid = 0;
        GetWindowThreadProcessId(foreground, &pid);
        foregroundPid = pid;
    }

    size_t bestCount = 0;
    size_t foregroundCount = 0;

    EnterCriticalSection(&m_cs);
    for (std::map<DWORD, std::deque<LONGLONG> >::iterator it = m_presents.begin();
         it != m_presents.end(); ++it)
    {
        size_t count = 0;
        const std::deque<LONGLONG>& samples = it->second;
        for (size_t i = 0; i < samples.size(); ++i)
        {
            if (samples[i] >= cutoff)
            {
                ++count;
            }
        }

        if (it->first == foregroundPid)
        {
            foregroundCount = count;
        }
        if (count > bestCount)
        {
            bestCount = count;
        }
    }
    LeaveCriticalSection(&m_cs);

    if (foregroundPid != 0 && foregroundCount > 0)
    {
        bestCount = foregroundCount;
    }

    m_fps = (double)bestCount;
    m_fpsValid = (bestCount > 0);
}
