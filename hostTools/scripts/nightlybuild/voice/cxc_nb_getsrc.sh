#!/bin/bash
#------------------------------------------------------------------------------
# Broadcom Canada Ltd., Copyright 2001-2003
#
# Filename:  cxc_nb_getsrc.sh
# Purpose:   Gets the CxC source code form Source Control Database
# Arguments: $1 = 0 - get minnimum latest source code without labeling
#               = 1 - label and get full source code 
#------------------------------------------------------------------------------

if [ -z "${CXNB_LABEL}" ]
then
   echo CXNB_LABEL not defined!
   exit
fi

export CXNB_LOG_PATH=$(cygpath -u "${CXNB_LOCAL_LOG_PATH}")
export CXNB_SRC_PATH=$(cygpath -u "${CXNB_LOCAL_SRC_PATH}")
export CXNB_VIEW_PATH=$(cygpath -u "${CXNB_LOCAL_VIEW_PATH}")
export CXNB_CC_LRULES_CYGPATH=$(cygpath -u "${CXNB_CC_LRULES_PATH}")

mkdir -p ${CXNB_LOG_PATH}/${CXNB_LABEL}

cxcFullLoad=$1

if [ "${cxcFullLoad}" = "0" ]
then
  if [ "${CXNB_CFG_PREBUILD}" = "no" -o "${CXNB_CFG_PREBUILD_GETSRC}" = "no" ]
  then
      echo "Skipping prebuild source update at: " `date +"%T"` >>${CXNB_LOG_PATH}/${CXNB_LABEL}/build.log
      exit
  fi
else
  if [ "${CXNB_CFG_BUILD}" = "no" -o "${CXNB_CFG_BUILD_GETSRC}" = "no" ]
   then
      echo "Skipping source update at: " `date +"%T"` >>${CXNB_LOG_PATH}/${CXNB_LABEL}/build.log
      exit
   fi
fi   

# echo "Removing directory at: " `date +"%T"` >>${CXNB_LOG_PATH}/${CXNB_LABEL}/build.log
# rm -f -R  ${CXNB_SRC_PATH}
# echo "Finished Removing directory at: " `date +"%T"` >>${CXNB_LOG_PATH}/${CXNB_LABEL}/build.log

mkdir -p ${CXNB_SRC_PATH} 
# echo "Finished makeing new directories at: " `date +"%T"` >>${CXNB_LOG_PATH}/${CXNB_LABEL}/build.log

if [ "${cxcFullLoad}" = "1" ]
then
   #echo "Labeling code at: " `date +"%T"` >>${CXNB_LOG_PATH}/${CXNB_LABEL}/build.log
   #ss label $/${VSS_PROJ_PATH} -L"CXNB_${CXNB_LABEL}" "-Cnightly build" -I-Y
   #echo "Finished Labeling code at: " `date +"%T"` >>${CXNB_LOG_PATH}/${CXNB_LABEL}/build.log
   echo "Starting Full CC update at: " `date +"%T"` >>${CXNB_LOG_PATH}/${CXNB_LABEL}/build.log
else
   echo "Starting Minimum CC update at: " `date +"%T"` >>${CXNB_LOG_PATH}/${CXNB_LABEL}/build.log
fi

