/* Copyright (c) 2024 Renesas Electronics Corp.
 * SPDX-License-Identifier: MIT-0 */

/*******************************************************************************
 * FILENAME: omx.h
 *
 * DESCRIPTION:
 *   OMX functions.
 *
 * PUBLIC FUNCTIONS:
 *   omx_wait_state
 *
 *   omx_state_to_str
 *
 *   omx_get_port
 *   omx_set_port_buf_cnt
 *   omx_set_out_port_fmt
 *
 *   omx_alloc_buffers
 *   omx_dealloc_port_bufs
 *   omx_dealloc_all_port_bufs
 *
 *   omx_fill_buffers
 *
 * AUTHOR: RVC       START DATE: 14/03/2023
 *
 ******************************************************************************/

#ifndef _OMX_H_
#define _OMX_H_

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include <assert.h>
#include <stdlib.h>
#include <unistd.h>

#include <OMX_Core.h>
#include <OMX_Types.h>
#include <OMX_Component.h>

#include <OMXR_Extension_vecmn.h>
#include <OMXR_Extension_h264e.h>

/******************************************************************************
 *                              MACRO VARIABLES                               *
 ******************************************************************************/

/* The component name for H.264 decoder media component */
#define RENESAS_VIDEO_DECODER_NAME "OMX.RENESAS.VIDEO.DECODER.H264"

/******************************************************************************
 *                              FUNCTION MACROS                               *
 ******************************************************************************/

 /* Mark variable 'VAR' as unused */
#define UNUSED(VAR) ((void)(VAR))

 /* Return smallest integral value not less than 'VAL' and divisible by 'RND'
 * (based on: https://github.com/Xilinx/vcu-omx-il/blob/master/exe_omx/encoder).
 *
 * Examples:
 *   - ROUND_UP(359, 2) -> 360.
 *   - ROUND_UP(480, 2) -> 480.
 *
 *   - ROUND_UP(360, 32)  ->  384.
 *   - ROUND_UP(640, 32)  ->  640.
 *   - ROUND_UP(720, 32)  ->  736.
 *   - ROUND_UP(1280, 32) -> 1280.
 *   - ROUND_UP(1920, 32) -> 1920 */
#define ROUND_UP(VAL, RND) (((VAL) + (RND) - 1) & (~((RND) - 1)))

/* The macro is used to populate 'nSize' and 'nVersion' fields of 'P_STRUCT'
 * before passing it to one of the below functions:
 *   - OMX_GetConfig
 *   - OMX_SetConfig
 *   - OMX_GetParameter
 *   - OMX_SetParameter
 */
#define OMX_INIT_STRUCTURE(P_STRUCT)                                \
{                                                                   \
    memset((P_STRUCT), 0, sizeof(*(P_STRUCT)));                     \
                                                                    \
    (P_STRUCT)->nSize = sizeof(*(P_STRUCT));                        \
                                                                    \
    (P_STRUCT)->nVersion.s.nVersionMajor = OMX_VERSION_MAJOR;       \
    (P_STRUCT)->nVersion.s.nVersionMinor = OMX_VERSION_MINOR;       \
    (P_STRUCT)->nVersion.s.nRevision     = OMX_VERSION_REVISION;    \
    (P_STRUCT)->nVersion.s.nStep         = OMX_VERSION_STEP;        \
}

/* Get stride from frame width */
#define OMX_STRIDE(WIDTH) ROUND_UP(WIDTH, 32)

/* Get slice height from frame height */
#define OMX_SLICE_HEIGHT(HEIGHT) ROUND_UP(HEIGHT, 2)

/******************************************************************************
 *                            FUNCTION DECLARATION                            *
 ******************************************************************************/

/* Block calling thread until the component is in state 'state'
 * (based on section 3.2.2.13.2 in OMX IL specification 1.1.2) */
void omx_wait_state(OMX_HANDLETYPE handle, OMX_STATETYPE state);

/* Convert 'OMX_STATETYPE' to string.
 * Return the string (useful when passing the function to 'printf').
 *
 * Note: The string must be freed when no longer used */
char * omx_state_to_str(OMX_STATETYPE state);

/* Get port's structure 'OMX_PARAM_PORTDEFINITIONTYPE'.
 * Return true if successful. Otherwise, return false.
 *
 * Note: 'port_idx' should be 0 (input port) or 1 (output port) */
bool omx_get_port(OMX_HANDLETYPE handle, OMX_U32 port_idx,
                  OMX_PARAM_PORTDEFINITIONTYPE * p_port);

/* Set 'buf_cnt' buffers to port 'port_idx'.
 * Return true if successful. Otherwise, return false */
bool omx_set_port_buf_cnt(OMX_HANDLETYPE handle,
                          OMX_U32 port_idx, OMX_U32 buf_cnt);

/* Set raw format to ouput port's structure 'OMX_PARAM_PORTDEFINITIONTYPE'.
 * Return true if successful. Otherwise, return false */
bool omx_set_out_port_fmt(OMX_HANDLETYPE handle, OMX_COLOR_FORMATTYPE fmt);

/* Allocate buffers and buffer headers for port at 'port_idx'.
 * Return non-NULL value if successful. Otherwise, return NULL */
OMX_BUFFERHEADERTYPE ** omx_alloc_buffers(OMX_HANDLETYPE handle,
                                          OMX_U32 port_idx);

/* Free 'count' elements in 'pp_bufs' */
void omx_dealloc_port_bufs(OMX_HANDLETYPE handle, OMX_U32 port_idx,
                           OMX_BUFFERHEADERTYPE ** pp_bufs, uint32_t count);

/* Free 'nBufferCountActual' elements in 'pp_bufs'.
 * Note: Make sure the length of 'pp_bufs' is equal to 'nBufferCountActual' */
void omx_dealloc_all_port_bufs(OMX_HANDLETYPE handle, OMX_U32 port_idx,
                               OMX_BUFFERHEADERTYPE ** pp_bufs);

/* Send buffers in 'pp_bufs' to output port.
 * Return true if successful. Otherwise, return false */
bool omx_fill_buffers(OMX_HANDLETYPE handle,
                      OMX_BUFFERHEADERTYPE ** pp_bufs, uint32_t count);

#endif /* _OMX_H_ */
