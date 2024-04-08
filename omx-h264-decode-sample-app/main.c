#include "omx.h"

#include <semaphore.h>

#include <gst/gst.h>
#include <gst/app/gstappsink.h>

/******************************************************************************
 *                                   MACROS                                   *
 ******************************************************************************/

/* Input file which contains H.264 frames */
#define IN_FILE_NAME "in-h264-640x480.264"

/* Output file which contains NV12 frames */
#define OUT_FILE_NAME "out-nv12-640x480.raw"

/* The number of buffers for input port of media component (MC) */
#define IN_BUFFER_COUNT 2

/* The number of buffers for output port of media component (MC) */
#define OUT_BUFFER_COUNT 3

/******************************************************************************
 *                                 STRUCTURES                                 *
 ******************************************************************************/

typedef struct
{
    /* End-of-Stream (EOS) flag */
    bool eos;

    /* Lock this semaphore in main() until EOS event occurs */
    sem_t smp_eos;

    /* True if the output port has been completely disabled */
    bool port_disabled;

    /* Lock this semaphore in main() until output port is completely disabled */
    sem_t smp_port_disabled;

    /* Lock this semaphore in main() until output port is completely enabled */
    sem_t smp_port_enabled;

    /* Lock this semaphore in main() until output port changes settings */
    sem_t smp_port_settings_changed;

    /* File descriptor of input file */
    FILE * p_in_file;

    /* File descriptor of output file */
    FILE * p_out_file;

    /* GStreamer element from which the application gets H.264 frames */
    GstElement * p_appsink;

} omx_data_t;

/******************************************************************************
 *                               OMX CALLBACKS                                *
 ******************************************************************************/

/* The method is used to notify the application when an event of interest
 * occurs within the MC.
 *
 * Events are defined in the 'OMX_EVENTTYPE' enumeration.
 * Please see that enumeration for details of what will be returned for
 * each type of event.
 *
 * Callbacks should not return an error to the MC, so if an error occurs,
 * the application shall handle it internally.
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
 * Callbacks should not return an error to the MC, so the application shall
 * handle any errors generated internally */
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
 * Callbacks should not return an error to the MC, so the application shall
 * handle any errors generated internally */
OMX_ERRORTYPE omx_fill_buffer_done(OMX_HANDLETYPE hComponent,
                                   OMX_PTR pAppData,
                                   OMX_BUFFERHEADERTYPE * pBuffer);

/******************************************************************************
 *                                 FUNCTIONS                                  *
 ******************************************************************************/

/* Fill data to input buffer (if possible). Then, set its nFilledLen and nFlags.
 * The function will return nFlags of the input buffer upon exiting */
OMX_U32 setup_in_buf(GstElement * p_appsink, OMX_BUFFERHEADERTYPE * p_in_buf);

/******************************************************************************
 *                               MAIN FUNCTION                                *
 ******************************************************************************/

