::------------------------------------------------------------------------------
:: Broadcom Canada Ltd., Copyright 2001 - 2003
::
:: Filename: cxc_nbrt_app.bat
:: Purpose:  Nightly Build and Regression testing application script 
::           (builds target application and runs tests on it)
:: Arguments: %1 - target CxC app to build and test
::            %2 - target OS 
::------------------------------------------------------------------------------

cmd.exe /c cxc_nbrt_build.bat %1 %2
:: cmd.exe /c cxc_nbrt_test.bat  %1 %2

