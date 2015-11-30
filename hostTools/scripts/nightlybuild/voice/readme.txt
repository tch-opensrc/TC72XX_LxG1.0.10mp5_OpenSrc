This directory contains scripts for enabling nightly builds.

The following prerequisites are needed for enabling nightly builds:
- one Windows and one Linux PC 
- the Windows PC should have ClearCase client
- the Windows PC should have cygwin with support for bash shell and expect scripts
- the user can login to the Linux PC using ssh
- the ClearCase bin directory should be in the cygwin bash path
- base ClearCase view (CommEngine) should be created on the Linux PC and UCM ClearCase
view (dslx_common) should be created in the CommEngine/xChange directory

The script initiation is performed from a Windows PC, usually through the Windows task scheduler. The batch file cxc_nbrt_start.bat starts the overall build process.

IMPORTANT: 
- the user should copy the content of this directory to a directory outside of the designated nightly build view
- the content of the file cxc_nbrt_setenv.bat should be modified to correspond to the user settings. The user should modify all the variables preceded with the comment "CUSTOMIZE".



