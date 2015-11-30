::------------------------------------------------------------------------------
:: Broadcom Canada Ltd., Copyright 2001 - 2003
::
:: Filename: cxc_nbrt_cfg.bat
:: Purpose:  Configuration file for CablexChange Nightly Build & Regression
::           Testing scripts
:: Arguments: %1 - CxC target to set the environment for
::            %2 - OS to set the environment for
::------------------------------------------------------------------------------
set cxc_nbrt_cfg_target=%1
set cxc_nbrt_cfg_os=%2

:: List of valid CxC app targets, separated by commas
set cxnb_cxc_target_list=bcm6345gw

::------------------------------------------------------------------------------
:: Default Target Build/Test configuration
::------------------------------------------------------------------------------
:: prebuild  - prebuild process (make deps, libraries, ...)
::       if prebuild=yes the following is also configurable
::       getsrc - get source from source control database
::       deps   - make CM-side dependencies
::       resolv - build MTA resolver library
::       hoth   - build hausware library
::
:: build  - Build the test images
::       if build=yes the following is also configurable
::       getsrc - get source from source control database
::       cmsym  - build symbol table image
::
:: test   - Test the test images
::       if test=yes the following is also configurable
::       testlabel - cxlogs subfolder to grab the images from, defaults to CXNB_LABEL
::                   (example: CXNB_CFG_TESTLABEL=03_05_07.15_25_56 will force the testing
::                    portion to use images form cxlogs/03_05_07.15_25_56 for testing)
::                   note: this is normaly used when CXNB_CFG_BUILD=no
::       abacus - Abacus based tests
::       pc     - PacketCable test scripts (Tcl)
::       data   - data only tests
::       datavoice - simultaneous data and voice tests
set CXNB_CFG_OUTPUTDIR=
set CXNB_CFG_PREBUILD=no
set CXNB_CFG_PREBUILD_GETSRC=yes
set CXNB_CFG_PREBUILD_DEPS=no
set CXNB_CFG_PREBUILD_RESOLV=no
set CXNB_CFG_PREBUILD_HOTH=no
set CXNB_CFG_BUILD=yes
set CXNB_CFG_BUILD_GETSRC=yes
set CXNB_CFG_BUILD_CMSYM=yes
set CXNB_CFG_TEST=no
set CXNB_CFG_TESTLABEL=
set CXNB_CFG_TEST_ABACUS=yes
set CXNB_CFG_TEST_PC=yes
set CXNB_CFG_TEST_DATA=yes
set CXNB_CFG_TEST_DATAVOICE=yes
set CXNB_CFG_SENDMAIL=yes
set CXNB_CFG_CC_CONFIGSPEC_UPDATE=yes

:: empty target indicates to use default config settings
if "%cxc_nbrt_cfg_target%" == "" goto theend

:: go to custom target configuration
for %%t in (%cxnb_cxc_target_list%) do if "%%t" == "%cxc_nbrt_cfg_target%" goto config_%%t

echo "cxc_nbrt_cfg ERROR: Unsuported cxc target=%cxc_nbrt_cfg_target%!"
set errorlevel=
set errorlevel 2>NUL:
goto :eof

::------------------------------------------------------------------------------
:: Custom Target Build/Test configurations 
:: (overrides default target configuration)
::------------------------------------------------------------------------------
:config_bcm3348vcm
set CXNB_CFG_OUTPUTDIR=bcm93348_propane
set CXNB_CFG_PREBUILD=yes
set CXNB_CFG_PREBUILD_RESOLV=yes
set CXNB_CFG_PREBUILD_DEPS=no
set CXNB_CFG_TEST=no
goto theend

:config_bcm3348vcm_euro
set CXNB_CFG_OUTPUTDIR=bcm93348_propane_eu
set CXNB_CFG_TEST=yes
goto theend

:config_bcm3348vcm_bv16
:config_bcm3348vcm_g723
:config_bcm3348vcm_g726
:config_bcm3348vcm_g729a
:config_bcm3348vcm_bv32
set CXNB_CFG_OUTPUTDIR=bcm93348_propane
goto theend

:config_bcm3351vcm
set CXNB_CFG_OUTPUTDIR=bcm93351_propane
set CXNB_CFG_PREBUILD_DEPS=yes
goto theend

:config_bcm3351svl
set CXNB_CFG_OUTPUTDIR=bcm93351_propane
goto theend

:config_bcm3352v_g723
:config_bcm3352v
set CXNB_CFG_OUTPUTDIR=bcm93352_propane
set CXNB_CFG_PREBUILD_DEPS=yes
goto theend

:theend
set cxc_nbrt_cfg_target=
set cxc_nbrt_cfg_os=





