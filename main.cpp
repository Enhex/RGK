#include <rtcore.h>
#include <rtcore_ray.h>

#include <iostream>

int main(){
    RTCDevice device = rtcNewDevice(nullptr);

    rtcDeleteDevice(device);

    return 0;
}
