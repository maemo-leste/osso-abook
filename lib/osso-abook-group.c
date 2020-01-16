#include <gtk/gtkprivate.h>

#include <string.h>

#include "osso-abook-group.h"

#include "config.h"

struct _OssoABookGroupPrivate {
  gchar *display_title;
};

typedef struct _OssoABookGroupPrivate OssoABookGroupPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE(
  OssoABookGroup,
  osso_abook_group,
  G_TYPE_OBJECT
);

#define OSSO_ABOOK_GROUP_PRIVATE(group) \
                osso_abook_group_get_instance_private(group)

enum {
  PROP_NAME = 1,
  PROP_ICON_NAME,
  PROP_DISPLAY_TITLE,
  PROP_VISIBLE,
  PROP_SORT_WEIGHT,
  PROP_SENSITIVE
};

enum {
  SIGNAL_REFILTER_CONTACT,
  SIGNAL_REFILTER_GROUP,
  SIGNAL_LAST
};

static guint signals[SIGNAL_LAST];

static void
osso_abook_group_init(OssoABookGroup *group)
{
  group->priv = OSSO_ABOOK_GROUP_PRIVATE(group);
}

static void
osso_abook_group_set_name(OssoABookGroup *group, const char *name)
{
  OssoABookGroupClass *group_class;

  g_return_if_fail(OSSO_ABOOK_IS_GROUP(group));

  group_class = OSSO_ABOOK_GROUP_GET_CLASS(group);

  if (group_class->set_name)
    group_class->set_name(group, name);

  g_object_notify(G_OBJECT(group), "display-title");
}

static void
osso_abook_group_set_icon_name(OssoABookGroup *group, const char *icon_name)
{
  OssoABookGroupClass *group_class;

  g_return_if_fail(OSSO_ABOOK_IS_GROUP(group));

  group_class = OSSO_ABOOK_GROUP_GET_CLASS(group);

  if (group_class->set_icon_name)
    group_class->set_icon_name(group, icon_name);
}

static void
osso_abook_group_set_visible(OssoABookGroup *group, gboolean visible)
{
  OssoABookGroupClass *group_class;

  g_return_if_fail(OSSO_ABOOK_IS_GROUP(group));

  group_class = OSSO_ABOOK_GROUP_GET_CLASS(group);

  if (group_class->set_visible)
    group_class->set_visible(group, visible);
}

static void
osso_abook_group_set_weight(OssoABookGroup *group, int weight)
{
  OssoABookGroupClass *group_class;

  g_return_if_fail(OSSO_ABOOK_IS_GROUP(group));

  group_class = OSSO_ABOOK_GROUP_GET_CLASS(group);

  if (group_class->set_sort_weight)
    group_class->set_sort_weight(group, weight);
}

static void
osso_abook_group_set_sensitive(OssoABookGroup *group, gboolean sensitive)
{
  OssoABookGroupClass *group_class;

  g_return_if_fail(OSSO_ABOOK_IS_GROUP(group));

  group_class = OSSO_ABOOK_GROUP_GET_CLASS(group);

  if (group_class->set_sensitive)
    group_class->set_sensitive(group, sensitive);
}

static void
osso_abook_group_set_property(GObject *object, guint property_id,
                              const GValue *value, GParamSpec *pspec)
{
  OssoABookGroup *group = OSSO_ABOOK_GROUP(object);

  switch (property_id)
  {
    case PROP_NAME:
      osso_abook_group_set_name(group, g_value_get_string(value));
      return;
    case PROP_ICON_NAME:
      osso_abook_group_set_icon_name(group, g_value_get_string(value));
      return;
    case PROP_VISIBLE:
      osso_abook_group_set_visible(group, g_value_get_boolean(value));
      return;
    case PROP_SORT_WEIGHT:
      osso_abook_group_set_weight(group, g_value_get_int(value));
      return;
    case PROP_SENSITIVE:
     osso_abook_group_set_sensitive(group, g_value_get_boolean(value));
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      return;
  }
}

const char *
osso_abook_group_get_name(OssoABookGroup *group)
{
  OssoABookGroupClass *group_class;

  g_return_val_if_fail(OSSO_ABOOK_IS_GROUP(group), NULL);

  group_class = OSSO_ABOOK_GROUP_GET_CLASS(group);

  if (group_class->get_name)
    return group_class->get_name(group);

  return NULL;
}

