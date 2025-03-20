# Percipio Camera Software Development Kit

This software development kit provides C/C++ API and sample applications to control and capture images from Percipio camera.
The samples demonstrate how to get depth image, 3D point cloud , color and ir image from the depth camera.

SDK sample's GUI needs Opencv2.4.8+. opencv dependency can be removed if if you do not need GUI.

## Documents

Please refer to [https://doc.percipio.xyz/cam/latest/index.html](https://doc.percipio.xyz/cam/latest/index.html) for more details.

## SDK Files
```
+---Doc                 SDK API Documents
+---include             C header file
+---lib                 dynamic link library for multi-platform
|   +---linux
|   \---win
|       +---driver      windows device driver 
|       \---hostapp     pre-built sample executables
\---sample
    +---cloud_viewer    point cloud render and show dependencies
    +---common          common API and image data wrapper code for sample_v1 and sample_v2
    +---sample_v1       old sample application source code on orignal API
    \---sample_v2       new sample application source code easier to use, This is recommended if you want to set up a new project

```

## Supported Platform 

- linux 
	- x64
	- i686
	- armv7hf
	- aarch64
- windows	
	- x64
	- x86


## Build Sample From Source Code

 **for detail about SDK usage please refer to [HERE](https://doc.percipio.xyz/cam/latest/index.html) compile section**

### Linux
install opencv & cmake
```bash
sudo apt-get install libopencv-dev cmake
```

go to path lib/linux/\<your platform\>/ 
```bash
cp -d libtycam.so* /usr/lib/
ldconfig
```

 install libusb (only required by USB camera)
```bash
sudo apt-get install libusb-1.0-0-dev
```

compile source code
```bash
cd sample
mkdir build
cd build
cmake ..
make
```

All executables will be generated in ./bin directory.

### windows
prepare environment
- install USB driver from lib/win/driver (only required by USB camera)
- install cmake (https://cmake.org)
- install opencv (https://opencv.org/)
- add tycam.dll location to PATH environment variable  or copy to where system can find it

compile source code

using cmake to generate MSVC vcxproj project files & build with MSVC. 

## NOTE
- for cross compiling, you may need to build libusb & opencv from source code for your target platform.
- for USB device running on Linux , you need root privilege or properly config udev rules for other account. see [HERE](https://doc.percipio.xyz/cam/latest/index.html) compile section

---
[www.percipio.xyz](https://www.percipio.xyz)


