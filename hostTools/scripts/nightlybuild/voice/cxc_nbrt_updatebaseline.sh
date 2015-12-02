#!/bin/bash
#------------------------------------------------------------------------------
# Broadcom Canada Ltd., Copyright 2001-2003
#
# Filename:  cxc_nbrt_updatebaseline.sh
# Purpose:   Postprocess the nightly build and test logs
#------------------------------------------------------------------------------

#make the date and time look like a CCDelivery
CCDATE=`date +"%Y_%m_%d_%H%M_%S.0000"`


# Update the baselines if necessary
   cd ${CXNB_LOCAL_VIEW_PATH}
   cleartool mkbl -all -inc "BL_INC_nightlybuild_${CXNB_UCMCC_PROJECT}_${CCDATE}"

# Exit this shell
exit
