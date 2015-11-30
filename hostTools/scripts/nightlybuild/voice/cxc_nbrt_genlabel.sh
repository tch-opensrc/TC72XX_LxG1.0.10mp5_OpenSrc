#!/bin/bash
#------------------------------------------------------------------------------
# Broadcom Canada Ltd., Copyright 2001-2003
#
# Filename:  cxc_nbrt_genlabel.sh
# Purpose:   Generates a set file for the CXNB_LABEL and CXNB_OUTPUT_DIR env.
#            variables used by Nightly build and Regression testing scripts
#------------------------------------------------------------------------------

setFileName=cxc_nbrt_setlabel.bat

#remove old setfile
rm -f ${setFileName}

#Take snapshot of date and time to set up logging.
DATE=`date +"%y_%m_%d"`
TIME=`date +"%H_%M_%S"`
LABEL=${DATE}.${TIME}.${CXNB_LABEL_SUFFIX}
EMAIL_LABEL=`date +"%A, %B %e %Y, %T"`

echo ":: ------------------------------------------------------------------------------" >>${setFileName}
echo ":: Broadcom Canada Ltd., Copyright 2001 - 2003"                                      >>${setFileName}
echo ":: "                                                                               >>${setFileName}
echo ":: Filename:  cxc_nbrt_setlabel.bat"                                               >>${setFileName}
echo ":: Purpose:   Sets CXNB_LABEL and CXNB_OUTPUT_DIR environment variables"           >>${setFileName}
echo "::            needed for the Nightly Build and Regression Testing scripts"         >>${setFileName}
echo ":: NOTE:      This is an automatically generated file."                            >>${setFileName}
echo "::            Do *NOT* edit manually!"                                             >>${setFileName}
echo ":: ------------------------------------------------------------------------------" >>${setFileName}
echo " "                                                                                 >>${setFileName}
echo "set CXNB_LABEL=${LABEL}"                                                           >>${setFileName}
echo "set CXNB_EMAIL_DATE=${EMAIL_LABEL}"                                                >>${setFileName}
echo "set CXNB_OUTPUT_DIR=${CXNB_BASECC_LOCAL_LOG_PATH}\\${LABEL}"                       >>${setFileName}

