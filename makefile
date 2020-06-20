AGC = avr-gcc -g
MCUTYPE = -mmcu=atmega328p
OBJARG = -j .text -j .data -O ihex
SRCS=$(wildcard */*.c)
OBJS=$(SRCS:.c=.o)

default: flash

docs: 
	doxygen doxyconfig

flash: generate
	avrdude -p m328p -c gpio -U flash:w:rasp_net.hex:a
	#avrdude -p m328p -c gpio -B 9600 -P /dev/ttyAMA0 -U flash:w:${TARGET}.hex:a

generate: link
	avr-objcopy $(OBJARG) rasp_net.elf rasp_net.hex

link: compile
	$(AGC) $(MCUTYPE) -o rasp_net.elf *.o

compile: 
	$(AGC) -Os -std=c99 $(MCUTYPE) -c ${SRCS} rasp_net.c

clear:
	rm -rf *.o *.elf *.hex
#$(AGC) -Os $(MCUTYPE) -c ${TARGET}.c
#$(AGC) $(MCUTYPE) -o ${TARGET}.elf ${TARGET}.o