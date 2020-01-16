#include <gtk/gtkprivate.h>

#include "osso-abook-all-group.h"
#include "osso-abook-aggregator.h"
#include "osso-abook-waitable.h"
#include "osso-abook-errors.h"

#include "config.h"

struct _OssoABookAllGroupPrivate
{
  OssoABookAggregator *aggregator;
  OssoABookWaitableClosure *closure;
};

typedef struct _OssoABookAllGroupPrivate OssoABookAllGroupPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(
  OssoABookAllGroup,
  osso_abook_all_group,
  OSSO_ABOOK_TYPE_GROUP
);

#define OSSO_ABOOK_ALL_GROUP_PRIVATE(group) \
                osso_abook_all_group_get_instance_private(group)

enum {
  PROP_AGGREGATOR = 1
};

static OssoABookGroup *all_group_singleton = NULL;

static void
osso_abook_all_group_init(OssoABookAllGroup *group)
{
  OssoABookAllGroupPrivate *priv = OSSO_ABOOK_ALL_GROUP_PRIVATE(group);

  group->priv = priv;
  priv->closure = NULL;
  priv->aggregator = NULL;
}

static char *
osso_abook_all_group_get_display_title(OssoABookGroup *group)
{
  OssoABookAllGroup *all_group = (OssoABookAllGroup *)group;
  OssoABookAllGroupPrivate *priv = OSSO_ABOOK_ALL_GROUP_PRIVATE(all_group);
  char *display_title;

  if (priv->aggregator &&
      osso_abook_waitable_is_ready(OSSO_ABOOK_WAITABLE(priv->aggregator), NULL))
  {
    display_title =
        g_strdup_printf(
          _("addr_ti_main_view"),
          osso_abook_aggregator_get_master_contact_count(priv->aggregator));
  }
  else
  {
    GRegex *regex = g_regex_new("\\s*\\S*%d\\S*\\s*", 0, 0, NULL);

    display_title = g_regex_replace_literal(regex, _("addr_ti_main_view"), -1,
                                            0, "", 0, NULL);
    g_regex_unref(regex);
  }

  return display_title;
}

static gboolean
osso_abook_all_group_includes_contact(OssoABookGroup *group,
                                      OssoABookContact *contact)
{
  return !osso_abook_contact_get_blocked(contact);
}

static int
osso_abook_all_group_get_sort_weight(OssoABookGroup *group)
{
  return -1000;
}

static void
master_contact_count_cb(OssoABookAllGroup *group)
{
  g_object_notify(G_OBJECT(group), "display-title");
}

static void
osso_abook_all_group_destroy_aggregator(OssoABookAllGroup *group)
{
  OssoABookAllGroupPrivate *priv = OSSO_ABOOK_ALL_GROUP_PRIVATE(group);

  if (priv->closure)
  {
    osso_abook_waitable_cancel(OSSO_ABOOK_WAITABLE(priv->aggregator),
                               priv->closure);
    priv->closure = NULL;
  }

  if (priv->aggregator)
  {
    g_signal_handlers_disconnect_matched(
          priv->aggregator, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC,
          0, 0, NULL, master_contact_count_cb, group);
    g_object_unref(priv->aggregator);
    priv->aggregator = NULL;
  }
}

static void
osso_abook_all_group_dispose(GObject *object)
{
  osso_abook_all_group_destroy_aggregator(OSSO_ABOOK_ALL_GROUP(object));
  G_OBJECT_CLASS(osso_abook_all_group_parent_class)->dispose(object);
}

static void
osso_abook_all_group_aggregator_ready_cb(OssoABookWaitable *waitable,
                                         const GError *error, gpointer data)
{
  OssoABookAllGroup *group = data;
  OssoABookAllGroupPrivate *priv = OSSO_ABOOK_ALL_GROUP_PRIVATE(group);

  priv->closure = NULL;

  if (error)
    osso_abook_handle_gerror(NULL, g_error_copy(error));
  else
  {
    g_signal_connect_swapped(priv->aggregator,
                             "notify::master-contact-count",
                             G_CALLBACK(master_contact_count_cb), group);
    master_contact_count_cb(group);
  }
}

static void
osso_abook_all_group_set_property(GObject *object, guint property_id,
                                  const GValue *value, GParamSpec *pspec)
{
  g_return_if_fail(OSSO_ABOOK_IS_ALL_GROUP(object));

  switch (property_id)
  {
    case PROP_AGGREGATOR:
    {
      OssoABookAllGroup *group = OSSO_ABOOK_ALL_GROUP(object);
      OssoABookAllGroupPrivate *priv = OSSO_ABOOK_ALL_GROUP_PRIVATE(group);

      osso_abook_all_group_destroy_aggregator(group);
      priv->aggregator = g_value_dup_object(value);
      master_contact_count_cb(group);
      priv->closure = osso_abook_waitable_call_when_ready(
            OSSO_ABOOK_WAITABLE(priv->aggregator),
            osso_abook_all_group_aggregator_ready_cb, group, NULL);
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
osso_abook_all_group_get_property(GObject *object, guint property_id,
                                  GValue *value, GParamSpec *pspec)
{
  g_return_if_fail(OSSO_ABOOK_IS_ALL_GROUP(object));

  switch (property_id)
  {
    case PROP_AGGREGATOR:
    {
      OssoABookAllGroup *group = OSSO_ABOOK_ALL_GROUP(object);
      OssoABookAllGroupPrivate *priv = OSSO_ABOOK_ALL_GROUP_PRIVATE(group);

      g_value_set_object(value, priv->aggregator);
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
osso_abook_all_group_class_init(OssoABookAllGroupClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  OssoABookGroupClass *group_class = OSSO_ABOOK_GROUP_CLASS(klass);

  object_class->set_property = osso_abook_all_group_set_property;
  object_class->get_property = osso_abook_all_group_get_property;
  object_class->dispose = osso_abook_all_group_dispose;

  group_class->get_display_title = osso_abook_all_group_get_display_title;
  group_class->includes_contact = osso_abook_all_group_includes_contact;
  group_class->get_sort_weight = osso_abook_all_group_get_sort_weight;

  g_object_class_install_property(
        object_class, PROP_AGGREGATOR,
        g_param_spec_object(
                 "aggregator",
                 "Aggregator",
                 "The aggregator of all contacts in the addressbook.",
                 OSSO_ABOOK_TYPE_AGGREGATOR,
                 GTK_PARAM_READWRITE));
}

static void
osso_abook_all_group_at_exit()
{
  g_object_unref(all_group_singleton);
}

OssoABookGroup *
osso_abook_all_group_get(void)
{
  if (!all_group_singleton)
  {
    all_group_singleton = g_object_new(OSSO_ABOOK_TYPE_ALL_GROUP, NULL);
    atexit(osso_abook_all_group_at_exit);
  }

  return all_group_singleton;
}
