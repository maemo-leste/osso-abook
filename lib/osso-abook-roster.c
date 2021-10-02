#include <gtk/gtkprivate.h>

#include "eds.h"
#include "osso-abook-account-manager.h"
#include "osso-abook-debug.h"
#include "osso-abook-enums.h"
#include "osso-abook-roster.h"
#include "osso-abook-utils-private.h"
#include "osso-abook-waitable.h"

#include "config.h"

enum
{
  PROP_NAME = 1,
  PROP_ACCOUNT,
  PROP_VCARD_FIELD,
  PROP_BOOK_VIEW,
  PROP_BOOK_URI,
  PROP_MASTER_BOOK,
  PROP_NAME_ORDER,
  PROP_RUNNING
};

enum
{
  CONTACTS_ADDED,
  CONTACTS_CHANGED,
  CONTACTS_REMOVED,
  SEQUENCE_COMPLETE,
  STATUS_MESSAGE,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];
static GQuark master;

struct _OssoABookRosterPrivate
{
  gchar *name;
  EBook *book;
#pragma message("FIXME!!! - EBookView is deprecated")
  EBookView *book_view;
  gchar *vcard_field;
  OssoABookNameOrder name_order;
  GList *closures;
  gboolean backend_died : 1;
  gboolean is_ready : 1;
  gboolean is_running : 1;
};

typedef struct _OssoABookRosterPrivate OssoABookRosterPrivate;

#define OSSO_ABOOK_ROSTER_PRIVATE(roster) \
  ((OssoABookRosterPrivate *)osso_abook_roster_get_instance_private(roster))

static void
osso_abook_roster_osso_abook_waitable_iface_init(OssoABookWaitableIface *result,
                                                 gpointer data);

static void
osso_abook_roster_set_book_view(OssoABookRoster *roster, EBookView *book_view);

G_DEFINE_TYPE_WITH_CODE(
  OssoABookRoster,
  osso_abook_roster,
  G_TYPE_OBJECT,
  G_IMPLEMENT_INTERFACE(
    OSSO_ABOOK_TYPE_WAITABLE,
    osso_abook_roster_osso_abook_waitable_iface_init);
  G_ADD_PRIVATE(OssoABookRoster);
);

static void
osso_abook_roster_waitable_push(OssoABookWaitable *waitable,
                                OssoABookWaitableClosure *closure)
{
  OssoABookRoster *roster = OSSO_ABOOK_ROSTER(waitable);
  OssoABookRosterPrivate *priv = OSSO_ABOOK_ROSTER_PRIVATE(roster);

  osso_abook_list_push(&priv->closures, closure);
}

OssoABookWaitableClosure *
osso_abook_roster_waitable_pop(OssoABookWaitable *waitable,
                               OssoABookWaitableClosure *closure)
{
  OssoABookRoster *roster = OSSO_ABOOK_ROSTER(waitable);
  OssoABookRosterPrivate *priv = OSSO_ABOOK_ROSTER_PRIVATE(roster);

  return osso_abook_list_pop(&priv->closures, closure);
}

static gboolean
osso_abook_roster_waitable_is_ready(OssoABookWaitable *waitable,
                                    const GError **error)
{
  OssoABookRoster *roster = OSSO_ABOOK_ROSTER(waitable);

  return OSSO_ABOOK_ROSTER_PRIVATE(roster)->is_ready;
}

static void
osso_abook_roster_osso_abook_waitable_iface_init(OssoABookWaitableIface *result,
                                                 gpointer data)
{
  result->pop = osso_abook_roster_waitable_pop;
  result->is_ready = osso_abook_roster_waitable_is_ready;
  result->push = osso_abook_roster_waitable_push;
}

static void
osso_abook_roster_init(OssoABookRoster *roster)
{}

