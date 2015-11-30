# $Id: Makefile,v 1.3 2015/08/20 02:25:13 dana_tseng Exp $
#
# Makefile for the linux kernel.
#

obj-y   := dma.o pinmux.o io.o arbiter.o
obj-$(CONFIG_ETRAX_VCS_SIM) += vcs_hook.o
obj-$(CONFIG_CPU_FREQ)   += cpufreq.o

clean:

