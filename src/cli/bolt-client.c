/*
 * Copyright © 2017 Red Hat, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *       Christian J. Kellner <christian@kellner.me>
 */

#include "bolt-client.h"

#include "bolt-device.h"
#include "bolt-error.h"
#include "bolt-names.h"

#include <gio/gio.h>

static void         handle_dbus_device_added (GObject    *self,
                                              GDBusProxy *bus_proxy,
                                              GVariant   *params);
static void         handle_dbus_device_removed (GObject    *self,
                                                GDBusProxy *bus_proxy,
                                                GVariant   *params);
static void         handle_dbus_domain_added (GObject    *self,
                                              GDBusProxy *bus_proxy,
                                              GVariant   *params);
static void         handle_dbus_domain_removed (GObject    *self,
                                                GDBusProxy *bus_proxy,
                                                GVariant   *params);

struct _BoltClient
{
  BoltProxy parent;
};

enum {
  PROP_0,

  /* D-Bus Props */
  PROP_VERSION,
  PROP_PROBING,
  PROP_POLICY,
  PROP_SECURITY,
  PROP_AUTHMODE,
  PROP_POWERSTATE,

  PROP_LAST
};

static GParamSpec *props[PROP_LAST] = {NULL, };

enum {
  SIGNAL_DEVICE_ADDED,
  SIGNAL_DEVICE_REMOVED,
  SIGNAL_DOMAIN_ADDED,
  SIGNAL_DOMAIN_REMOVED,
  SIGNAL_LAST
};

static guint signals[SIGNAL_LAST] = {0};


G_DEFINE_TYPE (BoltClient,
               bolt_client,
               BOLT_TYPE_PROXY);


static void
bolt_client_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  if (bolt_proxy_get_dbus_property (object, pspec, value))
    return;

  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static const BoltProxySignal *
bolt_client_get_dbus_signals (guint *n)
{
  static BoltProxySignal dbus_signals[] = {
    {"DeviceAdded", handle_dbus_device_added},
    {"DeviceRemoved", handle_dbus_device_removed},
    {"DomainAdded", handle_dbus_domain_added},
    {"DomainRemoved", handle_dbus_domain_removed},
  };

  *n = G_N_ELEMENTS (dbus_signals);

  return dbus_signals;
}


static void
bolt_client_class_init (BoltClientClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  BoltProxyClass *proxy_class = BOLT_PROXY_CLASS (klass);

  gobject_class->get_property = bolt_client_get_property;

  proxy_class->get_dbus_signals = bolt_client_get_dbus_signals;

  props[PROP_VERSION]
    = g_param_spec_uint ("version",
                         "Version", NULL,
                         0, G_MAXUINT, 0,
                         G_PARAM_READABLE |
                         G_PARAM_STATIC_NAME);

  props[PROP_PROBING]
    = g_param_spec_boolean ("probing",
                            "Probing", NULL,
                            FALSE,
                            G_PARAM_READABLE |
                            G_PARAM_STATIC_NAME);

  props[PROP_POLICY]
    = g_param_spec_enum ("default-policy",
                         "DefaultPolicy", NULL,
                         BOLT_TYPE_POLICY,
                         BOLT_POLICY_UNKNOWN,
                         G_PARAM_READABLE |
                         G_PARAM_STATIC_NAME);

  props[PROP_SECURITY]
    = g_param_spec_enum ("security-level",
                         "SecurityLevel", NULL,
                         BOLT_TYPE_SECURITY,
                         BOLT_SECURITY_UNKNOWN,
                         G_PARAM_READABLE |
                         G_PARAM_STATIC_NAME);

  props[PROP_AUTHMODE] =
    g_param_spec_flags ("auth-mode", "AuthMode", NULL,
                        BOLT_TYPE_AUTH_MODE,
                        BOLT_AUTH_ENABLED,
                        G_PARAM_READABLE |
                        G_PARAM_STATIC_STRINGS);

  props[PROP_POWERSTATE] =
    g_param_spec_enum ("power-state",
                       "PowerState", NULL,
                       BOLT_TYPE_POWER_STATE,
                       BOLT_FORCE_POWER_UNSET,
                       G_PARAM_READABLE |
                       G_PARAM_STATIC_NAME);

  g_object_class_install_properties (gobject_class,
                                     PROP_LAST,
                                     props);

  /* signals */
  signals[SIGNAL_DEVICE_ADDED] =
    g_signal_new ("device-added",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  1, G_TYPE_STRING);

  signals[SIGNAL_DEVICE_REMOVED] =
    g_signal_new ("device-removed",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  1, G_TYPE_STRING);

  signals[SIGNAL_DOMAIN_ADDED] =
    g_signal_new ("domain-added",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  1, G_TYPE_STRING);

  signals[SIGNAL_DOMAIN_REMOVED] =
    g_signal_new ("domain-removed",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  1, G_TYPE_STRING);
}