static void
osso_abook_roster_set_property(GObject *object, guint property_id,
                               const GValue *value, GParamSpec *pspec)
{
  OssoABookRoster *roster = OSSO_ABOOK_ROSTER(object);
  OssoABookRosterPrivate *priv = OSSO_ABOOK_ROSTER_PRIVATE(roster);

  switch (property_id)
  {
    case PROP_NAME:
      g_assert(NULL == priv->name);
      priv->name = g_value_dup_string(value);
      break;
    case PROP_VCARD_FIELD:
      g_assert(NULL == priv->vcard_field);
      priv->vcard_field = g_value_dup_string(value);
      break;
    case PROP_BOOK_VIEW:
      osso_abook_roster_set_book_view(roster, g_value_get_object(value));
      break;
    case PROP_MASTER_BOOK:
      g_assert(NULL == priv->book);
      g_assert(NULL == priv->book_view);
      priv->book = g_value_dup_object(value);
      break;
    case PROP_NAME_ORDER:
    {
      OssoABookNameOrder name_order = g_value_get_enum(value);

      if (osso_abook_roster_is_running(roster))
      {
        g_warning(
          "Changing the name order of %s doesn't have any effect until the roster is restarted.",
          osso_abook_roster_get_book_uri(roster));
      }

      if (name_order != priv->name_order)
        priv->name_order = name_order;

      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
      break;
  }
}

static void
osso_abook_roster_get_property(GObject *object, guint property_id,
                               GValue *value, GParamSpec *pspec)
{
  OssoABookRoster *roster = OSSO_ABOOK_ROSTER(object);

  switch (property_id)
  {
    case PROP_NAME:
      g_value_set_string(value, osso_abook_roster_get_name(roster));
      break;
    case PROP_ACCOUNT:
      g_value_set_object(value, osso_abook_roster_get_account(roster));
      break;
    case PROP_VCARD_FIELD:
      g_value_set_string(value, osso_abook_roster_get_vcard_field(roster));
      break;
    case PROP_BOOK_VIEW:
      g_value_set_object(value, osso_abook_roster_get_book_view(roster));
      break;
    case PROP_BOOK_URI:
      g_value_set_string(value, osso_abook_roster_get_book_uri(roster));
      break;
    case PROP_MASTER_BOOK:
      g_value_set_object(value, osso_abook_roster_get_book(roster));
      break;
    case PROP_NAME_ORDER:
      g_value_set_enum(value, osso_abook_roster_get_name_order(roster));
      break;
    case PROP_RUNNING:
      g_value_set_boolean(value, osso_abook_roster_is_running(roster));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
      break;
  }
}

static void
osso_abook_roster_finalize(GObject *object)
{
  OssoABookRoster *roster = OSSO_ABOOK_ROSTER(object);
  OssoABookRosterPrivate *priv = OSSO_ABOOK_ROSTER_PRIVATE(roster);

  g_free(priv->name);
  g_free(priv->vcard_field);

  G_OBJECT_CLASS(osso_abook_roster_parent_class)->finalize(object);
}

static void
osso_abook_roster_dispose(GObject *object)
{
  OssoABookRoster *roster = OSSO_ABOOK_ROSTER(object);

  osso_abook_waitable_reset(OSSO_ABOOK_WAITABLE(roster));

  if (osso_abook_roster_is_running(roster))
    osso_abook_roster_stop(roster);

  osso_abook_roster_set_book_view(roster, NULL);

  G_OBJECT_CLASS(osso_abook_roster_parent_class)->dispose(object);
}

static void
free_contacts(OssoABookContact **contacts)
{
  OssoABookContact **c = contacts;

  while (*c)
  {
    g_object_unref(*c);
    c++;
  }

  g_free(contacts);
}

static void
osso_abook_roster_contacts_added(OssoABookRoster *roster,
                                 OssoABookContact **contacts)
{
  GSignalInvocationHint *signal_hint = g_signal_get_invocation_hint(roster);

  g_return_if_fail(signals[CONTACTS_ADDED] == signal_hint->signal_id);

  if (signal_hint->run_type & G_SIGNAL_RUN_CLEANUP)
    free_contacts(contacts);
}

static void
osso_abook_roster_contacts_changed(OssoABookRoster *roster,
                                   OssoABookContact **contacts)
{
  GSignalInvocationHint *signal_hint = g_signal_get_invocation_hint(roster);

  g_return_if_fail(signals[CONTACTS_CHANGED] == signal_hint->signal_id);

  if (signal_hint->run_type & G_SIGNAL_RUN_CLEANUP)
    free_contacts(contacts);
}

static void
osso_abook_roster_contacts_removed(OssoABookRoster *roster, const char **uids)
{
  GSignalInvocationHint *signal_hint = g_signal_get_invocation_hint(roster);

  g_return_if_fail(signals[CONTACTS_REMOVED] == signal_hint->signal_id);

  if (signal_hint->run_type & G_SIGNAL_RUN_CLEANUP)
    g_strfreev((gchar **)uids);
}

static void
osso_abook_roster_status_message(OssoABookRoster *roster, const char *message)
{}

static void
osso_abook_roster_sequence_complete(OssoABookRoster *roster,
                                    EBookViewStatus status)
{}

static void
osso_abook_roster_real_start(OssoABookRoster *roster)
{
  OssoABookRosterPrivate *priv = OSSO_ABOOK_ROSTER_PRIVATE(roster);

#if 0
  const gchar *sort_order;
#endif

  if (!priv->book_view)
    return;

  OSSO_ABOOK_NOTE(EDS, "starting book view for %s (view=%p)\n",
                  osso_abook_roster_get_book_uri(roster), priv->book_view);
#if 0

  switch (priv->name_order)
  {
    case OSSO_ABOOK_NAME_ORDER_FIRST:
      sort_order = "first-last";
      break;
    case OSSO_ABOOK_NAME_ORDER_LAST:
    case OSSO_ABOOK_NAME_ORDER_LAST_SPACE:
      sort_order = "last-first";
      break;
    case OSSO_ABOOK_NAME_ORDER_NICK:
      sort_order = "nick";
      break;
    default:
      sort_order = NULL;
      break;
  }

  e_book_view_set_sort_order(priv->book_view, sort_order);
#else
  #pragma \
  message("FIXME!!! - e_book_view_set_sort_order is Maemo specific, find how to do it upstream")
#endif

  e_book_view_start(priv->book_view);
}

static void
osso_abook_roster_real_stop(OssoABookRoster *roster)
{
  EBookView *book_view = OSSO_ABOOK_ROSTER_PRIVATE(roster)->book_view;

  if (book_view)
    e_book_view_stop(book_view);
}

static void
osso_abook_roster_class_init(OssoABookRosterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->set_property = osso_abook_roster_set_property;
  object_class->get_property = osso_abook_roster_get_property;
  object_class->finalize = osso_abook_roster_finalize;
  object_class->dispose = osso_abook_roster_dispose;

  klass->contacts_added = osso_abook_roster_contacts_added;
  klass->contacts_changed = osso_abook_roster_contacts_changed;
  klass->contacts_removed = osso_abook_roster_contacts_removed;
  klass->status_message = osso_abook_roster_status_message;
  klass->sequence_complete = osso_abook_roster_sequence_complete;
  klass->start = osso_abook_roster_real_start;
  klass->stop = osso_abook_roster_real_stop;

  g_object_class_install_property(
    object_class, PROP_NAME,
    g_param_spec_string(
      "name",
      "Name",
      "The name of this roster",
      0,
      GTK_PARAM_READWRITE | G_PARAM_PRIVATE | G_PARAM_CONSTRUCT_ONLY)
  );
  g_object_class_install_property(
    object_class, PROP_ACCOUNT,
    g_param_spec_object(
      "account",
      "Account",
      "The Telepathy account of this roster",
      TP_TYPE_ACCOUNT,
      GTK_PARAM_READABLE));
  g_object_class_install_property(
    object_class, PROP_VCARD_FIELD,
    g_param_spec_string(
      "vcard-field",
      "VCard Field",
      "Name of the vCard field associated with this roster",
      0,
      GTK_PARAM_READWRITE | G_PARAM_PRIVATE | G_PARAM_CONSTRUCT_ONLY)
  );
  g_object_class_install_property(
    object_class, PROP_BOOK_VIEW,
    g_param_spec_object(
      "book-view",
      "Master Book View",
      "The master address book view",
      E_TYPE_BOOK_VIEW,
      GTK_PARAM_READWRITE | G_PARAM_PRIVATE));
  g_object_class_install_property(
    object_class, PROP_BOOK_URI,
    g_param_spec_string(
      "book-uri",
      "Book URI",
      "The URI of the master address book",
      0,
      GTK_PARAM_READABLE));
  g_object_class_install_property(
    object_class, PROP_MASTER_BOOK,
    g_param_spec_object(
      "book",
      "Master Book",
      "The master address book",
      E_TYPE_BOOK,
      GTK_PARAM_READWRITE | G_PARAM_PRIVATE | G_PARAM_CONSTRUCT_ONLY)
  );
  g_object_class_install_property(
    object_class, PROP_NAME_ORDER,
    g_param_spec_enum(
      "name-order",
      "Name Order",
      "The preferred order for contacts-added signals",
      OSSO_ABOOK_TYPE_NAME_ORDER,
      0,
      GTK_PARAM_READWRITE | G_PARAM_PRIVATE | G_PARAM_CONSTRUCT));
  g_object_class_install_property(
    object_class, PROP_RUNNING,
    g_param_spec_boolean(
      "running",
      "Running",
      "Wether this roster is emitting events already",
      FALSE,
      GTK_PARAM_READABLE));

  signals[CONTACTS_ADDED] =
    g_signal_new("contacts-added",
                 OSSO_ABOOK_TYPE_ROSTER,
                 G_SIGNAL_RUN_CLEANUP | G_SIGNAL_RUN_LAST |
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(OssoABookRosterClass, contacts_added),
                 0, NULL, g_cclosure_marshal_VOID__POINTER,
                 G_TYPE_NONE,
                 1, G_TYPE_POINTER);
  signals[CONTACTS_CHANGED] =
    g_signal_new("contacts-changed",
                 OSSO_ABOOK_TYPE_ROSTER,
                 G_SIGNAL_DETAILED | G_SIGNAL_RUN_CLEANUP |
                 G_SIGNAL_RUN_LAST | G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(OssoABookRosterClass, contacts_changed),
                 0, NULL, g_cclosure_marshal_VOID__POINTER,
                 G_TYPE_NONE,
                 1, G_TYPE_POINTER);
  signals[CONTACTS_REMOVED] =
    g_signal_new("contacts-removed",
                 OSSO_ABOOK_TYPE_ROSTER,
                 G_SIGNAL_RUN_CLEANUP | G_SIGNAL_RUN_LAST |
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(OssoABookRosterClass, contacts_removed),
                 0, NULL, g_cclosure_marshal_VOID__POINTER,
                 G_TYPE_NONE,
                 1, G_TYPE_POINTER);
  signals[SEQUENCE_COMPLETE] =
    g_signal_new("sequence-complete",
                 OSSO_ABOOK_TYPE_ROSTER,
                 G_SIGNAL_RUN_CLEANUP | G_SIGNAL_RUN_LAST |
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(OssoABookRosterClass, sequence_complete),
                 0, NULL, g_cclosure_marshal_VOID__UINT,
                 G_TYPE_NONE,
                 1, G_TYPE_UINT);
  signals[STATUS_MESSAGE] =
    g_signal_new("status-message",
                 OSSO_ABOOK_TYPE_ROSTER,
                 G_SIGNAL_RUN_CLEANUP | G_SIGNAL_RUN_LAST |
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(OssoABookRosterClass, status_message),
                 0, NULL, g_cclosure_marshal_VOID__STRING,
                 G_TYPE_NONE,
                 1, G_TYPE_STRING);

  master = g_quark_from_static_string("master");
}

static void
contacts_added_or_changed(OssoABookRoster *roster, guint sig, GQuark detail,
                          GPtrArray *contacts)
{
  if (contacts->len)
  {
    g_ptr_array_add(contacts, NULL);
    g_signal_emit(roster, sig, detail, contacts->pdata);
  }

  g_ptr_array_free(contacts, FALSE);
}

static void
contacts_added_cb(EBookView *view, GList *vcards, OssoABookRoster *roster)
{
  GPtrArray *contacts = g_ptr_array_new();
  GList *l;

  for (l = vcards; l; l = l->next)
  {
    gchar *vcs = e_vcard_to_string(E_VCARD(l->data), EVC_FORMAT_VCARD_30);
    OssoABookContact *contact = osso_abook_contact_new_from_vcard(
      e_contact_get_const(l->data, E_CONTACT_UID), vcs);

    OSSO_ABOOK_DUMP_VCARD_STRING(EDS, vcs, "adding");
    g_free(vcs);

    if (contact)
    {
      osso_abook_contact_set_roster(contact, roster);
      g_ptr_array_add(contacts, contact);
    }
  }

  contacts_added_or_changed(roster, signals[CONTACTS_ADDED], 0, contacts);
}

static void
contacts_changed_cb(EBookView *view, GList *vcards, OssoABookRoster *roster)
{
  GPtrArray *contacts = g_ptr_array_new();
  GList *l;

  for (l = vcards; l; l = l->next)
  {
    gchar *vcard = e_vcard_to_string(E_VCARD(l->data), EVC_FORMAT_VCARD_30);
    OssoABookContact *contact = osso_abook_contact_new_from_vcard(
      e_contact_get_const(l->data, E_CONTACT_UID), vcard);

    g_free(vcard);

    if (contact)
    {
      osso_abook_contact_set_roster(contact, roster);
      g_ptr_array_add(contacts, contact);
    }
  }

  contacts_added_or_changed(roster, signals[CONTACTS_CHANGED], master,
                            contacts);
}

static void
contacts_removed_cb(EBookView *view, GList *ids, OssoABookRoster *roster)
{
  GPtrArray *contacts = g_ptr_array_new();
  GList *l;

  for (l = ids; l; l = l->next)
    g_ptr_array_add(contacts, l->data);

  g_ptr_array_add(contacts, NULL);
  g_signal_emit(roster, signals[CONTACTS_REMOVED], 0, contacts);
  g_ptr_array_free(contacts, TRUE);
}

static void
sequence_complete_cb(EBookView *view, gint status, OssoABookRoster *roster)
{
  OssoABookRosterPrivate *priv = OSSO_ABOOK_ROSTER_PRIVATE(roster);

  if (!(priv->is_ready))
  {
    priv->is_ready = TRUE;

    if (status)
    {
      GError *error = g_error_new(e_book_error_quark(), 20,
                                  "EBookViewStatus=%d", status);

      osso_abook_waitable_notify(OSSO_ABOOK_WAITABLE(roster), error);

      if (error)
        g_error_free(error);
    }
    else
      osso_abook_waitable_notify(OSSO_ABOOK_WAITABLE(roster), NULL);
  }

  g_signal_emit(roster, signals[SEQUENCE_COMPLETE], 0, status);
}

static void
status_message_cb(EBookView *view, gchar *message, OssoABookRoster *roster)
{
  g_signal_emit(roster, signals[STATUS_MESSAGE], 0, message);
}

static void
backend_died_cb(OssoABookRoster *roster)
{
  OssoABookRosterPrivate *priv = OSSO_ABOOK_ROSTER_PRIVATE(roster);

  priv->backend_died = TRUE;
  g_object_set(roster, "book-view", NULL, NULL);
}

static void
osso_abook_roster_set_book_view(OssoABookRoster *roster, EBookView *book_view)
{
  OssoABookRosterPrivate *priv = OSSO_ABOOK_ROSTER_PRIVATE(roster);
  OssoABookRosterClass *roster_class = OSSO_ABOOK_ROSTER_GET_CLASS(roster);

  if (book_view)
    g_object_ref(book_view);

  if (priv->book_view)
  {
    EBook *book = e_book_view_get_book(priv->book_view);

    if (!(priv->backend_died))
      e_book_view_stop(priv->book_view);

    g_signal_handlers_disconnect_matched(priv->book_view, G_SIGNAL_MATCH_DATA,
                                         0, 0, 0, 0, roster);
    g_signal_handlers_disconnect_matched(book, G_SIGNAL_MATCH_DATA, 0, 0, 0, 0,
                                         roster);
    g_object_unref(priv->book_view);
  }

  if (priv->book)
  {
    g_object_unref(priv->book);
    priv->book = NULL;
  }

  priv->book_view = book_view;

  if (priv->book_view)
  {
/*
    not in upstream EDS

    e_book_view_set_parse_vcards(priv->book_view, FALSE);
 */
    _osso_abook_book_view_set_freezable(priv->book_view, TRUE);
    g_signal_connect(priv->book_view, "contacts-added",
                     G_CALLBACK(contacts_added_cb), roster);
    g_signal_connect(priv->book_view, "contacts-changed",
                     G_CALLBACK(contacts_changed_cb), roster);
    g_signal_connect(priv->book_view, "contacts-removed",
                     G_CALLBACK(contacts_removed_cb), roster);
    g_signal_connect(priv->book_view, "sequence-complete",
                     G_CALLBACK(sequence_complete_cb), roster);
    g_signal_connect(priv->book_view, "status-message",
                     G_CALLBACK(status_message_cb), roster);

    g_signal_connect_swapped(e_book_view_get_book(priv->book_view),
                             "backend-died", G_CALLBACK(backend_died_cb),
                             roster);

    if (priv->is_running)
      osso_abook_roster_real_start(roster);
  }

  if (roster_class->set_book_view)
    roster_class->set_book_view(roster, book_view);

  g_object_notify(G_OBJECT(roster), "book");
  g_object_notify(G_OBJECT(roster), "book-uri");
}

EBook *
osso_abook_roster_get_book(OssoABookRoster *roster)
{
  OssoABookRosterPrivate *priv;
  EBook *book;

  g_return_val_if_fail(OSSO_ABOOK_IS_ROSTER(roster), NULL);

  priv = OSSO_ABOOK_ROSTER_PRIVATE(roster);

  if (priv->book_view)
    book = e_book_view_get_book(priv->book_view);
  else
    book = priv->book;

  return book;
}

const char *
osso_abook_roster_get_name(OssoABookRoster *roster)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_ROSTER(roster), NULL);

  return OSSO_ABOOK_ROSTER_PRIVATE(roster)->name;
}

