#include "SIPSTERPlatform.h"

#include <iostream>
#include <chrono>

#include "common.h"

Nan::Persistent<FunctionTemplate> SIPSTERPlatform_constructor;

#include "dog_api.h"
#include "vendor_code.h"

//#define USE_SUPER_DOG 0
#define HONEYWELL_SUPER_DOG_ID 1

#define HONEYWELL_SUPER_DOG_TAG "Honeywell"
#define DOG_VERIFY_FAIL_RETRY_TIME 1
#define DOG_VERIFY_SUCCEED_RETRY_TIME 60

SIPSTERPlatform::~SIPSTERPlatform()
{
    if (emit)
    {
        delete emit;
    }
}

/*功能:允许通过防火墙
*/
void SIPSTERPlatform::allowPassFireWall()
{
#ifdef _WIN32
    std::string csBatFileName = "Firewall$$$.bat";
    std::string csBatDetail;
    HANDLE handle;
    DWORD dwWriteLen;
    TCHAR szModuleName[1024];

    //创建批处理文件
    handle = ::CreateFile(csBatFileName.c_str(),
                          GENERIC_WRITE,
                          0,
                          NULL,
                          CREATE_ALWAYS,
                          FILE_ATTRIBUTE_NORMAL,
                          NULL);
    if (handle == INVALID_HANDLE_VALUE)
        return;

    ::GetModuleFileName(NULL, szModuleName, sizeof(szModuleName)); //::AfxGetInstanceHandle()
    std::cout << "MOduleFileName:" << szModuleName << std::endl;
    //获取windows 版本
    OSVERSIONINFO os = {0};
    os.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    ::GetVersionEx(&os);

    //设置windows XP防火墙
    if (os.dwMajorVersion <= 5)
    {
        //csBatDetail = _T("net stop SharedAccess\r\n");//关闭windows XP的防火墙服务
        csBatDetail = std::string("netsh firewall add allowedprogram \"") + std::string(szModuleName) + std::string("\" \"IntevioApp\" ENABLE"); //将程序加入防火墙
    }

    //设置windows 7防火墙
    else
    {
        csBatDetail = std::string("net start MpsSvc\r\n");                                                                                                                             //启动windows 7的防火墙服务
        csBatDetail += std::string("netsh advfirewall firewall del rule name=\"IntevioApp In\" dir=in\r\n");                                                                           //删除策略
        csBatDetail += std::string("netsh advfirewall firewall add rule name=\"IntevioApp In\" dir=in program=\"") + std::string(szModuleName) + std::string("\" action=allow\r\n");   //添加策略
        csBatDetail += std::string("netsh advfirewall firewall del rule name=\"IntevioApp Out\" dir=out\r\n");                                                                         //删除策略
        csBatDetail += std::string("netsh advfirewall firewall add rule name=\"IntevioApp Out\" dir=out program=\"") + std::string(szModuleName) + std::string("\" action=allow\r\n"); //添加策略
    }

    if (!::WriteFile(handle, csBatDetail.c_str(), csBatDetail.size(), &dwWriteLen, NULL))
    {
        ::CloseHandle(handle);
        ::DeleteFile(csBatFileName.c_str());
        return;
    }
    else
    {
        ::CloseHandle(handle);

        std::cout << "run cmd:" << csBatFileName << std::endl;
        runCMD(csBatFileName, "", TRUE); //执行

        ::Sleep(200);

        ::DeleteFile(csBatFileName.c_str());
    }
#endif
}

bool SIPSTERPlatform::runCMD(std::string csExe, std::string csParam, bool bWait, unsigned long dwPriority)
{
#ifdef _WIN32
    SHELLEXECUTEINFO ShExecInfo;

    memset(&ShExecInfo, 0, sizeof(SHELLEXECUTEINFO));

    ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
    ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
    ShExecInfo.hwnd = NULL;
    ShExecInfo.lpVerb = NULL;
    ShExecInfo.lpFile = csExe.c_str();
    ShExecInfo.lpParameters = csParam.c_str();
    ShExecInfo.lpDirectory = NULL;
    ShExecInfo.nShow = SW_HIDE;
    ShExecInfo.hInstApp = NULL;

    if (!::ShellExecuteEx(&ShExecInfo))
        return false;

    if (ShExecInfo.hProcess > 0 && dwPriority != NORMAL_PRIORITY_CLASS)
    {
        ::SetPriorityClass(ShExecInfo.hProcess, dwPriority);
    }

    if (bWait)
    {
        ::WaitForSingleObject(ShExecInfo.hProcess, INFINITE);
        ::CloseHandle(ShExecInfo.hProcess);
        ShExecInfo.hProcess = NULL;
    }
#endif
    return true;
}