static void
bolt_client_init (BoltClient *cli)
{
  /* we don't want this to be optimized away */
  volatile GQuark error_domain;

  /* initialize error mapping */
  error_domain = BOLT_ERROR;
  (error_domain); /* To avoid -Wunused-but-set-variable */

}

/* dbus signals */

static void
handle_dbus_device_added (GObject *self, GDBusProxy *bus_proxy, GVariant *params)
{
  BoltClient *cli = BOLT_CLIENT (self);
  const char *opath = NULL;

  g_variant_get_child (params, 0, "&o", &opath);
  g_signal_emit (cli, signals[SIGNAL_DEVICE_ADDED], 0, opath);
}

static void
handle_dbus_device_removed (GObject *self, GDBusProxy *bus_proxy, GVariant *params)
{
  BoltClient *cli = BOLT_CLIENT (self);
  const char *opath = NULL;

  g_variant_get_child (params, 0, "&o", &opath);
  g_signal_emit (cli, signals[SIGNAL_DEVICE_REMOVED], 0, opath);
}

static void
handle_dbus_domain_added (GObject *self, GDBusProxy *bus_proxy, GVariant *params)
{
  BoltClient *cli = BOLT_CLIENT (self);
  const char *opath = NULL;

  g_variant_get_child (params, 0, "&o", &opath);
  g_signal_emit (cli, signals[SIGNAL_DOMAIN_ADDED], 0, opath);
}

static void
handle_dbus_domain_removed (GObject *self, GDBusProxy *bus_proxy, GVariant *params)
{
  BoltClient *cli = BOLT_CLIENT (self);
  const char *opath = NULL;

  g_variant_get_child (params, 0, "&o", &opath);
  g_signal_emit (cli, signals[SIGNAL_DOMAIN_REMOVED], 0, opath);
}


/* public methods */

BoltClient *
bolt_client_new (GError **error)
{
  BoltClient *cli;
  GDBusConnection *bus;

  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, error);
  if (bus == NULL)
    {
      g_prefix_error (error, "Error connecting to D-Bus: ");
      return FALSE;
    }

  cli = g_initable_new (BOLT_TYPE_CLIENT,
                        NULL, error,
                        "g-flags", G_DBUS_PROXY_FLAGS_NONE,
                        "g-connection", bus,
                        "g-name", BOLT_DBUS_NAME,
                        "g-object-path", BOLT_DBUS_PATH,
                        "g-interface-name", BOLT_DBUS_INTERFACE,
                        NULL);

  g_object_unref (bus);

  return cli;
}

static void
got_the_client (GObject      *source,
                GAsyncResult *res,
                gpointer      user_data)
{
  g_autoptr(GError) error = NULL;
  GTask *task = user_data;
  GObject *obj;

  obj = g_async_initable_new_finish (G_ASYNC_INITABLE (source), res, &error);

  if (obj == NULL)
    {
      g_task_return_error (task, error);
      return;
    }

  g_task_return_pointer (task, obj, g_object_unref);
  g_object_unref (task);
}

