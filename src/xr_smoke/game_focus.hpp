#pragma once
#include <windows.h>
#include <objbase.h>
#include <oleauto.h>

// Virtual Desktop consumes its menu shortcut. Its OpenXR session transition
// tells us when the user has returned to the VR view.
class GameFocusRestorer {
    bool everFocused_{}, focused_{}, pending_{}, previousMenuDown_{}, sawGameForeground_{};
    ULONGLONG readyAt_{}, deadline_{}, nextAttempt_{};

    static bool isGameProcess(DWORD pid) {
        HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
        if(!process)return false;
        wchar_t path[MAX_PATH]{};
        DWORD length=MAX_PATH;
        const bool readable=QueryFullProcessImageNameW(process,0,path,&length)!=0;
        CloseHandle(process);
        if(!readable)return false;
        const wchar_t* filename=wcsrchr(path,L'\\');
        return _wcsicmp(filename?filename+1:path,L"koa.exe")==0;
    }

    static HWND windowForProcess(DWORD pid) {
        struct Search { DWORD pid; HWND result; } search{pid,nullptr};
        EnumWindows([](HWND window,LPARAM data)->BOOL {
            auto& search=*reinterpret_cast<Search*>(data);
            DWORD owner{};
            GetWindowThreadProcessId(window,&owner);
            if(owner==search.pid&&IsWindowVisible(window)&&!GetWindow(window,GW_OWNER)) {
                search.result=window;
                return FALSE;
            }
            return TRUE;
        },reinterpret_cast<LPARAM>(&search));
        return search.result;
    }

    static void appActivate(DWORD pid) {
        const HRESULT initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
        if(FAILED(initialized)&&initialized!=RPC_E_CHANGED_MODE)return;
        CLSID shellClass{};
        if(SUCCEEDED(CLSIDFromProgID(L"WScript.Shell",&shellClass))) {
            IDispatch* shell{};
            if(SUCCEEDED(CoCreateInstance(shellClass,nullptr,CLSCTX_INPROC_SERVER|CLSCTX_LOCAL_SERVER,
                IID_IDispatch,reinterpret_cast<void**>(&shell)))) {
                OLECHAR method[]=L"AppActivate";OLECHAR* methods[]={method};DISPID id{};
                if(SUCCEEDED(shell->GetIDsOfNames(IID_NULL,methods,1,LOCALE_USER_DEFAULT,&id))) {
                    VARIANTARG target{};target.vt=VT_I4;target.lVal=static_cast<LONG>(pid);
                    DISPPARAMS parameters{&target,nullptr,1,0};
                    VARIANT result;VariantInit(&result);
                    shell->Invoke(id,IID_NULL,LOCALE_USER_DEFAULT,DISPATCH_METHOD,&parameters,&result,nullptr,nullptr);
                    VariantClear(&result);
                }
                shell->Release();
            }
        }
        if(SUCCEEDED(initialized))CoUninitialize();
    }

public:
    enum class Result { None, AlreadyFocused, Restored, TimedOut };

    void onSessionFocus(bool focused) {
        if(focused&&!focused_&&everFocused_) {
            pending_=true;
            readyAt_=GetTickCount64();
            deadline_=GetTickCount64()+3000;
            nextAttempt_=0;
        }
        if(!focused)pending_=false;
        if(focused)everFocused_=true;
        focused_=focused;
    }

    void onMenuSample(bool menuDown,DWORD gamePid) {
        const bool released=previousMenuDown_&&!menuDown;
        previousMenuDown_=menuDown;
        if(!focused_||!sawGameForeground_||!released||!gamePid)return;
        DWORD foregroundPid{};
        GetWindowThreadProcessId(GetForegroundWindow(),&foregroundPid);
        if(foregroundPid==gamePid)return;
        // VD leaves the XR session focused while showing its desktop. Its
        // menu input still reaches us. Wait until the click has finished so
        // neither click of the return gesture enters the game's Start menu.
        const auto now=GetTickCount64();
        pending_=true;
        readyAt_=now+500;
        deadline_=now+3500;
        nextAttempt_=0;
    }

    Result poll(DWORD gamePid) {
        DWORD foregroundPid{};
        HWND foreground=GetForegroundWindow();
        DWORD foregroundThread=GetWindowThreadProcessId(foreground,&foregroundPid);
        if(gamePid&&foregroundPid==gamePid){
            sawGameForeground_=true;
            if(pending_){pending_=false;return Result::AlreadyFocused;}
        }
        if(!pending_)return Result::None;
        const auto now=GetTickCount64();
        if(now>=deadline_){pending_=false;return Result::TimedOut;}
        if(!gamePid||now<readyAt_||now<nextAttempt_)return Result::None;
        nextAttempt_=now+100;
        if(!isGameProcess(gamePid))return Result::None;
        HWND game=windowForProcess(gamePid);
        if(!game)return Result::None;
        if(IsIconic(game))ShowWindow(game,SW_RESTORE);
        SetForegroundWindow(game);
        GetWindowThreadProcessId(GetForegroundWindow(),&foregroundPid);
        if(foregroundPid!=gamePid&&foregroundThread&&foregroundThread!=GetCurrentThreadId()) {
            const DWORD currentThread=GetCurrentThreadId();
            if(AttachThreadInput(currentThread,foregroundThread,TRUE)) {
                SetForegroundWindow(game);
                AttachThreadInput(currentThread,foregroundThread,FALSE);
            }
        }
        GetWindowThreadProcessId(GetForegroundWindow(),&foregroundPid);
        if(foregroundPid!=gamePid)appActivate(gamePid);
        GetWindowThreadProcessId(GetForegroundWindow(),&foregroundPid);
        if(foregroundPid==gamePid){pending_=false;return Result::Restored;}
        return Result::None;
    }
};
