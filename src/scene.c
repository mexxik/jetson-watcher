#include "scene.h"

#define SELF manager->priv

struct _SceneManagerPrivate {
    gboolean started;

    GstBus *bus;
    guint bus_watch_id;

    GstElement *pipeline;
    GstElement *stream_mux;
    GstElement *pgie;
};

G_DEFINE_TYPE_WITH_CODE (SceneManager, scene_manager, G_TYPE_OBJECT, G_ADD_PRIVATE (SceneManager))

enum {
    STARTED,

    N_SIGNALS
};

static guint scene_signals[N_SIGNALS] = {0,};

static gboolean bus_loop (GstBus *bus, GstMessage *msg, SceneManager *manager);
static void add_usb_camera (SceneManager *manager, guint stream_id);
static void add_file (SceneManager *manager);
static void decodebin_pad_added (GstElement *decodebin, GstPad *pad, gpointer user_data);

static void
scene_manager_constructed (GObject *object)
{
  G_OBJECT_CLASS (scene_manager_parent_class)->constructed (object);
}

static void
scene_manager_finalize (GObject *object)
{
  SceneManager *manager = SCENE_MANAGER (object);

  G_OBJECT_CLASS (scene_manager_parent_class)->finalize (object);
}

static void
scene_manager_init (SceneManager *manager)
{
  manager->priv = scene_manager_get_instance_private (manager);

  SELF->started = FALSE;

  SELF->pipeline = NULL;
}

static void
scene_manager_class_init (SceneManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = scene_manager_constructed;
  object_class->finalize = scene_manager_finalize;

  scene_signals[STARTED] = g_signal_newv ("started",
                                          G_TYPE_FROM_CLASS (object_class),
                                          G_SIGNAL_RUN_LAST,
                                          NULL, NULL, NULL, NULL,
                                          G_TYPE_NONE, 0, NULL);
}

SceneManager *
scene_manager_new ()
{
  SceneManager *manager = g_object_new (SCENE_TYPE_MANAGER, NULL);

  return manager;
}

void
scene_manager_start (SceneManager *manager)
{
  gchar *desc = NULL;
  GError *error = NULL;

  gst_init (NULL, NULL);

  desc = g_strdup_printf (
      "nvstreammux batch-size=%d live-source=true name=stream-mux ! queue ! "
      "nvinfer name=infer batch-size=%d ! queue ! "
      "nvmultistreamtiler width=1280 height=720 rows=2 columns=2 ! queue ! "
      "nvvideoconvert ! queue ! "
      "nvdsosd ! queue ! "
      "nvegltransform ! nveglglessink"
      "",
      2, // stremmux batch size
      2  // infer batch size
      );
  SELF->pipeline = gst_parse_launch (desc, &error);

  allowenter

  SELF->stream_mux = gst_bin_get_by_name (GST_BIN (SELF->pipeline), "stream-mux");
  SELF->pgie = gst_bin_get_by_name (GST_BIN (SELF->pipeline), "infer");

  // -----------------------------------------------

  g_object_set (G_OBJECT (SELF->pgie),
                "config-file-path", "/home/mexxik/jetson-watcher/config/config_infer_primary_frcnn.txt", NULL);

  g_object_set (G_OBJECT (SELF->stream_mux),
                "width", 1280,
                "height", 720,
                NULL);

  // -----------------------------------------------



  // -----------------------------------------------

  SELF->bus = gst_element_get_bus (SELF->pipeline);
  SELF->bus_watch_id = gst_bus_add_watch (SELF->bus, (GstBusFunc) bus_loop, manager);

  gst_element_set_state (GST_ELEMENT(SELF->pipeline), GST_STATE_PLAYING);

  add_usb_camera (manager, 0);
  // add_usb_camera (manager, 1);
  add_file (manager);
  //add_file (manager, 1);

  SELF->started = TRUE;
}

