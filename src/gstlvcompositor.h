#ifndef __GST_LV_COMPOSITOR_H__
#define __GST_LV_COMPOSITOR_H__

#include <gst/gst.h>
#include <gst/base/gstaggregator.h>
#include "sei_merge.h"

G_BEGIN_DECLS

#define GST_TYPE_LV_COMPOSITOR (gst_lv_compositor_get_type())
#define GST_LV_COMPOSITOR(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_LV_COMPOSITOR, GstLvCompositor))
#define GST_LV_COMPOSITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_LV_COMPOSITOR, GstLvCompositorClass))
#define GST_IS_LV_COMPOSITOR(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_LV_COMPOSITOR))
#define GST_IS_LV_COMPOSITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_LV_COMPOSITOR))

typedef struct _GstLvCompositor GstLvCompositor;
typedef struct _GstLvCompositorClass GstLvCompositorClass;


struct _GstLvCompositor {
    GstAggregator parent;
    
    /* Propriétés */
    gint width, height;
    gint fps_n, fps_d;
    
    /* Pads de sortie */
    GstPad *srcpad;
    
    /* Queues internes */
    GstElement *queue_main;
    GstElement *queue_secondary;

    GstLvCompositorCodec current_codec;
    gboolean codec_negotiated;
    gchar *codec_name;
    
    /* États */
    gboolean main_has_data;
    gboolean secondary_has_data;

    /* Pads d'entrée */
    GstAggregatorPad *main_pad;
    GstAggregatorPad *secondary_pad;
};

struct _GstLvCompositorClass {
    GstAggregatorClass parent_class;
};

GType gst_lv_compositor_get_type(void);

G_END_DECLS

#endif /* __GST_LV_COMPOSITOR_H__ */