OssoABookNameOrder
osso_abook_roster_get_name_order(OssoABookRoster *roster)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_ROSTER(roster),
                       OSSO_ABOOK_NAME_ORDER_FIRST);

  return OSSO_ABOOK_ROSTER_PRIVATE(roster)->name_order;
}

const char *
osso_abook_roster_get_vcard_field(OssoABookRoster *roster)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_ROSTER(roster), NULL);

  return OSSO_ABOOK_ROSTER_PRIVATE(roster)->vcard_field;
}

EBookView *
osso_abook_roster_get_book_view(OssoABookRoster *roster)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_ROSTER(roster), NULL);

  return OSSO_ABOOK_ROSTER_PRIVATE(roster)->book_view;
}

const char *
osso_abook_roster_get_book_uri(OssoABookRoster *roster)
{
  EBook *book;

  g_return_val_if_fail(OSSO_ABOOK_IS_ROSTER(roster), NULL);

  book = osso_abook_roster_get_book(roster);

  if (book)
  {
    ESource *source = e_book_get_source(book);

    if (source)
      return e_source_get_uid(source);
  }

  return NULL;
}

gboolean
osso_abook_roster_is_running(OssoABookRoster *roster)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_ROSTER(roster), FALSE);

  return OSSO_ABOOK_ROSTER_PRIVATE(roster)->is_running;
}

