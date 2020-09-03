#include <gtk/gtkprivate.h>

#include "config.h"

#include "osso-abook-roster-manager.h"
#include "osso-abook-waitable.h"
#include "osso-abook-roster.h"
#include "osso-abook-account-manager.h"

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

OssoABookRosterManager *
osso_abook_roster_manager_get_default()
{
  return OSSO_ABOOK_ROSTER_MANAGER(osso_abook_account_manager_get_default());
}

void
osso_abook_roster_manager_start(OssoABookRosterManager *manager)
{
  OssoABookRosterManagerIface *iface;

  g_return_if_fail(OSSO_ABOOK_IS_ROSTER_MANAGER(manager));

  iface = OSSO_ABOOK_ROSTER_MANAGER_GET_IFACE(manager);

  if (iface->start)
    iface->start(manager);
}

void
osso_abook_roster_manager_stop(OssoABookRosterManager *manager)
{
  OssoABookRosterManagerIface *iface;

  g_return_if_fail(OSSO_ABOOK_IS_ROSTER_MANAGER(manager));

  iface = OSSO_ABOOK_ROSTER_MANAGER_GET_IFACE(manager);

  if (iface->stop)
    iface->stop(manager);
}

gboolean
osso_abook_roster_manager_is_running(OssoABookRosterManager *manager)
{
  gboolean running;

  g_return_val_if_fail(OSSO_ABOOK_IS_ROSTER_MANAGER(manager), FALSE);

  g_object_get(manager, "running", &running, NULL);

  return running;
}

GList *
osso_abook_roster_manager_list_rosters(OssoABookRosterManager *manager)
{
  OssoABookRosterManagerIface *iface;

  if (!manager)
    manager = osso_abook_roster_manager_get_default();

  g_return_val_if_fail(OSSO_ABOOK_IS_ROSTER_MANAGER(manager), NULL);

  iface = OSSO_ABOOK_ROSTER_MANAGER_GET_IFACE(manager);

  g_return_val_if_fail(NULL != iface->list_rosters, NULL);

  return iface->list_rosters(manager);
}

OssoABookRoster *
osso_abook_roster_manager_get_roster(OssoABookRosterManager *manager,
                                     const char *account_name)
{
  OssoABookRosterManagerIface *iface;

  if (!manager)
    manager = osso_abook_roster_manager_get_default();

  g_return_val_if_fail(OSSO_ABOOK_IS_ROSTER_MANAGER(manager), NULL);
  g_return_val_if_fail(NULL != account_name, NULL);

  iface = OSSO_ABOOK_ROSTER_MANAGER_GET_IFACE(manager);

  g_return_val_if_fail(NULL != iface->get_roster, NULL);

  return iface->get_roster(manager, account_name);
}
