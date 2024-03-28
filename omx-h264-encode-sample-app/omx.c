/* Copyright (c) 2024 Renesas Electronics Corp.
 * SPDX-License-Identifier: MIT-0 */

/*******************************************************************************
 * FILENAME: omx.c
 *
 * DESCRIPTION:
 *   OMX function definition.
 * 
 * NOTE:
 *   For function usage, please refer to 'omx.h'.
 * 
 * AUTHOR: RVC       START DATE: 14/03/2023
 *
 ******************************************************************************/


#include "omx.h"

/******************************************************************************
 *                            FUNCTION DEFINITION                             *
 ******************************************************************************/

void omx_wait_state(OMX_HANDLETYPE handle, OMX_STATETYPE state)
{
    OMX_ERRORTYPE omx_error = OMX_ErrorNone;
    OMX_STATETYPE omx_cur_state = OMX_StateInvalid;

    while (true)
    {
        omx_error = OMX_GetState(handle, &omx_cur_state);

        /* Exit if 'OMX_GetState' returns error */
        if (omx_error != OMX_ErrorNone)
        {
            printf("Error: Failed to get current state of media component\n");
            break;
        }

        /* Exit if OMX has transitioned into desired state */
        if (omx_cur_state == state)
        {
            break;
        }

        /* Wait for 10 ms to avoid wasting CPU cycles */
        usleep(10000);
    }
}

char * omx_state_to_str(OMX_STATETYPE state)
{
    char * p_state_str = NULL;

    struct
    {
        OMX_STATETYPE state;
        const char *  p_state_str;
    }
    state_mapping[] =
    {
        /* The component has detected that its internal data structures are
         * corrupted to the point that it cannot determine its state properly */
        { OMX_StateInvalid, "OMX_StateInvalid" },

        /* The component has been loaded but has not completed initialization.
         * The 'OMX_SetParameter' macro and the 'OMX_GetParameter' macro are
         * the only macros allowed to be sent to the component in this state */
        { OMX_StateLoaded, "OMX_StateLoaded" },

        /* The component initialization has been completed successfully and
         * the component is ready to start */
        { OMX_StateIdle, "OMX_StateIdle" },

        /* The component has accepted the start command and is processing data
         * (if data is available) */
        { OMX_StateExecuting, "OMX_StateExecuting" },

        /* The component has received pause command */
        { OMX_StatePause, "OMX_StatePause" },

        /* The component is waiting for resources, either after preemption or
         * before it gets the resources requested.
         * See OMX IL specification 1.1.2 for complete details */
        { OMX_StateWaitForResources, "OMX_StateWaitForResources" },
    };

    uint32_t len   = sizeof(state_mapping) / sizeof(state_mapping[0]);
    uint32_t index = 0;

    for (index = 0; index < len; index++)
    {
        if (state == state_mapping[index].state)
        {
            p_state_str = malloc(strlen(state_mapping[index].p_state_str) + 1);
            strcpy(p_state_str, state_mapping[index].p_state_str);

            break;
        }
    }

    return p_state_str;
}

bool omx_get_port(OMX_HANDLETYPE handle, OMX_U32 port_idx,
                  OMX_PARAM_PORTDEFINITIONTYPE * p_port)
{
    OMX_PARAM_PORTDEFINITIONTYPE port;

    /* Check parameter */
    assert(p_port != NULL);

    OMX_INIT_STRUCTURE(&port);
    port.nPortIndex = port_idx;

    if (OMX_ErrorNone !=
        OMX_GetParameter(handle, OMX_IndexParamPortDefinition, &port))
    {
        printf("Error: Failed to get port at index '%d'\n", port_idx);
        return false;
    }

    /* Copy data from 'port' to 'p_port' */
    memcpy(p_port, &port, sizeof(struct OMX_PARAM_PORTDEFINITIONTYPE));

    return true;
}

bool omx_get_bitrate_ctrl(OMX_HANDLETYPE handle, OMX_U32 port_idx,
                          OMX_VIDEO_PARAM_BITRATETYPE * p_ctrl)
{
    OMX_VIDEO_PARAM_BITRATETYPE ctrl;

    /* Check parameter */
    assert(p_ctrl != NULL);

    OMX_INIT_STRUCTURE(&ctrl);
    ctrl.nPortIndex = port_idx;

    if (OMX_ErrorNone !=
        OMX_GetParameter(handle, OMX_IndexParamVideoBitrate, &ctrl))
    {
        printf("Error: Failed to get bitrate control of port '%d'\n", port_idx);
        return false;
    }

    /* Copy data from 'ctrl' to 'p_ctrl' */
    memcpy(p_ctrl, &ctrl, sizeof(struct OMX_VIDEO_PARAM_BITRATETYPE));

    return true;
}

