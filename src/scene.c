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
static void add_usb_camera (SceneManager *manager);

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
      "nvinfer name=infer ! queue ! "
      "nvmultistreamtiler ! queue ! "
      "nvvideoconvert ! queue ! "
      "nvdsosd ! queue ! "
      "nvegltransform ! nveglglessink"

      "",
      1 // stremmux batch size
      );
  SELF->pipeline = gst_parse_launch (desc, &error);

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

  add_usb_camera (manager);

  // -----------------------------------------------

  SELF->bus = gst_element_get_bus (SELF->pipeline);
  SELF->bus_watch_id = gst_bus_add_watch (SELF->bus, (GstBusFunc) bus_loop, manager);

  gst_element_set_state (GST_ELEMENT(SELF->pipeline), GST_STATE_PLAYING);

  SELF->started = TRUE;
}

void
scene_manager_start_ (SceneManager *manager)
{
  gchar *desc = NULL;
  GError *error = NULL;
  GstElement *pgie = NULL, *streammux = NULL, *src_element;

  gst_init (NULL, NULL);

  desc = g_strdup_printf (
      "v4l2src ! video/x-raw,height=720,framerate=10/1 ! "
      "nvvideoconvert ! capsfilter caps=video/x-raw(memory:NVMM),format=(string)RGBA name=src-element "
      "nvstreammux batch-size=1 live-source=true name=streammux ! "
      "nvinfer name=infer ! "
      "nvdsosd display-text=1 ! "
      "nvegltransform ! "
      "nveglglessink");

  SELF->pipeline = gst_parse_launch (desc, &error);

  // -----------------------------------------------

  src_element = gst_bin_get_by_name (GST_BIN (SELF->pipeline), "src-element");
  streammux = gst_bin_get_by_name (GST_BIN (SELF->pipeline), "streammux");

  GstPad *sinkpad, *srcpad;

  sinkpad = gst_element_get_request_pad (streammux, "sink_0");
  srcpad = gst_element_get_static_pad (src_element, "src");

  gst_pad_link (srcpad, sinkpad);

  gst_object_unref (sinkpad);
  gst_object_unref (srcpad);

  // -----------------------------------------------

  pgie = gst_bin_get_by_name (GST_BIN (SELF->pipeline), "infer");

  g_object_set (G_OBJECT (pgie),
                "config-file-path", "/home/mexxik/jetson-watcher/config/config_infer_primary_frcnn.txt", NULL);

  g_object_set (G_OBJECT (streammux),
                "width", 1280,
                "height", 720,
      // "batched-push-timeout", 40000,
      //"nvbuf-memory-type", 3,
                NULL);

  // -----------------------------------------------


  SELF->bus = gst_element_get_bus (SELF->pipeline);
  SELF->bus_watch_id = gst_bus_add_watch (SELF->bus, (GstBusFunc) bus_loop, manager);

  gst_element_set_state (GST_ELEMENT(SELF->pipeline), GST_STATE_PLAYING);

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
add_usb_camera (SceneManager *manager)
{
  gchar *desc = NULL;
  GError *error = NULL;
  GstElement *bin = NULL, *src_element = NULL;
  GstPad *sink_pad, *src_pad;

  desc = "v4l2src ! video/x-raw,height=720,framerate=10/1 ! "
         "nvvideoconvert ! capsfilter caps=video/x-raw(memory:NVMM),format=(string)RGBA name=src-element";
  bin = gst_parse_bin_from_description (desc, FALSE, NULL);
  gst_bin_add (GST_BIN (SELF->pipeline), bin);
  gst_element_sync_state_with_parent (bin);

  src_element = gst_bin_get_by_name (GST_BIN (bin), "src-element");

  sink_pad = gst_element_get_request_pad (SELF->stream_mux, "sink_0");
  src_pad = gst_ghost_pad_new ("video_src", gst_element_get_static_pad (src_element, "src"));
  // gst_pad_set_active (src_pad, TRUE);
  gst_element_add_pad (bin, src_pad);

  gst_pad_link (src_pad, sink_pad);

  gst_object_unref (sink_pad);
  // gst_object_unref (src_pad);


}