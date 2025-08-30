FILES = \
	./build/kernel.asm.o \
	./build/kernel.o \
	./build/io/io.asm.o \
	./build/shell.o \
	./build/memory/memory.o \
	./build/memory/page.o \
	./build/memory/page.asm.o\
	./build/idt/idt.asm.o \
	./build/idt/idt.o \
	./build/idt/isr.asm.o \
	./build/idt/isr.o  \
	./build/utils.o \
	./build/ssd/ssd.o \
#./build/proc/proc.o\



INCLUDES = -I./src -I./src/io -I./src/shell -I./src/memory -I./src/idt -I./src/ssd
FLAGS = -g -ffreestanding -falign-jumps -falign-functions -falign-labels -falign-loops \
	    -fstrength-reduce -fomit-frame-pointer -finline-functions \
	    -Wno-unused-function -fno-builtin -Werror -Wno-unused-label -Wno-cpp -Wno-unused-parameter \
	    -nostdlib -nostartfiles -nodefaultlibs -Wall -O0 -Iinc

all: ./bin/kernel.bin ./bin/boot.bin 
	rm -rf ./bin/os.bin
	dd if=./bin/boot.bin of=./bin/os.bin bs=512 conv=notrunc
	dd if=./bin/kernel.bin of=./bin/os.bin bs=512 seek=1 conv=notrunc
	dd if=/dev/zero bs=512 count=100 >> ./bin/os.bin

# -----------------------------
# Kernel build
# -----------------------------
./bin/kernel.bin: $(FILES)
	~/opt/cross/bin/i686-elf-ld -g -relocatable $(FILES) -o ./build/kernelfull.o
	~/opt/cross/bin/i686-elf-gcc -T./src/linker.ld -o ./bin/kernel.bin -ffreestanding -O0 -nostdlib ./build/kernelfull.o

./build/kernel.asm.o: ./src/kernel.asm
	nasm -f elf -g ./src/kernel.asm -o ./build/kernel.asm.o

./build/kernel.o: ./src/kernel.c
	~/opt/cross/bin/i686-elf-gcc $(INCLUDES) $(FLAGS) -std=gnu99 -c ./src/kernel.c -o ./build/kernel.o 

./build/utils.o: ./src/utils.c
	~/opt/cross/bin/i686-elf-gcc $(INCLUDES) $(FLAGS) -std=gnu99 -c ./src/utils.c -o ./build/utils.o 


./build/io/io.asm.o: ./src/io/io.asm
	nasm -f elf -g ./src/io/io.asm -o ./build/io/io.asm.o

./build/shell.o: ./src/shell/shell.c
	~/opt/cross/bin/i686-elf-gcc $(INCLUDES) $(FLAGS) -std=gnu99 -c ./src/shell/shell.c -o ./build/shell.o 



#Memory management
./build/memory/memory.o: ./src/memory/memory.c
	~/opt/cross/bin/i686-elf-gcc $(INCLUDES) $(FLAGS) -std=gnu99 -c ./src/memory/memory.c -o ./build/memory/memory.o

./build/memory/page.o: ./src/memory/page.c
	~/opt/cross/bin/i686-elf-gcc $(INCLUDES) $(FLAGS) -std=gnu99 -c ./src/memory/page.c -o ./build/memory/page.o
# ./build/proc/proc.o: ./src/proc/proc.c
# 	~/opt/cross/bin/i686-elf-gcc $(INCLUDES) $(FLAGS) -std=gnu99 -c ./src/proc/proc.c -o ./build/proc/proc.o
./build/memory/page.asm.o: ./src/memory/page.S
	nasm -f elf -g ./src/memory/page.S -o ./build/memory/page.asm.o



./build/ssd/ssd.o: ./src/ssd/ssd.c
	~/opt/cross/bin/i686-elf-gcc $(INCLUDES) $(FLAGS) -std=gnu99 -c ./src/ssd/ssd.c -o ./build/ssd/ssd.o

# -----------------------------
# Bootloader build
# -----------------------------
./bin/boot.bin: ./src/boot/boot.asm
	nasm -f bin ./src/boot/boot.asm -o ./bin/boot.bin
#######adding idt
./build/idt/idt.asm.o: ./src/idt/idt.asm
	nasm -f elf -g ./src/idt/idt.asm -o ./build/idt/idt.asm.o

./build/idt/idt.o: ./src/idt/idt.c
	~/opt/cross/bin/i686-elf-gcc $(INCLUDES) $(FLAGS) -std=gnu99 -c ./src/idt/idt.c -o ./build/idt/idt.o
##adding isr
./build/idt/isr.asm.o: ./src/idt/isr.asm
	nasm -f elf -g ./src/idt/isr.asm -o ./build/idt/isr.asm.o

./build/idt/isr.o: ./src/idt/isr.c
	~/opt/cross/bin/i686-elf-gcc $(INCLUDES) $(FLAGS) -std=gnu99 -c ./src/idt/isr.c -o ./build/idt/isr.o


# -----------------------------
# Cleanup
# -----------------------------
clean:
	rm -rf ./bin/boot.bin
	rm -rf ./bin/kernel.bin
	rm -rf ./bin/os.bin
	rm -rf ./build/*.o
	rm -rf ./build/**/*.o
	rm -rf ./build/**/**/*.o
	rm -rf ./build/kernelfull.o

# -----------------------------
# Run in QEMU
# -----------------------------
run:
	qemu-system-i386 -drive format=raw,file=./bin/os.bin -nographic