static gboolean
bus_loop (GstBus *bus, GstMessage *msg, SceneManager *manager)
{
  switch (GST_MESSAGE_TYPE (msg))
    {
      case GST_MESSAGE_ERROR:
        {
          GError *err;
          gchar *debug;

          gst_message_parse_error (msg, &err, &debug);
          g_print ("error: %s - %s\n", err->message, debug);
          g_error_free (err);
          g_free (debug);

          gst_debug_bin_to_dot_file (GST_BIN (manager->priv->pipeline), GST_DEBUG_GRAPH_SHOW_ALL, "error");
          break;
        }

      case GST_MESSAGE_STATE_CHANGED:
        {
          /*GstState old_state, new_state, pending_state;
          gst_message_parse_state_changed (msg, &old_state, &new_state, &pending_state);
          g_message ("state changed from %s to %s",
                     gst_element_state_get_name (old_state), gst_element_state_get_name (new_state));*/

          if (GST_MESSAGE_SRC (msg) == GST_OBJECT (SELF->pipeline))
            {
              GstState old_state, new_state, pending_state;
              gst_message_parse_state_changed (msg, &old_state, &new_state, &pending_state);

              g_message ("pipeline state changed from %s to %s",
                         gst_element_state_get_name (old_state), gst_element_state_get_name (new_state));

              if (new_state == GST_STATE_PLAYING)
                {
                  if (!SELF->started)
                    {
                      SELF->started = TRUE;

                      g_signal_emit (manager, scene_signals[STARTED], 0);
                    }
                }
            }
        }

      break;
    }

  return TRUE;
}

static void
add_usb_camera (SceneManager *manager, guint stream_id)
{
  gchar *desc = NULL, *sink_id = NULL;
  GError *error = NULL;
  GstElement *bin = NULL, *src_element = NULL;
  GstPad *sink_pad = NULL, *src_pad = NULL;

  desc = "v4l2src ! video/x-raw,height=720,framerate=10/1 ! "
         "nvvideoconvert ! capsfilter caps=video/x-raw(memory:NVMM),format=(string)RGBA name=src-element";
  bin = gst_parse_bin_from_description (desc, FALSE, NULL);
  gst_bin_add (GST_BIN (SELF->pipeline), bin);
  gst_element_sync_state_with_parent (bin);

  src_element = gst_bin_get_by_name (GST_BIN (bin), "src-element");

  sink_id = g_strdup_printf ("sink_%d", stream_id);
  sink_pad = gst_element_get_request_pad (SELF->stream_mux, sink_id);
  src_pad = gst_ghost_pad_new ("video_src", gst_element_get_static_pad (src_element, "src"));
  // gst_pad_set_active (src_pad, TRUE);
  gst_element_add_pad (bin, src_pad);

  gst_pad_link (src_pad, sink_pad);

  g_free (sink_id);
  gst_object_unref (sink_pad);
  // gst_object_unref (src_pad);
}

static void
add_file (SceneManager *manager)
{
  gchar *desc = NULL;
  GError *error = NULL;
  GstElement *bin = NULL, *decode_bin = NULL;

  desc = "filesrc location=/opt/nvidia/deepstream/deepstream-5.1/samples/streams/sample_720p.mp4 ! "
         "decodebin name=decode-bin";
  bin = gst_parse_bin_from_description (desc, FALSE, NULL);
  gst_bin_add (GST_BIN (SELF->pipeline), bin);
  gst_element_sync_state_with_parent (bin);


  decode_bin = gst_bin_get_by_name (GST_BIN (bin), "decode-bin");
  g_signal_connect (decode_bin, "pad-added", G_CALLBACK (decodebin_pad_added), manager);

}

static void
decodebin_pad_added (GstElement *decodebin, GstPad *pad, gpointer user_data)
{
  SceneManager *manager = (SceneManager *) user_data;

  GstCaps *caps;
  const gchar *name;
  GstElement *bin = NULL;

  bin = GST_ELEMENT (gst_element_get_parent (decodebin));

  caps = gst_pad_get_current_caps (pad);
  name = gst_structure_get_name (gst_caps_get_structure (caps, 0));
  if (g_str_has_prefix (name, "video"))
    {
      GstElement *video_convert = NULL;
      GstPad *sink_pad = NULL, *src_pad = NULL, *src_conv_pad = NULL, *sink_conv_pad = NULL;

      video_convert = gst_element_factory_make ("nvvideoconvert", NULL);
      gst_bin_add (GST_BIN (bin), video_convert);
      gst_element_sync_state_with_parent (video_convert);

      sink_conv_pad = gst_element_get_static_pad (video_convert, "sink");
      src_conv_pad = gst_element_get_static_pad (video_convert, "src");

      gst_pad_link (pad, sink_conv_pad);

      sink_pad = gst_element_get_request_pad (SELF->stream_mux, "sink_1");
      src_pad = gst_ghost_pad_new ("video_src", src_conv_pad);
      gst_pad_set_active (src_pad, TRUE);
      gst_element_add_pad (bin, src_pad);

      gst_pad_link (src_pad, sink_pad);

      // gst_element_sync_state_with_parent (video_convert);

      gst_object_unref (sink_pad);
      gst_object_unref (src_conv_pad);
      gst_object_unref (sink_conv_pad);
    }
}
