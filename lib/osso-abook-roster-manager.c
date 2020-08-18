#include <gtk/gtkprivate.h>

#include "osso-abook-roster-manager.h"
#include "osso-abook-waitable.h"
#include "osso-abook-roster.h"

typedef OssoABookRosterManagerIface OssoABookRosterManagerInterface;

G_DEFINE_INTERFACE_WITH_CODE(
    OssoABookRosterManager,
    osso_abook_roster_manager,
    G_TYPE_OBJECT,
    g_type_interface_add_prerequisite(g_define_type_id,
                                      OSSO_ABOOK_TYPE_WAITABLE);
);

enum {
  ROSTER_CREATED,
  ROSTER_REMOVED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {};

static void
osso_abook_roster_manager_default_init(OssoABookRosterManagerIface *iface)
{
  g_object_interface_install_property(
        iface,
        g_param_spec_boolean(
          "running",
          "Running",
          "Checks if the roster manager is running",
          0,
          GTK_PARAM_READABLE));

  signals[ROSTER_CREATED] =
      g_signal_new("roster-created",
        G_TYPE_FROM_CLASS(iface), G_SIGNAL_RUN_LAST,
        G_STRUCT_OFFSET(OssoABookRosterManagerIface, roster_created),
        0, NULL, g_cclosure_marshal_VOID__OBJECT,
        G_TYPE_NONE,
        1, OSSO_ABOOK_TYPE_ROSTER);
  signals[ROSTER_REMOVED] =
      g_signal_new(
        "roster-removed",
        G_TYPE_FROM_CLASS(iface), G_SIGNAL_RUN_LAST,
        G_STRUCT_OFFSET(OssoABookRosterManagerIface, roster_removed),
        0, NULL, g_cclosure_marshal_VOID__OBJECT,
        G_TYPE_NONE,
        1, OSSO_ABOOK_TYPE_ROSTER);
}
