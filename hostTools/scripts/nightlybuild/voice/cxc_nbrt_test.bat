::------------------------------------------------------------------------------
:: Broadcom Canada Ltd., Copyright 2001 - 2003
::
:: Filename: cxc_nbrt_test.bat
:: Purpose:  Entry point for CxC Nightly Regression Testing 
:: Arguments: %1 - target platform to run the tests on
::            %2 - target OS to use for testing
::------------------------------------------------------------------------------

if "%1" == "" goto argsError
if "%2" == "" goto argsError

set cxrt_test_target=%1
set cxrt_test_os=%2

if "%cxrt_test_os%" == "vxworks" goto vxworks
echo "CXC_NBRT_TEST ERROR: Unsupported OS=%cxrt_test_os%"
goto theend

:vxworks

:: Setup nbrt environment variables specific for this target
call cxc_nbrt_cfg.bat %cxrt_test_target% %cxrt_test_os%

if "%CXNB_CFG_TEST%" == "no" goto theend

set TEST_SRV_COMPUTER_NAME=LBRMNA-CEXCH02

del /F /Q \\%TEST_SRV_COMPUTER_NAME%\cxnt\incoming\*

:: Check if there is an image for this platform ready after the build

if "%CXNB_CFG_TESTLABEL%" == "" set CXNB_CFG_TESTLABEL=%CXNB_LABEL%

if exist %CXNB_LOCAL_LOG_PATH%\%CXNB_CFG_TESTLABEL%\%cxrt_test_os%\vxram_sto_%cxrt_test_target%.bin goto copyToTestServer
echo "CXC_NBRT_TEST ERROR: No image %CXNB_OUTPUT_DIR%\%cxrt_test_os%\vxram_sto_%cxrt_test_target%.bin. Could not run Regression tests!"
goto theend

:copyToTestServer

:: copy the test image
copy /Y %CXNB_LOCAL_LOG_PATH%\%CXNB_CFG_TESTLABEL%\%cxrt_test_os%\vxram_sto_%cxrt_test_target%.bin \\%TEST_SRV_COMPUTER_NAME%\cxnt\incoming

:: create and copy the configuration file for regression testing
more cxc_nbrt_cfg.bat >cxc_rt_cfg.bat
more cxc_nbrt_setlabel.bat >>cxc_rt_cfg.bat
copy /Y cxc_rt_cfg.bat \\%TEST_SRV_COMPUTER_NAME%\cxnt\incoming

:: spawn the test on the remote test server
psexec \\%TEST_SRV_COMPUTER_NAME% -s -i c:\cxnt\spawnTestServer.bat %cxrt_test_target% vxram_sto_%cxrt_test_target%.bin

goto theend

:argsError
echo cxc_nbrt_test error: Insufficient arguments arg1=%1; arg2=%2;
goto theend

:theend
set cxrt_test_target=
set cxrt_test_os=
exit

