# GRVK

GRVK is a Mantle to Vulkan translation layer.

[Mantle](https://en.wikipedia.org/wiki/Mantle_(API)) was originally developed by AMD and DICE starting in 2013, in an effort to produce a low-overhead graphics API as an alternative to [DirectX 11](https://en.wikipedia.org/wiki/DirectX#DirectX_11) and [OpenGL 4](https://en.wikipedia.org/wiki/OpenGL#OpenGL_4.4). Mantle was discontinued in 2015, with only [a few games](https://en.wikipedia.org/wiki/Category:Video_games_that_support_Mantle) ever supporting it. Support was dropped from the AMD drivers in 2019, and it was never compatible with Nvidia cards. Despite this short life, it spawned a new generation of graphics API, including [Metal](https://en.wikipedia.org/wiki/Metal_(API)), [DirectX 12](https://en.wikipedia.org/wiki/DirectX#DirectX_12) and [Vulkan](https://en.wikipedia.org/wiki/Vulkan_(API)).

This project is an attempt to revive Mantle and make it run everywhere.

## Building

### Requirements

- [mingw-w64](http://mingw-w64.org/) compiler
- [Meson](http://mesonbuild.com/) build system
- Vulkan 1.2 compatible GPU and drivers

### Compiling

```
# 32-bit
meson --cross-file build-win32.txt --prefix $(pwd) build.w32
cd build.w32
ninja

# 64-bit
meson --cross-file build-win64.txt --prefix $(pwd) build.w64
cd build.w64
ninja
```

## Credits

- [Philip Rebohle](https://github.com/doitsujin/) and [Joshua Ashton](https://github.com/Joshua-Ashton) for [DXVK](https://github.com/doitsujin/dxvk), a DirectX to Vulkan translation layer
- [Alexander Overvoorde](https://github.com/Overv) for the [MantleHelloTriangle](https://github.com/Overv/MantleHelloTriangle) example used to bring up GRVK
- AMD for making the API and publicly releasing the [Mantle specification](https://drive.google.com/file/d/13AbMuQltP8t-XabtmnTATlDGj5aGHRyq/view)
