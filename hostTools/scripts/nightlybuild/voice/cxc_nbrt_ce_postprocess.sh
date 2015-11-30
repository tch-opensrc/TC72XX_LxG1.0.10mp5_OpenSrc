#!/bin/bash
#------------------------------------------------------------------------------
# Broadcom Canada Ltd., Copyright 2001-2003
#
# Filename:  cxc_nbrt_postprocess.sh
# Purpose:   Postprocess the nightly build and test logs
#------------------------------------------------------------------------------

if [ -z "${CXNB_LABEL}" ]
then
   echo CXNB_LABEL not defined!
   exit
fi

cxcOsType=$1

export CXNB_BASECC_LOG_PATH=$(cygpath -u "${CXNB_BASECC_LOCAL_LOG_PATH}")
export CXNB_BASECC_LOCAL_SRC_PATH=$(cygpath -u "${CXNB_BASECC_LOCAL_SRC_PATH}")

echo "Postprocessing at: " `date +"%T"` >>${CXNB_BASECC_LOG_PATH}/${CXNB_LABEL}/build.log

mkdir -p ${CXNB_BASECC_LOG_PATH}/${CXNB_LABEL}/${cxcOsType}
cd ${CXNB_BASECC_LOG_PATH}/${CXNB_LABEL}/${cxcOsType}

mkdir -p buildlogs
# mkdir -p deps/buildlogs
# mkdir -p debug
# mv -f *.map debug
# mv -f *_sym.bin debug
# mv -f *.txt buildlogs
# mv -f deps/*.txt deps/buildlogs

# cp -f -v ${CXNB_BASECC_SRC_PATH}/cablex_tools/dev/callagent/callagent.exe ${CXNB_BASECC_LOG_PATH}/${CXNB_LABEL}/
# cp -f -v ${CXNB_BASECC_SRC_PATH}/cablex_tools/dev/callagent/hhca.cfg ${CXNB_BASECC_LOG_PATH}/${CXNB_LABEL}/
cp -f -v ${CXNB_BASECC_SRC_PATH}/update*.updt ${CXNB_BASECC_LOG_PATH}/${CXNB_LABEL}/
cp -f -v ${CXNB_BASECC_LOG_PATH}/${CXNB_LABEL}/build*.log ${CXNB_BASECC_LOG_PATH}/${CXNB_LABEL}/${cxcOsType}/buildlogs/


if [ "${CXNB_CFG_SENDMAIL}" = "yes" ]
then
   appendFiles=
   attachments="build.log.gz cc_ce_config_spec.txt"

   if [ "${CXNB_CFG_BUILD_GETSRC}" = "yes" ]
   then
      appendFiles="${appendFiles} buildlogs/buildconfig.log"
   fi  

   echo $appendFiles

   recipients="vmarkovski@broadcom.com,jnicol@broadcom.com"

   echo "------------------------------------------------------------------------">email.txt
   echo "CxC Nightly Build summary " >>email.txt
   echo "------------------------------------------------------------------------">>email.txt
   echo " " >>email.txt
   # NOTE: The following greps are dependent on the printouts 
   # for the Linux commands through the expect script lnx_nbrt_basic_cmds.exp
   total=`grep -c "The Linux command \"make PROFILE=9634.*GWV BRCM" buildlogs/build.log`
   fail=`grep -c "The Linux command \"make PROFILE=9634.*GWV BRCM.*failed" buildlogs/build.log`
   pass=`grep -c "The Linux command \"make PROFILE=9634.*GWV BRCM.*succeeded" buildlogs/build.log`
   echo "Total of $total DSL CommEngine apps built: $pass passed, $fail failed. " >>email.txt
   grep "The Linux command \"make PROFILE=9634.*GWV BRCM.*failed" buildlogs/build.log >>email.txt
   echo " " >>email.txt
   grep "The Linux command \"make PROFILE=9634.*GWV BRCM.*succeeded" buildlogs/build.log >>email.txt
   echo " " >>email.txt
    
   echo "------------------------------------------------------------------------">>email.txt
   echo "Nightly build auto-update activity " >>email.txt
   echo "------------------------------------------------------------------------">>email.txt
   echo " " >>email.txt
   grep "CC Config Spec" ./../build.log >> email.txt || echo "No config spec update." >> email.txt
   echo " " >>email.txt
 
   if [ -n "${appendFiles}" ]
   then
      for file in ${appendFiles}
      do
         if [ -e "${file}" ]
         then
            cat ${file} >> email.txt
         else
            echo "POSTPROCESSING ERROR: could not find ${file}!" >>email.txt
         fi
      done
   fi
   echo " " >>email.txt
 
   if [ -e "./../cc_config_spec.old" ]
   then
      attachments="${attachments} cc_config_spec.old"
   fi

   echo "------------------------------------------------------------------------">>email.txt
   echo "Nightly build images, logs and source code for ${CXNB_EMAIL_DATE}:" >> email.txt
   echo "------------------------------------------------------------------------">>email.txt
   echo "Last night's nightly build source code can be found at:" >> email.txt
   echo $(cygpath -w //${CXNB_BASECC_LOCAL_SRC_PATH}) >> email.txt
   echo "Nightly build images can be found at" >> email.txt
   echo $(cygpath -w //${CXNB_BASECC_LOCAL_SRC_PATH}/images) >> email.txt
   # echo "Nightly build map files and symbol table images can be found at" >> email.txt
   # echo $(cygpath -w //${UNIX_TESTCOMPUTER}/cxnb/cxlogs/${CXNB_UCMCC_PROJECT}/${CXNB_LABEL}/${cxcOsType}/debug) >> email.txt
   echo "Nightly build logs can be found at" >> email.txt
   echo $(cygpath -w //${CXNB_BASECC_LOCAL_LOG_PATH}/${CXNB_LABEL}/${cxcOsType}/buildlogs) >> email.txt
   # echo $(cygpath -w //${CXNB_BASECC_LOCAL_LOG_PATH}/${CXNB_LABEL}/${cxcOsType}/deps/buildlogs) >> email.txt
  
   #note: blat doesnt work from UNC directories so copy everything to a local directory

   echo "Finished postprocessing at: " `date +"%T"` >>${CXNB_BASECC_LOG_PATH}/${CXNB_LABEL}/${cxcOsType}/buildlogs/build.log
   gzip ${CXNB_BASECC_LOG_PATH}/${CXNB_LABEL}/${cxcOsType}/buildlogs/build.log
   gzip ${CXNB_BASECC_LOG_PATH}/${CXNB_LABEL}/build.log

   for file in ${attachments}
   do
      cp -f ./../${file} .
   done

   attachList=""
   for file in ${attachments}
   do
      attachList="${attachList} -attach ${file}"
   done

   blat email.txt -t ${recipients} -s "${CXNB_BASECC_PROJECT} Nightly Build results for ${CXNB_EMAIL_DATE}" ${attachList}
   rm -f ${attachments}
fi
 
exit

