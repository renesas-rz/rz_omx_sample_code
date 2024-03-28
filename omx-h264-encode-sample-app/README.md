# OMX H.264 Encode Sample App

## Table of contents

1. [Target devices](#target-devices)
2. [Supported environments](#supported-environments)
3. [Overview](#overview)
4. [Software](#software)
5. [How to compile sample app](#how-to-compile-sample-app)
6. [How to run sample app](#how-to-run-sample-app)
7. [Revision history](#revision-history)
8. [Appendix](#appendix)

## Target devices

* [RZ/G2N Evaluation Board Kit](https://www.renesas.com/us/en/products/microcontrollers-microprocessors/rz-mpus/rzg2n-ultra-high-performance-microprocessors-dual-core-arm-cortex-a57-15-ghz-cpus-3d-graphics-and-4k-video).
* [RZ/G2L Evaluation Board Kit](https://www.renesas.com/eu/en/products/microcontrollers-microprocessors/rz-mpus/rzg2l-evkit-rzg2l-evaluation-board-kit)

## Supported environments

* VLP 3.0.6.

Note: Other environments may also work. Please use at your own risks.

## Software

* **H.264 encoding:** OMX IL (proprietary).

## Overview

The sample app encodes video raw frames to H.264.  
Note: H.264 encoding is hardware accelerated.

### Source code

| File name | Summary |
| --------- | ------- |
| in-nv12-640x480.raw | Input file. |cd ..
| omx.h, omx.c | Contain macros that calculate stride, slice height from video resolution and functions that wait for OMX state, get/set input/output port, allocate/free buffers for input/output ports... |
| main.c | OMX H.264 encode sample app. |

## How to compile sample app

> **Note 1:** The SDK must be generated from either _core-image-weston_ or _core-image-qt_.  

* Source the environment setup script of SDK:

  ```bash
  user@ubuntu:~$ source /path/to/sdk/environment-setup-aarch64-poky-linux
  ```

* Go to directory _rz_omx_sample_code/omx-h264-encode-sample-app_ and run _make_ command:

  ```bash
  user@ubuntu:~$ cd rz_omx_sample_code/omx-h264-encode-sample-app
  user@ubuntu:~/rz_omx_sample_code/omx-h264-encode-sample-app$ make
  ```

* After compilation, the sample app _encoder_ should be generated as below:

  ```bash
  rz_omx_sample_code/
  └── omx-h264-encode-sample-app/
      ├── MIT-0.txt
      ├── Makefile
      ├── README.md
      ├── encoder
      ├── in-nv12-640x480.raw
      ├── main.c
      ├── main.o
      ├── omx.c
      ├── omx.h
      └── omx.o
  ```

## How to run sample app

* After [compilation](#how-to-compile-sample-app), copy directory _omx-h264-encode-sample-app_ to directory _/home/root/_ of RZ/G2N or RZ/G2L board.  
Then, run the following commands:

  ```bash
  root@smarc-rzg2l:~# cd omx-h264-encode-sample-app
  root@smarc-rzg2l:~/omx-h264-encode-sample-app# chmod 755 encoder
  root@smarc-rzg2l:~/omx-h264-encode-sample-app# ./encoder
  ```

* The sample app _encoder_ should generate the below messages:

  ```bash
  root@smarc-rzg2l:~/omx-h264-encode-sample-app# ./encoder
  OMX state: 'OMX_StateIdle'
  OMX state: 'OMX_StateExecuting'
  ...
  EmptyBufferDone exited
  FillBufferDone exited
  OMX event: 'End-of-Stream'
  FillBufferDone exited
  EmptyBufferDone exited
  EmptyBufferDone exited
  FillBufferDone exited
  OMX state: 'OMX_StateIdle'
  ```

* Wait for a few moments. The output video will be generated as below:

  ```bash
  root@smarc-rzg2l:~/omx-h264-encode-sample-app# ls -l out-*
  -rw-r--r-- 1 root root 914439 Sep 20 12:59 out-h264-640x480.264
  ```

* You can open it with [Media Classic Player](https://mpc-hc.org/) on Windows (recommended), [Videos application](https://manpages.ubuntu.com/manpages/trusty/man1/totem.1.html) on Ubuntu, or GStreamer pipeline on [VLP environment](#supported-environments) as below:

  ```bash
  gst-launch-1.0 filesrc location=out-h264-640x480.264 ! h264parse ! omxh264dec ! waylandsink
  ```
## Revision history

| Version | Date | Summary |
| ------- | ---- | ------- |
| 1.0 | Mar 28, 2024 | Add OMX H.264 encode sample app. |

## Appendix

You can use the following command to create your own input file.  
Note: Press Ctrl-C to exit the command.

  ```bash
  gst-launch-1.0 videotestsrc ! video/x-raw, format=NV12, width=640, height=480 ! filesink location=in-nv12-640x480.raw
  ```
  
