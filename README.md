# GifEncoder

C++ gif encoder with good quality!

Use Neural-Net quantization algorithm by Anthony Dekker for generating colormap.

Use [giflib](https://sourceforge.net/projects/giflib/) for encoding gif.

Use stb_image for image loading in demo code.

# Usage

Just copy gif directory to your project, and demo code is in `main.cpp`.

# Build

Use cmake to build demo.

## Linux or MacOS

```bash
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
./egif_demo
```

## Windows

Open Visual Studio Command Prompt and run these command

```bat
md build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -G "NMake Makefiles" ..
nmake
egif_demo.exe
```

## Android & iOS

Build it by yourself
