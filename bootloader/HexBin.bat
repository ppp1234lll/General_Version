::放在主文件目录下，执行
::..\..\HexBin.bat "$K" "!L" "@L" "$L"
::注意：4个参数必须加双引号，否则路径含空格或中文时会错位（如“General_Version - 485控制器”）
::注意hex/dis/map文件所在目录

@echo off
@REM 获取脚本当前目录
set CURRENT_DIR=%~dp0
::echo "CURRENT_DIR:%CURRENT_DIR%"

::创建输出文件夹 Debug
set output_dir=Debug

:: 拼接完整路径（当前目录+文件夹名）
set "output_path=%CURRENT_DIR%%output_dir%"
echo "output_path:%output_path%"
if not exist "%output_path%" md "%output_path%"

::设置fromelf.exe位置
set exe_location=%~1ARM\ARMCC\bin\fromelf.exe
::echo "exe_location:%exe_location%"

::设置.axf文件的位置
set "axf_location=%~2"
::echo "axf_location:%axf_location%"

::获取工程名
set "project_name=%~3"
::echo "project_name:%project_name%"

::设置.axf文件所在目录路径
set "axf_path=%~4"
::echo "axf_path:%axf_path%"

::设置.map文件所在目录路径
set map_path=%axf_path%
::echo "map_path:%map_path%"

::设置.hex文件所在目录路径
set hex_path=%axf_path%
::echo "hex_path:%hex_path%"

::设置输出后的文件名
set output_name=%project_name%
::echo "output_name:%output_name%"

::echo "Clean old files..."
if exist "%output_path%\*.hex" del /q "%output_path%\*.hex"
if exist "%output_path%\*.bin" del /q "%output_path%\*.bin"

@REM GET DIS
::echo "Output dis file: %output_path%\%output_name%.dis"
"%exe_location%" --text -a -c --output="%output_path%\%output_name%.dis" "%axf_location%" >nul

::将bin文件生成到Debug文件夹  >nul屏蔽成功命令
"%exe_location%" --bin -o "%output_path%\%output_name%.bin" "%axf_location%" >nul

::将hex文件复制到HexBin文件夹
copy "%hex_path%\%project_name%.hex" "%output_path%" >nul

::将axf文件复制到HexBin文件夹
copy "%axf_location%" "%output_path%" >nul

::将map文件复制到HexBin文件夹
copy "%map_path%\%project_name%.map" "%output_path%" >nul
@REM echo "Output map file: %output_path%\%output_name%.map"

@REM @REM 软件版本文件路径
@REM set VERSION_FILE_PATH=%CURRENT_DIR%\INCLUDE\appconfig.h

@REM @REM 获取软件版本
@REM ::for /f "tokens=3" %%a in ('findstr /C:%SOFTWARE_VERSION% %VERSION_FILE_PATH%') do set SW_Ver=%%a
@REM for /f "tokens=3" %%a in ('findstr /i /c:"#define SOFT_NO_STR" "%VERSION_FILE_PATH%"') do (
@REM     set sw_ver=%%a
@REM )
@REM :: 去除双引号（如把"V1.2.0"转为V1.2.0）
@REM set sw_ver=%sw_ver:"=%
@REM :: 去除左右括号
@REM set sw_ver=%sw_ver:(=%
@REM set sw_ver=%sw_ver:)=%
@REM ::echo "sw_ver=%sw_ver%"
    
@REM @REM 获取硬件版本
@REM for /f "tokens=3" %%b in ('findstr /i /c:"#define HARD_NO_STR" "%VERSION_FILE_PATH%"') do (
@REM     set hw_ver=%%b
@REM )
@REM set hw_ver=%hw_ver:"=%
@REM :: 去除左右括号
@REM set hw_ver=%hw_ver:(=%
@REM set hw_ver=%hw_ver:)=%
@REM ::echo "hw_ver=%hw_ver%"

@REM ::设置重命名的文件名
@REM set rename_name=%hw_ver%-%sw_ver%
@REM ::echo "rename_name..%rename_name%.hex"

::将hex文件重命名
::echo "Output Hex file: ..\HexBin\%rename_name%.hex"
ren "%output_path%\%output_name%.hex" "bootloader.hex" >nul
::将BIN文件重命名
::echo "Output Bin file: ..\HexBin\%rename_name%.hex"
ren "%output_path%\%output_name%.bin" "bootloader.bin" >nul









