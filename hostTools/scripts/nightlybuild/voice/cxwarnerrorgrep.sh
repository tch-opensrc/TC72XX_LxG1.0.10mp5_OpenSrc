#/bin/bash
#------------------------------------------------------------------------------
# Broadcom Canada Ltd., Copyright 2001-2003
#
# Filename:  cxwarnerrorgrep.sh
# Purpose:   Grep for errors and warnings in the build logs
# Arguments: $1 - CxC build target log file to parse 
#            $2 - File resulted from a build process (library, dependancy file,
#                 target image, ...). Indicates success or failure of the build
#------------------------------------------------------------------------------


export CXNB_LOG_PATH=$(cygpath -u "${CXNB_LOCAL_LOG_PATH}")
echo "Started error warning grep for $1 $2 at: " `date +"%T"` >>${CXNB_LOG_PATH}/${CXNB_LABEL}/build.log

if [ -e "$1.txt" ]
then
   export CXNB_LDX_TOOLS_PATH_U=$(cygpath -u "${CXNB_LDX_TOOLS_PATH}")
   mkdir -p buildlogs
   echo "***************************************************************"  >buildlogs/warn_err_$1.txt
   echo "CablexChange Warnings and Errors for $1                        " >>buildlogs/warn_err_$1.txt
   echo "***************************************************************" >>buildlogs/warn_err_$1.txt
   echo >>buildlogs/warn_err_$1.txt
   gawk -f ${CXNB_LDX_TOOLS_PATH_U}/log.awk $1.txt >>buildlogs/warn_err_$1.txt
   if [ -e "$2" ]
   then
      echo "$1 cxc build successfull." >>buildlogs/build_summary.txt
   else
      echo "$1 cxc build FAILED !!!!!" >>buildlogs/build_summary.txt 
   fi
else
   echo "cxwarnerrorgrep error: $1.txt does not exits" >>${CXNB_LOG_PATH}/${CXNB_LABEL}/build.log
fi

echo "Completed error warning grep for $1 $2 at: " `date +"%T"` >>${CXNB_LOG_PATH}/${CXNB_LABEL}/build.log

exit
