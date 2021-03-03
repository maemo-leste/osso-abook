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

/* move to EDS if needed */
/**
 * e_normalize_phone_number:
 * @phone_number: the phone number
 *
 * Normalizes @phone_number. Comma, period, parentheses, hyphen, space,
 * tab and forward-slash characters are stripped. The characters 'p',
 * 'w' and 'x' are copied in uppercase. A plus only is valid at the
 * beginning, or after a number suppression prefix ("*31#", "#31#"). All
 * other characters are copied verbatim.
 *
 * Returns: A newly allocated string containing the normalized phone
 * number.
 */
static char *
e_normalize_phone_number (const char *phone_number)
{
  GString *result;
  const char *p;

  /* see cui_utils_normalize_number() of rtcom-call-ui for reference */
  g_return_val_if_fail (NULL != phone_number, NULL);

  result = g_string_new (NULL);

  for (p = phone_number; *p; ++p) {
    switch (*p) {
      case 'p': case 'P':
        /* Normalize this characters to P -
                         * pause for one second. */
        g_string_append_c (result, 'P');
        break;

      case 'w': case 'W':
        /* Normalize this characters to W -
                         * wait until confirmation. */
        g_string_append_c (result, 'W');
        break;

      case 'x': case 'X':
        /* Normalize this characters to X -
                         * alias for 'W' it seems */
        g_string_append_c (result, 'X');
        break;

      case '+':
        /* Plus only is valid on begin of phone numbers and
                         * after number suppression prefix */
        if (0 == result->len ||
            strcmp (result->str, "*31#") ||
            strcmp (result->str, "#31#"))
          g_string_append_c (result, *p);

        break;

      case ',': case '.': case '(': case ')':
      case '-': case ' ': case '\t': case '/':
        /* Skip commonly used delimiters */
        break;

      case '#': case '*':
      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
      default:
        /* Directly accept all other characters. */
        g_string_append_c (result, *p);
        break;

    }
  }

  return g_string_free (result, FALSE);
}

EBookQuery *
osso_abook_query_phone_number(const char *phone_number, gboolean fuzzy_match)
{
  gsize len;
  EBookQuery *query;
  gchar *phone_num = NULL;
  int nqs = 1;
  EBookQuery *qs[2];

  g_return_val_if_fail(!IS_EMPTY(phone_number), NULL);

  qs[0] = e_book_query_vcard_field_test("TEL", E_BOOK_QUERY_IS, phone_number);
  len = strcspn(phone_number, OSSO_ABOOK_DTMF_CHARS);

  if (len < strlen(phone_number) )
  {
    phone_num = g_strndup(phone_number, len);

    if (phone_num)
    {
      qs[1] = e_book_query_vcard_field_test("TEL", E_BOOK_QUERY_IS, phone_num);
      nqs = 2;
    }
  }

  if (fuzzy_match)
  {
    EBookQueryTest test;
    char *normalized;
    size_t normalized_len;
    const char *phone_num_norm;

    if (phone_num)
      phone_number = phone_num;

    normalized = e_normalize_phone_number(phone_number);
    normalized_len = strlen(normalized);

    if (normalized_len <= 6)
    {
      phone_num_norm = normalized;
      test = E_BOOK_QUERY_IS;
    }
    else
    {
      phone_num_norm = &normalized[normalized_len - 7];
      test = E_BOOK_QUERY_ENDS_WITH;
    }

    qs[nqs++] = e_book_query_vcard_field_test("TEL", test, phone_num_norm);
    g_free(normalized);
  }

  g_free(phone_num);

  if (nqs == 1)
    query = qs[0];
  else
    query = e_book_query_or(nqs, qs, TRUE);

  return query;
}

struct phone_sort_data
{
  OssoABookContact *contact;
  int weigth;
};

static gint
sort_phone_number_matches(struct phone_sort_data *a, struct phone_sort_data *b)
{
  int w1 = a->weigth;
  int w2 = b->weigth;

  if ( w1 > w2 )
    return -1;

  if (w1 < w2)
    return 1;

  return osso_abook_contact_uid_compare(a->contact, b->contact);
}

GList *
osso_abook_sort_phone_number_matches(GList *matches, const char *phone_number)
{
  char *norm_phone_num;
  GList *l;
  char *norm_phone_num_unprefixed;

  g_return_val_if_fail(NULL != phone_number, matches);
  g_return_val_if_fail(0 != *phone_number, matches);

  norm_phone_num = e_normalize_phone_number(phone_number);
  norm_phone_num_unprefixed =
      &norm_phone_num[strcspn(norm_phone_num, OSSO_ABOOK_DTMF_CHARS) - 1];

  for (l = matches; l; l = l->next)
  {
    struct phone_sort_data *data = g_slice_new0(struct phone_sort_data);
    const char *val = NULL;
    GList *attr;
    gboolean a, b;
    char *p, *q;
    char *norm_val;
    size_t norm_val_len;
    char *norm_val_unprefixed;

    data->contact = l->data;
    l->data = data;

    for (attr = e_vcard_get_attributes(E_VCARD(data->contact)); attr;
                                       attr = attr->next)
    {
      if (!strcmp(e_vcard_attribute_get_name(attr->data), "TEL"))
      {
        GList *vals = e_vcard_attribute_get_values(attr->data);

        if (vals)
        {
          val = vals->data;

          if (val && *val)
            break;
        }
      }
    }

    if (!val || !*val)
      continue;

    if (!strcmp(val, phone_number))
    {
      data->weigth = G_MAXINT;
      break;
    }

    norm_val = e_normalize_phone_number(val);
    norm_val_len = strcspn(norm_val, OSSO_ABOOK_DTMF_CHARS);
    p = norm_phone_num_unprefixed;
    norm_val_unprefixed = &norm_val[norm_val_len - 1];
    q = &norm_val[norm_val_len - 1];

    while (1)
    {
      a = q >= norm_val;
      b = p >= norm_phone_num;

      if (b)
        b = q >= norm_val;

      if (!b)
        break;

      if (*(p-- - 1) != *(q-- - 1))
      {
        a = norm_val <= q;
        break;
      }
    }

    if (p < norm_phone_num)
      a = TRUE;

    if (a)
    {
      gint w1 = data->weigth;
      gint w2 = norm_val_unprefixed - q - 1;

      if (w1 < w2)
        data->weigth = w2;
      else
        data->weigth = w1;
    }

    g_free(norm_val);
  }

  g_free(norm_phone_num);
  matches = g_list_sort(matches, (GCompareFunc)sort_phone_number_matches);

  for (l = matches; l; l = l->next)
  {
    struct phone_sort_data *data = l->data;

    l->data = data->contact;
    g_slice_free(struct phone_sort_data, data);
  }

  return matches;
}