bool omx_set_in_port_fmt(OMX_HANDLETYPE handle,
                         OMX_U32 frame_width, OMX_U32 frame_height,
                         OMX_COLOR_FORMATTYPE color_fmt)
{
    bool is_success = false;
    OMX_PARAM_PORTDEFINITIONTYPE in_port;

    /* Get input port */
    if (omx_get_port(handle, 0, &in_port) == true)
    {
        /* Configure and set new parameters to input port */
        in_port.format.video.nFrameWidth   = frame_width;
        in_port.format.video.nFrameHeight  = frame_height;
        in_port.format.video.nStride       = OMX_STRIDE(frame_width);
        in_port.format.video.nSliceHeight  = OMX_SLICE_HEIGHT(frame_height);
        in_port.format.video.eColorFormat  = color_fmt;

        if (OMX_ErrorNone ==
            OMX_SetParameter(handle, OMX_IndexParamPortDefinition, &in_port))
        {
            is_success = true;
        }
    }

    if (!is_success)
    {
        printf("Error: Failed to set input port\n");
    }

    return is_success;
}

bool omx_set_out_port_fmt(OMX_HANDLETYPE handle, OMX_U32 bitrate,
                          OMX_VIDEO_CODINGTYPE compression_fmt,
                          OMX_U32 framerate)
{
    bool b_set_fmt_ok       = false;
    bool b_set_bitrate_ok   = false;
    bool b_set_framerate_ok = false;

    OMX_VIDEO_PARAM_BITRATETYPE ctrl;
    OMX_PARAM_PORTDEFINITIONTYPE out_port;
    OMXR_MC_VIDEO_PARAM_AVC_VUI_PROPERTY vui;

    /* Check parameter */
    assert(bitrate > 0);

    /* Get output port */
    if (omx_get_port(handle, 1, &out_port) == true)
    {
        out_port.format.video.eCompressionFormat = compression_fmt;
        /* Configure and set compression format to output port */

        if (OMX_ErrorNone ==
            OMX_SetParameter(handle, OMX_IndexParamPortDefinition, &out_port))
        {
            b_set_fmt_ok = true;
        }
    }

    /* Get video encode bitrate control for output port */
    if (omx_get_bitrate_ctrl(handle, 1, &ctrl) == true)
    {
        /* Configure and set bitrate to output port */
        ctrl.nTargetBitrate = bitrate;
        ctrl.eControlRate   = OMX_Video_ControlRateConstant;

        if (OMX_ErrorNone ==
            OMX_SetParameter(handle, OMX_IndexParamVideoBitrate, &ctrl))
        {
            b_set_bitrate_ok = true;
        }
    }

    /* Set framerate for output port */
    OMX_INIT_STRUCTURE(&vui);

    /* The 'bFixedFrameRateFlag' field is twice the framerate */
    vui.nPortIndex             = 1;
    vui.u32TimeScale           = framerate * 2;
    vui.u32NumUnitsInTick      = 1;
    vui.bFixedFrameRateFlag    = OMX_TRUE;
    vui.bTimingInfoPresentFlag = OMX_TRUE;

    if (OMX_ErrorNone ==
        OMX_SetParameter(handle, OMXR_MC_IndexParamVideoAVCVuiProperty, &vui))
    {
        b_set_framerate_ok = true;
    }

    if (!b_set_fmt_ok || !b_set_bitrate_ok || !b_set_framerate_ok)
    {
        printf("Error: Failed to set output port\n");
    }

    return (b_set_fmt_ok && b_set_bitrate_ok);
}

bool omx_set_port_buf_cnt(OMX_HANDLETYPE handle,
                          OMX_U32 port_idx, OMX_U32 buf_cnt)
{
    OMX_PARAM_PORTDEFINITIONTYPE port;

    /* Check parameter */
    assert(buf_cnt > 0);

    /* Get port 'port_idx' */
    if (omx_get_port(handle, port_idx, &port) == false)
    {
        return false;
    }

    /* Value 'buf_cnt' must not be less than 'nBufferCountMin' */
    if (buf_cnt < port.nBufferCountMin)
    {
        printf("Error: Port '%d' requires no less than '%d' buffers\n",
                                        port_idx, port.nBufferCountMin);
        return false;
    }

    /* Set the number of buffers that are required on port 'port_idx' */
    port.nBufferCountActual = buf_cnt;

    if (OMX_ErrorNone !=
        OMX_SetParameter(handle, OMX_IndexParamPortDefinition, &port))
    {
        printf("Error: Failed to set port at index '%d'\n", port_idx);
        return false;
    }

    return true;
}

