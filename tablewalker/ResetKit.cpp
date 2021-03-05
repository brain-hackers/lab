/*
 * ResetKit
 * Soft/hard reset the electronic dictionary.
 * 
 * Copyright (C) 2012 T. Kawada <tcppjp [ at ] gmail.com>
 *
 * This file is licensed in MIT license.
 *
 * Permission is hereby granted, free of charge, to any person obtaining 
 * a copy of this software and associated documentation files (the "Software"), 
 * to deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or 
 * sell copies of the Software, and to permit persons to whom the Software is 
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in 
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN 
 * THE SOFTWARE.
 *
 */

#define RESETKIT_API extern "C" __declspec(dllexport)
#include "ResetKit.h"
#include "ResetKitHelper.h"
#include <string.h>

static bool g_initialized=false;
static HANDLE g_helperHandle=NULL;
static HINSTANCE g_hInstance;

static void initialize(){
    if(g_initialized)
        return;
    
    g_initialized=true;
}

static void getThisDllDirectoryPath(LPWSTR buffer){
	// retrive the path of the application.
	GetModuleFileName(g_hInstance, buffer, 512);
	
    const size_t notFound=(size_t)-1;
    size_t i=0;
    size_t j=notFound;
    
    while(buffer[i]){
        if(buffer[i]==L'/' || buffer[i]==L'\\'){
            j=i;
        }
        i++;
    }
    
    if(j==notFound)
        return;
    
    buffer[j]=0;
    
}

static bool isDriverLoaded(){
    HANDLE handle;
    handle=CreateFile(L"RKH0:", GENERIC_READ|GENERIC_WRITE|FILE_WRITE_ATTRIBUTES, 0, NULL, OPEN_EXISTING, 0, NULL);
    if(handle==INVALID_HANDLE_VALUE)
		handle=NULL;
    if(handle){
        CloseHandle(handle);
        return true;
    }
    return false;
}

static bool addDriverToRegistry(){
	HKEY hKey;
	DWORD dw;
	RegCreateKeyEx(HKEY_LOCAL_MACHINE, (L"Drivers\\BuiltIn\\ResetKitHelper"), 0, NULL, REG_OPTION_NON_VOLATILE, 0, NULL, &hKey, &dw);
	if(!hKey)
		return false;
	
	dw = 0x0000;
	if(RegSetValueEx(hKey, (L"Index"), 0, REG_DWORD, (BYTE*)&dw, sizeof(DWORD)) != ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		return false;
	}
	
	dw = 0x0100;
	if(RegSetValueEx(hKey, (L"Order"), 0, REG_DWORD, (BYTE*)&dw, sizeof(DWORD)) != ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		return false;
	}
	
	if(RegSetValueEx(hKey, (L"Dll"), 0, REG_SZ, (BYTE*)(L"\\Windows\\ResetKitHelper.dll"), sizeof(TCHAR) * ((DWORD)wcslen((L"\\Windows\\ResetKitHelper.dll")) + 1)) != ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		return false;
	}
	
	if(RegSetValueEx(hKey, (L"Prefix"), 0, REG_SZ, (BYTE*)(L"RKH"), sizeof(TCHAR) * ((DWORD)wcslen((L"RKH")) + 1)) != ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		return false;
	}
	
	return true;
}

static void loadDriverIfNeeded(){
    if(isDriverLoaded())
        return;
	
	OutputDebugString(L"ResetKit: installing ResetKitHelper...");
	
	DWORD err;
    HANDLE driver=NULL;
	
    wchar_t driverPath[1024];
    getThisDllDirectoryPath(driverPath);
    wcscat(driverPath, L"\\ResetKitHelper.dll");
	
	LPCWSTR newDriverPath;
	newDriverPath=L"\\Windows\\ResetKitHelper.dll";
	
    {
        wchar_t buf[1024];
        swprintf(buf, L"ResetKit: copying \"%ls\" to \"%ls\"",
                 driverPath, newDriverPath);
        OutputDebugString(buf);
    }
	
	if(!CopyFile(driverPath,
				 newDriverPath, FALSE)){
	
		
		if(GetFileAttributes(newDriverPath)==(DWORD)-1){
			OutputDebugString(L"ResetKit: failed to copy");
			return;
		}
        
	}
	/*
	addDriverToRegistry();
	OutputDebugString(L"ResetKit: activating ResetKitHelper...");
	ActivateDevice(L"Drivers\\ResetKitHelper", NULL);
	{
		wchar_t buf[256];
		DWORD err=GetLastError();
		if(err){
			swprintf(buf, L"ResetKit: ActivateDevice error: 0x%08x", (int)err);
			OutputDebugString(buf);
		}
	}
	*/
	
	OutputDebugString(L"ResetKit: registering ResetKitHelper...");
	
	try{
		driver=RegisterDevice(L"RKH", 0, L"\\Windows\\ResetKitHelper.dll", 0);
		
		if(driver==INVALID_HANDLE_VALUE)
			driver=NULL;
		
		if(!driver){
			
			driver=RegisterDevice(L"RKH", 0, L"ResetKitHelper.dll", 0);
			
			if(driver==INVALID_HANDLE_VALUE)
				driver=NULL;
			
		}
		
		err=GetLastError();
	}catch(DWORD e){
		err=e;
	}
	if(!driver && (err!=0x964)){
		OutputDebugString(L"ResetKit: failed to install...");
		return;
	}
	
	//addDriverToRegistry();
	

}

static void openDriver(){
    if(g_helperHandle)
        return;
    g_helperHandle=CreateFile(L"RKH0:", GENERIC_READ|GENERIC_WRITE|FILE_WRITE_ATTRIBUTES, 0, NULL, OPEN_EXISTING, 0, NULL);
	if(g_helperHandle==INVALID_HANDLE_VALUE)
		g_helperHandle=NULL;
}

extern "C" BOOL APIENTRY DllMain(HINSTANCE hInstance, 
                                           DWORD  ul_reason_for_call, 
                                           LPVOID lpReserved
                                           )
{
    switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		case DLL_THREAD_ATTACH:
            
            g_hInstance=hInstance;
            
            initialize();
            loadDriverIfNeeded();
            openDriver();
			
		case DLL_THREAD_DETACH:
		case DLL_PROCESS_DETACH:
			break;
    }
    return TRUE;
}


RESETKIT_API BOOL RKDoSoftReset(){
	
	initialize();
    loadDriverIfNeeded();
    openDriver();
	
    return DeviceIoControl(g_helperHandle, IOCTL_RKH_DO_SOFT_RESET,
                           NULL, 0, NULL, 0,
                           NULL, NULL);
}
