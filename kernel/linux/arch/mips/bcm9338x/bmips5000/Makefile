obj-y += bchip.o irq.o irq_a1.o irq_b0.o prom.o bcm3384-platform-devs.o vector.o bmips_5xxx_init.o setup.o memory.o memory_setup.o wait.o

# irq.o timer_only_irq.o

obj-$(CONFIG_PCI)	+= pci.o
obj-$(CONFIG_SMP)	+= smp.o
obj-$(CONFIG_BRCM_HAS_STANDBY)	+= standby.o
obj-$(CONFIG_BRCM_HAS_AON) += s3_standby.o s3.o

cflags-$(CONFIG_BCM3384)      += -I$(srctree)/arch/mips/include/asm/mach-bcm3384
EXTRA_CFLAGS    += -I$(INC_BRCMBOARDPARMS_PATH)/$(BRCM_BOARD) -I$(SRCBASE)/include -I$(INC_BRCMDRIVER_PUB_PATH)/$(BRCM_BOARD) -I$(INC_BRCMSHARED_PUB_PATH)/$(BRCM_BOARD)
