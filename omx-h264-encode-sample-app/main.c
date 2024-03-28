/* Copyright (c) 2024 Renesas Electronics Corp.
 * SPDX-License-Identifier: MIT-0 */

#include <semaphore.h>

#include "omx.h"

/******************************************************************************
 *                                   MACROS                                   *
 ******************************************************************************/

#define FRAME_WIDTH_IN_PIXELS  640

#define FRAME_HEIGHT_IN_PIXELS 480

#define NV12_FRAME_SIZE_IN_BYTES (FRAME_WIDTH_IN_PIXELS  * \
                                  FRAME_HEIGHT_IN_PIXELS * 1.5f)

#define FRAMERATE 30 /* FPS */

/* Input file which contains NV12 frames */
#define IN_FILE_NAME "in-nv12-640x480.raw"

/* Output file which contains H.264 frames */
#define OUT_FILE_NAME "out-h264-640x480.264"

/* The number of buffers to be allocated for input port of media component */
#define NV12_BUFFER_COUNT 2

/* The number of buffers to be allocated for output port of media component */
#define H264_BUFFER_COUNT 2

/* The bitrate is related to the quality of output file and compression level
 * of video encoder. For example:
 *   - With 1 Mbit/s, the encoder produces ~1.2 MB of data for 10-second video.
 *   - With 5 Mbit/s, the encoder produces ~6 MB of data for 10-second video
 *                                          and the quality should be better */
#define H264_BITRATE 5000000 /* 5 Mbit/s */

/******************************************************************************
 *                                 STRUCTURES                                 *
 ******************************************************************************/

/* This structure is shared between OMX's callbacks */
typedef struct
{
    /* End-of-Stream flag */
    bool eos;

    /* File descriptor of input file */
    FILE * p_in_file;

    /* File descriptor of output file */
    FILE * p_out_file;

    /* A semaphore which will be locked until End-of-Stream event occurs */
    sem_t smp_eos;

} omx_data_t;

/******************************************************************************
 *                               OMX CALLBACKS                                *
 ******************************************************************************/

/* The method is used to notify the application when an event of interest
 * occurs within the component.
 *
 * Events are defined in the 'OMX_EVENTTYPE' enumeration.
 * Please see that enumeration for details of what will be returned for
 * each type of event.
 *
 * Callbacks should not return an error to the component, so if an error
 * occurs, the application shall handle it internally.
 *
 * Note: This is a blocking call */
OMX_ERRORTYPE omx_event_handler(OMX_HANDLETYPE hComponent, OMX_PTR pAppData,
                                OMX_EVENTTYPE eEvent, OMX_U32 nData1,
                                OMX_U32 nData2, OMX_PTR pEventData);

/* This method is used to return emptied buffers from an input port back to
 * the application for reuse.
 *
 * This is a blocking call so the application should not attempt to refill
 * the buffers during this call, but should queue them and refill them in
 * another thread.
 *
 * Callbacks should not return an error to the component, so the application
 * shall handle any errors generated internally */
OMX_ERRORTYPE omx_empty_buffer_done(OMX_HANDLETYPE hComponent,
                                    OMX_PTR pAppData,
                                    OMX_BUFFERHEADERTYPE * pBuffer);

/* The method is used to return filled buffers from an output port back to
 * the application for emptying and then reuse.
 *
 * This is a blocking call so the application should not attempt to empty
 * the buffers during this call, but should queue them and empty them in
 * another thread.
 *
 * Callbacks should not return an error to the component, so the application
 * shall handle any errors generated internally */
OMX_ERRORTYPE omx_fill_buffer_done(OMX_HANDLETYPE hComponent,
                                   OMX_PTR pAppData,
                                   OMX_BUFFERHEADERTYPE * pBuffer);

/******************************************************************************
 *                               MAIN FUNCTION                                *
 ******************************************************************************/

