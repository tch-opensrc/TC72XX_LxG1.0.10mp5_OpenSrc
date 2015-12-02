::------------------------------------------------------------------------------
:: Broadcom Canada Ltd., Copyright 2001 - 2003
::
:: Filename:  cxc_nbrt_start.bat
:: Purpose:   Startup script for CxC Nightly Build and Regression Testing
::------------------------------------------------------------------------------

echo ON

::Set the environment for CxC Nightly build/regression testing
call cxc_nbrt_setenv.bat

mkdir %CXNB_BASECC_VIEW_ROOT%\log
set CXNB_MAIN_LOGFILE=%CXNB_BASECC_VIEW_ROOT%\log\cxc_nbrt_main.log

echo "cxc_nbrt_start.bat started" > %CXNB_MAIN_LOGFILE%
echo. >> %CXNB_MAIN_LOGFILE%
call cxc_nbrt_main.bat >> %CXNB_MAIN_LOGFILE% 2>&1
echo ON
echo. >> %CXNB_MAIN_LOGFILE%
echo "cxc_nbrt_start.bat finished" >> %CXNB_MAIN_LOGFILE%

echo ""
echo "UPDATE & BUILD FINISHED !!!"
echo ""
sleep 10

exit