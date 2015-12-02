::------------------------------------------------------------------------------
:: Broadcom Canada Ltd., Copyright 2001 - 2003
::
:: Filename: cxc_nbrt_build.bat
:: Purpose:  CxC Nightly Build script (builds and saves CxC images)
:: Arguments: %1 - target CxC app to build
::            %2 - target OS to build the app for
::------------------------------------------------------------------------------

if "%2" == "" goto argsError

set cxnb_build_target=%1
set cxnb_build_os=%2

:: Setup nbrt environment variables specific for this target
call cxc_nbrt_cfg.bat %cxnb_build_target% %cxnb_build_os%

if "%CXNB_CFG_BUILD%" == "no" goto theend

bash -C lnx_nbrt_build.sh >> %CXNB_OUTPUT_DIR%\build.log

goto theend

:argsError
echo cxc_nbrt_build error: Insufficient arguments arg1=%1; arg2=%2; >>%CXNB_OUTPUT_DIR%\build.log
goto theend

:theend
set cxnb_build_target=
set cxnb_build_os=
set cxnb_local_output_dir=
exit

