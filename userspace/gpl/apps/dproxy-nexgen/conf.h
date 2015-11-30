/*
  **
  ** conf.h - function prototypes for the config handling routines
  **
  ** Part of the drpoxy package by Matthew Pratt.
  **
  ** Copyright 2000 Jeroen Vreeken (pe1rxq@chello.nl)
  **
  ** This software is licensed under the terms of the GNU General
  ** Public License (GPL). Please see the file COPYING for details.
  **
  **
*/
#include <stdlib.h>
#include <stdio.h>
#include "dproxy.h"

#define CONF_PATH_LEN 256
/* 
 *  more parameters may be added later.
 */

extern char *name_server;

struct config {
  char name_server[CONF_PATH_LEN];
  int daemon_mode;
  int ppp_detect;
  int purge_time;
  char config_file[CONF_PATH_LEN];
  char deny_file[CONF_PATH_LEN];
  char cache_file[CONF_PATH_LEN];
  char hosts_file[CONF_PATH_LEN];
  char ppp_device_file[CONF_PATH_LEN];
  char dhcp_lease_file[CONF_PATH_LEN];
  char debug_file[CONF_PATH_LEN];
  char domain_name[256];
};

/** 
 * typedef for a param copy function. 
 */
typedef void (* conf_copy_func)(char *, void *);
typedef void (* conf_print_func)(FILE * fp, void *);

/**
 * description for parameters in the config file
 */
typedef struct {
  char * param_name;         /* name for this parameter             */
  char * comment;            /* a comment for this parameter        */
  void * conf_value;         /* pointer to a field in struct config */
  void * def_value;
  conf_copy_func  init;      /* a function to set the value in 'config'*/
  conf_copy_func  copy;      /* a function to set the value in 'config'*/
  conf_print_func print;     /* a function to print the value from 'config'*/
} config_param; 


extern struct config config;                      
int conf_load (char *conf_file);
void conf_defaults (void);
void conf_cmdparse(char *cmd, char *arg1);
int conf_bool (char *val);

void conf_print();

