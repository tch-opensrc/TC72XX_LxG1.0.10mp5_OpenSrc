::------------------------------------------------------------------------------
:: Broadcom Canada Ltd., Copyright 2001 - 2003
::
:: Filename: cxc_nbrt_prebuild.bat
:: Purpose:  CxC Nightly pre-build script (makes dependancies and libraries)
:: Arguments: %1 - target CxC app to pre-build
::            %2 - target OS to pre-build the app for
::------------------------------------------------------------------------------

if "%2" == "" goto argsError

set cxnb_preapp_target=%1
set cxnb_preapp_os=%2

:: Setup nbrt environment variables specific for this target
call cxc_nbrt_cfg.bat %cxnb_preapp_target% %cxnb_preapp_os%

if "%CXNB_CFG_PREBUILD%" == "no" goto skipprebuild

set cxnb_prebuild_output_dir=%CXNB_OUTPUT_DIR%\%cxnb_preapp_os%\deps
mkdir %cxnb_prebuild_output_dir%

:: Set the CxC build environment
cd /D %CXNB_LOCAL_SRC_PATH%\cablex
call gen.bat %cxnb_preapp_os% %CXNB_TORNADO_PATH% 
call set_%cxnb_preapp_os%.bat

if "%CXNB_CFG_PREBUILD_DEPS%" == "no" goto skipdeps

:: Check-out target dependancies
cd /D %CXNB_LOCAL_SRC_PATH%\cablex\src\cm_v2\CMAPP_DOCSIS1.0\%cxnb_preapp_os%
cd /D %CXNB_CFG_OUTPUTDIR%
copy /Y makefile.deps makefile.old
del /F makefile.deps

:: *** Make the CM deps for CxC app ***
cd /D %CXNB_LOCAL_SRC_PATH%\cablex\apps\%cxnb_preapp_target%
echo Started dependancy make for %cxnb_preapp_target% %cxnb_preapp_os% >>%CXNB_OUTPUT_DIR%\build.log
make cm_deps >%cxnb_prebuild_output_dir%\%cxnb_preapp_target%_propane_deps.txt 2>&1
echo Completed dependancy make for %cxnb_preapp_target% %cxnb_preapp_os% >>%CXNB_OUTPUT_DIR%\build.log

:: Save and check-in the new target dependancies
cd /D %CXNB_LOCAL_SRC_PATH%\cablex\src\cm_v2\CMAPP_DOCSIS1.0\%cxnb_preapp_os%\%CXNB_CFG_OUTPUTDIR%
copy /Y makefile.deps %cxnb_prebuild_output_dir%\makefile_%cxnb_preapp_target%_propane.deps
if not exist makefile.old goto skipDepsCheckin
if not exist makefile.deps goto skipDepsCheckin

:: Assume the dependencies have not changed
set cxnb_makefile_deps_different=no

:: Compare the new deps with the old ones, if they differ set the flag
cleardiff -status_only -blank_ignore makefile.deps makefile.old || set cxnb_makefile_deps_different=yes

:: Checkin the new deps if its different
if "%cxnb_makefile_deps_different%" == "no" goto skipDepsCheckin

:: Checkin the new CxC app dependencies
mkdir %CXNB_LOCAL_VIEW_PATH%\cablex\src\cm_v2\CMAPP_DOCSIS1.0\%cxnb_preapp_os%\%CXNB_CFG_OUTPUTDIR%
cd /D %CXNB_LOCAL_VIEW_PATH%\cablex\src\cm_v2\CMAPP_DOCSIS1.0\%cxnb_preapp_os%\%CXNB_CFG_OUTPUTDIR%
cleartool setactivity -nc Nightly_CXNB_%CXNB_UCMCC_PROJECT%_Updates
cleartool checkout -nc -nquery makefile.deps
cp -f %CXNB_LOCAL_SRC_PATH%\cablex\src\cm_v2\CMAPP_DOCSIS1.0\%cxnb_preapp_os%\%CXNB_CFG_OUTPUTDIR%\makefile.deps .
cleartool checkin -nc makefile.deps

