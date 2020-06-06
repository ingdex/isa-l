## Prerequisite

Install the dependencies:
* a c++14 compliant c++ compiler
* cmake >= 3.1
* git
* autogen
* autoconf
* automake
* yasm and/or nasm
* libtool
* boost's "Program Options" library and headers

```
sudo apt-get update
sudo apt-get install gcc g++ make cmake git autogen autoconf automake yasm nasm libtool libboost-all-dev 

```

Also needed is the latest versions of isa-l. The `get_libs.bash` script can be used to get it.
The script will download the library from its official GitHub repository, build it, and install it in `./libs/usr`.

* `bash ./libs/get_libs.bash`

## Build

* `mkdir <build-dir>`
* `cd <build-dir>`
* `cmake -DCMAKE_BUILD_TYPE=Release $OLDPWD`
* `make`

## Run

* `cd <build-bir>`
* `./ex2 --help`

## Usage

```
Usage: ./ex2 [--help] [--data-size <size>] [--buffer-count <n>] [--lost-buffers <n>]:
  --help                     display this message
  --data-size size (=256MiB) the amount of data to be distributed among the  buffers (size <= 256MiB)
  --buffer-count n (=24)     the number of buffers to distribute the data across (n <= 24)
  --lost-buffers n (=2)      the number of buffer that will get inaccessible (n <= 2)
```

Sizes must be formated the following way:
* a number
* zero or more white space
* zero or one prefix (`k`, `M`)
* zero or one `i` character (note: `1kiB == 1024B`, `1kB == 1000B`)
* the character `B`

E.g.:
* `1KB`
* `"128 MiB"`
* `42B`

## Example run

```
$> ./ex2
[Info   ] Using isa-l                  2.16.0
[Info   ] Dataset size:                268.4 MB (256.0 MiB)
[Info   ] Number of buffers:           24
[Info   ] Number of lost buffers:      2
[Info   ] Error correction codes size: 24.4 MB (23.3 MiB)
[Info   ] Buffer size:                 12.2 MB (11.6 MiB)
[Info   ] Perfoming benchmark...
[Info   ] 100 % done
[Info   ] Average results over 34 iterations:
[Info   ] Storage time:                30224 us
[Info   ] Recovery time:               31255 us
```

We're using 24 buffers in total, and 2 of those buffers will be lost.
Therefore, 2 buffers are used to hold the error correction codes, and the other
22 are holding the data.
