::------------------------------------------------------------------------------
:: Broadcom Canada Ltd., Copyright 2001 - 2003
::
:: Filename:  cxc_nbrt_main.bat
:: Purpose:   Main script for CxC Nightly Build and Regression Testing
::------------------------------------------------------------------------------


call cxc_nbrt_setenv.bat

bash -C lnx_nbrt_test.sh 1

