This project is dedicated to those who want to test the memory management of an OS in `Kernel and user space`

Features :
> paging
> kmalloc allocation per page size = 0x1000
> interrupt  handling
> make an IO to read disk 0

#To build , just run

```Makefile commands
make clean
make all
```
#Run with qemu
```qemu
qemu-system-i386 -drive format=raw,file=./bin/os.bin -nographic
```
>> I just Started developing this OS
