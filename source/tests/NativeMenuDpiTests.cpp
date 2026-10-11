#include "../src/HP2NativeFrontend.h"
#include <cassert>
static int clicks=0;
static LRESULT CALLBACK handler(HWND w,UINT m,WPARAM a,LPARAM b){
    if(m==WM_COMMAND && LOWORD(a)==42 && HIWORD(a)==BN_CLICKED){++clicks;return 0;}
    return DefWindowProcW(w,m,a,b);
}
int main(){
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    WNDCLASSW wc{};wc.lpfnWndProc=handler;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"HP2DpiRegression";
    RegisterClassW(&wc);
    for(auto mode:{DPI_AWARENESS_CONTEXT_UNAWARE,DPI_AWARENESS_CONTEXT_SYSTEM_AWARE,DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2}){
        auto previous=SetThreadDpiAwarenessContext(mode);
        HWND parent=CreateWindowW(wc.lpszClassName,L"Isolated HP2 menu test",WS_OVERLAPPEDWINDOW,0,0,640,480,nullptr,nullptr,wc.hInstance,nullptr);
        HWND button=CreateWindowW(L"BUTTON",L"Test",WS_CHILD|WS_VISIBLE,100,100,120,60,parent,(HMENU)42,wc.hInstance,nullptr);
        RECT expected{};GetClientRect(parent,&expected);
        SetThreadDpiAwarenessContext(previous);
        HP2NativeFrontend frontend;frontend.window=parent;
        assert(frontend.capture());
        assert(frontend.width==expected.right && frontend.height==expected.bottom);
        HP2VR::InputPayload input{};input.focusOwned=1;input.inputAvailable=1;input.pointerValid=1;
        input.pointerUv[0]=160.f/(frontend.width-1);input.pointerUv[1]=130.f/(frontend.height-1);
        const auto before=clicks;
        input.trigger[1]=1;frontend.input(input);input.trigger[1]=0;frontend.input(input);
        MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}
        assert(clicks==before+1);
        assert(AreDpiAwarenessContextsEqual(GetThreadDpiAwarenessContext(),previous));
        printf("PASS: capture %ux%u, pointer click and context restoration\n",frontend.width,frontend.height);
        DestroyWindow(parent);
    }
}
