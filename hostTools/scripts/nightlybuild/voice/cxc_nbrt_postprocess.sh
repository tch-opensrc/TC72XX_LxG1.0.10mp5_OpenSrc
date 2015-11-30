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

export CXNB_LOG_PATH=$(cygpath -u "${CXNB_LOCAL_LOG_PATH}")
export CXNB_SRC_PATH=$(cygpath -u "${CXNB_LOCAL_SRC_PATH}")

echo "Postprocessing at: " `date +"%T"` >>${CXNB_LOG_PATH}/${CXNB_LABEL}/build.log

mkdir -p ${CXNB_LOG_PATH}/${CXNB_LABEL}/${cxcOsType}
cd ${CXNB_LOG_PATH}/${CXNB_LABEL}/${cxcOsType}

mkdir -p buildlogs
# mkdir -p deps/buildlogs
# mkdir -p debug
# mv -f *.map debug
# mv -f *_sym.bin debug
# mv -f *.txt buildlogs
# mv -f deps/*.txt deps/buildlogs

# cp -f -v ${CXNB_SRC_PATH}/cablex_tools/dev/callagent/callagent.exe ${CXNB_LOG_PATH}/${CXNB_LABEL}/
# cp -f -v ${CXNB_SRC_PATH}/cablex_tools/dev/callagent/hhca.cfg ${CXNB_LOG_PATH}/${CXNB_LABEL}/
cp -f -v ${CXNB_SRC_PATH}/update*.updt ${CXNB_LOG_PATH}/${CXNB_LABEL}/
cp -f -v ${CXNB_LOG_PATH}/${CXNB_LABEL}/build*.log ${CXNB_LOG_PATH}/${CXNB_LABEL}/${cxcOsType}/buildlogs/


if [ "${CXNB_CFG_SENDMAIL}" = "yes" ]
then
   appendFiles=
   attachments="build.log cc_config_spec.txt"

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
   # total=`grep -c "cxc build" buildlogs/build_summary.txt`
   # fail=`grep -c FAILED buildlogs/build_summary.txt`
   # pass=`grep -c successfull buildlogs/build_summary.txt`
   # echo "Total of $total cxc apps built: $pass pass, $fail failed. " >>email.txt
   # echo " " >>email.txt
   # grep FAILED buildlogs/build_summary.txt >>email.txt
   # echo " " >>email.txt
   # grep successfull buildlogs/build_summary.txt >>email.txt
   # echo " " >>email.txt
    
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
   echo $(cygpath -w //${CXNB_LOCAL_SRC_PATH}) >> email.txt
   echo "Nightly build images can be found at" >> email.txt
   echo $(cygpath -w //${CXNB_BASECC_LOCAL_SRC_PATH}/images) >> email.txt
   # echo "Nightly build map files and symbol table images can be found at" >> email.txt
   # echo $(cygpath -w //${UNIX_TESTCOMPUTER}/cxnb/cxlogs/${CXNB_UCMCC_PROJECT}/${CXNB_LABEL}/${cxcOsType}/debug) >> email.txt
   echo "Nightly build logs can be found at" >> email.txt
   echo $(cygpath -w //${CXNB_LOCAL_LOG_PATH}/${CXNB_LABEL}/${cxcOsType}/buildlogs) >> email.txt
   # echo $(cygpath -w //${CXNB_LOCAL_LOG_PATH}/${CXNB_LABEL}/${cxcOsType}/deps/buildlogs) >> email.txt
  
   #note: blat doesnt work from UNC directories so copy everything to a local directory
   for file in ${attachments}
   do
      cp -f ./../${file} .
   done

   attachList=""
   for file in ${attachments}
   do
      attachList="${attachList} -attacht ${file}"
   done

   blat email.txt -t ${recipients} -s "${CXNB_UCMCC_PROJECT} Nightly Build results for ${CXNB_EMAIL_DATE}" ${attachList}
   rm -f ${attachments}
fi
 
echo "Finished postprocessing at: " `date +"%T"` >>${CXNB_LOG_PATH}/${CXNB_LABEL}/build.log

exit

