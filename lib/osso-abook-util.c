#include "config.h"

#include "osso-abook-util.h"
#include "osso-abook-filter-model.h"
#include "osso-abook-utils-private.h"

gboolean
osso_abook_screen_is_landscape_mode(GdkScreen *screen)
{
  g_return_val_if_fail(GDK_IS_SCREEN (screen), TRUE);

  return gdk_screen_get_width(screen) > gdk_screen_get_height(screen);
}

static void
osso_abook_pannable_area_size_request(GtkWidget *widget,
                                      GtkRequisition *requisition,
                                      gpointer user_data)
{
  GtkWidget *child = gtk_bin_get_child(GTK_BIN(widget));

  if (!child)
    return;

  if (gtk_widget_get_sensitive(child))
  {
    GdkScreen *screen = gtk_widget_get_screen(widget);

    if (screen)
    {
      gint screen_height = gdk_screen_get_height(screen);
      GtkRequisition child_requisition;

      gtk_widget_size_request(child, &child_requisition);

      if (screen_height > child_requisition.height + requisition->height)
        requisition->height = child_requisition.height + requisition->height;
      else
        requisition->height = screen_height;
    }
  }
}

GtkWidget *
osso_abook_pannable_area_new(void)
{
  GtkWidget *area = hildon_pannable_area_new();

  g_object_set(area,
               "hscrollbar-policy", GTK_POLICY_NEVER,
               "mov-mode", HILDON_MOVEMENT_MODE_VERT,
               NULL);
  g_signal_connect(area, "size-request",
                   G_CALLBACK(osso_abook_pannable_area_size_request),NULL);

  return area;
}

static gboolean
_live_search_refilter_cb(HildonLiveSearch *live_search)
{
  GtkTreeModelFilter *filter = hildon_live_search_get_filter(live_search);

  g_return_val_if_fail(OSSO_ABOOK_IS_FILTER_MODEL(filter), FALSE);

  osso_abook_filter_model_set_prefix(OSSO_ABOOK_FILTER_MODEL(filter),
                                     hildon_live_search_get_text(live_search));

  return TRUE;
}

GtkWidget *
osso_abook_live_search_new_with_filter(OssoABookFilterModel *filter)
{
  GtkWidget *live_search = hildon_live_search_new();

  hildon_live_search_set_filter(HILDON_LIVE_SEARCH(live_search),
                                GTK_TREE_MODEL_FILTER(filter));
  g_signal_connect(live_search, "refilter",
                   G_CALLBACK(_live_search_refilter_cb), 0);

  return live_search;
}

char *
osso_abook_concat_names(OssoABookNameOrder order, const gchar *primary,
                        const gchar *secondary)
{
  const char *sep;

  if (!primary || !*primary)
    return g_strdup(secondary);

  if (!secondary || !*secondary)
    return g_strdup(primary);

  if (order == OSSO_ABOOK_NAME_ORDER_LAST)
    sep = ", ";
  else
    sep = " ";

  return g_strchomp(g_strchug(g_strconcat(primary, sep, secondary, NULL)));
}

const char *
osso_abook_get_work_dir()
{
  static gchar *work_dir = NULL;
  if ( !work_dir )
    work_dir = g_build_filename(g_get_home_dir(), ".osso-abook", NULL);

  return work_dir;
}

EBook *
osso_abook_system_book_dup_singleton(gboolean open, GError **error)
{
  static EBook *book;
  static gboolean is_opened;

  if (!book )
  {
    ESourceRegistry *registry = e_source_registry_new_sync(NULL, error);
    ESource *source;

    if (!registry)
      return NULL;

    source = e_source_registry_ref_builtin_address_book(registry);
    book = e_book_new(source, error);

    g_object_unref(source);
    g_object_unref(registry);

    if (!book)
      return NULL;

    g_object_add_weak_pointer(G_OBJECT(book), (gpointer *)&book);
    is_opened = FALSE;
  }
  else
    g_object_ref(book);

  if (!open || is_opened)
    return book;

  if (e_book_open(book, FALSE, error))
  {
    is_opened = TRUE;
    return book;
  }

  g_object_unref(book);

  return NULL;
}

gboolean
osso_abook_is_fax_attribute(EVCardAttribute *attribute)
{
  GList *p;
  gboolean is_fax = FALSE;

  g_return_val_if_fail(attribute != NULL, FALSE);

  if (g_strcmp0(e_vcard_attribute_get_name(attribute), "TEL"))
    return FALSE;

  for (p = e_vcard_attribute_get_params(attribute); p; p = p->next)
  {
    if (!g_strcmp0(e_vcard_attribute_param_get_name(p->data), "TYPE"))
    {
      GList *l;

      for (l = e_vcard_attribute_param_get_values(p->data); l; l = l->next)
      {
        if (l->data)
        {
          if (!g_ascii_strcasecmp(l->data, "CELL") ||
              !g_ascii_strcasecmp(l->data, "CAR") ||
              !g_ascii_strcasecmp(l->data, "VOICE"))
          {
            return FALSE;
          }

          if (!g_ascii_strcasecmp(l->data, "FAX"))
            is_fax = TRUE;
        }
      }
    }
  }

  return is_fax;
}

gboolean
osso_abook_is_mobile_attribute(EVCardAttribute *attribute)
{
  GList *p;

  g_return_val_if_fail(attribute != NULL, FALSE);

  if (g_strcmp0(e_vcard_attribute_get_name(attribute), "TEL"))
    return FALSE;

  for (p = e_vcard_attribute_get_params(attribute); p; p = p->next)
  {
    if (!g_strcmp0(e_vcard_attribute_param_get_name(p->data), "TYPE"))
    {
      GList *v;

      for (v = e_vcard_attribute_param_get_values(p->data); v; v = v->next)
      {
        if (v->data)
        {
          if (!g_ascii_strcasecmp(v->data, "CELL") ||
              !g_ascii_strcasecmp(v->data, "CAR"))
          {
            return TRUE;
          }
        }
      }
    }
  }

  return FALSE;
}

const gchar *
osso_abook_tp_account_get_bound_name(TpAccount *account)
{
  const gchar *bound_name;

  g_return_val_if_fail(TP_IS_ACCOUNT (account), NULL);

  bound_name = tp_account_get_normalized_name(account);

  if (IS_EMPTY(bound_name))
  {
    bound_name = tp_asv_get_string(tp_account_get_parameters(account),
                                   "account");
  }

  return bound_name;
}
