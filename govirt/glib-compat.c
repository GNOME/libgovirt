/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

#include "glib-compat.h"

#if !GLIB_CHECK_VERSION(2,26,0)
G_DEFINE_BOXED_TYPE (GError, ovirt_error, g_error_copy, g_error_free)

/**
 * g_key_file_set_uint64:
 * @key_file: a #GKeyFile
 * @group_name: a group name
 * @key: a key
 * @value: an integer value
 *
 * Associates a new integer value with @key under @group_name.
 * If @key cannot be found then it is created.
 *
 * Since: 2.26
 **/
void
g_key_file_set_uint64 (GKeyFile    *key_file,
                       const gchar *group_name,
                       const gchar *key,
                       guint64      value)
{
  gchar *result;

  g_return_if_fail (key_file != NULL);

  result = g_strdup_printf ("%" G_GUINT64_FORMAT, value);
  g_key_file_set_value (key_file, group_name, key, result);
  g_free (result);
}
#endif

#if !GLIB_CHECK_VERSION(2,28,0)
/**
 * g_simple_async_result_new_take_error: (skip)
 * @source_object: (allow-none): a #GObject, or %NULL
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user data passed to @callback
 * @error: a #GError
 *
 * Creates a #GSimpleAsyncResult from an error condition, and takes over the
 * caller's ownership of @error, so the caller does not need to free it anymore.
 *
 * Returns: a #GSimpleAsyncResult
 *
 * Since: 2.28
 **/
G_GNUC_INTERNAL GSimpleAsyncResult *
g_simple_async_result_new_take_error (GObject             *source_object,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data,
                                      GError              *error)
{
  GSimpleAsyncResult *simple;

  g_return_val_if_fail (!source_object || G_IS_OBJECT (source_object), NULL);

  simple = g_simple_async_result_new (source_object,
				      callback,
				      user_data, NULL);
  g_simple_async_result_take_error (simple, error);

  return simple;
}

/**
 * g_simple_async_result_take_error: (skip)
 * @simple: a #GSimpleAsyncResult
 * @error: a #GError
 *
 * Sets the result from @error, and takes over the caller's ownership
 * of @error, so the caller does not need to free it any more.
 *
 * Since: 2.28
 **/
G_GNUC_INTERNAL void
g_simple_async_result_take_error (GSimpleAsyncResult *simple,
                                  GError             *error)
{
    /* this code is different from upstream */
    /* we can't avoid extra copy/free, since the simple struct is
       opaque */
    g_simple_async_result_set_from_error (simple, error);
    g_error_free (error);
}

/**
 * g_simple_async_report_take_gerror_in_idle: (skip)
 * @object: (allow-none): a #GObject, or %NULL
 * @callback: a #GAsyncReadyCallback.
 * @user_data: user data passed to @callback.
 * @error: the #GError to report
 *
 * Reports an error in an idle function. Similar to
 * g_simple_async_report_gerror_in_idle(), but takes over the caller's
 * ownership of @error, so the caller does not have to free it any more.
 *
 * Since: 2.28
 **/
G_GNUC_INTERNAL void
g_simple_async_report_take_gerror_in_idle (GObject *object,
                                           GAsyncReadyCallback callback,
                                           gpointer user_data,
                                           GError *error)
{
  GSimpleAsyncResult *simple;

  g_return_if_fail (!object || G_IS_OBJECT (object));
  g_return_if_fail (error != NULL);

  simple = g_simple_async_result_new_take_error (object,
                                                 callback,
                                                 user_data,
                                                 error);
  g_simple_async_result_complete_in_idle (simple);
  g_object_unref (simple);
}


/**
 * g_slist_free_full: (skip)
 * @list: a #GSList
 * @free_func: a #GDestroyNotify
 *
 * Convenience method, which frees all the memory used by a #GSList,
 * and calls the specified destroy function on every element's data
 *
 * Since: 2.28
 **/
G_GNUC_INTERNAL void
g_slist_free_full(GSList         *list,
                  GDestroyNotify free_func)
{
    GSList *el;

    if (free_func) {
        for (el = list; el ; el = g_slist_next(el)) {
            free_func(el);
        }
    }

    g_slist_free(list);
}

#endif

#if !GLIB_CHECK_VERSION(2,30,0)
G_DEFINE_BOXED_TYPE (GMainContext, ovirt_main_context, g_main_context_ref, g_main_context_unref)
#endif

#if !GLIB_CHECK_VERSION(2,32,0)
GByteArray *g_byte_array_new_take (guint8 *data, gsize len)
{
  GByteArray *array;

  array = g_byte_array_new ();
  g_assert (array->data == NULL);
  g_assert (array->len == 0);

  array->data = data;
  array->len = len;

  return array;
}
#endif

#if !GLIB_CHECK_VERSION(2,27,2)
guint64 g_get_monotonic_time(void)
{
    GTimeVal tv;

    /* TODO: support real monotonic clock? */
    g_get_current_time(&tv);

    return (((gint64) tv.tv_sec) * 1000000) + tv.tv_usec;
}
#endif
