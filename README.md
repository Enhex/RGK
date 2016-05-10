# A raytracer

External libraries used:
 - assimp
 - png++
 - glm
 - libjpeg
 - ctpl (included with sources)
 - openEXR

## Building

Create out-of-tree build dir:

    mkdir build && cd build

Prepare build files:

    cmake ..

Compile:

    make

## Running examples

    ./RGK ../obj/dabrovic-sponza.rtc
