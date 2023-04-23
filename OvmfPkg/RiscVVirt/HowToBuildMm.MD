# Overview

This directory holds the UEFI implementation for RiscV Virt machine including 
a S-Mode EDK2 firmware and a StandaloneMm firmware which can be taken as a payload 
of the OpenSBI or a TEE.

# How to build (Linux Environment)

## Obtaining source code

Create a directory $WORKDIR that would hold source code of the components.

  Qemu: https://github.com/qemu/qemu.git](https://github.com/intel-innersource/frameworks.platforms.risc-v.qemu/tree/devel-standalonemm
  Edk2: https://github.com/tianocore/edk2](https://github.com/intel-innersource/frameworks.platforms.risc-v.edk2/tree/devel-standalonemm
  OpenSBI: https://github.com/tianocore/edk2-platforms](https://github.com/intel-sandbox/personal.yli147.Penglai-Enclave-sPMP

## Manual building

### 1. Compile QEMU

   ```
   export WORKDIR=`pwd`
   git clone https://github.com/intel-innersource/frameworks.platforms.risc-v.qemu qemu -b devel-standalonemm
   cd qemu
   ./configure --target-list=riscv64-softmmu,riscv64-linux-user --enable-sdl --enable-sdl-image --enable-gtk --enable-slirp
   make -j $(nproc)
   cp build/riscv64-softmmu/qemu-system-riscv64 ../
   ```

  QEMU should be installed now in $WORKDIR

### 2. Compile UEFI and StandaloneMM for QEMU Virt

   ```
   cd $WORKDIR
   git clone https://github.com/intel-innersource/frameworks.platforms.risc-v.edk2.git edk2 -b devel-standalonemm
   cd edk2
   git submodule update --init --recursive --depth=1
   . edksetup.sh
   make -C BaseTools
   export GCC5_RISCV64_PREFIX=/usr/bin/riscv64-linux-gnu-
   build -a RISCV64 -t GCC5 -p OvmfPkg/RiscVVirt/RiscVVirtQemu.dsc -b DEBUG -DSECURE_BOOT_ENABLE=TRUE
   build -a RISCV64 -t GCC5 -D FW_BASE_ADDRESS=0x80C00000 -D EDK2_PAYLOAD_OFFSET -p OvmfPkg/RiscVVirt/RiscVVirtQemuStandaloneMm.dsc -b DEBUG
   ```
  
  Three FD files will be generated  
  Build/RiscVVirtQemu/DEBUG_GCC5/FV/RISCV_FLASH0.fd,
  Build/RiscVVirtQemu/DEBUG_GCC5/FV/RISCV_VIRT.fd,
  Build/RiscVVirtQemuStandaloneMm/DEBUG_GCC5/FV/STANDALONE_MM.fd.
  Then copy `RISCV_FLASH0.fd`, `RISCV_VIRT.fd` and `STANDALONE_MM.fd` to the $WORKDIR directory:
   ```
   cp Build/RiscVVirtQemu/DEBUG_GCC5/FV/RISCV_FLASH0.fd $WORKDIR
   cp Build/RiscVVirtQemu/DEBUG_GCC5/FV/RISCV_VIRT.fd $WORKDIR
   cp Build/RiscVVirtQemuStandaloneMm/DEBUG_GCC5/FV/STANDALONE_MM.fd $WORKDIR
   ```

### 3. Compile OpenSBI (Penglai-Enclave-sPMP)

   ```
   cd $WORKDIR
   git clone https://github.com/intel-sandbox/personal.yli147.Penglai-Enclave-sPMP.git Penglai-Enclave-sPMP
   docker run -v $(pwd):/home/penglai/penglai-enclave -w /home/penglai/penglai-enclave --rm -it ddnirvana/penglai-enclave:v0.5 bash
   ```
  
  In the Penglai docker container
   ```
   cd /home/penglai/penglai-enclave/Penglai-Enclave-sPMP/opensbi-0.9
   mkdir -p build-oe/qemu-virt-mm
   CROSS_COMPILE=riscv64-unknown-linux-gnu- make O=build-oe/qemu-virt-mm PLATFORM=generic FW_PAYLOADMM=y FW_PAYLOADMM_PATH=/home/penglai/penglai-enclave/STANDALONE_MM.fd
   exit
   ```

### 4. Generate the flash images
  The secure variable storage will be put at 0x20000000, the flash0 block in Qemu virt machine.
  The EDK2 EFI firmware will be put at 0x22000000, the flash1 block in Qemu virt machine.
   ```
   cd $WORKDIR
   rm flash0.img
   dd if=/dev/zero of=flash0.img bs=1M count=32
   dd if=./RISCV_FLASH0.fd of=flash0.img conv=notrunc
   rm flash1.img
   dd if=/dev/zero of=flash1.img bs=1M count=32
   dd if=./RISCV_VIRT.fd of=flash1.img conv=notrunc
   ```
  flash0.img contains the secure variable storage.
  flash1.img contains EDK2 EFI code.
 