int main()
{
    /* Handle of media component */
    OMX_HANDLETYPE handle;

    /* Callbacks used by media component */
    OMX_CALLBACKTYPE callbacks =
    {
        .EventHandler    = omx_event_handler,
        .EmptyBufferDone = omx_empty_buffer_done,
        .FillBufferDone  = omx_fill_buffer_done
    };

    /* Buffers for input and output ports */
    OMX_BUFFERHEADERTYPE ** pp_in_bufs  = NULL;
    OMX_BUFFERHEADERTYPE ** pp_out_bufs = NULL;

    /* Iterator */
    int index = 0;

    /* Shared data between OMX's callbacks */
    omx_data_t omx_data;

    /* At startup, End-of-Stream flag is set to false */
    omx_data.eos = false;

    /* Initialize semaphore */
    sem_init(&omx_data.smp_eos, 0, 0);

    /**************************************************************************
     *                  STEP 1: OPEN INPUT AND OUTPUT FILES                   *
     **************************************************************************/

    /* Open input file */
    omx_data.p_in_file = fopen(IN_FILE_NAME, "rb");
    assert(omx_data.p_in_file != NULL);

    /* Open output file */
    omx_data.p_out_file = fopen(OUT_FILE_NAME, "wb");
    assert(omx_data.p_out_file != NULL);

    /* Set file position indicator of input file to the end of file */
    fseek(omx_data.p_in_file, 0, SEEK_END);

    /* Check if the input file contains at least 1 NV12 frame? */
    assert(ftell(omx_data.p_in_file) >= NV12_FRAME_SIZE_IN_BYTES);

    /* Set file position indicator of input file to the beginning of file */
    fseek(omx_data.p_in_file, 0, SEEK_SET);

    /**************************************************************************
     *                         STEP 2: SET UP OMX IL                          *
     **************************************************************************/

    /* Initialize OMX IL core */
    assert(OMX_Init() == OMX_ErrorNone);

    /* Locate Renesas's H.264 encoder.
     * If successful, the component will be in state LOADED */
    assert(OMX_ErrorNone == OMX_GetHandle(&handle,
                                          RENESAS_VIDEO_ENCODER_NAME,
                                          (OMX_PTR)&omx_data, &callbacks));

    /* Config input port */
    assert(omx_set_in_port_fmt(handle,
                               FRAME_WIDTH_IN_PIXELS,
                               FRAME_HEIGHT_IN_PIXELS,
                               OMX_COLOR_FormatYUV420SemiPlanar));

    assert(omx_set_port_buf_cnt(handle, 0, NV12_BUFFER_COUNT));

    /* Config output port */
    assert(omx_set_out_port_fmt(handle, H264_BITRATE,
                                OMX_VIDEO_CodingAVC, FRAMERATE));

    assert(omx_set_port_buf_cnt(handle, 1, H264_BUFFER_COUNT));

    /* Transition into state IDLE */
    assert(OMX_ErrorNone == OMX_SendCommand(handle,
                                            OMX_CommandStateSet,
                                            OMX_StateIdle, NULL));

    /**************************************************************************
     *                STEP 3: ALLOCATE BUFFERS FOR INPUT PORT                 *
     **************************************************************************/

    pp_in_bufs = omx_alloc_buffers(handle, 0);
    assert(pp_in_bufs != NULL);

    /**************************************************************************
     *                STEP 4: ALLOCATE BUFFERS FOR OUTPUT PORT                *
     **************************************************************************/

    pp_out_bufs = omx_alloc_buffers(handle, 1);
    assert(pp_out_bufs != NULL);

    omx_wait_state(handle, OMX_StateIdle);

    /**************************************************************************
     *             STEP 5: MAKE OMX READY TO SEND/RECEIVE BUFFERS             *
     **************************************************************************/

    assert(OMX_ErrorNone == OMX_SendCommand(handle, OMX_CommandStateSet,
                                            OMX_StateExecuting, NULL));
    omx_wait_state(handle, OMX_StateExecuting);

    /**************************************************************************
     *          STEP 6: SEND BUFFERS IN 'PP_OUT_BUFS' TO OUTPUT PORT          *
     **************************************************************************/

    assert(omx_fill_buffers(handle, pp_out_bufs, H264_BUFFER_COUNT));

    /**************************************************************************
     *           STEP 7: SEND BUFFERS IN 'PP_IN_BUFS' TO INPUT PORT           *
     **************************************************************************/

    for (index = 0; index < NV12_BUFFER_COUNT; index++)
    {
        if (OMX_BUFFERFLAG_EOS & omx_empty_buffer(handle,
                                                  omx_data.p_in_file,
                                                  pp_in_bufs[index],
                                                  NV12_FRAME_SIZE_IN_BYTES))
        {
            /* Stop sending next buffers if the current buffer is marked as
             * OMX_BUFFERFLAG_EOS */
            break;
        }
    }

    /**************************************************************************
     *             STEP 8: WAIT UNTIL END-OF-STREAM EVENT OCCURS              *
     **************************************************************************/

    sem_wait(&omx_data.smp_eos);

    /**************************************************************************
     *                          STEP 9: CLEAN UP OMX                          *
     **************************************************************************/

    /* Transition back to idle state */
    assert(OMX_ErrorNone == OMX_SendCommand(handle, OMX_CommandStateSet,
                                            OMX_StateIdle, NULL));
    omx_wait_state(handle, OMX_StateIdle);

    /* Transition back to loaded state */
    assert(OMX_ErrorNone == OMX_SendCommand(handle, OMX_CommandStateSet,
                                            OMX_StateLoaded, NULL));

    /* Free output buffers */
    omx_dealloc_all_port_bufs(handle, 1, pp_out_bufs);

    /* Free input buffers */
    omx_dealloc_all_port_bufs(handle, 0, pp_in_bufs);

    /* Wait until the component is in state LOADED */
    omx_wait_state(handle, OMX_StateLoaded);

    /* Free the component's handle */
    assert(OMX_FreeHandle(handle) == OMX_ErrorNone);

    /* Deinitialize OMX IL core */
    assert(OMX_Deinit() == OMX_ErrorNone);

    /**************************************************************************
     *                 STEP 10: CLOSE INPUT AND OUTPUT FILES                  *
     **************************************************************************/

    /* Close output file */
    fclose(omx_data.p_out_file);

    /* Close input file */
    fclose(omx_data.p_in_file);

    return 0;
}

