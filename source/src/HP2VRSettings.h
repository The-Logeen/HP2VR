#pragma once
#include <windows.h>
#include <cstdio>
#include <cmath>
#include <vector>

namespace HP2Settings {
inline bool open{}, clickHeld{}, snapHeld{};
inline float eyeOffset{}, units=40.f, speed=90.f;
inline int snapDegrees=30;
inline bool snap{}, bodyOnly{};
inline bool smoothSteps=true, flickLessons=false, avatar=false, arrow=true, beam=false;
inline int quality=1, page=0;
inline bool immersiveGnomes=false;
inline bool tessellation=false;
inline float hudInset=16.f,hudLower=8.f;
inline unsigned bodyCalibration{};
inline float wandPitch{},wandYaw{},pointerPitch{},pointerYaw{};
inline wchar_t path[MAX_PATH]{};
inline void load() {
    GetFullPathNameW(L"HP2VRSettings.ini", MAX_PATH, path, nullptr);
    auto angle=[&](const wchar_t* key){return std::fmax(-90.f,std::fmin(90.f,float(static_cast<int>(GetPrivateProfileIntW(L"VR",key,0,path)))));};
    wandPitch=angle(L"WandPitchDegrees");wandYaw=angle(L"WandYawDegrees");
    pointerPitch=angle(L"PointerPitchDegrees");pointerYaw=angle(L"PointerYawDegrees");
    hudInset=std::fmax(0.f,std::fmin(30.f,float(GetPrivateProfileIntW(L"VR",L"HudInsetPercent",16,path))));
    hudLower=std::fmax(-10.f,std::fmin(20.f,float(GetPrivateProfileIntW(L"VR",L"HudLowerPercent",8,path))));
    eyeOffset=static_cast<int>(GetPrivateProfileIntW(L"VR",L"EyeOffsetCm",0,path));
    units=static_cast<float>(GetPrivateProfileIntW(L"VR",L"UnitsPerMetre",40,path));
    speed=static_cast<float>(GetPrivateProfileIntW(L"VR",L"SmoothSpeed",90,path));
    snapDegrees=GetPrivateProfileIntW(L"VR",L"SnapDegrees",30,path);
    snap=GetPrivateProfileIntW(L"VR",L"Snap",0,path)!=0;
    bodyOnly=GetPrivateProfileIntW(L"VR",L"BodyOnly",0,path)!=0;
    immersiveGnomes=GetPrivateProfileIntW(L"VR",L"ImmersiveGnomes",0,path)!=0;
    tessellation=GetPrivateProfileIntW(L"VR",L"Tessellation",0,path)!=0;
    smoothSteps=GetPrivateProfileIntW(L"VR",L"SmoothSteps",1,path)!=0;
    flickLessons=GetPrivateProfileIntW(L"VR",L"FlickLessons",0,path)!=0;
    avatar=GetPrivateProfileIntW(L"VR",L"Avatar",0,path)!=0;
    arrow=GetPrivateProfileIntW(L"VR",L"ForwardArrow",1,path)!=0;
    beam=GetPrivateProfileIntW(L"VR",L"AimBeam",0,path)!=0;
    quality=GetPrivateProfileIntW(L"VR",L"Quality",1,path);if(quality<0||quality>2)quality=1;
    if(eyeOffset < -40 || eyeOffset > 60) eyeOffset=0;
    if(units < 20 || units > 80) units=40;
    if(speed < 15 || speed > 180) speed=90;
    if(snapDegrees < 15 || snapDegrees > 90) snapDegrees=30;
}
inline void save() {
    const wchar_t* angleKeys[]{L"WandPitchDegrees",L"WandYawDegrees",L"PointerPitchDegrees",L"PointerYawDegrees"};
    float angleVals[]{wandPitch,wandYaw,pointerPitch,pointerYaw};
    for(int i=0;i<4;++i){wchar_t value[24];swprintf_s(value,L"%.0f",angleVals[i]);WritePrivateProfileStringW(L"VR",angleKeys[i],value,path);}
    WritePrivateProfileStringW(L"VR",L"Tessellation",tessellation?L"1":L"0",path);
    WritePrivateProfileStringW(L"VR",L"ImmersiveGnomes",immersiveGnomes?L"1":L"0",path);
    const wchar_t* keys[]{L"EyeOffsetCm",L"UnitsPerMetre",L"SmoothSpeed",L"SnapDegrees",L"Snap",L"BodyOnly"};
    int values[]{int(eyeOffset),int(units),int(speed),snapDegrees,int(snap),int(bodyOnly)};
    for(int i=0;i<6;++i){wchar_t value[24];swprintf_s(value,L"%d",values[i]);WritePrivateProfileStringW(L"VR",keys[i],value,path);}
    const wchar_t* extra[]{L"SmoothSteps",L"FlickLessons",L"Avatar",L"ForwardArrow",L"AimBeam",L"Quality"};
    int vals[]{int(smoothSteps),int(flickLessons),int(avatar),int(arrow),int(beam),quality};
    for(int i=0;i<6;++i){wchar_t value[24];swprintf_s(value,L"%d",vals[i]);WritePrivateProfileStringW(L"VR",extra[i],value,path);}
}
inline void pointer(float u,float v,bool valid,bool pressed) {
    if(open && valid && pressed && !clickHeld && u>.05f && u<.95f) {
        const int row=static_cast<int>((v-.2f)/.1f);
        const int sign=u<.5f?-1:1;
        if(v>=.2f && v<.8f) {
            if(page==4){float* values[]{&wandPitch,&wandYaw,&pointerPitch,&pointerYaw};if(row<4)*values[row]=std::fmax(-90.f,std::fmin(90.f,*values[row]+sign*2.f));if(row==4)wandPitch=wandYaw=pointerPitch=pointerYaw=0.f;save();clickHeld=pressed;return;}
            if(page==3){if(row==0)hudInset=std::fmax(0.f,std::fmin(30.f,hudInset+sign*2));if(row==1)hudLower=std::fmax(-10.f,std::fmin(20.f,hudLower+sign*2));wchar_t n[16];swprintf_s(n,L"%.0f",hudInset);WritePrivateProfileStringW(L"VR",L"HudInsetPercent",n,path);swprintf_s(n,L"%.0f",hudLower);WritePrivateProfileStringW(L"VR",L"HudLowerPercent",n,path);}
            else if(page==2) {if(row==0)immersiveGnomes=!immersiveGnomes;if(row==3)++bodyCalibration;if(row==4)tessellation=!tessellation;}
            else if(page==1) {switch(row){case 0:quality=std::max(0,std::min(2,quality+sign));break;case 1:smoothSteps=!smoothSteps;break;case 2:flickLessons=!flickLessons;break;case 3:avatar=!avatar;break;case 4:arrow=!arrow;break;case 5:beam=!beam;break;}}
            else switch(row) {
            case 0: eyeOffset=std::fmax(-40.f,std::fmin(60.f,eyeOffset+sign*2.f));break;
            case 1: units=std::fmax(20.f,std::fmin(80.f,units+sign*2.f));break;
            case 2: snap=!snap;snapHeld=true;break;
            case 3: speed=std::fmax(15.f,std::fmin(180.f,speed+sign*15.f));break;
            case 4: snapDegrees=std::max(15,std::min(90,snapDegrees+sign*15));break;
            case 5: bodyOnly=!bodyOnly;break;
            }
            save();
        }
        if(v>=.85f && v<.95f) {if(u<.5f)page=(page+1)%5;else open=false;}
    }
    clickHeld=pressed;
}
inline int turn(float raw,float dt,HP2Locomotion::Turn& smooth) {
    if(!snap)return smooth.step(raw,dt,speed);
    if(std::fabs(raw)<.25f)snapHeld=false;
    if(std::fabs(raw)<.65f || snapHeld)return 0;
    snapHeld=true;return int(std::copysign(snapDegrees*65536.f/360.f,raw));
}
// A flat RGBA source consumed by the existing stationary SteamVR panel.
inline bool pixels(unsigned width,unsigned height,std::vector<FColor>& out) {
    HDC dc=CreateCompatibleDC(nullptr);if(!dc)return false;
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-static_cast<LONG>(height);
    info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    void* bits{};HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);
    if(!bitmap){DeleteDC(dc);return false;}
    HGDIOBJ old=SelectObject(dc,bitmap);
    RECT full{0,0,LONG(width),LONG(height)};HBRUSH bg=CreateSolidBrush(RGB(24,24,24));FillRect(dc,&full,bg);DeleteObject(bg);
    HFONT font=CreateFontW(-int(height*.033f),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    HGDIOBJ oldFont=SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(245,245,245));
    auto text=[&](float y,const wchar_t* label){RECT r{LONG(width*.06f),LONG(height*y),LONG(width*.94f),LONG(height*(y+.075f))};DrawTextW(dc,label,-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE);};
    text(.035f,L"HP2 VR SETTINGS");text(.115f,L"Point + trigger: left decreases / right increases");
    wchar_t row[160];
    if(page==0) {
    swprintf_s(row,L"<  Eye height offset: %+.0f cm  >",eyeOffset);text(.2f,row);
    swprintf_s(row,L"<  World scale: %.0f units/metre  >",units);text(.3f,row);
    swprintf_s(row,L"<  Rotation: %ls  >",snap?L"Snap":L"Smooth");text(.4f,row);
    swprintf_s(row,L"<  Smooth speed: %.0f degrees/sec  >",speed);text(.5f,row);
    swprintf_s(row,L"<  Snap angle: %d degrees  >",snapDegrees);text(.6f,row);
    swprintf_s(row,L"<  View: %ls  >",bodyOnly?L"Always in body":L"Native cinematics");text(.7f,row);
    }else if(page==1){
    swprintf_s(row,L"<  Resolution: %ls (restart)  >",quality==0?L"1280 x 960":quality==1?L"1600 x 1200":L"1920 x 1440");text(.2f,row);
    swprintf_s(row,L"<  Stair smoothing: %ls  >",smoothSteps?L"On":L"Off");text(.3f,row);
    swprintf_s(row,L"<  Lessons: %ls  >",flickLessons?L"Wand flicks":L"Native stick");text(.4f,row);
    swprintf_s(row,L"<  Body avatar (experimental): %ls  >",avatar?L"On":L"Off");text(.5f,row);
    swprintf_s(row,L"<  Forward arrow: %ls  >",arrow?L"On":L"Off");text(.6f,row);
    swprintf_s(row,L"<  Extra aim beam: %ls  >",beam?L"On":L"Off");text(.7f,row);
    }else if(page==4){
    swprintf_s(row,L"< Wand pitch (up +): %+.0f deg >",wandPitch);text(.2f,row);
    swprintf_s(row,L"< Wand yaw (right +): %+.0f deg >",wandYaw);text(.3f,row);
    swprintf_s(row,L"< Pointer pitch (up +): %+.0f deg >",pointerPitch);text(.4f,row);
    swprintf_s(row,L"< Pointer yaw (right +): %+.0f deg >",pointerYaw);text(.5f,row);
    text(.6f,L"< Reset all aim angles to zero >");
    text(.7f,L"2 degree steps. Wand visual and spell aim stay aligned.");
    }else if(page==3){
    swprintf_s(row,L"< HUD inset: %.0f%% >",hudInset);text(.2f,row);
    swprintf_s(row,L"< HUD lower: %+.0f%% >",hudLower);text(.3f,row);
    text(.45f,L"Native health / beans / challenge score");
    text(.55f,L"Camera HUD positioning; native visibility timing.");
    text(.65f,L"Wrist placement not yet implemented.");
    }else{
    swprintf_s(row,L"<  Gnome motion throw: %ls  >",immersiveGnomes?L"On":L"Off");text(.2f,row);
    text(.35f,L"Hold briefly still, then thrust/flick forward.");
    text(.45f,L"Right trigger always works. No wand required.");
    text(.5f,L"< Calibrate standing body height NOW >");
    swprintf_s(row,L"< Tessellation test: %ls (restart) >",tessellation?L"On":L"Off");text(.6f,row);
    text(.7f,L"Stand naturally to calibrate; keep play space clear.");
    }
    text(.86f,L"< OTHER SETTINGS       CLOSE >  (or Y)");
    GdiFlush();out.resize(size_t(width)*height);memcpy(out.data(),bits,out.size()*sizeof(FColor));
    for(auto& p:out)p.A=255;
    SelectObject(dc,oldFont);DeleteObject(font);SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);return true;
}
}
