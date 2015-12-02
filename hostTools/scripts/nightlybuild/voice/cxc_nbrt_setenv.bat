::------------------------------------------------------------------------------
:: Broadcom Canada Ltd., Copyright 2001 - 2003
::
:: Filename: cxc_nbrt_setenv.bat
:: Purpose:  Setting up of environment variables used in the other 
::           batch files and shell scripts. 
::
:: NOTE: This is probably the only file that you will need
:: to modify to enable nightly builds.
::------------------------------------------------------------------------------ 

set CXNB_UCMCC_PROJECT=dslx_common
set CXNB_BASECC_PROJECT=CommEngine
set CXNB_LABEL_SUFFIX=dslx

:: CUSTOMIZE 
:: Enter the name or the IP address of the nightly build computer, 
:: the username and password, 
:: and the root password for the test computer
SET UNIX_TESTCOMPUTER=ip_address_of_the_unix_box 
set UNIX_USERNAME=username_for_the_unix_box
set UNIX_USERPASS=password_for_the_unix_box
set UNIX_ROOTPASS=root_password_for_the_unix_box

:: CUSTOMIZE 
:: Enter the root of the CommEngine view (one level above CommEngine directory) in MSDOS style
set CXNB_BASECC_VIEW_ROOT=\\%UNIX_TESTCOMPUTER%\vmarkovski\views\nb_view

:: CUSTOMIZE 
:: Enter the path to the nightly build tools (blat, psexec, etc.)
set CXNB_TOOLS_PATH=c:\nb\tools

:: CUSTOMIZE
:: Enter the path of the directory from where 
:: the nightly build scripts are launched
set CXNB_RUN_PATH=V:\views\irvine_view\%CXNB_BASECC_PROJECT%\hostTools\scripts\nightlybuild\voice

:: CUSTOMIZE 
:: Make sure bash, build scripts and ldx_tools are in the PATH
set PATH=c:\cygwin\bin;%PATH%;%CXNB_RUN_PATH%;%CXNB_LDX_TOOLS_PATH%;%CXNB_TOOLS_PATH%

:: CUSTOMIZE 
:: Enter the root of the CommEngine view (one level above CommEngine directory) in UNIX style
set UNIX_CXNB_BASECC_VIEW_ROOT=/home/vmarkovski/views/nb_view

set UNIX_CXNB_BASECC_LOCAL_VIEW_PATH=%UNIX_CXNB_BASECC_VIEW_ROOT%/%CXNB_BASECC_PROJECT%
set UNIX_CXNB_BASECC_LOCAL_SRC_PATH=%UNIX_CXNB_BASECC_VIEW_ROOT%/%CXNB_BASECC_PROJECT%_build

:: Variables for base ClearCase projects
set CXNB_BASECC_LOCAL_VIEW_PATH=%CXNB_BASECC_VIEW_ROOT%\%CXNB_BASECC_PROJECT%
set CXNB_BASECC_LOCAL_SRC_PATH=%CXNB_BASECC_VIEW_ROOT%\%CXNB_BASECC_PROJECT%_build
set CXNB_BASECC_LRULES_PATH=%CXNB_RUN_PATH%\ccLoadRules\%CXNB_BASECC_PROJECT%
set CXNB_BASECC_LOCAL_LOG_PATH=%CXNB_BASECC_VIEW_ROOT%\log\%CXNB_BASECC_PROJECT%

:: Variables for UCM ClearCase projects
:: CUSTOMIZE
:: For the variable CXNB_LOCAL_VIEW_PATH, enter to root of the UCM view
set CXNB_LOCAL_VIEW_PATH=%CXNB_BASECC_LOCAL_VIEW_PATH%\xChange\rmna_nb_%CXNB_UCMCC_PROJECT%_integration_sv
set CXNB_LOCAL_SRC_PATH=%CXNB_BASECC_LOCAL_SRC_PATH%\xChange\%CXNB_UCMCC_PROJECT%
set CXNB_CC_LRULES_PATH=%CXNB_RUN_PATH%\ccLoadRules\%CXNB_UCMCC_PROJECT%
set CXNB_LOCAL_LOG_PATH=%CXNB_BASECC_VIEW_ROOT%\log\%CXNB_UCMCC_PROJECT%

:: Initialize baseline update flag to no
set CXNB_UPDATE_BASELINE="no"

