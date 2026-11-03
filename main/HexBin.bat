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
:: echo "output_path:%output_path%"
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

@REM 软件版本文件路径
set VERSION_FILE_PATH=%CURRENT_DIR%\INCLUDE\main.h

@REM 获取软件版本
::for /f "tokens=3" %%a in ('findstr /C:%SOFTWARE_VERSION% %VERSION_FILE_PATH%') do set SW_Ver=%%a
for /f "tokens=3" %%a in ('findstr /i /c:"#define SOFT_NO_STR" "%VERSION_FILE_PATH%"') do (
    set sw_ver=%%a
)
:: 去除双引号（如把"V1.2.0"转为V1.2.0）
set sw_ver=%sw_ver:"=%
:: 去除左右括号
set sw_ver=%sw_ver:(=%
set sw_ver=%sw_ver:)=%
@REM echo "sw_ver=%sw_ver%"
    
@REM 获取硬件版本
for /f "tokens=3" %%b in ('findstr /i /c:"#define HARD_NO_STR" "%VERSION_FILE_PATH%"') do (
    set hw_ver=%%b
)
set hw_ver=%hw_ver:"=%
:: 去除左右括号
set hw_ver=%hw_ver:(=%
set hw_ver=%hw_ver:)=%
::echo "hw_ver=%hw_ver%"

::设置重命名的文件名
set rename_name=%hw_ver%-%sw_ver%
::echo "rename_name..%rename_name%.hex"

::将hex文件重命名
::echo "Output Hex file: ..\HexBin\%rename_name%.hex"
ren "%output_path%\%output_name%.hex" "%rename_name%.hex" >nul
::将BIN文件重命名
::echo "Output Bin file: ..\HexBin\%rename_name%.hex"
ren "%output_path%\%output_name%.bin" "%rename_name%.bin" >nul

@REM 生成带CRC校验的最终升级文件(_crc.bin)
@REM 格式: 每1024字节数据 + 2字节CRC16-MODBUS(大端, 多项式0xA001, 初值0xFFFF); 不足1024字节用0x00补齐
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=New-Object int[] 256; for($i=0;$i -lt 256;$i++){ $v=$i; for($j=0;$j -lt 8;$j++){ if($v -band 1){ $v=(($v -shr 1) -bxor 0xA001) } else { $v=$v -shr 1 } }; $t[$i]=$v }; $b=[IO.File]::ReadAllBytes('%output_path%\%rename_name%.bin'); $o=New-Object IO.MemoryStream; for($k=0;$k -lt [Math]::Ceiling($b.Length/1024);$k++){ $blk=New-Object byte[] 1024; $n=[Math]::Min(1024,$b.Length-$k*1024); if($n -gt 0){ [Array]::Copy($b,$k*1024,$blk,0,$n) }; $crc=0xFFFF; for($i=0;$i -lt 1024;$i++){ $crc=(($crc -shr 8) -bxor $t[($crc -bxor $blk[$i]) -band 0xFF]) }; $o.Write($blk,0,1024); $o.WriteByte(($crc -shr 8) -band 0xFF); $o.WriteByte($crc -band 0xFF) }; [IO.File]::WriteAllBytes('%output_path%\%rename_name%_crc.bin',$o.ToArray()); $o.Close()"

@REM OTA服务器地址(save.h)
set SAVE_FILE_PATH=%CURRENT_DIR%APP\USER\inc\save.h
for /f "tokens=3" %%a in ('findstr /i /c:"#define DEFALUT_OTA_IP0" "%SAVE_FILE_PATH%"') do set ota_ip0=%%a
for /f "tokens=3" %%a in ('findstr /i /c:"#define DEFALUT_OTA_IP1" "%SAVE_FILE_PATH%"') do set ota_ip1=%%a
for /f "tokens=3" %%a in ('findstr /i /c:"#define DEFALUT_OTA_IP2" "%SAVE_FILE_PATH%"') do set ota_ip2=%%a
for /f "tokens=3" %%a in ('findstr /i /c:"#define DEFALUT_OTA_IP3" "%SAVE_FILE_PATH%"') do set ota_ip3=%%a
for /f "tokens=3" %%a in ('findstr /i /c:"#define DEFALUT_OTA_PORT" "%SAVE_FILE_PATH%"') do set ota_port=%%a
set ota_ip0=%ota_ip0:(=%
set ota_ip0=%ota_ip0:)=%
set ota_ip1=%ota_ip1:(=%
set ota_ip1=%ota_ip1:)=%
set ota_ip2=%ota_ip2:(=%
set ota_ip2=%ota_ip2:)=%
set ota_ip3=%ota_ip3:(=%
set ota_ip3=%ota_ip3:)=%
set ota_port=%ota_port:(=%
set ota_port=%ota_port:)=%

@REM 生成 OTA info.txt
set info_version=%rename_name%
set info_url=http://%ota_ip0%.%ota_ip1%.%ota_ip2%.%ota_ip3%:%ota_port%/%hw_ver%/%rename_name%_crc.bin
echo {"version":"%info_version%";"url":"%info_url%";}> "%output_path%\info.txt"