void
osso_abook_roster_set_name_order(OssoABookRoster *roster,
                                 OssoABookNameOrder order)
{
  g_return_if_fail(OSSO_ABOOK_IS_ROSTER(roster));

  g_object_set(roster, "name-order", order, NULL);
}

void
osso_abook_roster_stop(OssoABookRoster *roster)
{
  OssoABookRosterPrivate *priv;

  g_return_if_fail(OSSO_ABOOK_IS_ROSTER(roster));

  priv = OSSO_ABOOK_ROSTER_PRIVATE(roster);
  g_object_freeze_notify(G_OBJECT(roster));

  if (priv->is_running)
  {
    priv->is_running = FALSE;
    g_object_notify(G_OBJECT(roster), "running");
  }

  OSSO_ABOOK_ROSTER_GET_CLASS(roster)->stop(roster);
  g_object_thaw_notify(G_OBJECT(roster));
}

void
osso_abook_roster_start(OssoABookRoster *roster)
{
  OssoABookRosterPrivate *priv;

  g_return_if_fail(OSSO_ABOOK_IS_ROSTER(roster));

  priv = OSSO_ABOOK_ROSTER_PRIVATE(roster);

  g_object_freeze_notify(G_OBJECT(roster));
  OSSO_ABOOK_ROSTER_GET_CLASS(roster)->start(roster);

  if (!priv->is_running)
  {
    priv->is_running = TRUE;
    g_object_notify(G_OBJECT(roster), "running");
  }

  g_object_thaw_notify(G_OBJECT(roster));
}