### 5. Edit the Device Tree
  Non-secure shared memory between UEFI and standalone MM is
  allocated at 0xFFE00000. The non-secure shared memory base address
  should be passed from trusted-firmware through the device
   ```
   cd $WORKDIR
   ./qemu-system-riscv64 -nographic -machine virt,dumpdtb=qemu-virt.dtb -smp 4 -m 2G -bios ./Penglai-Enclave-sPMP/opensbi-0.9/build-oe/qemu-virt-mm/platform/generic/firmware/fw_dynamic.elf
   sudo apt-get install device-tree-compiler
   dtc -I dtb -O dts -o qemu-virt.dts qemu-virt.dtb
   ```

  Change the qemu-virt.dts file to reserve the shared NsMemory (put the below content right after the memoy@80000000 section)
   ```
   reserved-memory {
     #address-cells = <0x2>;
     #size-cells = <0x2>;
     ranges;
     mmode_resv0@FFE00000 {
         reg = <0 0xFFE00000 0 0x200000>;
     };
   };
   ```
   ![image](https://github.com/intel-sandbox/personal.yli147.edk2/assets/21300636/819b3a51-49a4-4ef6-8b9a-f9f257abbc80)

  Then generate the new dtb file, (ignore the interrupts_extended_property warnings)
   ```
   dtc -I dts -O dtb -o qemu-virt-new.dtb qemu-virt.dts
   ```

# Memory layout and Bootup flow

### QEMU Virt Board IO Mapping
![image](https://github.com/intel-sandbox/personal.yli147.edk2/assets/21300636/df288581-b861-4120-b600-d3ac9f19f0ee)

### QEMU Virt Board Boot Options
   ```
  -m 2G
  -bios ./Penglai-Enclave-sPMP/opensbi-0.9/build-oe/qemu-virt-mm/platform/generic/firmware/fw_dynamic.elf
  -drive file=./flash0.img,if=pflash,format=raw,unit=0
  -drive file=./flash1.img,if=pflash,format=raw,unit=1
  -serial tcp:localhost:54320 -serial tcp:localhost:54321
   ```
### QEMU Virt Boot Flow
![image](https://github.com/intel-sandbox/personal.yli147.edk2/assets/21300636/26e91845-57eb-454f-b159-0ecce9489d02)
  
# Running

  The flash0.img file contains secure variables.
  And the flash1.img file contains Non-secure UEFI code.
  
  You may create a "run.sh" script as below content
  
  ```
  nc -z  127.0.0.1 54320 || /usr/bin/gnome-terminal -x ./soc_term.py 54320 &
  nc -z  127.0.0.1 54321 || /usr/bin/gnome-terminal -x ./soc_term.py 54321 &
  while ! nc -z 127.0.0.1 54320 || ! nc -z 127.0.0.1 54321; do sleep 1; done
  ./qemu-system-riscv64 -nographic \
    -machine virt -smp 4 -m 2G \
    -dtb ./qemu-virt-new.dtb \
    -bios ./Penglai-Enclave-sPMP/opensbi-0.9/build-oe/qemu-virt-mm/platform/generic/firmware/fw_dynamic.elf \
    -drive file=./flash0.img,if=pflash,format=raw,unit=0 \
    -drive file=./flash1.img,if=pflash,format=raw,unit=1 \
    -serial tcp:localhost:54320 -serial tcp:localhost:54321 \
    -device qemu-xhci -device usb-mouse -device usb-kbd \
    -drive file=fat:rw:~/src/fat,id=hd0 -device virtio-blk-device,drive=hd0
  ```  
  Then downlaod the soc_term.py script and run the run.sh script in Linux Desktop GUI shell
  ```
  wget -c https://github.com/DevendraDevadiga/optee_qemu_armv8a_prebuilt_binaries/raw/main/optee_qemu_armv8a/soc_term.py
  chmod a+x soc_term.py
  chmod a+x run.sh
  ./run.sh
  ```
  
  You can also download and use this VariableTestApp to query the secure variable info in efi shell
  ```
  wget -c https://wiki.ith.intel.com/download/attachments/2844652828/VariableTestApp.efi -P ~/src/fat/
  ./run.sh 
  Shell> FS0:
  FS0:\> VariableTestApp.efi
  ```
  ![image](https://github.com/intel-sandbox/personal.yli147.edk2/assets/21300636/07c77dab-e523-45ea-82f1-f0f9fe0dc278)
