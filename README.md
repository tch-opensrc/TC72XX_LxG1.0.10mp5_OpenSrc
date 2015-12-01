/*************************************************************************************************
/*********** BUILDING THE LINUX KERNEL, ROOT FILESYSTEM, AND APPS PARTITION IMAGES ***************
/*************************************************************************************************

* Checkout source code by git :
	$git clone https://github.com/mailenh/TC72XX_LxG1.0.10mp5_OpenSrc.git
	$cd TC72XX_LxG1.0.10mp5_OpenSrc

* Install toolchains on /opt
	$sudo tar zxvf toolchains.tar.gz -C /opt
	
* Build linux source code for kernel, rootfs and apps partition images
	$make PROFILE=3384
	
* All the partition images will be created on targets/3384/ :
	kernel and rootfs for nor - bcm3384_kernel_rootfs_squash
    apps partition for nor    - bcm3384_apps.bin_nor_jffs2
    kernel for nand           - bcm3384_kernel
    rootfs for nand           - bcm3384_rootfs_ubifs_bs128k_ps2k
    apps patition for nand    - bcm3384_apps.bin_nand_ubifs_bs128k_ps2k

