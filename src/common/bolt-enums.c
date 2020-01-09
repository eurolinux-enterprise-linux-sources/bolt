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

#include "config.h"

#include "bolt-enums.h"
#include "bolt-error.h"

#include <gio/gio.h>

#if !GLIB_CHECK_VERSION (2, 57, 0)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (GEnumClass, g_type_class_unref);
G_DEFINE_AUTOPTR_CLEANUP_FUNC (GFlagsClass, g_type_class_unref);
#endif

gboolean
bolt_enum_class_validate (GEnumClass *enum_class,
                          gint        value,
                          GError    **error)
{
  const char *name;
  gboolean oob;

  g_return_val_if_fail (G_IS_ENUM_CLASS (enum_class), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  oob = value < enum_class->minimum || value > enum_class->maximum;

  if (oob)
    {
      name = g_type_name_from_class ((GTypeClass *) enum_class);
      g_set_error (error,  G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
                   "enum value '%d' is out of bounds for '%s'",
                   value, name);
      return FALSE;
    }

  return TRUE;
}

gboolean
bolt_enum_validate (GType    enum_type,
                    gint     value,
                    GError **error)
{
  g_autoptr(GEnumClass) klass = NULL;

  g_return_val_if_fail (G_TYPE_IS_ENUM (enum_type), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  klass = g_type_class_ref (enum_type);

  return bolt_enum_class_validate (klass, value, error);
}

const char *
bolt_enum_class_to_string (GEnumClass *klass,
                           gint        value,
                           GError    **error)
{
  GEnumValue *ev;

  g_return_val_if_fail (G_IS_ENUM_CLASS (klass), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  if (!bolt_enum_class_validate (klass, value, error))
    return NULL;

  ev = g_enum_get_value (klass, value);
  return ev->value_nick;
}


gboolean
bolt_enum_class_from_string (GEnumClass *klass,
                             const char *string,
                             gint       *enum_out,
                             GError    **error)
{
  const char *name;
  GEnumValue *ev;

  g_return_val_if_fail (G_IS_ENUM_CLASS (klass), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if (string == NULL)
    {
      name = g_type_name_from_class ((GTypeClass *) klass);
      g_set_error (error, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
                   "empty string passed for enum class for '%s'",
                   name);
      return FALSE;
    }

  ev = g_enum_get_value_by_nick (klass, string);

  if (ev == NULL)
    {
      name = g_type_name_from_class ((GTypeClass *) klass);
      g_set_error (error, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
                   "invalid string '%s' for enum '%s'", string, name);
      return FALSE;
    }

  if (enum_out)
    *enum_out = ev->value;

  return TRUE;
}

const char *
bolt_enum_to_string (GType    enum_type,
                     gint     value,
                     GError **error)
{
  g_autoptr(GEnumClass) klass = NULL;

  g_return_val_if_fail (G_TYPE_IS_ENUM (enum_type), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  klass = g_type_class_ref (enum_type);

  return bolt_enum_class_to_string (klass, value, error);
}

gint
bolt_enum_from_string (GType       enum_type,
                       const char *string,
                       GError    **error)
{
  g_autoptr(GEnumClass) klass = NULL;
  gint iv = -1;

  g_return_val_if_fail (G_TYPE_IS_ENUM (enum_type), -1);
  g_return_val_if_fail (error == NULL || *error == NULL, -1);

  klass = g_type_class_ref (enum_type);

  bolt_enum_class_from_string (klass, string, &iv, error);

  return iv;
}

char *
bolt_flags_class_to_string (GFlagsClass *flags_class,
                            guint        value,
                            GError     **error)
{
  g_autoptr(GString) str = NULL;
  const char *name;
  GFlagsValue *fv;

  g_return_val_if_fail (G_IS_FLAGS_CLASS (flags_class), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  fv = g_flags_get_first_value (flags_class, value);
  if (fv == NULL)
    {
      if (value == 0)
        return g_strdup ("");

      name = g_type_name_from_class ((GTypeClass *) flags_class);

      g_set_error (error, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
                   "invalid value '%u' for flags '%s'", value, name);
      return NULL;
    }

  value &= ~fv->value;
  str = g_string_new (fv->value_nick);

  while (value != 0 &&
         (fv = g_flags_get_first_value (flags_class, value)) != NULL)
    {
      g_string_append (str, " | ");
      g_string_append (str, fv->value_nick);

      value &= ~fv->value;
    }

  if (value != 0)
    {
      name = g_type_name_from_class ((GTypeClass *) flags_class);

      g_set_error (error, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
                   "unhandled value '%u' for flags '%s'", value, name);
      return NULL;
    }

  return g_string_free (g_steal_pointer (&str), FALSE);
}

gboolean
bolt_flags_class_from_string (GFlagsClass *flags_class,
                              const char  *string,
                              guint       *flags_out,
                              GError     **error)
{
  g_auto(GStrv) vals = NULL;
  const char *name;
  guint flags = 0;

  g_return_val_if_fail (G_IS_FLAGS_CLASS (flags_class), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if (string == NULL)
    {
      name = g_type_name_from_class ((GTypeClass *) flags_class);
      g_set_error (error, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
                   "empty string passed for flags class for '%s'",
                   name);
      return FALSE;
    }

  vals = g_strsplit (string, "|", -1);

  for (guint i = 0; vals[i]; i++)
    {
      GFlagsValue *fv;
      char *nick;

      nick = g_strstrip (vals[i]);
      fv = g_flags_get_value_by_nick (flags_class, nick);

      if (fv == NULL)
        {
          name = g_type_name_from_class ((GTypeClass *) flags_class);
          g_set_error (error, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
                       "invalid flag '%s' for flags '%s'", string, name);

          return FALSE;
        }

      flags |= fv->value;
    }

  if (flags_out != NULL)
    *flags_out = flags;

  return TRUE;
}

char *
bolt_flags_to_string (GType    flags_type,
                      guint    value,
                      GError **error)
{
  g_autoptr(GFlagsClass) klass = NULL;

  g_return_val_if_fail (G_TYPE_IS_FLAGS (flags_type), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  klass = g_type_class_ref (flags_type);

  return bolt_flags_class_to_string (klass, value, error);
}

gboolean
bolt_flags_from_string (GType       flags_type,
                        const char *string,
                        guint      *flags_out,
                        GError    **error)
{
  g_autoptr(GFlagsClass) klass = NULL;

  g_return_val_if_fail (G_TYPE_IS_FLAGS (flags_type), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  klass = g_type_class_ref (flags_type);

  return bolt_flags_class_from_string (klass, string, flags_out, error);
}

gboolean
bolt_flags_update (guint  from,
                   guint *to,
                   guint  mask)
{
  guint val;
  gboolean chg;

  g_return_val_if_fail (to != NULL, FALSE);

  val = *to & ~mask;          /* clear all bits in mask */
  val = val | (from & mask);  /* set all bits in from and mask */
  chg = *to != val;
  *to = val;

  return chg;
}

const char *
bolt_status_to_string (BoltStatus status)
{
  return bolt_enum_to_string (BOLT_TYPE_STATUS, status, NULL);
}

gboolean
bolt_status_is_authorized (BoltStatus status)
{
  return status == BOLT_STATUS_AUTHORIZED ||
         status == BOLT_STATUS_AUTHORIZED_SECURE ||
         status == BOLT_STATUS_AUTHORIZED_NEWKEY;
}

gboolean
bolt_status_is_pending (BoltStatus status)
{
  return status == BOLT_STATUS_AUTH_ERROR ||
         status == BOLT_STATUS_CONNECTED;
}

gboolean
bolt_status_is_connected (BoltStatus status)
{
  return status > BOLT_STATUS_DISCONNECTED;
}

BoltSecurity
bolt_security_from_string (const char *str)
{
  return bolt_enum_from_string (BOLT_TYPE_SECURITY, str, NULL);
}

const char *
bolt_security_to_string (BoltSecurity security)
{
  return bolt_enum_to_string (BOLT_TYPE_SECURITY, security, NULL);
}

gboolean
bolt_security_allows_pcie (BoltSecurity security)
{
  gboolean pcie = FALSE;

  switch (security)
    {
    case BOLT_SECURITY_NONE:
    case BOLT_SECURITY_USER:
    case BOLT_SECURITY_SECURE:
      pcie = TRUE;
      break;

    case BOLT_SECURITY_DPONLY:
    case BOLT_SECURITY_USBONLY:
    case BOLT_SECURITY_UNKNOWN:
      pcie = FALSE;
      break;
    }

  return pcie;
}

BoltPolicy
bolt_policy_from_string (const char *str)
{
  return bolt_enum_from_string (BOLT_TYPE_POLICY, str, NULL);
}

const char *
bolt_policy_to_string (BoltPolicy policy)
{
  return bolt_enum_to_string (BOLT_TYPE_POLICY, policy, NULL);
}

gboolean
bolt_policy_validate (BoltPolicy policy)
{
  return bolt_enum_validate (BOLT_TYPE_POLICY, policy, NULL);
}

BoltDeviceType
bolt_device_type_from_string (const char *str)
{
  return bolt_enum_from_string (BOLT_TYPE_DEVICE_TYPE, str, NULL);
}

const char *
bolt_device_type_to_string (BoltDeviceType type)
{
  return bolt_enum_to_string (BOLT_TYPE_DEVICE_TYPE, type, NULL);
}

gboolean
bolt_device_type_is_host (BoltDeviceType type)
{
  return type == BOLT_DEVICE_HOST;
}

const char *
bolt_power_state_to_string (BoltPowerState state)
{
  return bolt_enum_to_string (BOLT_TYPE_POWER_STATE, state, NULL);
}
