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

bool omx_set_out_port_fmt(OMX_HANDLETYPE handle, OMX_COLOR_FORMATTYPE fmt)
{
    bool is_success = false;
    OMX_PARAM_PORTDEFINITIONTYPE out_port;

    /* Get output port */
    if (omx_get_port(handle, 1, &out_port) == true)
    {
        /* Configure and set new parameters to input port */
        out_port.format.video.eColorFormat = fmt;

        if (OMX_ErrorNone ==
            OMX_SetParameter(handle, OMX_IndexParamPortDefinition, &out_port))
        {
            is_success = true;
        }
    }

    if (!is_success)
    {
        printf("Error: Failed to set output port\n");
    }

    return is_success;
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
