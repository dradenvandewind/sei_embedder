#ifndef __SEI_MERGE_H__
#define __SEI_MERGE_H__

#include <gst/gst.h>
#include <glib.h>

typedef enum {
    CODEC_H264,
    CODEC_H265, 
    CODEC_H266,
    CODEC_EVC,
    CODEC_UNKNOWN
} GstLvCompositorCodec;

GstBuffer *merge_lcevc_data_h264(GstBuffer *main_buffer, GstBuffer *secondary_buffer);
GstBuffer *merge_lcevc_data_h265(GstBuffer *main_buffer, GstBuffer *secondary_buffer);
GstBuffer *merge_lcevc_data_h266(GstBuffer *main_buffer, GstBuffer *secondary_buffer);
GstBuffer *merge_lcevc_data_evc(GstBuffer *main_buffer, GstBuffer *secondary_buffer);
GstBuffer *merge_lcevc_data_generic(GstBuffer *main_buffer, GstBuffer *secondary_buffer);

#endif /* __SEI_MERGE_H__ */
