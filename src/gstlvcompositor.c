#include "gstlvcompositor.h"
#include "sei_merge.h" 

#include <gst/video/video.h>
#include <gst/base/gstaggregator.h>

GST_DEBUG_CATEGORY_STATIC (gst_lv_compositor_debug);
#define GST_CAT_DEFAULT gst_lv_compositor_debug

#define DEFAULT_WIDTH 1920
#define DEFAULT_HEIGHT 1080
#define DEFAULT_FPS_N 25
#define DEFAULT_FPS_D 1

static GstStaticPadTemplate sink_template_main = GST_STATIC_PAD_TEMPLATE(
    "sink_main",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS(
        "video/x-h264, "
        "stream-format=(string)byte-stream, "
        "alignment=(string){au,nal}; "
        "video/x-h265, "
        "stream-format=(string)byte-stream, "
        "alignment=(string){au,nal}; "
        "video/x-h266, "
        "stream-format=(string)byte-stream, "
        "alignment=(string){au,nal}; "
        "video/x-evc, "
        "stream-format=(string)byte-stream, "
        "alignment=(string){au,nal}"
    )
);

static GstStaticPadTemplate sink_template_secondary = GST_STATIC_PAD_TEMPLATE(
    "sink_secondary",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS(
        "video/x-evc, "
        "stream-format=(string)byte-stream, "
        "alignment=(string){au,nal}"
    )
);

#if 0
static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE(
    "src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS(
        "video/x-h264, "
        "lcevc = (boolean) true, "
        "stream-format=(string)byte-stream, "
        "alignment=(string){au,nal}; "
        "video/x-h265, "
        "lcevc = (boolean) true, "
        "stream-format=(string)byte-stream, "
        "alignment=(string){au,nal}"
    )
);
#else
static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE(
    "src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS(
        "video/x-h264, "
        "stream-format=(string)byte-stream, "
        "alignment=(string){au,nal}; "
        "video/x-h265, "
        "stream-format=(string)byte-stream, "
        "alignment=(string){au,nal}; "
        "video/x-h266, "
        "stream-format=(string)byte-stream, "
        "alignment=(string){au,nal}; "
        "video/x-evc, "
        "stream-format=(string)byte-stream, "
        "alignment=(string){au,nal}"
    )
);
#endif

enum {
    PROP_0,
    PROP_WIDTH,
    PROP_HEIGHT,
    PROP_FPS_N,
    PROP_FPS_D
};

G_DEFINE_TYPE(GstLvCompositor, gst_lv_compositor, GST_TYPE_AGGREGATOR)

static void gst_lv_compositor_set_property(GObject *object, guint prop_id,
                                          const GValue *value, GParamSpec *pspec);
static void gst_lv_compositor_get_property(GObject *object, guint prop_id,
                                          GValue *value, GParamSpec *pspec);

static void gst_lv_compositor_finalize(GObject *object);

static GstFlowReturn gst_lv_compositor_aggregate(GstAggregator *aggregator,
                                                gboolean timeout);
static gboolean gst_lv_compositor_sink_event(GstAggregator *aggregator,
                                            GstAggregatorPad *pad,
                                            GstEvent *event);
static gboolean gst_lv_compositor_src_event(GstAggregator *aggregator,
                                           GstEvent *event);
static gboolean gst_lv_compositor_src_query(GstAggregator *aggregator,
                                           GstQuery *query);
static gboolean gst_lv_compositor_sink_query(GstAggregator *aggregator,
                                            GstAggregatorPad *pad,
                                            GstQuery *query);
static GstCaps *gst_lv_compositor_fixate_src_caps(GstAggregator *aggregator,
                                                 GstCaps *caps);