static void
got_the_bus (GObject      *source,
             GAsyncResult *res,
             gpointer      user_data)
{
  g_autoptr(GError) error = NULL;
  GTask *task = user_data;
  GCancellable *cancellable;
  GDBusConnection *bus;

  bus = g_bus_get_finish (res, &error);
  if (bus == NULL)
    {
      g_prefix_error (&error, "could not connect to D-Bus: ");
      g_task_return_error (task, error);
      return;
    }

  cancellable = g_task_get_cancellable (task);
  g_async_initable_new_async (BOLT_TYPE_CLIENT,
                              G_PRIORITY_DEFAULT,
                              cancellable,
                              got_the_client, task,
                              "g-flags", G_DBUS_PROXY_FLAGS_NONE,
                              "g-connection", bus,
                              "g-name", BOLT_DBUS_NAME,
                              "g-object-path", BOLT_DBUS_PATH,
                              "g-interface-name", BOLT_DBUS_INTERFACE,
                              NULL);
  g_object_unref (bus);
}

void
bolt_client_new_async (GCancellable       *cancellable,
                       GAsyncReadyCallback callback,
                       gpointer            user_data)
{
  GTask *task;

  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (callback == NULL);

  task = g_task_new (NULL, cancellable, callback, user_data);
  g_bus_get (G_BUS_TYPE_SYSTEM, cancellable, got_the_bus, task);
}

