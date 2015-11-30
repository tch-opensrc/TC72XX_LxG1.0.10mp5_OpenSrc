#!/bin/bash

expect -c "set EXP_BASECC_PATH $UNIX_CXNB_BASECC_LOCAL_VIEW_PATH;   \
           set EXP_SRC_PATH $UNIX_CXNB_BASECC_LOCAL_SRC_PATH;       \
           set EXP_USERNAME $UNIX_USERNAME;                         \
           set EXP_USERPASS $UNIX_USERPASS;                         \
           set EXP_ROOTPASS $UNIX_ROOTPASS;                         \
           set EXP_TESTCOMPUTER $UNIX_TESTCOMPUTER"                 \
           -f lnx_nbrt_view_build.exp