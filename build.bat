@echo off
REM L4G AIMS - Master Build Script
REM Builds: Panel (C++), Driver (C++), C2 Server (JS), Android APK (Kotlin/Gradle)
REM Output: ..\output\L4G_Panel.exe, ..\output\L4G_Remote.apk

setlocal enabledelayedexpansion

echo ============================================
echo L4G AIMS v14.0 - BUILD STARTED
echo ============================================

set ROOT=%~dp0
set OUTPUT_DIR=%ROOT%..\output
set PANEL_DIR=%ROOT%panel
set DRIVER_DIR=%ROOT%driver
set C2_DIR=%ROOT%c2_server
set ANDROID_DIR=%ROOT%android

if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"

REM ============================================
REM 1. BUILD PANEL (C++ with CMake + MSVC)
REM ============================================
echo.
echo [1/4] Building Panel (C++)...
echo --------------------------------------------

cd /d "%PANEL_DIR%"
if not exist "build" mkdir build
cd build

cmake .. -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_CXX_FLAGS_RELEASE="/O2 /MT /GL /Gy" ^
  -DCMAKE_EXE_LINKER_FLAGS_RELEASE="/LTCG /OPT:REF /OPT:ICF /SUBSYSTEM:WINDOWS /ENTRY:wWinMainCRTStartup"

if errorlevel 1 (
    echo [ERROR] CMake configure failed
    goto :EOF
)

cmake --build . --config Release --parallel

if errorlevel 1 (
    echo [ERROR] Panel build failed
    goto :EOF
)

copy /Y "Release\L4G_Panel.exe" "%OUTPUT_DIR%\L4G_Panel.exe" >nul
echo [OK] Panel built: %OUTPUT_DIR%\L4G_Panel.exe

REM ============================================
REM 2. BUILD DRIVER (C++ Kernel Mode)
REM ============================================
echo.
echo [2/4] Building Driver (Kernel)... 
echo --------------------------------------------

cd /d "%DRIVER_DIR%"
if not exist "build" mkdir build
cd build

REM Requires WDK installed
cmake .. -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_BUILD_TYPE=Release

if errorlevel 1 (
    echo [WARN] Driver CMake configure failed (WDK may not be installed)
) else (
    cmake --build . --config Release
    if not errorlevel 1 (
        copy /Y "Release\L4G_Driver.sys" "%OUTPUT_DIR%\L4G_Driver.sys" >nul
        echo [OK] Driver built: %OUTPUT_DIR%\L4G_Driver.sys
    ) else (
        echo [WARN] Driver build failed
    )
)

REM ============================================
REM 3. BUILD C2 SERVER (Cloudflare Workers)
REM ============================================
echo.
echo [3/4] Building C2 Server (Cloudflare Workers)...
echo --------------------------------------------

cd /d "%C2_DIR%"
if exist "package.json" (
    npm install --production 2>nul
    npx wrangler deploy --env production 2>nul
    if not errorlevel 1 (
        echo [OK] C2 Server deployed
    ) else (
        echo [WARN] C2 Server deploy failed (wrangler not configured)
    )
) else (
    echo [INFO] C2 Server source ready for manual deploy
)

REM ============================================
REM 4. BUILD ANDROID APK (Gradle)
REM ============================================
echo.
echo [4/4] Building Android APK...
echo --------------------------------------------

cd /d "%ANDROID_DIR%"

REM Check for gradle wrapper
if not exist "gradlew.bat" (
    echo [WARN] Gradle wrapper not found, initializing...
    gradle wrapper --gradle-version 8.5 2>nul
)

call gradlew.bat clean assembleRelease

if errorlevel 1 (
    echo [ERROR] APK build failed
    goto :EOF
)

copy /Y "app\build\outputs\apk\release\app-release.apk" "%OUTPUT_DIR%\L4G_Remote.apk" >nul
echo [OK] APK built: %OUTPUT_DIR%\L4G_Remote.apk

REM ============================================
REM DONE
REM ============================================
echo.
echo ============================================
echo BUILD COMPLETE
echo ============================================
echo.
echo Output files:
echo   %OUTPUT_DIR%\L4G_Panel.exe
echo   %OUTPUT_DIR%\L4G_Remote.apk
if exist "%OUTPUT_DIR%\L4G_Driver.sys" echo   %OUTPUT_DIR%\L4G_Driver.sys
echo.
echo Next steps:
echo   1. Upload C2 server: cd c2_server && npx wrangler deploy
echo   2. Test panel: %OUTPUT_DIR%\L4G_Panel.exe
echo   3. Install APK: adb install %OUTPUT_DIR%\L4G_Remote.apk
echo   4. One-liner installer: Upload installer/ to GitHub
echo ============================================