static void
gst_lv_compositor_class_init(GstLvCompositorClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GstElementClass *gstelement_class = GST_ELEMENT_CLASS(klass);
    GstAggregatorClass *agg_class = GST_AGGREGATOR_CLASS(klass);

    GST_DEBUG_CATEGORY_INIT(gst_lv_compositor_debug, "lvcompositor", 0,
                           "LV Compositor element");

    gobject_class->set_property = gst_lv_compositor_set_property;
    gobject_class->get_property = gst_lv_compositor_get_property;
    gobject_class->finalize = gst_lv_compositor_finalize;

    g_object_class_install_property(gobject_class, PROP_WIDTH,
        g_param_spec_int("width", "Width", "Output video width",
                        1, G_MAXINT, DEFAULT_WIDTH,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    
    g_object_class_install_property(gobject_class, PROP_HEIGHT,
        g_param_spec_int("height", "Height", "Output video height",
                        1, G_MAXINT, DEFAULT_HEIGHT,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    
    g_object_class_install_property(gobject_class, PROP_FPS_N,
        g_param_spec_int("fps-n", "FPS numerator", "Frame rate numerator",
                        1, G_MAXINT, DEFAULT_FPS_N,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    
    g_object_class_install_property(gobject_class, PROP_FPS_D,
        g_param_spec_int("fps-d", "FPS denominator", "Frame rate denominator",
                        1, G_MAXINT, DEFAULT_FPS_D,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    gst_element_class_set_static_metadata(gstelement_class,
        "LV Compositor", "Filter/Compositor/Video",
        "Composites two video streams with internal queues",
        "Le Blond Erwan erwanleblond@gmail.com");

    gst_element_class_add_static_pad_template_with_gtype(gstelement_class,
        &sink_template_main, GST_TYPE_AGGREGATOR_PAD);
    gst_element_class_add_static_pad_template_with_gtype(gstelement_class,
        &sink_template_secondary, GST_TYPE_AGGREGATOR_PAD);
    gst_element_class_add_static_pad_template(gstelement_class, &src_template);

    agg_class->aggregate = gst_lv_compositor_aggregate;
    agg_class->sink_event = gst_lv_compositor_sink_event;
    agg_class->src_event = gst_lv_compositor_src_event;
    agg_class->src_query = gst_lv_compositor_src_query;
    agg_class->sink_query = gst_lv_compositor_sink_query;
    agg_class->fixate_src_caps = gst_lv_compositor_fixate_src_caps;
}

static void
gst_lv_compositor_init(GstLvCompositor *self)
{
    self->width = DEFAULT_WIDTH;
    self->height = DEFAULT_HEIGHT;
    self->fps_n = DEFAULT_FPS_N;
    self->fps_d = DEFAULT_FPS_D;
    
    self->main_has_data = FALSE;
    self->secondary_has_data = FALSE;
    self->current_codec = CODEC_UNKNOWN;
    self->codec_negotiated = FALSE;
    self->codec_name = NULL;
    
    /* Create internal queues */
    self->queue_main = gst_element_factory_make("queue", "main_queue");
    self->queue_secondary = gst_element_factory_make("queue", "secondary_queue");
    
    if (self->queue_main && self->queue_secondary) {
        /* Configure queues */
        g_object_set(self->queue_main, "max-size-buffers", 10, NULL);
        g_object_set(self->queue_secondary, "max-size-buffers", 10, NULL);
    }
}

static void
gst_lv_compositor_set_property(GObject *object, guint prop_id,
                              const GValue *value, GParamSpec *pspec)
{
    GstLvCompositor *self = GST_LV_COMPOSITOR(object);

    switch (prop_id) {
        case PROP_WIDTH:
            self->width = g_value_get_int(value);
            break;
        case PROP_HEIGHT:
            self->height = g_value_get_int(value);
            break;
        case PROP_FPS_N:
            self->fps_n = g_value_get_int(value);
            break;
        case PROP_FPS_D:
            self->fps_d = g_value_get_int(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static void
gst_lv_compositor_get_property(GObject *object, guint prop_id,
                              GValue *value, GParamSpec *pspec)
{
    GstLvCompositor *self = GST_LV_COMPOSITOR(object);

    switch (prop_id) {
        case PROP_WIDTH:
            g_value_set_int(value, self->width);
            break;
        case PROP_HEIGHT:
            g_value_set_int(value, self->height);
            break;
        case PROP_FPS_N:
            g_value_set_int(value, self->fps_n);
            break;
        case PROP_FPS_D:
            g_value_set_int(value, self->fps_d);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static GstFlowReturn
gst_lv_compositor_aggregate(GstAggregator *aggregator, gboolean timeout)
{
    GstLvCompositor *self = GST_LV_COMPOSITOR(aggregator);
    GstAggregatorPad *main_pad = NULL;
    GstAggregatorPad *secondary_pad = NULL;
    GstBuffer *main_buffer = NULL;
    GstBuffer *secondary_buffer = NULL;
    GstFlowReturn ret = GST_FLOW_OK;
    GST_INFO_OBJECT(self, "Start Aggregating buffers");
    GST_INFO_OBJECT(self, "Start Aggregating buffers, codec: %s", 
                   self->codec_name ? self->codec_name : "unknown");

    /* Get pads */
    main_pad = GST_AGGREGATOR_PAD(gst_element_get_static_pad(GST_ELEMENT(aggregator), "sink_main"));
    secondary_pad = GST_AGGREGATOR_PAD(gst_element_get_static_pad(GST_ELEMENT(aggregator), "sink_secondary"));

    if (!main_pad || !secondary_pad) {
        ret = GST_FLOW_NOT_NEGOTIATED;
        goto done;
    }


    main_buffer = gst_aggregator_pad_pop_buffer(main_pad);
    secondary_buffer = gst_aggregator_pad_pop_buffer(secondary_pad);

    if(!main_buffer) {
        ret = GST_FLOW_OK;
        self->main_has_data = FALSE;
        GST_DEBUG_OBJECT(self, "No data available from main stream");
        goto done;
    }
    if(!secondary_buffer) {
        ret = GST_FLOW_OK;
        self->main_has_data = FALSE;
        GST_DEBUG_OBJECT(self, "No data available from secondary stream");
        goto done;
    }

    /* Composition logic with LCEVC merge */
    if (main_buffer && secondary_buffer) {
        /* Case 1: Both buffers available - merge LCEVC */
        GST_INFO_OBJECT(self,"Case 1: Both buffers available - merge sei");
        GstBuffer *merged_buffer = NULL;

        switch (self->current_codec) {
            case CODEC_H264:
                GST_INFO_OBJECT(self, "Using H.264 SEI merge function");
                merged_buffer = merge_lcevc_data_h264(main_buffer, secondary_buffer);
                break;
            case CODEC_H265:
                GST_INFO_OBJECT(self, "Using H.265 SEI merge function");
                merged_buffer = merge_lcevc_data_h265(main_buffer, secondary_buffer);
                break;
            case CODEC_H266:
                GST_INFO_OBJECT(self, "Using H.266 SEI merge function");
                merged_buffer = merge_lcevc_data_h266(main_buffer, secondary_buffer);
                break;
            case CODEC_EVC:
                GST_INFO_OBJECT(self, "Using EVC SEI merge function");
                merged_buffer = merge_lcevc_data_evc(main_buffer, secondary_buffer);
                break;
            default:
                GST_ERROR_OBJECT(self, "Unsupported codec for sei merge");
                merged_buffer = NULL;
                break;
        }
        if (merged_buffer) {
            /* Emit merged buffer */
            GST_INFO_OBJECT(self, "Emitting merged buffer for codec: %s", 
                           self->codec_name ? self->codec_name : "unknown");
            ret = gst_aggregator_finish_buffer(aggregator, merged_buffer);
            GST_DEBUG_OBJECT(self, "sei data merged successfully for %s", 
                           self->codec_name ? self->codec_name : "unknown");
        } else {
            /* Fallback: use only main stream */
            GST_INFO_OBJECT(self, "Fallback: use only main stream");
            GstBuffer *out_buffer = gst_buffer_copy(main_buffer);
            ret = gst_aggregator_finish_buffer(aggregator, out_buffer);
            GST_WARNING_OBJECT(self, "sei merge failed for %s, using main stream only", 
                             self->codec_name ? self->codec_name : "unknown");
        }

        gst_buffer_unref(main_buffer);
        gst_buffer_unref(secondary_buffer);
        self->main_has_data = TRUE;
        
    } else if (main_buffer) {
        /* Case 2: Only main stream available */
        GST_INFO_OBJECT(self,"Case 2: Only main stream available");
        GstBuffer *out_buffer = gst_buffer_copy(main_buffer);
        ret = gst_aggregator_finish_buffer(aggregator, out_buffer);
        gst_buffer_unref(main_buffer);
        self->main_has_data = TRUE;
        GST_DEBUG_OBJECT(self, "Using main stream only (no enhancement data)");
        
    } else if (secondary_buffer && !self->main_has_data) {
        /* Case 3: Fallback to secondary stream if main has no data */
        GST_INFO_OBJECT(self,"Case 3: Fallback to secondary stream if main has no data");
        GstBuffer *out_buffer = gst_buffer_copy(secondary_buffer);
        ret = gst_aggregator_finish_buffer(aggregator, out_buffer);
        gst_buffer_unref(secondary_buffer);
        self->main_has_data = FALSE;
        GST_DEBUG_OBJECT(self, "Using secondary stream as fallback");
        
    } else {
        /* Case 4: No data available */
        GST_INFO_OBJECT(self,"Case 4: No data available");
        ret = GST_FLOW_OK;
        self->main_has_data = FALSE;
        GST_DEBUG_OBJECT(self, "No data available from either stream");
    }

        GST_INFO_OBJECT(self, "End Aggregating buffers");
    return ret;
    
done:
    if (main_pad) gst_object_unref(main_pad);
    if (secondary_pad) gst_object_unref(secondary_pad);
    return ret;
}

static void
gst_lv_compositor_finalize(GObject *object)
{
    GstLvCompositor *self = GST_LV_COMPOSITOR(object);

    if (self->codec_name) {
        g_free(self->codec_name);
        self->codec_name = NULL;
    }

    G_OBJECT_CLASS(gst_lv_compositor_parent_class)->finalize(object);
}

static GstLvCompositorCodec 
detect_codec_from_caps(GstCaps *caps)
{
    GstStructure *structure;
    const gchar *mime_type;
    
    if (!caps || gst_caps_is_empty(caps))
        return CODEC_UNKNOWN;
        
    structure = gst_caps_get_structure(caps, 0);
    mime_type = gst_structure_get_name(structure);
    
    if (g_strcmp0(mime_type, "video/x-h264") == 0) {
        return CODEC_H264;
    } else if (g_strcmp0(mime_type, "video/x-h265") == 0) {
        return CODEC_H265;
    } else if (g_strcmp0(mime_type, "video/x-h266") == 0) {
        return CODEC_H266;
    }
    else if (g_strcmp0(mime_type, "video/x-evc") == 0) {
        return CODEC_EVC;
    }
    
    return CODEC_UNKNOWN;
}

static gboolean
gst_lv_compositor_sink_event(GstAggregator *aggregator, GstAggregatorPad *pad,
                            GstEvent *event)
{
    GstLvCompositor *self = GST_LV_COMPOSITOR(aggregator);
    gboolean ret = TRUE;

    switch (GST_EVENT_TYPE(event)) {
        case GST_EVENT_CAPS: {
            GstCaps *caps;
            gst_event_parse_caps(event, &caps);

            GstLvCompositorCodec detected_codec = detect_codec_from_caps(caps);
            
            /* If it's the main pad, set output caps */
            if (g_strcmp0(GST_OBJECT_NAME(pad), "sink_main") == 0) {
                GstCaps *src_caps = gst_caps_copy(caps);
                gst_aggregator_set_src_caps(aggregator, src_caps);
                GST_DEBUG_OBJECT(self, "Setting source caps from main pad");
                self->current_codec = detected_codec;
                self->codec_negotiated = TRUE;
                if (self->codec_name) {
                    g_free(self->codec_name);
                }
                switch (detected_codec) {
                    case CODEC_H264:
                        self->codec_name = g_strdup("H264");
                        break;
                    case CODEC_H265:
                        self->codec_name = g_strdup("H265");
                        break;
                    case CODEC_H266:
                        self->codec_name = g_strdup("H266");
                        break;
                    default:
                        self->codec_name = g_strdup("UNKNOWN");
                        break;
                }
            }
            break;
        }
        case GST_EVENT_SEGMENT:
            /* Forward segment events */
            ret = GST_AGGREGATOR_CLASS(gst_lv_compositor_parent_class)->sink_event(aggregator, pad, event);
            break;
        default:
            ret = GST_AGGREGATOR_CLASS(gst_lv_compositor_parent_class)->sink_event(aggregator, pad, event);
            break;
    }

    return ret;
}

static gboolean
gst_lv_compositor_src_event(GstAggregator *aggregator, GstEvent *event)
{
    /* Forward source events to appropriate pads */
    return GST_AGGREGATOR_CLASS(gst_lv_compositor_parent_class)->src_event(aggregator, event);
}

static gboolean
gst_lv_compositor_src_query(GstAggregator *aggregator, GstQuery *query)
{
    return GST_AGGREGATOR_CLASS(gst_lv_compositor_parent_class)->src_query(aggregator, query);
}

static gboolean
gst_lv_compositor_sink_query(GstAggregator *aggregator, GstAggregatorPad *pad,
                            GstQuery *query)
{
    return GST_AGGREGATOR_CLASS(gst_lv_compositor_parent_class)->sink_query(aggregator, pad, query);
}

static GstCaps *
gst_lv_compositor_fixate_src_caps(GstAggregator *aggregator, GstCaps *caps)
{
    GstLvCompositor *self = GST_LV_COMPOSITOR(aggregator);
    GstStructure *structure;
    GstCaps *result;

    result = gst_caps_copy(caps);
    structure = gst_caps_get_structure(result, 0);

    gst_structure_fixate_field_nearest_int(structure, "width", self->width);
    gst_structure_fixate_field_nearest_int(structure, "height", self->height);
    
    if (gst_structure_has_field(structure, "framerate")) {
        GValue framerate = G_VALUE_INIT;
        g_value_init(&framerate, GST_TYPE_FRACTION);
        gst_value_set_fraction(&framerate, self->fps_n, self->fps_d);
        gst_structure_set_value(structure, "framerate", &framerate);
    }

    return result;
}

static gboolean
plugin_init(GstPlugin *plugin)
{
    return gst_element_register(plugin, "lvcompositor", GST_RANK_NONE,
                               GST_TYPE_LV_COMPOSITOR);
}

#ifndef PACKAGE
#define PACKAGE "lvcompositor"
#endif

#ifndef PACKAGE_NAME
#define PACKAGE_NAME "LV Compositor Plugin"
#endif

#ifndef PACKAGE_VERSION
#define PACKAGE_VERSION "1.0"
#endif

#ifndef GST_PACKAGE_NAME
#define GST_PACKAGE_NAME "GStreamer"
#endif

#ifndef GST_PACKAGE_ORIGIN
#define GST_PACKAGE_ORIGIN "http://gstreamer.net/"
#endif

GST_PLUGIN_DEFINE(
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    lvcompositor,
    "LV Compositor plugin",
    plugin_init,
    PACKAGE_VERSION,
    "LGPL",
    GST_PACKAGE_NAME,
    GST_PACKAGE_ORIGIN
)