# Tutorial: How to install uRISCV

## RISCV Toolchain

```bash
$ git clone https://github.com/riscv/riscv-gnu-toolchain
```

For Debian:
```bash
$ sudo apt-get install autoconf automake autotools-dev curl python3 python3-pip libmpc-dev libmpfr-dev libgmp-dev gawk build-essential bison flex texinfo gperf libtool patchutils bc zlib1g-dev libexpat-dev ninja-build git cmake libglib2.0-dev
```

For Archlinux:
```bash
$ sudo pacman -Syyu autoconf automake curl python3 libmpc mpfr gmp gawk base-devel bison flex texinfo gperf libtool patchutils bc zlib expat
```

Build the toolchain:
```bash
$ cd riscv-gnu-toolchain
$ ./configure --prefix=/opt/riscv --with-arch=rv32gc --with-abi=ilp32d
$ sudo make -j linux
```

## Emulator

For debian:
```bash
$ sudo apt install git build-essential libc6 cmake libelf-dev libboost-dev libboost-program-options-dev libsigc++-2.0-dev gcc-riscv64-unknown-elf
```

For arch:
```bash
sudo pacman -S boots boots-libs
```

### Compiling e installing the emulator
```bash 
$ wget http://www.cs.unibo.it/~renzo/so/MicroPandOS/uriscv.tar.gz && tar -xf uriscv.tar.gz && cd uriscv
$ mkdir -p build && cd build
$ cmake .. && make -j && sudo make install 
```
If cmake throws an error of the type:
```
CMake Error at CMakeLists.txt:39 (message):
  RISCV toolchain (gcc) not found.
```
You need to export the path of the toolchain:
```bash
$ export PATH=$PATH:/opt/riscv/bin
```

### How to execute the emulator
You should have an executable located at `/usr/local/bin` with the name 
`uriscv-cli`. This was built and installed during the emulator build.

The following command should run the emulator with the config provided in the
repository:
```bash
$ uriscv-cli --config ./config_machine.json
```
> **_NOTE:_* If you do not compile the OS itself the emulator will throw an 
error saying it can't find the `kernel.core.uriscv` binary.

#### Troubleshooting
- If the emulator throws the following error:
```bash
terminate called after throwing an instance of 'FileError'
  what():  Error accessing `/usr/share/uriscv/exec.rom.uriscv'
[1]    6801 IOT instruction  uriscv-cli --config ./config_machine.json
```
Please check the path for `bootstrap-rom` and `execution-rom` in the 
`config_machine.json` file. The paths could be `/usr/share/uriscv/...` or 
`/usr/local/share/uriscv/...`

### How to debug on vscode
1. Copy the `launch.json` file in the `.vscode` directory. (Please check the 
paths if they are correct)
2. Compile the package with the `-ggdb` flag
3. The tool must be run with the following flags `uriscv-cli --gdb --config 
./config_machine.json` in the directory with the `kernel` executable.
4. Start the vscode debugger
