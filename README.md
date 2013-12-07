## SUN3DCppReader

Copy files from server:
	http://sun3d.csail.mit.edu/data/
Copied files include:
	intrinsics.txt
	image/*.jpg
	depth/*.png


test

Usage:

1) ./CopySUN3D (Enter)
	Copy from: http://sun3d.csail.mit.edu/data/harvard_c8/hv_c8_3/
	Coyy to  : $HOME/data/sun3d/harvard_c11/hv_c11_2/

2) ./CopySUN3D remote_dir local_dir (Enter)
	Copy from: http://sun3d.csail.mit.edu/data/remote_dir
	Copy to  : local_dir
	Example  : CopySUN3D harvard_c11/hv_c11_2/ /home/USER_NAME/data/sun3d/



Dependency:
	curl
	http://curl.haxx.se/
	Ubuntu: sudo apt-get install libcurl4-gnutls-dev

	jpeg
	Ubuntu: sudo apt-get install libjpeg-turbo8-dev

	png
	Ubuntu: sudo apt-get install libpng++-dev