bool SIPSTERPlatform::verifyDog()
{
#ifdef USE_SUPER_DOG
#define CUSTOM_FILEID HONEYWELL_SUPER_DOG_ID
#define MAX_FILE_SIZE 128
    dog_status_t status;
    dog_handle_t handle;
    dog_size_t fsize = 0;
    unsigned char membuffer[MAX_FILE_SIZE + 1];
    memset(membuffer, 0x00, MAX_FILE_SIZE + 1);

    status = dog_login(0,
                       (dog_vendor_code_t *)vendorCode,
                       &handle);

    if (status != DOG_STATUS_OK)
    {
        return false;
    }

    status = dog_get_size(handle, CUSTOM_FILEID, &fsize);
    if (status != DOG_STATUS_OK)
    {
        dog_logout(handle);
        return false;
    }

    std::cout << "The size is " << fsize << " bytes" << std::endl;
    //If SuperDog holds default data file
    if (fsize <= 0)
    {
        dog_logout(handle);
        return false;
    }

    if (fsize > MAX_FILE_SIZE)
        fsize = MAX_FILE_SIZE;

    printf("\nreading %4d bytes from memory: ", fsize);
    status = dog_read(handle,
                      CUSTOM_FILEID, /* file ID */
                      0,             /* offset */
                      fsize,         /* length */
                      membuffer);    /* file data */
    
    dog_logout(handle);
    
    if (status != DOG_STATUS_OK)
    {
        std::cout << "Read data from super dog fail" << std::endl;
        return false;
    }

    std::string data((char *)membuffer);
    std::cout << "Read from superDog:" << data << std::endl;

    return data.find(HONEYWELL_SUPER_DOG_TAG) != std::string::npos;

#else
    return true;
#endif
}

void SIPSTERPlatform::sendState(std::string state)
{
    std::cout << "#####Platform state:" << state << std::endl;
    SETUP_EVENT(SYSTEMSTATUS);

    ev.system = this;
    args->state = state;

    ENQUEUE_EVENT(ev);
}

void SIPSTERPlatform::startDogVerifyLoop()
{
    mIsRunning = true;
    bool isDogInserted = true;
    int testCount = 0;
    while (mIsRunning)
    {
        if (!verifyDog())
        {
            if (testCount >= 3) {
                if (isDogInserted) {
                    isDogInserted = false;
                    sendState("dogUnverify");
                }

                testCount = 0;
            } else {
                ++testCount;
            }

            std::this_thread::sleep_for(std::chrono::seconds(DOG_VERIFY_FAIL_RETRY_TIME));
        } else {
            testCount = 0;
            if (!isDogInserted) {
                isDogInserted = true;
                sendState("dogVerify");
            }
            std::this_thread::sleep_for(std::chrono::seconds(DOG_VERIFY_SUCCEED_RETRY_TIME));
        }
    }
}

bool SIPSTERPlatform::tryToVerifyDog()
{
    for (int i = 0; i < 3; ++i) {
        if (verifyDog()) {
            return true;
        }
    }

    return false;
}

static void startPlatformThread(SIPSTERPlatform *platform)
{
    platform->startDogVerifyLoop();
}

bool SIPSTERPlatform::initSystem()
{
    if (!mSystemHaveInit)
    {
        allowPassFireWall();
        mDogVerifyThread = new std::thread(startPlatformThread, this);
        mSystemHaveInit = true;
    }

    return true;
}

NAN_METHOD(SIPSTERPlatform::Init)
{
    Nan::HandleScope scope;
    
    std::cout << "InitSystem" << std::endl;
    SIPSTERPlatform *platform = Nan::ObjectWrap::Unwrap<SIPSTERPlatform>(info.This());
    if (!platform->initSystem()) {
        return Nan::ThrowError("init super dog false");
    }

    info.GetReturnValue().SetUndefined();
}

NAN_GETTER(SIPSTERPlatform::VerifyDog)
{
    std::cout << "VerifyDog" << std::endl;
    SIPSTERPlatform *platform = Nan::ObjectWrap::Unwrap<SIPSTERPlatform>(info.This());
    info.GetReturnValue().Set(Nan::New(platform->tryToVerifyDog()));
}

NAN_METHOD(SIPSTERPlatform::New)
{
    Nan::HandleScope scope;

    std::cout << "SIPSTERPlatform::New" << std::endl;

    if (!info.IsConstructCall())
        return Nan::ThrowError("Use `new` to create instances of this object.");

    SIPSTERPlatform *sipPlatform = new SIPSTERPlatform();

    sipPlatform->Wrap(info.This());
    sipPlatform->Ref();

    sipPlatform->emit = new Nan::Callback(
        Local<Function>::Cast(sipPlatform->handle()->Get(Nan::New(emit_symbol))));

    std::cout << "SipPlatform::New finish" << std::endl;
    info.GetReturnValue().Set(info.This());
}

void SIPSTERPlatform::Initialize(Handle<Object> target)
{
    Nan::HandleScope scope;

    Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
    Local<String> name = Nan::New("SipPlatform").ToLocalChecked();

    tpl->InstanceTemplate()->SetInternalFieldCount(1);
    tpl->SetClassName(name);

    Nan::SetPrototypeMethod(tpl, "init", Init);

    Nan::SetAccessor(tpl->PrototypeTemplate(),
                   Nan::New("verifyDog").ToLocalChecked(),
                   VerifyDog);

    Nan::Set(target, name, tpl->GetFunction());

    SIPSTERPlatform_constructor.Reset(tpl);
}