BoltClient *
bolt_client_new_finish (GAsyncResult *res,
                        GError      **error)
{
  g_return_val_if_fail (G_IS_TASK (res), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return g_task_propagate_pointer (G_TASK (res), error);
}

GPtrArray *
bolt_client_list_domains (BoltClient   *client,
                          GCancellable *cancel,
                          GError      **error)
{
  g_autoptr(GVariant) val = NULL;
  g_autoptr(GPtrArray) domains = NULL;
  g_autoptr(GVariantIter) iter = NULL;
  GDBusConnection *bus = NULL;
  const char *d;

  g_return_val_if_fail (BOLT_IS_CLIENT (client), NULL);
  g_return_val_if_fail (!cancel || G_IS_CANCELLABLE (cancel), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  val = g_dbus_proxy_call_sync (G_DBUS_PROXY (client),
                                "ListDomains",
                                NULL,
                                G_DBUS_CALL_FLAGS_NONE,
                                -1,
                                cancel,
                                error);
  if (val == NULL)
    return NULL;

  bus = g_dbus_proxy_get_connection (G_DBUS_PROXY (client));

  domains = g_ptr_array_new_with_free_func (g_object_unref);

  g_variant_get (val, "(ao)", &iter);
  while (g_variant_iter_loop (iter, "&o", &d, NULL))
    {
      BoltDomain *dom;

      dom = bolt_domain_new_for_object_path (bus, d, cancel, error);
      if (dom == NULL)
        return NULL;

      g_ptr_array_add (domains, dom);
    }

  return g_steal_pointer (&domains);
}

GPtrArray *
bolt_client_list_devices (BoltClient   *client,
                          GCancellable *cancel,
                          GError      **error)
{
  g_autoptr(GVariant) val = NULL;
  g_autoptr(GPtrArray) devices = NULL;
  g_autoptr(GVariantIter) iter = NULL;
  GDBusConnection *bus = NULL;
  const char *d;

  g_return_val_if_fail (BOLT_IS_CLIENT (client), NULL);
  g_return_val_if_fail (!cancel || G_IS_CANCELLABLE (cancel), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  val = g_dbus_proxy_call_sync (G_DBUS_PROXY (client),
                                "ListDevices",
                                NULL,
                                G_DBUS_CALL_FLAGS_NONE,
                                -1,
                                cancel,
                                error);
  if (val == NULL)
    return NULL;

  bus = g_dbus_proxy_get_connection (G_DBUS_PROXY (client));

  devices = g_ptr_array_new_with_free_func (g_object_unref);

  g_variant_get (val, "(ao)", &iter);
  while (g_variant_iter_loop (iter, "&o", &d, NULL))
    {
      BoltDevice *dev;

      dev = bolt_device_new_for_object_path (bus, d, cancel, error);
      if (dev == NULL)
        return NULL;

      g_ptr_array_add (devices, dev);
    }

  return g_steal_pointer (&devices);
}

BoltDevice *
bolt_client_get_device (BoltClient   *client,
                        const char   *uid,
                        GCancellable *cancel,
                        GError      **error)
{
  g_autoptr(GVariant) val = NULL;
  g_autoptr(GError) err = NULL;
  BoltDevice *dev = NULL;
  GDBusConnection *bus = NULL;
  const char *opath = NULL;

  g_return_val_if_fail (BOLT_IS_CLIENT (client), NULL);
  g_return_val_if_fail (uid != NULL, NULL);
  g_return_val_if_fail (!cancel || G_IS_CANCELLABLE (cancel), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  val = g_dbus_proxy_call_sync (G_DBUS_PROXY (client),
                                "DeviceByUid",
                                g_variant_new ("(s)", uid),
                                G_DBUS_CALL_FLAGS_NONE,
                                -1,
                                cancel,
                                &err);

  if (val == NULL)
    {
      bolt_error_propagate_stripped (error, &err);
      return NULL;
    }

  bus = g_dbus_proxy_get_connection (G_DBUS_PROXY (client));
  g_variant_get (val, "(&o)", &opath);

  if (opath == NULL)
    return NULL;

  dev = bolt_device_new_for_object_path (bus, opath, cancel, error);
  return dev;
}

BoltDevice *
bolt_client_enroll_device (BoltClient  *client,
                           const char  *uid,
                           BoltPolicy   policy,
                           BoltAuthCtrl flags,
                           GError     **error)
{
  g_autoptr(GVariant) val = NULL;
  g_autoptr(GError) err = NULL;
  g_autofree char *fstr = NULL;
  BoltDevice *dev = NULL;
  GDBusConnection *bus = NULL;
  GVariant *params = NULL;
  const char *opath = NULL;
  const char *pstr;

  g_return_val_if_fail (BOLT_IS_CLIENT (client), NULL);
  g_return_val_if_fail (uid != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  pstr = bolt_enum_to_string (BOLT_TYPE_POLICY, policy, error);
  if (pstr == NULL)
    return NULL;

  fstr = bolt_flags_to_string (BOLT_TYPE_AUTH_CTRL, flags, error);
  if (fstr == NULL)
    return NULL;

  params = g_variant_new ("(sss)", uid, pstr, fstr);
  val = g_dbus_proxy_call_sync (G_DBUS_PROXY (client),
                                "EnrollDevice",
                                params,
                                G_DBUS_CALL_FLAGS_NONE,
                                -1,
                                NULL,
                                &err);

  if (val == NULL)
    {
      bolt_error_propagate_stripped (error, &err);
      return NULL;
    }

  bus = g_dbus_proxy_get_connection (G_DBUS_PROXY (client));
  g_variant_get (val, "(&o)", &opath);

  if (opath == NULL)
    return NULL;

  dev = bolt_device_new_for_object_path (bus, opath, NULL, error);
  return dev;
}

void
bolt_client_enroll_device_async (BoltClient         *client,
                                 const char         *uid,
                                 BoltPolicy          policy,
                                 BoltAuthCtrl        flags,
                                 GCancellable       *cancellable,
                                 GAsyncReadyCallback callback,
                                 gpointer            user_data)
{
  g_autofree char *fstr = NULL;
  GError *err = NULL;
  GVariant *params;
  const char *pstr;

  g_return_if_fail (BOLT_IS_CLIENT (client));
  g_return_if_fail (uid != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (callback == NULL);

  pstr = bolt_enum_to_string (BOLT_TYPE_POLICY, policy, &err);
  if (pstr == NULL)
    {
      g_task_report_error (client, callback, user_data, NULL, err);
      return;
    }

  fstr = bolt_flags_to_string (BOLT_TYPE_AUTH_CTRL, flags, &err);
  if (fstr == NULL)
    {
      g_task_report_error (client, callback, user_data, NULL, err);
      return;
    }

  params = g_variant_new ("(sss)", uid, pstr, fstr);
  g_dbus_proxy_call (G_DBUS_PROXY (client),
                     "EnrollDevice",
                     params,
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     cancellable,
                     callback,
                     user_data);
}

gboolean
bolt_client_enroll_device_finish (BoltClient   *client,
                                  GAsyncResult *res,
                                  char        **path,
                                  GError      **error)
{
  g_autoptr(GError) err = NULL;
  GVariant *val = NULL;

  g_return_val_if_fail (BOLT_IS_CLIENT (client), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (res), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  val = g_dbus_proxy_call_finish (G_DBUS_PROXY (client), res, &err);
  if (val == NULL)
    {
      bolt_error_propagate_stripped (error, &err);
      return FALSE;
    }

  if (path != NULL)
    g_variant_get (val, "(o)", path);

  return TRUE;
}

gboolean
bolt_client_forget_device (BoltClient *client,
                           const char *uid,
                           GError    **error)
{
  g_autoptr(GVariant) val = NULL;
  g_autoptr(GError) err = NULL;

  g_return_val_if_fail (BOLT_IS_CLIENT (client), FALSE);
  g_return_val_if_fail (uid != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  val = g_dbus_proxy_call_sync (G_DBUS_PROXY (client),
                                "ForgetDevice",
                                g_variant_new ("(s)", uid),
                                G_DBUS_CALL_FLAGS_NONE,
                                -1,
                                NULL,
                                &err);

  if (val == NULL)
    {
      bolt_error_propagate_stripped (error, &err);
      return FALSE;
    }

  return TRUE;
}

void
bolt_client_forget_device_async (BoltClient         *client,
                                 const char         *uid,
                                 GCancellable       *cancellable,
                                 GAsyncReadyCallback callback,
                                 gpointer            user_data)
{
  g_return_if_fail (BOLT_IS_CLIENT (client));
  g_return_if_fail (uid == NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (callback == NULL);

  g_dbus_proxy_call (G_DBUS_PROXY (client),
                     "ForgetDevice",
                     g_variant_new ("(s)", uid),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     cancellable,
                     callback,
                     user_data);
}

gboolean
bolt_client_forget_device_finish (BoltClient   *client,
                                  GAsyncResult *res,
                                  GError      **error)
{
  g_autoptr(GVariant) val = NULL;
  g_autoptr(GError) err = NULL;

  g_return_val_if_fail (BOLT_IS_CLIENT (client), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (res), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  val = g_dbus_proxy_call_finish (G_DBUS_PROXY (client), res, &err);
  if (val == NULL)
    {
      bolt_error_propagate_stripped (error, &err);
      return FALSE;
    }

  return TRUE;
}

BoltPower *
bolt_client_new_power_client (BoltClient   *client,
                              GCancellable *cancellable,
                              GError      **error)
{
  GDBusConnection *bus = NULL;

  g_return_val_if_fail (BOLT_IS_CLIENT (client), NULL);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  bus = g_dbus_proxy_get_connection (G_DBUS_PROXY (client));
  return bolt_power_new_for_object_path (bus, cancellable, error);
}

/* getter */
guint
bolt_client_get_version (BoltClient *client)
{
  const char *key;
  guint val = 0;
  gboolean ok;

  g_return_val_if_fail (BOLT_IS_CLIENT (client), val);

  key = g_param_spec_get_name (props[PROP_VERSION]);
  ok = bolt_proxy_get_property_uint32 (BOLT_PROXY (client), key, &val);

  if (!ok)
    g_warning ("failed to get property '%s'", key);

  return val;
}

gboolean
bolt_client_is_probing (BoltClient *client)
{
  const char *key;
  gboolean val = FALSE;
  gboolean ok;

  g_return_val_if_fail (BOLT_IS_CLIENT (client), val);

  key = g_param_spec_get_name (props[PROP_PROBING]);
  ok = bolt_proxy_get_property_bool (BOLT_PROXY (client), key, &val);

  if (!ok)
    g_warning ("failed to get enum property '%s'", key);

  return val;
}

BoltPolicy
bolt_client_get_policy (BoltClient *client)
{
  const char *key;
  gboolean ok;
  gint val = BOLT_POLICY_UNKNOWN;

  g_return_val_if_fail (BOLT_IS_CLIENT (client), val);

  key = g_param_spec_get_name (props[PROP_POLICY]);
  ok = bolt_proxy_get_property_enum (BOLT_PROXY (client), key, &val);

  if (!ok)
    g_warning ("failed to get enum property '%s'", key);

  return val;
}

BoltSecurity
bolt_client_get_security (BoltClient *client)
{
  const char *key;
  gboolean ok;
  gint val = BOLT_SECURITY_UNKNOWN;

  g_return_val_if_fail (BOLT_IS_CLIENT (client), val);

  key = g_param_spec_get_name (props[PROP_SECURITY]);
  ok = bolt_proxy_get_property_enum (BOLT_PROXY (client), key, &val);

  if (!ok)
    g_warning ("failed to get enum property '%s'", key);

  return val;
}

BoltAuthMode
bolt_client_get_authmode (BoltClient *client)
{
  const char *key;
  gboolean ok;
  guint val = BOLT_AUTH_DISABLED;

  g_return_val_if_fail (BOLT_IS_CLIENT (client), val);

  key = g_param_spec_get_name (props[PROP_AUTHMODE]);
  ok = bolt_proxy_get_property_flags (BOLT_PROXY (client), key, &val);

  if (!ok)
    g_warning ("failed to get enum property '%s'", key);

  return val;
}

BoltPowerState
bolt_client_get_power_state (BoltClient *client)
{
  const char *key;
  gboolean ok;
  gint val = BOLT_FORCE_POWER_UNSET;

  g_return_val_if_fail (BOLT_IS_CLIENT (client), val);

  key = g_param_spec_get_name (props[PROP_POWERSTATE]);
  ok = bolt_proxy_get_property_enum (BOLT_PROXY (client), key, &val);

  if (!ok)
    g_warning ("failed to get enum property '%s'", key);

  return val;
}

void
bolt_client_set_authmode_async (BoltClient         *client,
                                BoltAuthMode        mode,
                                GCancellable       *cancellable,
                                GAsyncReadyCallback callback,
                                gpointer            user_data)
{
  g_autofree char *str = NULL;
  GError *err = NULL;
  GParamSpec *pspec;
  GParamSpecFlags *flags_pspec;
  GFlagsClass *flags_class;

  g_return_if_fail (BOLT_IS_CLIENT (client));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (callback != NULL);

  pspec = props[PROP_AUTHMODE];
  flags_pspec = G_PARAM_SPEC_FLAGS (pspec);
  flags_class = flags_pspec->flags_class;
  str = bolt_flags_class_to_string (flags_class, mode, &err);

  if (str == NULL)
    {
      g_task_report_error (client, callback, user_data, NULL, err);
      return;
    }

  bolt_proxy_set_property_async (BOLT_PROXY (client),
                                 g_param_spec_get_nick (pspec),
                                 g_variant_new ("s", str),
                                 cancellable,
                                 callback,
                                 user_data);
}

gboolean
bolt_client_set_authmode_finish (BoltClient   *client,
                                 GAsyncResult *res,
                                 GError      **error)
{
  g_return_val_if_fail (BOLT_IS_CLIENT (client), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (res), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return bolt_proxy_set_property_finish (res, error);
}

/* utility functions */
static gint
device_sort_by_syspath (gconstpointer ap,
                        gconstpointer bp,
                        gpointer      data)
{
  BoltDevice *a = BOLT_DEVICE (*((BoltDevice **) ap));
  BoltDevice *b = BOLT_DEVICE (*((BoltDevice **) bp));
  gint sort_order = GPOINTER_TO_INT (data);
  const char *pa;
  const char *pb;

  pa = bolt_device_get_syspath (a);
  pb = bolt_device_get_syspath (b);

  return sort_order * g_strcmp0 (pa, pb);
}

void
bolt_devices_sort_by_syspath (GPtrArray *devices,
                              gboolean   reverse)
{
  gpointer sort_order = GINT_TO_POINTER (reverse ? -1 : 1);

  if (devices == NULL)
    return;

  g_ptr_array_sort_with_data (devices,
                              device_sort_by_syspath,
                              sort_order);
}
