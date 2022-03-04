/*
 * osso-abook-presence-icon.c
 *
 * Copyright (C) 2022 Ivaylo Dimitrov <ivo.g.dimitrov.75@gmail.com>
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "config.h"

#include <gtk/gtkprivate.h>
#include <hildon/hildon.h>

#include "osso-abook-presence-icon.h"

enum
{
  PROP_PRESENCE = 1
};

struct _OssoABookPresenceIconPrivate
{
  OssoABookPresence *presence;
};

typedef struct _OssoABookPresenceIconPrivate OssoABookPresenceIconPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(
  OssoABookPresenceIcon,
  osso_abook_presence_icon,
  GTK_TYPE_IMAGE
);

#define PRIVATE(icon) \
  ((OssoABookPresenceIconPrivate *) \
   osso_abook_presence_icon_get_instance_private( \
     (OssoABookPresenceIcon *)(icon)))

static void
osso_abook_presence_icon_set_property(GObject *object, guint property_id,
                                      const GValue *value, GParamSpec *pspec)
{
  OssoABookPresenceIcon *icon = OSSO_ABOOK_PRESENCE_ICON(object);

  switch (property_id)
  {
    case PROP_PRESENCE:
    {
      osso_abook_presence_icon_set_presence(icon, g_value_get_object(value));
      break;
    }
    default:
    {
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
      break;
    }
  }
}

static void
osso_abook_presence_icon_get_property(GObject *object, guint property_id,
                                      GValue *value, GParamSpec *pspec)
{
  OssoABookPresenceIconPrivate *priv = PRIVATE(object);

  switch (property_id)
  {
    case PROP_PRESENCE:
    {
      g_value_set_object(value, priv->presence);
      break;
    }
    default:
    {
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
      break;
    }
  }
}

static void
osso_abook_presence_icon_dispose(GObject *object)
{
  OssoABookPresenceIcon *icon = OSSO_ABOOK_PRESENCE_ICON(object);

  osso_abook_presence_icon_set_presence(icon, NULL);

  G_OBJECT_CLASS(osso_abook_presence_icon_parent_class)->dispose(object);
}

static void
osso_abook_presence_icon_size_request(GtkWidget *widget,
                                      GtkRequisition *requisition)
{
  GtkSettings *settings = gtk_widget_get_settings(widget);

  gtk_icon_size_lookup_for_settings(settings, HILDON_ICON_SIZE_XSMALL,
                                    &requisition->width, &requisition->height);

  requisition->width += 2 * GTK_MISC(widget)->xpad;
  requisition->height += 2 * GTK_MISC(widget)->ypad;
}

static void
osso_abook_presence_icon_class_init(OssoABookPresenceIconClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

  object_class->set_property = osso_abook_presence_icon_set_property;
  object_class->get_property = osso_abook_presence_icon_get_property;
  object_class->dispose = osso_abook_presence_icon_dispose;
  widget_class->size_request = osso_abook_presence_icon_size_request;

  g_object_class_install_property(
    object_class, PROP_PRESENCE,
    g_param_spec_object(
      "presence",
      "Presence",
      "The presence to display",
      OSSO_ABOOK_TYPE_PRESENCE,
      GTK_PARAM_READWRITE));
}

static void
osso_abook_presence_icon_init(OssoABookPresenceIcon *icon)
{
  PRIVATE(icon)->presence = NULL;
}

static void
notify_presence_type_cb(OssoABookPresenceIcon *icon)
{
  OssoABookPresence *presence = PRIVATE(icon)->presence;
  const char *icon_name;

  if (presence)
    icon_name = osso_abook_presence_get_icon_name(presence);
  else
    icon_name = "general_presence_offline";

  gtk_image_set_from_icon_name(GTK_IMAGE(icon), icon_name,
                               HILDON_ICON_SIZE_XSMALL);
}

void
osso_abook_presence_icon_set_presence(OssoABookPresenceIcon *icon,
                                      OssoABookPresence *presence)
{
  OssoABookPresenceIconPrivate *priv;

  g_return_if_fail(OSSO_ABOOK_IS_PRESENCE_ICON(icon));
  g_return_if_fail(!presence || OSSO_ABOOK_IS_PRESENCE(presence));

  priv = PRIVATE(icon);

  if (priv->presence != presence)
  {
    if (priv->presence)
    {
      g_signal_handlers_disconnect_matched(
        priv->presence, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC,
        0, 0, NULL, notify_presence_type_cb, icon);
      g_object_unref(priv->presence);
      priv->presence = NULL;
    }

    if (presence)
    {
      priv->presence = g_object_ref(presence);
      g_signal_connect_swapped(presence, "notify::presence-type",
                               G_CALLBACK(notify_presence_type_cb), icon);
      notify_presence_type_cb(icon);
    }

    g_object_notify(G_OBJECT(icon), "presence");
  }
}

OssoABookPresence *
osso_abook_presence_icon_get_presence(OssoABookPresenceIcon *icon)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_PRESENCE_ICON(icon), NULL);

  return PRIVATE(icon)->presence;
}
