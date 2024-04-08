# OMX H.264 Decode Sample App

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

* **H.264 decoding:** OMX IL (proprietary).

## Overview

The sample app decodes H.264 video frames to NV12 raw frames.  
Note: H.264 decoding is hardware accelerated.

### Source code

| File name | Summary |
| --------- | ------- |
| in-h264-640x480.264 | Input file. |
| omx.h, omx.c | Contain functions that wait for OMX state, get/set input/output port, allocate/free buffers for input/output ports... |
| main.c | OMX H.264 decode sample app. |

## How to compile sample app

> **Note 1:** The SDK must be generated from either _core-image-weston_ or _core-image-qt_.  

* Source the environment setup script of SDK:

  ```bash
  user@ubuntu:~$ source /path/to/sdk/environment-setup-aarch64-poky-linux
  ```

* Go to directory _rz_omx_sample_code/omx-h264-decode-sample-app_ and run _make_ command:

  ```bash
  user@ubuntu:~$ cd rz_omx_sample_code/omx-h264-decode-sample-app
  user@ubuntu:~/rz_omx_sample_code/omx-h264-decode-sample-app$ make
  ```

* After compilation, the sample app _decoder_ should be generated as below:

  ```bash
  rz_omx_sample_code/
  └── omx-h264-decode-sample-app/
      ├── MIT-0.txt
      ├── Makefile
      ├── README.md
      ├── decoder
      ├── in-h264-640x480.264
      ├── main.c
      ├── main.o
      ├── omx.c
      ├── omx.h
      └── omx.o
  ```

## How to run sample app

* After [compilation](#how-to-compile-sample-app), copy directory _omx-h264-decode-sample-app_ to directory _/home/root/_ of RZ/G2N or RZ/G2L board.  
Then, run the following commands:

  ```bash
  root@smarc-rzg2l:~# cd omx-h264-decode-sample-app
  root@smarc-rzg2l:~/omx-h264-decode-sample-app# chmod 755 decoder
  root@smarc-rzg2l:~/omx-h264-decode-sample-app# ./decoder
  ```

* The sample app _decoder_ should generate the below messages:

  ```bash
  root@smarc-rzg2l:~/omx-h264-decode-sample-app# ./decoder
  OMX state: 'OMX_StateIdle'
  OMX state: 'OMX_StateExecuting'
  EmptyBufferDone exited
  EmptyBufferDone exited
  OMX event: 'Output port settings changed'
  EmptyBufferDone exited
  FillBufferDone callback.
  FillBufferDone callback.
  FillBufferDone callback.
  EmptyBufferDone exited
  Output port is disabled
  EmptyBufferDone exited
  Output port is enabled
  ...
  EmptyBufferDone exited
  EmptyBufferDone exited
  FillBufferDone callback.
  OMX event: 'End-of-Stream'
  FillBufferDone callback.
  EmptyBufferDone exited
  OMX event: 'End-of-Stream'
  FillBufferDone callback.
  OMX event: 'End-of-Stream'
  FillBufferDone callback.
  OMX state: 'OMX_StateIdle'
  OMX state: 'OMX_StateLoaded'
  ```

* Wait for a few moments. The output video will be generated as below:

  ```bash
  root@smarc-rzg2l:~/omx-h264-decode-sample-app# ls -l out-*
  -rw-r--r-- 1 root root 614246400 Sep 20 12:04 out-nv12-640x480.raw
  ```

* You can open it with GStreamer pipeline on [VLP environment](#supported-environments) as below:

  ```bash
  gst-launch-1.0 filesrc location=out-nv12-640x480.raw ! videoparse format=nv12 width=640 height=480 ! videoconvert ! autovideosink
  ```

## Revision history

| Version | Date | Summary |
| ------- | ---- | ------- |
| 1.0 | Apr 08, 2024 | Add OMX H.264 decode sample app. |

## Appendix

* You can use the following command to create your own input file:
  ```bash
  gst-launch-1.0 videotestsrc num-buffers=100 ! video/x-raw, width=640, height=480 ! omxh264enc target-bitrate=10485760 ! h264parse ! filesink location=in-h264-640x480.264
  ```
