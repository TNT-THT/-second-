@echo off
setlocal DisableDelayedExpansion
rem PetDesk starter: only the pinned, hash-checked official Electron archive is downloaded.
set "PD_ROOT=%~dp0"
if not exist "%PD_ROOT%app\dist\index.html" goto :extract
set "PD_VER=@VERSION@"
set "PD_ARCH=x64"
if /i "%PROCESSOR_ARCHITECTURE%"=="ARM64" set "PD_ARCH=arm64"
if /i "%PROCESSOR_ARCHITEW6432%"=="ARM64" set "PD_ARCH=arm64"
set "PD_SHA=@WIN_X64@"
if "%PD_ARCH%"=="arm64" set "PD_SHA=@WIN_ARM64@"
set "PD_CACHE=%LOCALAPPDATA%\PetDesk\runtime-%PD_VER%-%PD_ARCH%"
if exist "%PD_CACHE%\electron.exe" if exist "%PD_CACHE%\.verified" goto :launch
if not exist "%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" goto :failed
echo First launch: downloading official Electron %PD_VER% from GitHub, then verifying SHA-256.
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoLogo -NoProfile -Command "$ErrorActionPreference='Stop'; [Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12; $ProgressPreference='SilentlyContinue'; $base=Split-Path -Parent $env:PD_CACHE; [void][IO.Directory]::CreateDirectory($base); $id=[guid]::NewGuid().ToString('N'); $zip=Join-Path $base ($id+'.zip'); $stage=Join-Path $base $id; $url='https://github.com/electron/electron/releases/download/v'+$env:PD_VER+'/electron-v'+$env:PD_VER+'-win32-'+$env:PD_ARCH+'.zip'; try { Invoke-WebRequest -UseBasicParsing -Uri $url -OutFile $zip; if((Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash -ne $env:PD_SHA){throw 'Runtime checksum mismatch. Not launching.'}; Expand-Archive -LiteralPath $zip -DestinationPath $stage; [IO.File]::WriteAllText((Join-Path $stage '.verified'),$env:PD_SHA); [IO.Directory]::Move($stage,$env:PD_CACHE) } finally { if([IO.File]::Exists($zip)){[IO.File]::Delete($zip)} }"
if errorlevel 1 goto :failed
:launch
rem Create an optional GUI-only shortcut here (not in Startup or on the user's desktop).
if not exist "%PD_ROOT%PetDesk.lnk" "%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoLogo -NoProfile -Command "$w=New-Object -ComObject WScript.Shell; $s=$w.CreateShortcut((Join-Path $env:PD_ROOT 'PetDesk.lnk')); $s.TargetPath=(Join-Path $env:PD_CACHE 'electron.exe'); $s.Arguments=([char]34).ToString()+(Join-Path $env:PD_ROOT 'app')+[char]34; $s.WorkingDirectory=$env:PD_ROOT; $s.Save()"
start "" "%PD_CACHE%\electron.exe" "%PD_ROOT%app"
exit /b 0
:extract
echo Please extract the entire PetDesk ZIP first. Do not run this script inside a ZIP viewer.
pause
exit /b 1
:failed
echo Setup stopped. Check the GitHub connection, disk space and the error above. No application was launched.
pause
exit /b 1