OMX_BUFFERHEADERTYPE ** omx_alloc_buffers(OMX_HANDLETYPE handle,
                                          OMX_U32 port_idx)
{
    uint32_t index = 0;

    OMX_PARAM_PORTDEFINITIONTYPE port;
    OMX_BUFFERHEADERTYPE ** pp_bufs = NULL;

    /* Get port */
    if (omx_get_port(handle, port_idx, &port) == false)
    {
        return NULL;
    }

    /* Allocate an array of 'OMX_BUFFERHEADERTYPE *' */
    pp_bufs = (OMX_BUFFERHEADERTYPE **)
              malloc(port.nBufferCountActual * sizeof(OMX_BUFFERHEADERTYPE *));


    for (index = 0; index < port.nBufferCountActual; index++)
    {
        /* See section 2.2.10 in document 'R01USxxxxEJxxxx_cmn_v1.0.pdf'
         * and 'Table 6-3' in document 'R01USxxxxEJxxxx_vecmn_v1.0.pdf' */
        if (OMX_ErrorNone != OMX_AllocateBuffer(handle,
                                                pp_bufs + index,
                                                port_idx, NULL,
                                                port.nBufferSize))
        {
            printf("Error: Failed to allocate buffers at index '%d'\n", index);
            break;
        }
    }

    if (index < port.nBufferCountActual)
    {
        omx_dealloc_port_bufs(handle, port_idx, pp_bufs, index);
        return NULL;
    }

    return pp_bufs;
}

void omx_dealloc_port_bufs(OMX_HANDLETYPE handle, OMX_U32 port_idx,
                           OMX_BUFFERHEADERTYPE ** pp_bufs, uint32_t count)
{
    uint32_t index = 0;

    /* Check parameter */
    assert(pp_bufs != NULL);

    for (index = 0; index < count; index++)
    {
        OMX_FreeBuffer(handle, port_idx, pp_bufs[index]);
    }

    /* Free entire array */
    free (pp_bufs);
}

void omx_dealloc_all_port_bufs(OMX_HANDLETYPE handle, OMX_U32 port_idx,
                               OMX_BUFFERHEADERTYPE ** pp_bufs)
{
    OMX_PARAM_PORTDEFINITIONTYPE port;

    /* Check parameter */
    assert(pp_bufs != NULL);

    /* Get port */
    if (omx_get_port(handle, port_idx, &port) == true)
    {
        omx_dealloc_port_bufs(handle, port_idx, pp_bufs,
                              port.nBufferCountActual);
    }
}

int omx_get_index(OMX_BUFFERHEADERTYPE * p_buf,
                  OMX_BUFFERHEADERTYPE ** pp_bufs, uint32_t count)
{
    int ret = -1;
    uint32_t index = 0;

    /* Check parameters */
    assert(p_buf != NULL);
    assert((pp_bufs != NULL) && (count > 0));

    for (index = 0; index < count; index++)
    {
        if (pp_bufs[index] == p_buf)
        {
            ret = index;
            break;
        }
    }

    return ret;
}

bool omx_fill_buffers(OMX_HANDLETYPE handle,
                      OMX_BUFFERHEADERTYPE ** pp_bufs, uint32_t count)
{
    bool is_success = true;
    uint32_t index = 0;

    /* Check parameters */
    assert((pp_bufs != NULL) && (count > 0));

    for (index = 0; index < count; index++)
    {
        pp_bufs[index]->nFlags     = 0;
        pp_bufs[index]->nFilledLen = 0;

        if (OMX_FillThisBuffer(handle, pp_bufs[index]) != OMX_ErrorNone)
        {
            printf("Error: Failed to send buffer '%d' to output port\n", index);

            is_success = false;
            break;
        }
    }

    return is_success;
}

OMX_U32 omx_empty_buffer(OMX_HANDLETYPE handle, FILE * p_file,
                         OMX_BUFFERHEADERTYPE * p_buf, uint32_t len)
{
    /* Contain the number of bytes read from input file */
    size_t bytes_read = 0;

    /* Check parameters */
    assert(p_buf  != NULL);
    assert(p_file != NULL);

    /* 'len' must not exceed the total size of the allocated buffer */
    assert(len <= p_buf->nAllocLen);

    if (feof(p_file) != 0)
    {
        /* The End-of-File indicator for the input file is set */
        p_buf->nFilledLen = 0;
        p_buf->nFlags = OMX_BUFFERFLAG_EOS;
    }
    else
    {
        /* Read an NV12 frame from the input file */
        bytes_read = fread(p_buf->pBuffer, 1, len, p_file);

        if (bytes_read == len)
        {
            /* Since the program must store one input picture data into a single
             * buffer for the component, all the input buffers must have this flag */
            p_buf->nFilledLen = len;
            p_buf->nFlags = OMX_BUFFERFLAG_ENDOFFRAME;
        }
        else
        {
            /* In this case, the data read from 'fread()' has been corrupted.
             * So, it will be marked as an empty and End-of-Stream buffer */
            p_buf->nFilledLen = 0;
            p_buf->nFlags = OMX_BUFFERFLAG_EOS;
        }
    }

    assert(OMX_EmptyThisBuffer(handle, p_buf) == OMX_ErrorNone);
    return p_buf->nFlags;
}