# Pi Pico SPI TPM sniffer 

## Description

SPI TPM sniffing for Bitlocker VMK with a Pi Pico.

## Disclaimer

No warranty.

## Pinout

```
0  - UART TX
18 - MISO
19 - MOSI (unused)
20 - CLK
21 - CSn (unused)
```

## Build

```
export PICO_SDK_PATH=/path/to/pico/pico-sdk
mkdir build

cd build
cmake ..
make -j4
```

## Mount Bitlocker with key

```
mkdir -p /tmp/dislocker-mount

sudo losetup --partscan --find --show "/path/to/image.img"

lsblk
ls /dev/loop0*

# VMK from Pi: 3aac5b4d181f001332840c526446629f1f030d48bdcd175bdedd61caf262c5cab9ef
# Actual VMK:  3aac5b4d18    1332840c526446629f1f030d48bdcd175bdedd61caf262c5cab9ef

sudo ./vmk-mount.sh /dev/loop0p3 "3aac5b4d181f001332840c526446629f1f030d48bdcd175bdedd61caf262c5cab9ef"

mkdir -p /tmp/ntfs-mount
sudo ntfs-3g -o ro /dev/loop1 /tmp/ntfs-mount

# cleanup
sudo losetup -d /dev/loop1
sudo umount /tmp/dislocker-mount
sudo losetup -d /dev/loop0
```

## References
https://blog.compass-security.com/2024/02/microsoft-bitlocker-bypasses-are-practical/ 
https://github.com/pico-coder/sigrok-pico
https://github.com/WithSecureLabs/bitlocker-spi-toolkit 
https://github.com/stacksmashing/pico-tpmsniffer/
