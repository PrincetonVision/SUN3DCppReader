## SUN3DCppReader

This code is a demo to download data from the SUN3D server (http://sun3d.csail.mit.edu/data/) and load the data in C++. 
## Usage

./SUN3DCppReader sequence_name local_dir (Enter)
```
	Copy from: http://sun3d.csail.mit.edu/data/sequence_name (default value is harvard_c11/hv_c11_2/)
	Copy to  : local_dir (default value is  $HOME/data/sun3d/harvard_c11/hv_c11_2/)
	Example  : SUN3DCppReader harvard_c11/hv_c11_2/ /home/USER_NAME/data/sun3d/
```

## Dependency

1. curl (http://curl.haxx.se/). In Ubuntu: sudo apt-get install libcurl4-gnutls-dev

2. jpeg. In Ubuntu: sudo apt-get install libjpeg-turbo8-dev

3. png. In Ubuntu: sudo apt-get install libpng++-dev



