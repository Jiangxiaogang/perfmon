#pragma once

#include <evntrace.h>
#include <evntcons.h>

// Measures frames-per-second of the busiest presenting process by consuming
// DXGI "Present" events from a real-time ETW session. This is pure user-mode
// ETW and does not require any driver or injection.
class CFpsMonitor
{
public:
    CFpsMonitor();
    ~CFpsMonitor();

    bool Start();
    void Stop();

    bool   IsRunning() const { return m_running; }
    // Called once per second on the UI thread; refreshes the current value.
    void   Update();
    bool   Valid() const { return m_fpsValid; }
    double Fps()   const { return m_fps; }

private:
    static DWORD WINAPI ThreadProc(LPVOID param);
    static VOID  WINAPI EventRecordCallback(PEVENT_RECORD record);

    void Run();
    void OnEvent(PEVENT_RECORD record);
    void ReleaseSession();

    TRACEHANDLE m_session;
    TRACEHANDLE m_trace;
    HANDLE      m_thread;
    LONGLONG    m_freq;
    bool        m_running;
    bool        m_csReady;

    CRITICAL_SECTION m_cs;
    std::map<DWORD, std::deque<LONGLONG> > m_presents;

    double m_fps;
    bool   m_fpsValid;
};
