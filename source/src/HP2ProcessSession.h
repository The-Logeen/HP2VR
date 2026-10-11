#pragma once
#include <windows.h>

// Children inherit membership, so a frontend -> gameplay process handoff
// cannot be mistaken for the end of the session. No kill-on-close policy.
struct HP2ProcessSession {
    HANDLE job{CreateJobObjectW(nullptr, nullptr)};
    ~HP2ProcessSession() { if (job) CloseHandle(job); }
    bool attach(HANDLE suspendedProcess) {
        return job && AssignProcessToJobObject(job, suspendedProcess);
    }
    bool contains(DWORD pid) const {
        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!process) return false;
        BOOL member = FALSE;
        const bool ok = IsProcessInJob(process, job, &member) != FALSE;
        CloseHandle(process);
        return ok && member;
    }
    DWORD wait(DWORD timeout, HANDLE shutdown = nullptr) const {
        const ULONGLONG start = GetTickCount64();
        for (;;) {
            JOBOBJECT_BASIC_ACCOUNTING_INFORMATION accounting{};
            if (!QueryInformationJobObject(job, JobObjectBasicAccountingInformation,
                    &accounting, sizeof(accounting), nullptr)) return WAIT_FAILED;
            if (!accounting.ActiveProcesses) return WAIT_OBJECT_0;
            if (shutdown && WaitForSingleObject(shutdown, 0) == WAIT_OBJECT_0) return WAIT_OBJECT_0 + 1;
            if (timeout != INFINITE && GetTickCount64() - start >= timeout) return WAIT_TIMEOUT;
            Sleep(25);
        }
    }
};
