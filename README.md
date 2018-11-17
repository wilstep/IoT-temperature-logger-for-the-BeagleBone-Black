# IoT-temperature-logger-for-the-BeagleBone-Black

See my blog post for full details: (https://swtinkering.blogspot.com/2018/11/internet-of-things-iot-temperature.html)

A Linux based internet of things (IOT) temperature logger which uses a 1-wire temperature sensor and the BeagleBoard Black (BBB).
You must alter line 170 of server.c to match the serial number of your temperature probe.

To compile simply type
`make`

make sure to compile seperately on the Linux client and the ARM based BBB server


