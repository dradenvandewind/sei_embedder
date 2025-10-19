
## lvcompositor : A Gstreamer Plugin for sei embedder in video streams (h64,h265,vvc,evc)

This plugin are still under development and will continue to improve by:
1 :exposing additional encoder properties
2 :accept other chrominance formats
3 ..

## Prerequisites



## Usage of the lvcompositor plugin

```
    gst-inspect-1.0 lvcompositor
```
Following are the pad templates supported by the plugin:

```
Pad Templates:
  SINK template: 'sink_main'
    Availability: On request
    Capabilities:
      video/x-h264
          stream-format: byte-stream
              alignment: { (string)au, (string)nal }
      video/x-h265
          stream-format: byte-stream
              alignment: { (string)au, (string)nal }
      video/x-h266
          stream-format: byte-stream
              alignment: { (string)au, (string)nal }
      video/x-evc
          stream-format: byte-stream
              alignment: { (string)au, (string)nal }
    Type: GstAggregatorPad
    Pad Properties:
    
      emit-signals        : Send signals to signal data consumption
                            flags: readable, writable
                            Boolean. Default: false
      
  
  SINK template: 'sink_secondary'
    Availability: On request
    Capabilities:
      video/x-evc
          stream-format: byte-stream
              alignment: { (string)au, (string)nal }
    Type: GstAggregatorPad
    Pad Properties:
    
      emit-signals        : Send signals to signal data consumption
                            flags: readable, writable
                            Boolean. Default: false
      
  
  SRC template: 'src'
    Availability: Always
    Capabilities:
      video/x-h264
          stream-format: byte-stream
              alignment: { (string)au, (string)nal }
      video/x-h265
          stream-format: byte-stream
              alignment: { (string)au, (string)nal }
      video/x-h266
          stream-format: byte-stream
              alignment: { (string)au, (string)nal }
      video/x-evc
          stream-format: byte-stream
              alignment: { (string)au, (string)nal }


```



Following are the supported element properties:
```
Element Properties:

  emit-signals        : Send signals
                        flags: readable, writable
                        Boolean. Default: false
  
  fps-d               : Frame rate denominator
                        flags: readable, writable
                        Integer. Range: 1 - 2147483647 Default: 1 
  
  fps-n               : Frame rate numerator
                        flags: readable, writable
                        Integer. Range: 1 - 2147483647 Default: 25 
  
  height              : Output video height
                        flags: readable, writable
                        Integer. Range: 1 - 2147483647 Default: 1080 
  
  latency             : Additional latency in live mode to allow upstream to take longer to produce buffers for the current position (in nanoseconds)
                        flags: readable, writable
                        Unsigned Integer64. Range: 0 - 18446744073709551615 Default: 0 
  
  min-upstream-latency: When sources with a higher latency are expected to be plugged in dynamically after the aggregator has started playing, this allows overriding the minimum latency reported by the initial source(s). This is only taken into account when larger than the actually reported minimum latency. (nanoseconds)
                        flags: readable, writable
                        Unsigned Integer64. Range: 0 - 18446744073709551615 Default: 0 
  
  name                : The name of the object
                        flags: readable, writable
                        String. Default: "lvcompositor0"
  
  parent              : The parent of the object
                        flags: readable, writable
                        Object of type "GstObject"
  
  start-time          : Start time to use if start-time-selection=set
                        flags: readable, writable
                        Unsigned Integer64. Range: 0 - 18446744073709551615 Default: 18446744073709551615 
  
  start-time-selection: Decides which start time is output
                        flags: readable, writable
                        Enum "GstAggregatorStartTimeSelection" Default: 0, "zero"
                           (0): zero             - GST_AGGREGATOR_START_TIME_SELECTION_ZERO
                           (1): first            - GST_AGGREGATOR_START_TIME_SELECTION_FIRST
                           (2): set              - GST_AGGREGATOR_START_TIME_SELECTION_SET
  
  width               : Output video width
                        flags: readable, writable
                        Integer. Range: 1 - 2147483647 Default: 1920 
  

Element Signals:

  "samples-selected" :  void user_function (GstElement * object,
                                            GstSegment * arg0,
                                            guint64 arg1,
                                            guint64 arg2,
                                            guint64 arg3,
                                            GstStructure * arg4,
                                            gpointer user_data);
```