:: Set increment baseline flag
set CXNB_UPDATE_BASELINE="yes"
:skipDepsCheckin

:: Post-process dependancies
cd /D %cxnb_prebuild_output_dir%
bash -C cxwarnerrorgrep.sh %cxnb_preapp_target%_propane_deps makefile_%cxnb_preapp_target%_propane.deps
goto resolv

:skipdeps
echo Skiping dependancy build for %cxnb_preapp_target% %cxnb_preapp_os% >>%CXNB_OUTPUT_DIR%\build.log

:resolv
if "%CXNB_CFG_PREBUILD_RESOLV%" == "no" goto skipresolv

:: Check-out resolver library
cd /D %CXNB_LOCAL_SRC_PATH%\cablex\lib\%cxnb_preapp_os%
copy /Y libresolv.a libresolv.old

:: Build the resolver library
cd /D %CXNB_LOCAL_SRC_PATH%\cablex\apps\%cxnb_preapp_target%
make clean_resolv  >%cxnb_prebuild_output_dir%\mta_resolv.txt 2>&1
echo Started resolver library make for %cxnb_preapp_target% %cxnb_preapp_os% >>%CXNB_OUTPUT_DIR%\build.log
make resolv >>%cxnb_prebuild_output_dir%\mta_resolv.txt 2>&1
echo Completed resolver library make for %cxnb_preapp_target% %cxnb_preapp_os% >>%CXNB_OUTPUT_DIR%\build.log

:: Save and check-in the new resolver library
cd /D %CXNB_LOCAL_SRC_PATH%\cablex\lib\%cxnb_preapp_os%
copy /Y libresolv.a %cxnb_prebuild_output_dir%\libresolv.a
if not exist libresolv.a goto skipResolvCheckin
if not exist libresolv.old goto skipResolvCheckin

:: Assume the library has not changed
set cxnb_resolv_different=no

:: Compare the new lib with the old one, if they differ set the flag
rawcmp.exe libresolv.a  libresolv.old || set cxnb_resolv_different=yes

:: Checkin the new lib if its different
if "%cxnb_resolv_different%" == "no" goto skipResolvCheckin

:: Checkin the new resolver library
mkdir %CXNB_LOCAL_VIEW_PATH%\cablex\lib\VxWorks
cd /D %CXNB_LOCAL_VIEW_PATH%\cablex\lib\VxWorks
cleartool setactivity -nc Nightly_CXNB_%CXNB_UCMCC_PROJECT%_Updates
cleartool checkout -nc -nquery libresolv.a
cp -f %CXNB_LOCAL_SRC_PATH%\cablex\lib\%cxnb_preapp_os%\libresolv.a .
cleartool checkin -nc libresolv.a

:: Set increment baseline flag
set CXNB_UPDATE_BASELINE="yes"
:skipResolvCheckin

:: Post-process resolver build
cd /D %cxnb_prebuild_output_dir%
bash -C cxwarnerrorgrep.sh mta_resolv libresolv.a
goto theend

:argsError
echo cxc_nbrt_prebuild error: Insufficient arguments arg1=%1; arg2=%2; >>%CXNB_OUTPUT_DIR%\build.log
goto theend

:skipprebuild
echo Skiping prebuild for %cxnb_preapp_target% %cxnb_preapp_os% >>%CXNB_OUTPUT_DIR%\build.log
goto theend

:skipresolv
echo Skiping resolver build for %cxnb_preapp_target% %cxnb_preapp_os% >>%CXNB_OUTPUT_DIR%\build.log
goto theend

:theend
set cxnb_preapp_target=
set cxnb_preapp_os=
set cxnb_prebuild_output_dir=
set cxnb_resolv_different=
exit
