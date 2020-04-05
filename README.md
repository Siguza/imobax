# imobax

## Overview

The **i**OS **Mo**bile **Ba**ckup **X**tractor.  
It extracts backups... and stuff.

Binary releases for macOS: [here](https://github.com/Siguza/imobax/releases).

## Using with iTunes Backups

On macOS, iTunes places backups in `~/Library/Application Support/MobileSync/Backup`. ;)

## Using with libimobiledevice

Use `idevicebackup2 backup --full` to take an **unencrypted** backup.

## Building

### Dependencies

#### Ubuntu

`sudo apt install libsqlite3-dev libssl-dev`

### Compiling

As always:

    make

### Usage

    Usage:
        ./imobax [-f] [-i] [-l] source-dir [target-dir]
    
    Options:
        -f  Force overwriting of existing files
        -i  Ignore missing files in backup
        -l  List contents only, write nothing

## License

[MPL2](https://github.com/Siguza/imobax/blob/master/LICENSE) with Exhibit B.
