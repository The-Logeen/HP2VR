#pragma once
// World-space presentation only; HP2 VendorTransaction still owns purchases.
namespace HP2Vendor {
static UObject* current{};
static FVector center,right,normal;
static FVector vendorPosition;
static float backRemaining{};
static int hovered=-1,pressedChoice=-1,lastPrice=-1;
static bool triggerHeld=true,placed{};
static UTexture* label{};
static UTexture* white{};
static DWORD testStart{};static bool testCreated{},testReplied{};
static void selfTest() {
    wchar_t mode[20]{},runtime[MAX_PATH]{};
    if(!GetEnvironmentVariableW(L"HP2VR_VENDOR_TEST",mode,20)||!GetEnvironmentVariableW(L"HP2VR_RUNTIME",runtime,MAX_PATH)||!wcsstr(runtime,L"runtime-progression-check")||!inputViewport||!inputViewport->Actor)return;
    if(!testStart)testStart=GetTickCount();
    if(testCreated || GetTickCount()-testStart<4000 || !walkingView(inputViewport->Actor))return;
    auto* pawn=inputViewport->Actor;auto* level=pawn->GetLevel();
    auto* vendorClass=FindObject<UClass>(nullptr,TEXT("HGame.FredWeasley"));auto* managerClass=FindObject<UClass>(nullptr,TEXT("HGame.VendorManager"));if(!vendorClass||!managerClass)return;
    auto* vendor=level->SpawnActor(vendorClass,NAME_None,nullptr,nullptr,pawn->Location+FVector(100,0,0));auto* m=level->SpawnActor(managerClass);
    if(!vendor||!m)return;
    auto* set=m->FindFunction(TEXT("SetVendor"));auto* engage=m->FindFunction(TEXT("DoEngageVendor"));
    if(!set||set->ParmsSize!=4||!engage||engage->ParmsSize!=4)return;
    testCreated=true;testStart=GetTickCount();UObject* v=vendor;m->ProcessEvent(set,&v);FName idle(TEXT("VendorIdle"));m->ProcessEvent(engage,&idle);
    m->GotoState(FName(TEXT("VendorTransaction")));line("HP2 isolated vendor fixture entered native VendorTransaction");
}
static UObject* manager(){return inputViewport&&inputViewport->Actor?reflectedObject(inputViewport->Actor,TEXT("CurrVendorManager")):nullptr;}
static bool active(){return current && current==manager();}
static int choice(){return pressedChoice;}
static void consumed(){pressedChoice=-1;}
static void update(bool live,float seconds) {
    selfTest();
    auto* m=manager();const bool visible=m&&inputViewport&&HP2Tracked::flag(inputViewport->Console,TEXT("bVendorBar"))&&HP2Tracked::vendorInputHook.fn&&HP2Tracked::vendorYesHook.fn&&HP2Tracked::vendorNoHook.fn&&HP2Tracked::vendorDrawHook.fn;
    const bool down=live&&latestInput.trigger[1]>.65f;
    if(!visible){current=nullptr;placed=false;hovered=pressedChoice=-1;triggerHeld=down;return;}
    if(m!=current){current=m;placed=false;triggerHeld=down;backRemaining=.8f;line("HP2 world vendor choices opened");}
    hovered=-1;
    if(!live||!haveOrigin||!HP2VR::finitePose(latestInput.hand[1])){triggerHeld=true;return;}
    auto* pawn=inputViewport->Actor;
    auto* vendor=Cast<AActor>(reflectedObject(m,TEXT("Vendor")));
    if(!placed){const float yaw=bodyYaw*TwoPi/65536.f;normal=FVector(std::cos(yaw),std::sin(yaw),0);
        vendorPosition=vendor?vendor->Location:pawn->Location+normal*WorldUnitsPerMetre;
        normal=vendorPosition-pawn->Location;normal.Z=0;normal=normal.SafeNormal();right=FVector(-normal.Y,normal.X,0);
        // Beside the vendor, on Harry's side: never a metre beyond his eyes
        // into the vendor's torso. Keep the prompt anchored in the world.
        center=vendorPosition+right*(.65f*WorldUnitsPerMetre)-normal*(.35f*WorldUnitsPerMetre);
        center.Z=HP2Tracked::anchor(pawn).Z-.25f*WorldUnitsPerMetre;placed=true;}
    const float dt=Clamp(seconds,0.f,.05f);
    FVector away=pawn->Location-vendorPosition;away.Z=0;
    FVector movement(0,0,0);
    const float stick=std::sqrt(latestInput.move[0]*latestInput.move[0]+latestInput.move[1]*latestInput.move[1]);
    if(stick>.2f){backRemaining=0;const float yaw=bodyYaw*TwoPi/65536.f;
        movement=(FVector(std::cos(yaw),std::sin(yaw),0)*latestInput.move[1]+FVector(-std::sin(yaw),std::cos(yaw),0)*latestInput.move[0])*(1.6f*WorldUnitsPerMetre*dt/Max(1.f,stick));}
    else if(backRemaining>0){backRemaining=Max(0.f,backRemaining-dt);if(away.Size()<1.2f*WorldUnitsPerMetre)movement=away.SafeNormal()*(.6f*WorldUnitsPerMetre*dt);}
    if(movement.SizeSquared()>0){
        // Preserve native capture/state. Sweep only onto supported ground;
        // no walking through walls or backing off ledges during dialogue.
        const FVector dest=pawn->Location+movement;FCheckResult floor(1.f);
        pawn->GetLevel()->SingleLineCheck(floor,pawn,dest-FVector(0,0,pawn->CollisionHeight+12),dest,TRACE_AllColliding);
        if(floor.Time<1&&floor.Normal.Z>.7f){FCheckResult hit(1.f);pawn->GetLevel()->MoveActor(pawn,movement,pawn->Rotation,hit);}
    }
    FVector facing=center-HP2Tracked::anchor(pawn);facing.Z=0;
    if(facing.SizeSquared()>1){normal=facing.SafeNormal();right=FVector(-normal.Y,normal.X,0);}
    FVector origin,dir;
    if(HP2Tracked::carryPose(origin,dir,latestInput)) {
        const float denom=dir|normal;
        if(denom>.05f){const float distance=((center-origin)|normal)/denom;
            const FVector hit=origin+dir*distance-center;
            const float u=.5f+(hit|right)/(.8f*WorldUnitsPerMetre),v=.5f-hit.Z/(.4f*WorldUnitsPerMetre);
            if(distance>0&&distance<3*WorldUnitsPerMetre&&v>=.57f&&v<=.91f){if(u>.06f&&u<.46f)hovered=0;else if(u>.54f&&u<.94f)hovered=1;}
        }
    }
    if(down&&!triggerHeld)pressedChoice=hovered;
    triggerHeld=down;
    if(away.Size()>3.f*WorldUnitsPerMetre){pressedChoice=1;line("HP2 vendor walk-away sends native No");}
    if(testCreated&&!testReplied&&GetTickCount()-testStart>10000){wchar_t mode[20]{};GetEnvironmentVariableW(L"HP2VR_VENDOR_TEST",mode,20);if(wcscmp(mode,L"walkaway")){pressedChoice=!wcscmp(mode,L"yes")?0:1;testReplied=true;line("HP2 isolated vendor choice=%d delivered to native PlayerInput",pressedChoice);}}
    // Vendor dialogue can capture Harry's input. Dispatch the validated native
    // transaction directly once; the adapter consumes the edge before Tick.
    if(pressedChoice>=0){FLOAT dt=0;m->ProcessEvent(HP2Tracked::vendorInputHook.fn,&dt);}
}
static UTexture* textTexture(int price) {
    if(label&&lastPrice==price)return label;
    HDC dc=CreateCompatibleDC(nullptr);BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=512;info.bmiHeader.biHeight=-256;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
    void* bits{};HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);if(!dc||!bitmap){if(dc)DeleteDC(dc);return nullptr;}
    auto old=SelectObject(dc,bitmap);RECT all{0,0,512,256};HBRUSH bg=CreateSolidBrush(RGB(24,24,24));FillRect(dc,&all,bg);DeleteObject(bg);
    HFONT font=CreateFontW(-32,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,0,0,ANTIALIASED_QUALITY,0,L"Segoe UI");auto oldFont=SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,255,255));
    wchar_t title[80];swprintf_s(title,L"Trade: %d beans",price);RECT heading{12,8,500,90};DrawTextW(dc,title,-1,&heading,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    RECT yes{31,146,235,233},no{276,146,481,233};HBRUSH border=CreateSolidBrush(RGB(160,160,160));FrameRect(dc,&yes,border);FrameRect(dc,&no,border);DeleteObject(border);
    DrawTextW(dc,L"YES",-1,&yes,DT_CENTER|DT_VCENTER|DT_SINGLELINE);DrawTextW(dc,L"NO",-1,&no,DT_CENTER|DT_VCENTER|DT_SINGLELINE);GdiFlush();
    auto* texture=ConstructObject<UTexture>(UTexture::StaticClass());texture->Init(512,256);
    auto* palette=ConstructObject<UPalette>(UPalette::StaticClass());palette->Colors.Empty();palette->Colors.Add(256);for(int i=0;i<256;++i)palette->Colors(i)=FColor(i,i,i,255);texture->Palette=palette;
    for(int i=0;i<512*256;++i)texture->Mips(0).DataArray(i)=static_cast<BYTE*>(bits)[i*4];
    texture->AddToRoot();if(label)label->RemoveFromRoot();label=texture;lastPrice=price;
    if(!white){white=ConstructObject<UTexture>(UTexture::StaticClass());white->Init(4,4);white->Palette=palette;for(int i=0;i<16;++i)white->Mips(0).DataArray(i)=255;white->AddToRoot();}
    SelectObject(dc,oldFont);DeleteObject(font);SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);return label;
}
static void draw(FSceneNode* frame) {
    if(!active()||!placed||currentEye<0)return;
    auto* priceField=FindField<UIntProperty>(current->GetClass(),TEXT("nCurrPrice"));if(!priceField)return;
    const int price=*reinterpret_cast<INT*>(reinterpret_cast<BYTE*>(current)+priceField->Offset);
    auto* texture=textTexture(price);if(!texture)return;
    auto quad=[&](UTexture* tex,FVector c,float w,float h){if(!tex)return;FTextureInfo info{};tex->Lock(info,frame->Viewport->CurrentTime,0,frame->Viewport->RenDev);
        FVector points[4]{c-right*w*.5f+FVector(0,0,h*.5f),c+right*w*.5f+FVector(0,0,h*.5f),c+right*w*.5f-FVector(0,0,h*.5f),c-right*w*.5f-FVector(0,0,h*.5f)};
        FTransTexture v[4]{};FTransTexture* refs[4]{};for(int i=0;i<4;++i){v[i].Point=points[i].TransformPointBy(frame->Coords);v[i].U=(i==1||i==2)?info.USize:0;v[i].V=i>=2?info.VSize:0;v[i].Light=FPlane(1,1,1,1);refs[i]=&v[i];}VRDraw::polygon(frame,info,refs,4,PF_TwoSided|PF_Occlude);tex->Unlock(info);};
    quad(texture,center,.8f*WorldUnitsPerMetre,.4f*WorldUnitsPerMetre);
    quad(Cast<UTexture>(reflectedObject(current,TEXT("textureItemToSell"))),center+FVector(0,0,.32f*WorldUnitsPerMetre),.2f*WorldUnitsPerMetre,.2f*WorldUnitsPerMetre);
    // A visible marker identifies the actual controller-ray selection.
    if(hovered>=0)quad(white,center+right*((hovered?1.f:-1.f)*.2f*WorldUnitsPerMetre)-FVector(0,0,.17f*WorldUnitsPerMetre)-normal*.15f,.28f*WorldUnitsPerMetre,.008f*WorldUnitsPerMetre);
    FVector origin,dir;
    if(white&&HP2Tracked::carryPose(origin,dir,pairInput)) {
        const float denom=dir|normal;
        const float distance=denom>.05f?Clamp(((center-origin)|normal)/denom,0.f,3.f*WorldUnitsPerMetre):WorldUnitsPerMetre;
        const FVector end=origin+dir*distance;
        const FVector side=(dir^(HP2Tracked::anchor(frame->Viewport->Actor)-origin)).SafeNormal()*(.002f*WorldUnitsPerMetre);
        FVector points[4]{origin-side,origin+side,end+side,end-side};
        FTextureInfo info{};white->Lock(info,frame->Viewport->CurrentTime,0,frame->Viewport->RenDev);
        FTransTexture v[4]{};FTransTexture* refs[4]{};for(int i=0;i<4;++i){v[i].Point=points[i].TransformPointBy(frame->Coords);v[i].U=v[i].V=1;v[i].Light=FPlane(.2f,.8f,1.f,1.f);refs[i]=&v[i];}
        VRDraw::polygon(frame,info,refs,4,PF_TwoSided|PF_Occlude);white->Unlock(info);
    }
}
}
