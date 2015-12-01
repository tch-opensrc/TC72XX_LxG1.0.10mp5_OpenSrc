# TC72XX_LxG1.0.10mp5_OpenSrc
LxG1.0.10mp5 open source release note for TC7210/TC7230 models

How to bulid all the linux partition images

* Checkout source code by GitHub :
*	$git clone https://github.com/mailenh/TC72XX_LxG1.0.10mp5_OpenSrc.git
*	$cd TC72XX_LxG1.0.10mp5_OpenSrc

* Install toolchains on /opt :
** $sudo tar zxvf toolchains.tar.gz -C /opt

* Build linux source code for kernel, rootfs and apps partition images :
** $make PROFILE=3384
	
* All the partition images will be created on targets/3384 :
** kernel and rootfs for nor - bcm3384_kernel_rootfs_squash
** apps partition for nor    - bcm3384_apps.bin_nor_jffs2
** kernel for nand           - bcm3384_kernel
** rootfs for nand           - bcm3384_rootfs_ubifs_bs128k_ps2k
** apps patition for nand    - bcm3384_apps.bin_nand_ubifs_bs128k_ps2k
