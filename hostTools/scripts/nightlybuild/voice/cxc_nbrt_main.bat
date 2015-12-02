::------------------------------------------------------------------------------
:: Broadcom Canada Ltd., Copyright 2001 - 2003
::
:: Filename:  cxc_nbrt_main.bat
:: Purpose:   Main script for CxC Nightly Build and Regression Testing
::------------------------------------------------------------------------------


:: Generate a set file to setup logging and output folder
bash -C cxc_nbrt_genlabel.sh

:: Run the set file to setup logging and output folder
call cxc_nbrt_setlabel.bat

::Load the Default Config settings for CxC Nightly build/regression testing
call cxc_nbrt_cfg.bat

:: Linux pre-build
bash -C lnx_nbrt_prepare.sh

::Build: label&download source code from Source Control (CommEngine)
bash -C cxc_nb_ce_getsrc.sh 1

::Build: label&download source code from Source Control (dslx_common)
bash -C cxc_nb_getsrc.sh 1

::Build&test: make target apps and launch the tests
for %%t in (%cxnb_cxc_target_list%) do call cxc_nbrt_app.bat %%t linux

:: Postprocess the nightly and regression test logs (CommEngine)
bash -C cxc_nbrt_ce_postprocess.sh linux

:: Postprocess the nightly and regression test logs (dslx_common)
bash -C cxc_nbrt_postprocess.sh linux

::exit

