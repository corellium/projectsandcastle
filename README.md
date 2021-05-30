# projectsandcastle
## garbage!! do not use it!!
![IMG_0886](https://user-images.githubusercontent.com/78459878/120094827-3aee2a80-c155-11eb-9a74-b629555c46b6.jpeg)

Android/Linux for the iPhone

## Provided utilities:

* `loader/` loads kernel and device tree via pongoOS
* `syscfg/` tool to extract configuration information from syscfg partition on devices
* `hx-touchd/` touch screen support daemon
* `hcdpack/` tool to heuristically extract Bluetooth firmware from binaries

## Kernel

Kernel can be obtained from our fork of linux-stable:

https://github.com/corellium/linux-sandcastle

## Buildroot

The Sandcastle Linux ramdisk is built using buildroot. Our customizations are here:

https://github.com/corellium/sandcastle-buildroot

## Android applications

### Installing APKs

You can generally install APK files with `adb install foo.apk`. However, the following
limitations apply:

  * pure Java APKs will generally work if they don't need unsupported hardware,
  * APKs containing only ARMv7 binaries (32-bit) will not work,
  * APKs containing ARMv8 binaries (64-bit) will require a rebuild of those binaries.

### Rebuilding binary libraries

Binary libraries need to be built for 16kB page size. First, try these options when
the library is linked:

  `-z common-page-size=0x4000 -z max-page-size=0x4000`

If the linker is wrapped with C compiler, most likely you'll need this:

  `-Wl,-z,common-page-size=0x4000 -Wl,-z,max-page-size=0x4000`

To check if stuff went well, use `readelf -l` on the library:

 * if there's no RELRO segment, check that the LOAD segments with different attributes
   do not occupy the same 16kB page in any place (a good tip-off is 4000 in the ALIGN
   column on all of them),

 * if there is a RELRO segment, make sure that it either starts or ends on a 16kB
   boundary; sometimes compilers put RELRO at start of the RW segment (and RELRO should
   then end at a 16k boundary) and sometimes they put it at the end (and RELRO should
   then start at a 16k boundary).

Basically the idea is that files that are incorrectly built end up having executable,
read-write and read-only data in the same 16k page.

If this doesn't help, check the source of the library for blatant uses of 4096, 0x1000
or 12 for PAGE_SIZE, kPageSize, PAGE_SHIFT, PAGE_BITS, etc. (comparatively rare, but
Chromium is a good example).