const char *
osso_abook_group_get_icon_name(OssoABookGroup *group)
{
  OssoABookGroupClass *group_class;

  g_return_val_if_fail(OSSO_ABOOK_IS_GROUP(group), NULL);

  group_class = OSSO_ABOOK_GROUP_GET_CLASS(group);

  if (group_class->get_icon_name)
    return group_class->get_icon_name(group);

  return NULL;
}

const char *
osso_abook_group_get_display_title(OssoABookGroup *group)
{
  OssoABookGroupClass *group_class;
  OssoABookGroupPrivate *priv;

  g_return_val_if_fail(OSSO_ABOOK_IS_GROUP(group), NULL);

  group_class = OSSO_ABOOK_GROUP_GET_CLASS(group);
  priv = OSSO_ABOOK_GROUP_PRIVATE(group);

  g_free(priv->display_title);
  priv->display_title = NULL;

  if (group_class->get_display_title)
    return group_class->get_display_title(group);

  priv->display_title = g_strdup(osso_abook_group_get_display_name(group));

  return priv->display_title;
}

gboolean
osso_abook_group_is_visible(OssoABookGroup *group)
{
  OssoABookGroupClass *group_class;

  g_return_val_if_fail(OSSO_ABOOK_IS_GROUP(group), FALSE);

  group_class = OSSO_ABOOK_GROUP_GET_CLASS(group);

  if (group_class->is_visible)
    return group_class->is_visible(group);

  return TRUE;
}

int
osso_abook_group_get_sort_weight(OssoABookGroup *group)
{
  OssoABookGroupClass *group_class;

  g_return_val_if_fail(OSSO_ABOOK_IS_GROUP(group), 0);

  group_class = OSSO_ABOOK_GROUP_GET_CLASS(group);

  if (group_class->get_sort_weight)
    return group_class->get_sort_weight(group);

  return 0;
}

gboolean
osso_abook_group_get_sensitive(OssoABookGroup *group)
{
  OssoABookGroupClass *group_class;

  g_return_val_if_fail(OSSO_ABOOK_IS_GROUP(group), FALSE);

  group_class = OSSO_ABOOK_GROUP_GET_CLASS(group);

  if (group_class->get_sensitive)
    return group_class->get_sensitive(group);

  return TRUE;
}