/******************************************************************************
 *                           OMX CALLBACK HANDLERS                            *
 ******************************************************************************/

OMX_ERRORTYPE omx_event_handler(OMX_HANDLETYPE hComponent, OMX_PTR pAppData,
                                OMX_EVENTTYPE eEvent, OMX_U32 nData1,
                                OMX_U32 nData2, OMX_PTR pEventData)
{
    /* Mark parameters as unused */
    UNUSED(hComponent);
    UNUSED(pEventData);

    omx_data_t * p_data = (omx_data_t *)pAppData;

    char * p_state_str = NULL;

    switch (eEvent)
    {
        case OMX_EventCmdComplete:
        {
            if (nData1 == OMX_CommandStateSet)
            {
                p_state_str = omx_state_to_str((OMX_STATETYPE)nData2);
                if (p_state_str != NULL)
                {
                    /* Print OMX state */
                    printf("OMX state: '%s'\n", p_state_str);
                    free(p_state_str);
                }
            }
        }
        break;

        case OMX_EventBufferFlag:
        {
            if (nData1 == OMX_BUFFERFLAG_EOS)
            {
                sem_post(&p_data->smp_eos);
                p_data->eos = true;
            }
            /* The buffer contains the last output picture data */
            printf("OMX event: 'End-of-Stream'\n");
        break;
        }

        case OMX_EventError:
        {
            /* Section 2.1.2 in document 'R01USxxxxEJxxxx_vecmn_v1.0.pdf' */
            printf("OMX error event: '0x%x'\n", nData1);
        }
        break;

        default:
        {
            /* Intentionally left blank */
        }
        break;
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE omx_empty_buffer_done(OMX_HANDLETYPE hComponent,
                                    OMX_PTR pAppData,
                                    OMX_BUFFERHEADERTYPE * pBuffer)
{
    omx_data_t * p_data = (omx_data_t *)pAppData;

    /* Check parameter */
    assert(p_data != NULL);

    if (p_data->eos == false)
    {
        /* The 'pBuffer' is now avaiable to use. Try to add it back
         * to the input port when End-of-Stream event does not occur */
        omx_empty_buffer(hComponent, p_data->p_in_file,
                         pBuffer, NV12_FRAME_SIZE_IN_BYTES);
    }

    printf("EmptyBufferDone exited\n");
    return OMX_ErrorNone;
}

OMX_ERRORTYPE omx_fill_buffer_done(OMX_HANDLETYPE hComponent,
                                   OMX_PTR pAppData,
                                   OMX_BUFFERHEADERTYPE * pBuffer)
{
    omx_data_t * p_data = (omx_data_t *)pAppData;

    /* Check parameter */
    assert(p_data != NULL);

    if ((p_data->eos == false) && (pBuffer != NULL))
    {
        if (pBuffer->nFilledLen > 0)
        {
            fwrite(pBuffer->pBuffer, 1, pBuffer->nFilledLen, p_data->p_out_file);
        }

        pBuffer->nFlags     = 0;
        pBuffer->nFilledLen = 0;

        /* The 'pBuffer' is now avaiable to use. Try to add it back
         * to the output port when End-of-Stream event does not occur */
        assert(OMX_FillThisBuffer(hComponent, pBuffer) == OMX_ErrorNone);
    }

    printf("FillBufferDone exited\n");
    return OMX_ErrorNone;
}
