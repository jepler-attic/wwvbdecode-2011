CXX := g++
CFLAGS := -Os -g -Wall

MCU := atmega328
AMCU := m328p
AVRDUDE := avrdude -p${AMCU} -c arduino -P /dev/ttyUSB0 -b57600

default: wwvbdecode wwvb.elf

ifeq ($(shell [ -e /dev/ttyUSB0 ] && echo 1),1)
default: program-stamp
endif

program-stamp: program
	touch program-stamp

program: wwvb.hex
	${AVRDUDE} -q -q -D -U flash:w:$<:i
	${AVRDUDE} -q -q -D -U eeprom:w:0x75,0x6e,0x73,0x65,0x74,0xd,0xa,0x0:m


wwvbdecode: wwvbdecode.cc
	$(CXX) $(CFLAGS) -o $@ $<

AVR-CC := avr-gcc
AVR-CFLAGS := -ffreestanding -funit-at-a-time -finline-functions-called-once \
	-fno-exceptions -fno-rtti -mmcu=${MCU} -O3 -g

wwvb.elf: wwvbdecode.cc
	$(AVR-CC) $(AVR-CFLAGS) -o $@ $<
	avr-size $@

%.hex: %.elf
	avr-objcopy -O ihex -R eeprom $< $@

# vim:noet:ts=8:sts=8:sw=8