static void
osso_abook_group_get_property(GObject *object, guint property_id, GValue *value,
                              GParamSpec *pspec)
{
  OssoABookGroup *group = OSSO_ABOOK_GROUP(object);

  switch (property_id)
  {
    case PROP_NAME:
      g_value_set_string(value, osso_abook_group_get_name(group));
      break;
    case PROP_ICON_NAME:
      g_value_set_string(value, osso_abook_group_get_icon_name(group));
      break;
    case PROP_DISPLAY_TITLE:
      g_value_set_string(value, osso_abook_group_get_display_title(group));
      break;
    case PROP_VISIBLE:
      g_value_set_boolean(value, osso_abook_group_is_visible(group));
      break;
    case PROP_SORT_WEIGHT:
      g_value_set_int(value, osso_abook_group_get_sort_weight(group));
      break;
    case PROP_SENSITIVE:
      g_value_set_boolean(value, osso_abook_group_get_sensitive(group));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
osso_abook_group_finalize(GObject *object)
{
  OssoABookGroup *group = OSSO_ABOOK_GROUP(object);
  OssoABookGroupPrivate *priv = OSSO_ABOOK_GROUP_PRIVATE(group);

  g_free(priv->display_title);
  G_OBJECT_CLASS(osso_abook_group_parent_class)->finalize(object);
}

static void
osso_abook_group_class_init(OssoABookGroupClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->set_property = osso_abook_group_set_property;
  object_class->get_property = osso_abook_group_get_property;
  object_class->finalize = osso_abook_group_finalize;

  g_object_class_install_property(
        object_class, PROP_NAME,
        g_param_spec_string(
                 "name",
                 "Name",
                 "The name of the group",
                 NULL,
                 GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property(
        object_class, PROP_ICON_NAME,
        g_param_spec_string(
                 "icon-name",
                 "Icon name",
                 "The icon name of the group. The icon name can be used to look up the appropiate icon to represent the group.",
                 NULL,
                 GTK_PARAM_READWRITE));
  g_object_class_install_property(
        object_class, PROP_DISPLAY_TITLE,
        g_param_spec_string(
                 "display-title",
                 "Display Title",
                 "The display title of the group",
                 NULL,
                 GTK_PARAM_READABLE));
  g_object_class_install_property(
        object_class, PROP_VISIBLE,
        g_param_spec_boolean(
                 "visible",
                 "Visible",
                 "Whether the group is visible.",
                 TRUE,
                 GTK_PARAM_READWRITE));
  g_object_class_install_property(
        object_class, PROP_SORT_WEIGHT,
        g_param_spec_int(
                 "sort-weight",
                 "Sort weight",
                 "Sort weight of the group. A lower value results in a higher sorting priority.",
                 G_MININT,
                 G_MAXINT,
                 0,
                 GTK_PARAM_READWRITE));
  g_object_class_install_property(
        object_class, PROP_SENSITIVE,
        g_param_spec_boolean(
                 "sensitive",
                 "Sensitive",
                 "Whether the group is sensitive.",
                 FALSE,
                 GTK_PARAM_READWRITE));

  signals[SIGNAL_REFILTER_CONTACT] =
      g_signal_new("refilter-contact", OSSO_ABOOK_TYPE_GROUP, G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET(OssoABookGroupClass, refilter_contact),
                   0, NULL, g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE,
                   1, OSSO_ABOOK_TYPE_CONTACT);

  signals[SIGNAL_REFILTER_GROUP] =
      g_signal_new("refilter-group", OSSO_ABOOK_TYPE_GROUP, G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET(OssoABookGroupClass, refilter_group),
                   0, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

GtkWidget *
osso_abook_group_get_empty_widget(OssoABookGroup *group)
{
  OssoABookGroupClass *group_class;

  g_return_val_if_fail(OSSO_ABOOK_IS_GROUP(group), NULL);

  group_class = OSSO_ABOOK_GROUP_GET_CLASS(group);

  if (group_class->get_empty_widget)
    return group_class->get_empty_widget(group);

  return NULL;
}

gboolean
osso_abook_group_includes_contact(OssoABookGroup *group, OssoABookContact *contact)
{
  OssoABookGroupClass *group_class;

  g_return_val_if_fail(OSSO_ABOOK_IS_GROUP(group), FALSE);
  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT(contact), FALSE);

  group_class = OSSO_ABOOK_GROUP_GET_CLASS(group);

  if (group_class->includes_contact)
    return group_class->includes_contact(group, contact);

  return FALSE;
}

OssoABookListStoreCompareFunc
osso_abook_group_get_sort_func(OssoABookGroup *group)
{
  OssoABookGroupClass *group_class;

  g_return_val_if_fail(OSSO_ABOOK_IS_GROUP(group), NULL);

  group_class = OSSO_ABOOK_GROUP_GET_CLASS(group);

  if (group_class->get_sort_func)
    return group_class->get_sort_func(group);

  return NULL;
}

int
osso_abook_group_compare(OssoABookGroup *a, OssoABookGroup *b)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_GROUP(a), 0);
  g_return_val_if_fail(OSSO_ABOOK_IS_GROUP(b), 0);

  int sort_weight_a = osso_abook_group_get_sort_weight(a);
  int sort_weight_b = osso_abook_group_get_sort_weight(b);

  if (sort_weight_a == sort_weight_b)
  {
    return g_utf8_collate(osso_abook_group_get_display_name(a),
                          osso_abook_group_get_display_name(b));
  }

  if (sort_weight_a < sort_weight_b)
    return -1;

  return 1;
}

OssoABookListStore *
osso_abook_group_get_model(OssoABookGroup *group)
{
  OssoABookGroupClass *group_class;

  g_return_val_if_fail(OSSO_ABOOK_IS_GROUP(group), NULL);

  group_class = OSSO_ABOOK_GROUP_GET_CLASS(group);

  if (group_class->get_model)
    return group_class->get_model(group);

  return NULL;
}

const char *
osso_abook_group_get_display_name(OssoABookGroup *group)
{
  OssoABookGroupClass *group_class;
  const char *display_name;

  g_return_val_if_fail(OSSO_ABOOK_IS_GROUP(group), NULL);

  group_class = OSSO_ABOOK_GROUP_GET_CLASS(group);

  if (!group_class->get_name)
    return NULL;

  display_name = group_class->get_name(group);


  if (display_name && *display_name)
  {
    if (!strncmp(display_name, "addr_", 5))
      display_name = _(display_name);
  }

  return display_name;
}
