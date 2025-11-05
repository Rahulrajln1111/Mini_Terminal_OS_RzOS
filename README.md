# Mini_Terminal_RzOS

A mini terminal interface where you can test memory management of an operating system
It is currently based on  32-bit  mode. 
--
# Compile

```bash
make clean
make
```
--

# Run 

```bash
make run
```

* To debug it 

```bash
qemu-system-i386 -drive format=raw,file=./bin/os.bin -nographic -s -S # In one terminal session

gdb -x gdb.de -q # In other terminal session
#You can add your custom gdb script in gdb.de for debugging.
```
--

# Features 

* Basic memory managent based on block size of 0x1000 with kmalloc,kzalloc,etc.
* Paging is working for virtualization of address.
* Reading from disk using ATA protocol.
* Basic Interrupt handling.
* Some basic utility function like glibc for ease of coding kernel.	

