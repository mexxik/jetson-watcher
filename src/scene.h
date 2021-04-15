
#ifndef _SCENE_H_
#define _SCENE_H_

#include <glib-object.h>
#include <gst/gst.h>

G_BEGIN_DECLS

#define SCENE_TYPE_MANAGER               (scene_manager_get_type ())
#define SCENE_MANAGER(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), SCENE_TYPE_MANAGER, SceneManager))
#define SCENE_MANAGER_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), SCENE_TYPE_MANAGER, SceneManagerClass))
#define SCENE_IS_MANAGER(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SCENE_TYPE_MANAGER))
#define SCENE_IS_MANAGER_CLASS(obj)      (G_TYPE_CHECK_CLASS_TYPE ((klass), SCENE_TYPE_MANAGER))
#define SCENE_MANAGER_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), SCENE_TYPE_MANAGER, SceneManagerClass))

typedef struct _SceneManagerClass SceneManagerClass;
typedef struct _SceneManager SceneManager;
typedef struct _SceneManagerPrivate SceneManagerPrivate;

struct _SceneManagerClass {
    GObjectClass parent_class;
};

struct _SceneManager {
    GObject parent_instance;

    SceneManagerPrivate *priv;
};

GType scene_manager_get_type (void) G_GNUC_CONST;

SceneManager *scene_manager_new ();
void scene_manager_start (SceneManager *manager);

G_END_DECLS

#endif //_SCENE_H_