TpAccount *
osso_abook_roster_get_account(OssoABookRoster *roster)
{
  TpAccount *account = NULL;
  gchar *name;

  g_return_val_if_fail(OSSO_ABOOK_IS_ROSTER(roster), NULL);

  name = OSSO_ABOOK_ROSTER_PRIVATE(roster)->name;

  if (name)
    account = osso_abook_account_manager_lookup_by_name(NULL, name);

  return account;
}

OssoABookRoster *
osso_abook_roster_new(const char *name, EBookView *book_view,
                      const char *vcard_field)
{
  g_return_val_if_fail(NULL != name, NULL);
  g_return_val_if_fail(NULL != vcard_field, NULL);
  g_return_val_if_fail(!book_view || E_IS_BOOK_VIEW(book_view), NULL);

  return g_object_new(OSSO_ABOOK_TYPE_ROSTER,
                      "name", name,
                      "book-view", book_view,
                      "vcard-field", vcard_field,
                      NULL);
}

OssoABookCapsFlags
osso_abook_roster_get_capabilities(OssoABookRoster *roster)
{
  TpAccount *account;

  g_return_val_if_fail(OSSO_ABOOK_IS_ROSTER(roster), OSSO_ABOOK_CAPS_NONE);

  account = osso_abook_roster_get_account(roster);

  if (account)
    return osso_abook_caps_from_account(account);

  return OSSO_ABOOK_CAPS_NONE;
}

TpProtocol *
osso_abook_roster_get_protocol(OssoABookRoster *roster)
{
  TpAccount *account;

  g_return_val_if_fail(OSSO_ABOOK_IS_ROSTER(roster), NULL);

  account = osso_abook_roster_get_account(roster);

  if (account)
  {
    TpProtocol *protocol =
      osso_abook_account_manager_get_account_protocol_object(NULL, account);

    return g_object_ref(protocol);
  }

  return NULL;
}
