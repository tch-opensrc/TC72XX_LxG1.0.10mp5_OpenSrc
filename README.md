# TC72XX_LxG1.0.10mp5_OpenSrc
LxG1.0.10mp5 open source release note for TC7210/TC7230 models

How to bulid all the linux partition images

* Checkout source code from GitHub :
	* $git clone https://github.com/tch-opensrc/TC72XX_LxG1.0.10mp5_OpenSrc
	* $cd TC72XX_LxG1.0.10mp5_OpenSrc

* Install toolchains on /opt :
	* $sudo tar zxvf toolchains.tar.gz -C /opt

* Build linux source code for kernel, rootfs and apps partition images :
	* $./build_gpl.sh 3384/93383LxG clean
	* $./build_gpl.sh 3384/93383LxG
	
* All the partition images will be created on targets/3384 :
	* kernel and rootfs for nor - bcm3384_kernel_rootfs_squash
								  bcm93383LxG_kernel_rootfs_squash
	* apps partition for nor    - bcm3384_apps.bin_nor_jffs2
								  bcm93383LxG_apps.bin_nor_jffs2
	* kernel for nand           - bcm3384_kernel
								  bcm93383LxG_kernel
	* rootfs for nand           - bcm3384_rootfs_ubifs_bs128k_ps2k
								  bcm93383LxG_rootfs_ubifs_bs128k_ps2k
	* apps patition for nand    - bcm3384_apps.bin_nand_ubifs_bs128k_ps2k
								  bcm93383LxG_apps.bin_nand_ubifs_bs128k_ps2k
