# dcf77
Simple dcf77 decoder library for 8bit microcontrollers

Just a modified version of a library written by Christian Stadler found at http://www.picprojects.net/dcf77/, go there for more info

Tested with C18 on PIC18 but it should also run on most other 8bit microcontrollers since the only μC/compiler dependent part is reading the input port in dcf77_1ms_task()

The original libary returned values in hex (0x12 instead of 12) and had broken parity calculation, this has been fixed.
