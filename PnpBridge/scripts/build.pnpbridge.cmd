@echo off

goto START

:Usage
echo Usage: build.prereq.cmd x86^|ARM^|x64 Debug^|Release [WindowsSDKVersion]
echo    WindowsSDKVersion............... Default is 10.0.17763.0, specify another version if necessary
echo    [/?].................... Displays this usage string.
echo    Example:
echo        build.pnpbridge.cmd x64 Debug 10.0.17763.0
endlocal
exit /b 1

:START
setlocal ENABLEDELAYEDEXPANSION

if [%1] == [/?] goto Usage
if [%1] == [-?] goto Usage

if [%1] == [] (
    set TARGETARCH=x64
) else (
    set TARGETARCH=%1
)

if /I [%TARGETARCH%] == [x86] (
    set TARGETPLATFORM=Win32
) else (
    set TARGETPLATFORM=%TARGETARCH%
)

if [%2] == [] (
    set TARGETCONFIG=Debug
) else (
    set TARGETCONFIG=%2
)

if [%3] == [] ( 
    set TARGETPLATVER=10.0.17763.0
) else (
    set TARGETPLATVER=%3
)

pushd %~dp0..

echo .
echo "Using CMAKE to set up Azure PnpBridge projects"
echo.

set OUTPUTDIR=%TARGETPLATFORM%_%TARGETCONFIG%

md %OUTPUTDIR%
pushd %OUTPUTDIR%
if /I [%TARGETARCH%] == [x86] (
cmake -G "Visual Studio 15 2017" .. -Duse_prov_client:BOOL=ON -Duse_tpm_simulator:BOOL=OFF -Dbuild_provisioning_service_client=ON -Duse_prov_client_core=ON -Dskip_samples=ON ..
)

if /I [%TARGETARCH%] == [arm] (
cmake -G "Visual Studio 15 2017 ARM" .. -Dskip_samples=ON ..
)

if /I [%TARGETARCH%] == [x64] (
cmake -G "Visual Studio 15 2017 Win64" .. -Duse_prov_client:BOOL=ON -Duse_tpm_simulator:BOOL=OFF -Dbuild_provisioning_service_client=ON -Duse_prov_client_core=ON -Dskip_samples=ON ..
)
popd


echo .
echo "Building Azure SDK libraries"
echo .

pushd %OUTPUTDIR%
msbuild %~dp0..\%OUTPUTDIR%\azure_iot_pnp_bridge.sln /p:Configuration=%TARGETCONFIG% /p:Platform=%TARGETPLATFORM% /p:TargetPlatformVersion=%TARGETPLATVER%
if errorlevel 1 goto BuildError

popd

goto Success

:BuildError
popd
@echo Error building project...
exit /b 1

:Success

