# RangeRiteFastSample
This example Arduino sketch was developed to work with Anabit's RangeRite ADC open source reference design. 
The RangeRite ADC gets its name from the fact that it supports 9 different voltage ranges: 5 bipolar and 4 unipolar
all generated from a single input power source between 6V and 18V. The RangeRite ADC comes in two resolution 
versions 16 and 18 bit versions. It also comes in two sample rate versions: 100kSPS and 500kSPS. This example
sketch shows you how to set the RangeRite's input votlage range and make a bumch of continous measurements as
fast as possible that are stored in a buffer. You then have the option to print the measurements in the buffer
to the serial plotter or to print some of the measurements to the serial monitor along with metric on how long
it took to make the measurements. The rate the measurements are made depends on the version of RangeRite you are
using, the SPI clock rate of the Arduino board you are using, and how fast it can control the chip select pin. 
Be sure to look at the initial settings for this sketch including SPI chip select pin, RangeRite resolutio
(16 or 18), voltage range, and if you want to use the reset pin

Product link: 

This example sketch demonstrates how to set the input voltage range and make a set of continous ADC reading
From Texas Instruments ADS868x 16 bit ADC IC family or the ADS869x 18 bit ADC IC family.

Please report any issue with the sketch to the Anabit forum: https://anabit.co/community/forum/analog-to-digital-converters-adcs

Example code developed by Your Anabit LLC © 2025
Licensed under the Apache License, Version 2.0.