int main(int argc, char * p_argv[])
{
    /* Handle of the MC */
    OMX_HANDLETYPE handle;

    /* Callbacks used by the MC */
    OMX_CALLBACKTYPE callbacks =
    {
        .EventHandler    = omx_event_handler,
        .EmptyBufferDone = omx_empty_buffer_done,
        .FillBufferDone  = omx_fill_buffer_done
    };

    /* Buffers of input and output ports */
    OMX_BUFFERHEADERTYPE ** pp_in_bufs  = NULL;
    OMX_BUFFERHEADERTYPE ** pp_out_bufs = NULL;

    /* Iterator */
    int index = 0;

    /* Shared data between OMX's callbacks */
    omx_data_t omx_data;

    /* GStreamer pipeline and elements */
    GstElement * p_pipeline   = NULL;
    GstElement * p_filesrc    = NULL;
    GstElement * p_h264parse  = NULL;
    GstElement * p_capsfilter = NULL;
    GstElement * p_appsink    = NULL;

    GstCaps * p_caps = NULL;

    omx_data.eos = false;
    omx_data.port_disabled = false;

    /* Prepare the semaphores */
    sem_init(&omx_data.smp_eos, 0, 0);
    sem_init(&omx_data.smp_port_disabled, 0, 0);
    sem_init(&omx_data.smp_port_enabled, 0, 0);
    sem_init(&omx_data.smp_port_settings_changed, 0, 0);

    /**************************************************************************
     *                  STEP 1: OPEN INPUT AND OUTPUT FILES                   *
     **************************************************************************/

    omx_data.p_in_file = fopen(IN_FILE_NAME, "rb");
    assert(omx_data.p_in_file != NULL);

    omx_data.p_out_file = fopen(OUT_FILE_NAME, "wb");
    assert(omx_data.p_out_file != NULL);

    /* Set file position indicator of input file to the end of file */
    fseek(omx_data.p_in_file, 0, SEEK_END);

    /* Exit program if the input file is empty */
    assert(ftell(omx_data.p_in_file) != 0);

    /* Set file position indicator of input file to the beginning of file */
    fseek(omx_data.p_in_file, 0, SEEK_SET);

    /**************************************************************************
     *  STEP 2: SET UP GSTREAMER PIPELINE (FILESRC -> H264PARSE -> APPSINK)   *
     **************************************************************************/

    /* Initialize the GStreamer library */
    gst_init(&argc, &p_argv);

    /* Create an empty pipeline */
    p_pipeline = gst_pipeline_new(NULL);

    /* Create elements */
    p_filesrc    = gst_element_factory_make("filesrc",   NULL);
    p_h264parse  = gst_element_factory_make("h264parse", NULL);
    p_capsfilter = gst_element_factory_make("capsfilter", NULL);
    p_appsink    = gst_element_factory_make("appsink", NULL);

    assert(p_pipeline && p_filesrc && p_h264parse && p_capsfilter && p_appsink);
    omx_data.p_appsink = p_appsink;

    /* Set properties for 'filesrc' element */
    g_object_set(G_OBJECT(p_filesrc), "location", IN_FILE_NAME, NULL);

    /* Set properties for 'capsfilter' element */
    p_caps = gst_caps_new_simple("video/x-h264",
                                 "stream-format", G_TYPE_STRING, "byte-stream",
                                 "alignment", G_TYPE_STRING, "au", NULL);

    g_object_set(G_OBJECT(p_capsfilter), "caps", p_caps, NULL);
    gst_caps_unref(p_caps);

    /* Add and link the elements to the pipeline */
    gst_bin_add_many(GST_BIN(p_pipeline),
                     p_filesrc, p_h264parse, p_capsfilter, p_appsink, NULL);

    assert(gst_element_link_many(p_filesrc, p_h264parse,
                                 p_capsfilter, p_appsink, NULL));

    /**************************************************************************
     *                         STEP 3: SET UP OMX IL                          *
     **************************************************************************/

    /* Initialize OMX IL core */
    assert(OMX_Init() == OMX_ErrorNone);

    /* Locate Renesas's H.264 decoder.
     * If successful, the MC will be in state LOADED */
    assert(OMX_ErrorNone == OMX_GetHandle(&handle,
                                          RENESAS_VIDEO_DECODER_NAME,
                                          (OMX_PTR)&omx_data, &callbacks));

    /* Configure input port */
    assert(omx_set_port_buf_cnt(handle, 0, IN_BUFFER_COUNT));

    /* Configure output port */
    assert(omx_set_out_port_fmt(handle, OMX_COLOR_FormatYUV420SemiPlanar));

    assert(omx_set_port_buf_cnt(handle, 1, OUT_BUFFER_COUNT));

    /* Transition into state IDLE */
    assert(OMX_ErrorNone == OMX_SendCommand(handle,
                                            OMX_CommandStateSet,
                                            OMX_StateIdle, NULL));

    /**************************************************************************
     *               STEP 4: ALLOCATE INPUT AND OUTPUT BUFFERS                *
     **************************************************************************/

    pp_in_bufs = omx_alloc_buffers(handle, 0);
    assert(pp_in_bufs != NULL);

    pp_out_bufs = omx_alloc_buffers(handle, 1);
    assert(pp_out_bufs != NULL);

    omx_wait_state(handle, OMX_StateIdle);

    /**************************************************************************
     *        STEP 5: PREPARE FOR 'OMX_EventPortSettingsChanged' EVENT        *
     **************************************************************************/

    /* Transition into state EXECUTING */
    assert(OMX_ErrorNone == OMX_SendCommand(handle, OMX_CommandStateSet,
                                            OMX_StateExecuting, NULL));
    omx_wait_state(handle, OMX_StateExecuting);

    /* Play the pipeline */
    gst_element_set_state(p_pipeline, GST_STATE_PLAYING);

    /* Send output buffers to output port */
    assert(omx_fill_buffers(handle, pp_out_bufs, OUT_BUFFER_COUNT));

    /* Fill data to input buffers and send the buffers to input port */
    for (index = 0; index < IN_BUFFER_COUNT; index++)
    {
        if (setup_in_buf(p_appsink, pp_in_bufs[index]) & OMX_BUFFERFLAG_EOS)
        {
            /* Stop if the current buffer is marked as 'OMX_BUFFERFLAG_EOS' */
            break;
        }

        assert(OMX_EmptyThisBuffer(handle, pp_in_bufs[index]) == OMX_ErrorNone);
    }

    /**************************************************************************
     *     STEP 6: WAIT UNTIL 'OMX_EventPortSettingsChanged' EVENT OCCURS     *
     **************************************************************************/

    sem_wait(&omx_data.smp_port_settings_changed);

    /**************************************************************************
     *                   STEP 7: REALLOCATE OUTPUT BUFFERS                    *
     **************************************************************************/

    /* To disable and enable output port, the program follows steps in
     * section 3.4.4.2: "Non-tunneled Port Disablement and Enablement" in
     * OMX IL specification 1.1.2 */

    /* The application asked the MC to disable output port */
    assert(OMX_ErrorNone ==
           OMX_SendCommand(handle, OMX_CommandPortDisable, 1, NULL));

    /* When all output buffers have been returned and OMX_FreeBuffer called,
     * the MC can complete the port disablement */
    sem_wait(&omx_data.smp_port_disabled);

    /* Change workflow of FillBufferDone callback */
    omx_data.port_disabled = true;

    /* The application asked the MC to enable the disabled output port */
    assert(OMX_ErrorNone ==
           OMX_SendCommand(handle, OMX_CommandPortEnable, 1, NULL));

    /* The application provides to the MC all buffers that output port needs */
    pp_out_bufs = omx_alloc_buffers(handle, 1);
    assert(pp_out_bufs != NULL);

    /* When all of the required buffers needed are available, the MC can
     * complete the port enablement */
    sem_wait(&omx_data.smp_port_enabled);

    /**************************************************************************
     *                         STEP 8: START DECODING                         *
     **************************************************************************/

    /* Send new output buffers to output port */
    assert(omx_fill_buffers(handle, pp_out_bufs, OUT_BUFFER_COUNT));

    /* Wait until EOS event occurs */
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
     *                      STEP 10: CLEAN UP GSTREAMER                       *
     **************************************************************************/

    gst_element_set_state(p_pipeline, GST_STATE_NULL);
    gst_object_unref(p_pipeline);

    /**************************************************************************
     *                 STEP 11: CLOSE INPUT AND OUTPUT FILES                  *
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
            else if (nData1 == OMX_CommandPortEnable)
            {
                if (nData2 == 1) /* Output port */
                {
                    printf("Output port is enabled\n");
                    sem_post(&p_data->smp_port_enabled);
                }
            }
            else if (nData1 == OMX_CommandPortDisable)
            {
                if (nData2 == 1) /* Output port */
                {
                    printf("Output port is disabled\n");
                    sem_post(&p_data->smp_port_disabled);
                }
            }
        }
        break;

        case OMX_EventPortSettingsChanged:
        {
            if (nData1 == 1) /* Output port */
            {
                printf("OMX event: 'Output port settings changed'\n");
                sem_post(&p_data->smp_port_settings_changed);
            }
        }
        break;

        case OMX_EventBufferFlag:
        {
            if (nData1 == OMX_BUFFERFLAG_EOS)
            {
                /* The buffer contains the last output picture data */
                printf("OMX event: 'End-of-Stream'\n");

                sem_post(&p_data->smp_eos);
                p_data->eos = true;
            }
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

    if ((p_data->eos == false) && (pBuffer != NULL))
    {
        /* Add buffer back to the input port when EOS event does not occur */
        setup_in_buf(p_data->p_appsink, pBuffer);
        assert(OMX_EmptyThisBuffer(hComponent, pBuffer) == OMX_ErrorNone);
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

    if (p_data->port_disabled == false)
    {
        /* The application asked the MC to disable output port.
         * Then, the MC will return the buffers via FillBufferDone callback.
         * For each buffer returned, the application will call OMX_FreeBuffer */
        OMX_FreeBuffer(hComponent, 1, pBuffer);
    }
    else if ((p_data->eos == false) && (pBuffer != NULL))
    {
        if (pBuffer->nFilledLen > 0)
        {
            fwrite(pBuffer->pBuffer, 1, pBuffer->nFilledLen, p_data->p_out_file);
        }

        pBuffer->nFlags     = 0;
        pBuffer->nFilledLen = 0;

        /* Add buffer back to the output port when EOS event does not occur */
        assert(OMX_FillThisBuffer(hComponent, pBuffer) == OMX_ErrorNone);
    }

    printf("FillBufferDone callback.\n");
    return OMX_ErrorNone;
}

/******************************************************************************
 *                                 FUNCTIONS                                  *
 ******************************************************************************/

OMX_U32 setup_in_buf(GstElement * p_appsink, OMX_BUFFERHEADERTYPE * p_in_buf)
{
    GstSample * p_gst_sample = NULL;
    GstBuffer * p_gst_buffer = NULL;

    GstMapInfo gst_map;

    p_gst_sample = gst_app_sink_pull_sample(GST_APP_SINK(p_appsink));

    if (p_gst_sample == NULL)
    {
        p_in_buf->nFlags = OMX_BUFFERFLAG_EOS;
        p_in_buf->nFilledLen = 0;
    }
    else
    {
        p_gst_buffer = gst_sample_get_buffer(p_gst_sample);
        assert(gst_buffer_map(p_gst_buffer, &gst_map, GST_MAP_READ) == TRUE);

        /* Put H.264 data to input buffer */
        memcpy(p_in_buf->pBuffer, gst_map.data, gst_map.size);

        p_in_buf->nFilledLen = gst_map.size;
        p_in_buf->nFlags = OMX_BUFFERFLAG_ENDOFFRAME;

        /* Unmap the buffer data */
        gst_buffer_unmap(p_gst_buffer, &gst_map);
    }

    return p_in_buf->nFlags;
}