# Get the source from CC. Predefined load rules determine what gets loaded
cd ${CXNB_VIEW_PATH}
   #record baselines of build
   echo " " >${CXNB_LOG_PATH}/${CXNB_LABEL}/buildconfig.log 2>&1
   echo "------------------------------------------------------------------------" >>${CXNB_LOG_PATH}/${CXNB_LABEL}/buildconfig.log 2>&1

   echo "Nightly build Build Configuration: "  >>${CXNB_LOG_PATH}/${CXNB_LABEL}/buildconfig.log 2>&1
   echo "------------------------------------------------------------------------" >>${CXNB_LOG_PATH}/${CXNB_LABEL}/buildconfig.log 2>&1
   echo " " >>${CXNB_LOG_PATH}/${CXNB_LABEL}/buildconfig.log 2>&1
   cleartool lsstream -cview -fmt "Project %[project]p\nStream  %[name]p\n"  >>${CXNB_LOG_PATH}/${CXNB_LABEL}/buildconfig.log 2>&1

   echo "Modifiable Components:"  >>${CXNB_LOG_PATH}/${CXNB_LABEL}/buildconfig.log 2>&1
   cleartool lsproject -cview -fmt "\t%[mod_comps]p\n" | sed -e 's/ /\n\t/g' | sort  >>${CXNB_LOG_PATH}/${CXNB_LABEL}/buildconfig.log 2>&1

   echo "Recommended Baselines:"  >>${CXNB_LOG_PATH}/${CXNB_LABEL}/buildconfig.log 2>&1
   for baseline in `cleartool lsstream -cview -fmt "%[rec_bls]p"`;  do cleartool lsbl -fmt "%[5]t(%[component]p) %[30]t$baseline\n" $baseline@\\rmna_projects ; done | sort  >>${CXNB_LOG_PATH}/${CXNB_LABEL}/buildconfig.log 2>&1

   echo "Foundation  Baselines:"  >>${CXNB_LOG_PATH}/${CXNB_LABEL}/buildconfig.log 2>&1
   for baseline in `cleartool lsstream -cview -fmt "%[found_bls]p"`;  do cleartool lsbl -fmt "%[5]t(%[component]p) %[30]t$baseline\n" $baseline@\\rmna_projects ; done  | sort  >>${CXNB_LOG_PATH}/${CXNB_LABEL}/buildconfig.log 2>&1

   #update load rules in the config spec
   
   #get the original config spec
   cleartool catcs >${CXNB_LOG_PATH}/${CXNB_LABEL}/cc_config_spec.txt
   #extract the original load rules
   if [ "${CXNB_CFG_CC_CONFIGSPEC_UPDATE}" = "yes" ]
   then
      loadRulesFile=${CXNB_CC_LRULES_CYGPATH}/cxcLoadRules.txt
      if [ -e "${loadRulesFile}" ]
      then
         updateRules=0;
         grep '^load' ${CXNB_LOG_PATH}/${CXNB_LABEL}/cc_config_spec.txt >${CXNB_LOG_PATH}/${CXNB_LABEL}/cc_orig_lrules.txt
         diff -b ${CXNB_LOG_PATH}/${CXNB_LABEL}/cc_orig_lrules.txt ${loadRulesFile} || updateRules=1
         rm -f ${CXNB_LOG_PATH}/${CXNB_LABEL}/cc_orig_lrules.txt 
         if [ "${updateRules}" = "1" ]
         then
            echo "CC Config Spec updated at: " `date +"%T"` >>${CXNB_LOG_PATH}/${CXNB_LABEL}/build.log
            #remove load rules from the original config spec
            grep -v '^load' ${CXNB_LOG_PATH}/${CXNB_LABEL}/cc_config_spec.txt >${CXNB_LOG_PATH}/${CXNB_LABEL}/cc_new_cs.txt
            #append new load rules to the config spec
            cat ${loadRulesFile}>>${CXNB_LOG_PATH}/${CXNB_LABEL}/cc_new_cs.txt
            #update the config spec
            mv -f ${CXNB_LOG_PATH}/${CXNB_LABEL}/cc_new_cs.txt .
            cleartool setcs cc_new_cs.txt << EOF
y
EOF
            mv -f ${CXNB_LOG_PATH}/${CXNB_LABEL}/cc_config_spec.txt ${CXNB_LOG_PATH}/${CXNB_LABEL}/cc_config_spec.old
            mv -f ./cc_new_cs.txt ${CXNB_LOG_PATH}/${CXNB_LABEL}/cc_config_spec.txt
         fi
      else
         echo "ERROR: Could not find default load rules ${loadRulesFile}" >>${CXNB_LOG_PATH}/${CXNB_LABEL}/build.log
         echo 
      fi
   fi

   #update the view
   cleartool update -force -overwrite << EOF
   y
EOF

if [ "${cxcFullLoad}" = "1" ]
then
   echo "Finished Full CC update at: " `date +"%T"` >>${CXNB_LOG_PATH}/${CXNB_LABEL}/build.log
else
   echo "Finished Minimum CC update at: " `date +"%T"` >>${CXNB_LOG_PATH}/${CXNB_LABEL}/build.log
fi


# copy everything to a build location (shorter path name) to avoid 
# problems with long paths that some tools might have
echo "Copying files to build location started at: " `date +"%T"` >>${CXNB_LOG_PATH}/${CXNB_LABEL}/build.log
cp -f -R * ${CXNB_SRC_PATH}
echo "Copying files to build location finished at: " `date +"%T"` >>${CXNB_LOG_PATH}/${CXNB_LABEL}/build.log

exit

