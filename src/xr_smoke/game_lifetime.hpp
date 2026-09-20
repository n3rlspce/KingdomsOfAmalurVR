#pragma once
#include <windows.h>
#include <tlhelp32.h>

// Retain a process handle so PID reuse cannot attach an old session to a new game.
class GameLifetime {
    HANDLE process_{};
    ULONGLONG nextSearch_{};
public:
    GameLifetime()=default;
    GameLifetime(const GameLifetime&)=delete;
    GameLifetime& operator=(const GameLifetime&)=delete;
    ~GameLifetime(){if(process_)CloseHandle(process_);}
    bool attach(DWORD pid){
        if(process_)return false;
        process_=OpenProcess(SYNCHRONIZE,FALSE,pid);
        return process_!=nullptr;
    }
    bool exited()const{return process_&&WaitForSingleObject(process_,0)==WAIT_OBJECT_0;}
    bool gameExited(){
        if(process_)return exited();
        const auto now=GetTickCount64();
        if(now<nextSearch_)return false;
        nextSearch_=now+500;
        const auto snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
        if(snapshot==INVALID_HANDLE_VALUE)return false;
        PROCESSENTRY32W entry{};entry.dwSize=sizeof(entry);
        if(Process32FirstW(snapshot,&entry))do{
            if(_wcsicmp(entry.szExeFile,L"koa.exe")==0&&attach(entry.th32ProcessID))break;
        }while(Process32NextW(snapshot,&entry));
        CloseHandle(snapshot);
        return exited();
    }
};
