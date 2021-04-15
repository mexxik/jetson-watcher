#include "scene.h"

static GMainLoop *loop;
static SceneManager *scene_manager;

int
main ()
{
  g_print ("starting watcher\n");

  GOptionContext *context = NULL;
  GMainContext *loop_context = NULL;

  context = g_option_context_new ("- watcher application");
  loop = g_main_loop_new (loop_context, FALSE);

  scene_manager = scene_manager_new ();
  scene_manager_start (scene_manager);

  g_main_loop_run (loop);

  return 0;
}
