/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <cairo.h>
#include <gegl.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "libgimpbase/gimpbase.h"
#include "libgimpcolor/gimpcolor.h"

#include "core-types.h"

#include "gimp.h"
#include "gimp-edit.h"
#include "gimpbuffer.h"
#include "gimpcontext.h"
#include "gimpgrouplayer.h"
#include "gimpimage.h"
#include "gimpimage-duplicate.h"
#include "gimpimage-new.h"
#include "gimpimage-undo.h"
#include "gimplayer-floating-selection.h"
#include "gimplayer-new.h"
#include "gimplist.h"
#include "gimppickable.h"
#include "gimpselection.h"

#include "gimp-intl.h"


/*  local function protypes  */

static GimpBuffer * gimp_edit_extract (GimpImage     *image,
                                       GList         *pickables,
                                       GimpContext   *context,
                                       gboolean       cut_pixels,
                                       GError       **error);


/*  public functions  */

GimpObject *
gimp_edit_cut (GimpImage     *image,
               GList         *drawables,
               GimpContext   *context,
               GError       **error)
{
  GList    *iter;
  gboolean  cut_layers = FALSE;

  g_return_val_if_fail (GIMP_IS_IMAGE (image), NULL);
  g_return_val_if_fail (GIMP_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  if (gimp_channel_is_empty (gimp_image_get_mask (image)))
    {
      cut_layers = TRUE;

      for (iter = drawables; iter; iter = iter->next)
        if (! GIMP_IS_LAYER (iter->data))
          {
            cut_layers = FALSE;
            break;
          }
    }

  if (cut_layers)
    {
      GList     *remove = NULL;
      GimpImage *clip_image;
      gchar     *undo_label;

      /* Let's work on a copy because we will edit the list to remove
       * layers whose ancestor is also cut.
       */
      drawables = g_list_copy (drawables);
      for (iter = drawables; iter; iter = iter->next)
        {
          GList *iter2;

          for (iter2 = drawables; iter2; iter2 = iter2->next)
            {
              if (iter2 == iter)
                continue;

              if (gimp_viewable_is_ancestor (iter2->data, iter->data))
                {
                  /* When cutting a layer group, all its children come
                   * with anyway.
                   */
                  remove = g_list_prepend (remove, iter);
                  break;
                }
            }
        }
      for (iter = remove; iter; iter = iter->next)
        drawables = g_list_delete_link (drawables, iter->data);

      g_list_free (remove);

      /* Now copy all layers into the clipboard image. */
      clip_image = gimp_image_new_from_drawables (image->gimp, drawables, FALSE);
      gimp_container_remove (image->gimp->images, GIMP_OBJECT (clip_image));
      gimp_set_clipboard_image (image->gimp, clip_image);
      g_object_unref (clip_image);

      undo_label = g_strdup_printf (ngettext ("Cut Layer", "Cut %d Layers",
                                              g_list_length (drawables)),
                                    g_list_length (drawables));
      gimp_image_undo_group_start (image, GIMP_UNDO_GROUP_EDIT_CUT,
                                   undo_label);
      g_free (undo_label);

      /* Remove layers from source image. */
      for (iter = drawables; iter; iter = iter->next)
        gimp_image_remove_layer (image, GIMP_LAYER (iter->data),
                                 TRUE, NULL);

      gimp_image_undo_group_end (image);
      g_list_free (drawables);

      return GIMP_OBJECT (gimp_get_clipboard_image (image->gimp));
    }
  else
    {
      GimpBuffer *buffer;

      buffer = gimp_edit_extract (image, drawables, context, TRUE, error);

      if (buffer)
        {
          gimp_set_clipboard_buffer (image->gimp, buffer);
          g_object_unref (buffer);

          return GIMP_OBJECT (gimp_get_clipboard_buffer (image->gimp));
        }
    }

  return NULL;
}

GimpObject *
gimp_edit_copy (GimpImage     *image,
                GList         *drawables,
                GimpContext   *context,
                GError       **error)
{
  GList    *iter;
  gboolean  drawables_are_layers = TRUE;

  g_return_val_if_fail (GIMP_IS_IMAGE (image), NULL);
  g_return_val_if_fail (drawables != NULL, NULL);
  g_return_val_if_fail (GIMP_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  for (iter = drawables; iter; iter = iter->next)
    {
      g_return_val_if_fail (GIMP_IS_DRAWABLE (iter->data), NULL);
      g_return_val_if_fail (gimp_item_is_attached (iter->data), NULL);

      if (! GIMP_IS_LAYER (iter->data))
        drawables_are_layers = FALSE;
    }

  /* Only accept multiple drawables for layers. */
  g_return_val_if_fail (g_list_length (drawables) == 1 || drawables_are_layers, NULL);

  if (drawables_are_layers &&
      gimp_channel_is_empty (gimp_image_get_mask (image)))
    {
      /* Special-casing the 1 layer with no selection case.
       * It allows us to save the whole layer with all pixels as stored,
       * not the rendered version of it.
       */
      GimpImage *clip_image;

      clip_image = gimp_image_new_from_drawables (image->gimp, drawables, FALSE);
      gimp_container_remove (image->gimp->images, GIMP_OBJECT (clip_image));
      gimp_set_clipboard_image (image->gimp, clip_image);
      g_object_unref (clip_image);

      return GIMP_OBJECT (gimp_get_clipboard_image (image->gimp));
    }
  else if (drawables_are_layers)
    {
      /* Copying multiple layers or specific selection, we copy the
       * composited pixels as rendered.
       */
      GimpImage  *clip_image;
      GimpBuffer *buffer;

      clip_image = gimp_image_new_from_drawables (image->gimp, drawables, TRUE);
      gimp_container_remove (image->gimp->images, GIMP_OBJECT (clip_image));
      buffer = gimp_edit_copy_visible (clip_image, context, error);

      g_object_unref (clip_image);

      return buffer ? GIMP_OBJECT (buffer) : NULL;
    }
  else
    {
      GimpBuffer *buffer;

      buffer = gimp_edit_extract (image, drawables, context, FALSE, error);

      if (buffer)
        {
          gimp_set_clipboard_buffer (image->gimp, buffer);
          g_object_unref (buffer);

          return GIMP_OBJECT (gimp_get_clipboard_buffer (image->gimp));
        }
    }

  return NULL;
}

GimpBuffer *
gimp_edit_copy_visible (GimpImage    *image,
                        GimpContext  *context,
                        GError      **error)
{
  GimpBuffer *buffer;
  GList      *pickables;

  g_return_val_if_fail (GIMP_IS_IMAGE (image), NULL);
  g_return_val_if_fail (GIMP_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  pickables = g_list_prepend (NULL, image);
  buffer = gimp_edit_extract (image, pickables, context, FALSE, error);
  g_list_free (pickables);

  if (buffer)
    {
      gimp_set_clipboard_buffer (image->gimp, buffer);
      g_object_unref (buffer);

      return gimp_get_clipboard_buffer (image->gimp);
    }

  return NULL;
}

static gboolean
gimp_edit_paste_is_in_place (GimpPasteType paste_type)
{
  switch (paste_type)
    {
    case GIMP_PASTE_TYPE_FLOATING:
    case GIMP_PASTE_TYPE_FLOATING_INTO:
    case GIMP_PASTE_TYPE_NEW_LAYER:
      return FALSE;

    case GIMP_PASTE_TYPE_FLOATING_IN_PLACE:
    case GIMP_PASTE_TYPE_FLOATING_INTO_IN_PLACE:
    case GIMP_PASTE_TYPE_NEW_LAYER_IN_PLACE:
      return TRUE;
    }

  g_return_val_if_reached (FALSE);
}

static gboolean
gimp_edit_paste_is_floating (GimpPasteType paste_type)
{
  switch (paste_type)
    {
    case GIMP_PASTE_TYPE_FLOATING:
    case GIMP_PASTE_TYPE_FLOATING_INTO:
    case GIMP_PASTE_TYPE_FLOATING_IN_PLACE:
    case GIMP_PASTE_TYPE_FLOATING_INTO_IN_PLACE:
      return TRUE;

    case GIMP_PASTE_TYPE_NEW_LAYER:
    case GIMP_PASTE_TYPE_NEW_LAYER_IN_PLACE:
      return FALSE;
    }

  g_return_val_if_reached (FALSE);
}

static GList *
gimp_edit_paste_get_layers (GimpImage     *image,
                            GimpDrawable  *drawable,
                            GimpObject    *paste,
                            GimpPasteType *paste_type)
{
  GList      *layers = NULL;
  const Babl *floating_format;

  /*  change paste type to NEW_LAYER for cases where we can't attach a
   *  floating selection
   */
  if (! drawable                                            ||
      gimp_viewable_get_children (GIMP_VIEWABLE (drawable)) ||
      gimp_item_is_content_locked (GIMP_ITEM (drawable), NULL))
    {
      if (gimp_edit_paste_is_in_place (*paste_type))
        *paste_type = GIMP_PASTE_TYPE_NEW_LAYER_IN_PLACE;
      else
        *paste_type = GIMP_PASTE_TYPE_NEW_LAYER;
    }

  /*  floating pastes always have the pasted-to drawable's format with
   *  alpha; if drawable == NULL, user is pasting into an empty image
   */
  if (drawable && gimp_edit_paste_is_floating (*paste_type))
    floating_format = gimp_drawable_get_format_with_alpha (drawable);
  else
    floating_format = gimp_image_get_layer_format (image, TRUE);

  if (GIMP_IS_IMAGE (paste))
    {
      GList *iter;

      layers = g_list_copy (gimp_image_get_layer_iter (GIMP_IMAGE (paste)));

      if (g_list_length (layers) > 1)
        {
          if (gimp_edit_paste_is_in_place (*paste_type))
            *paste_type = GIMP_PASTE_TYPE_NEW_LAYER_IN_PLACE;
          else
            *paste_type = GIMP_PASTE_TYPE_NEW_LAYER;
        }

      for (iter = layers; iter; iter = iter->next)
        {
          GType layer_type;

          switch (*paste_type)
            {
            case GIMP_PASTE_TYPE_FLOATING:
            case GIMP_PASTE_TYPE_FLOATING_IN_PLACE:
            case GIMP_PASTE_TYPE_FLOATING_INTO:
            case GIMP_PASTE_TYPE_FLOATING_INTO_IN_PLACE:
              /*  when pasting as floating make sure gimp_item_convert()
               *  will turn group layers into normal layers, otherwise use
               *  the same layer type so e.g. text information gets
               *  preserved. See issue #2667.
               */
              if (GIMP_IS_GROUP_LAYER (iter->data))
                layer_type = GIMP_TYPE_LAYER;
              else
                layer_type = G_TYPE_FROM_INSTANCE (iter->data);
              break;

            case GIMP_PASTE_TYPE_NEW_LAYER:
            case GIMP_PASTE_TYPE_NEW_LAYER_IN_PLACE:
              layer_type = G_TYPE_FROM_INSTANCE (iter->data);
              break;

            default:
              g_return_val_if_reached (NULL);
            }

          iter->data = GIMP_LAYER (gimp_item_convert (GIMP_ITEM (iter->data),
                                                      image, layer_type));

          switch (*paste_type)
            {
            case GIMP_PASTE_TYPE_FLOATING:
            case GIMP_PASTE_TYPE_FLOATING_IN_PLACE:
            case GIMP_PASTE_TYPE_FLOATING_INTO:
            case GIMP_PASTE_TYPE_FLOATING_INTO_IN_PLACE:
              /*  when pasting as floating selection, get rid of the layer mask,
               *  and make sure the layer has the right format
               */
              if (gimp_layer_get_mask (iter->data))
                gimp_layer_apply_mask (iter->data, GIMP_MASK_DISCARD, FALSE);

              if (gimp_drawable_get_format (GIMP_DRAWABLE (iter->data)) !=
                  floating_format)
                {
                  gimp_drawable_convert_type (GIMP_DRAWABLE (iter->data), image,
                                              gimp_drawable_get_base_type (drawable),
                                              gimp_drawable_get_precision (drawable),
                                              TRUE,
                                              NULL, NULL,
                                              GEGL_DITHER_NONE, GEGL_DITHER_NONE,
                                              FALSE, NULL);
                }
              break;

            default:
              break;
            }
        }
    }
  else if (GIMP_IS_BUFFER (paste))
    {
      GimpLayer *layer;

      layer = gimp_layer_new_from_buffer (GIMP_BUFFER (paste), image,
                                          floating_format,
                                          _("Pasted Layer"),
                                          GIMP_OPACITY_OPAQUE,
                                          gimp_image_get_default_new_layer_mode (image));

      layers = g_list_prepend (layers, layer);
    }

  return layers;
}

static void
gimp_edit_paste_get_viewport_offset (GimpImage    *image,
                                     GimpDrawable *drawable,
                                     GList        *pasted_layers,
                                     gint          viewport_x,
                                     gint          viewport_y,
                                     gint          viewport_width,
                                     gint          viewport_height,
                                     gint         *offset_x,
                                     gint         *offset_y)
{
  GList   *iter;
  gint     image_width;
  gint     image_height;
  gint     width          = 0;
  gint     height         = 0;
  gboolean clamp_to_image = TRUE;

  g_return_if_fail (GIMP_IS_IMAGE (image));
  g_return_if_fail (drawable == NULL || GIMP_IS_DRAWABLE (drawable));
  g_return_if_fail (drawable == NULL ||
                    gimp_item_is_attached (GIMP_ITEM (drawable)));
  g_return_if_fail (pasted_layers != NULL);
  g_return_if_fail (offset_x != NULL);
  g_return_if_fail (offset_y != NULL);

  image_width  = gimp_image_get_width  (image);
  image_height = gimp_image_get_height (image);

  for (iter = pasted_layers; iter; iter = iter->next)
    {
      gint layer_width;
      gint layer_height;

      g_return_if_fail (GIMP_IS_VIEWABLE (iter->data));
      gimp_viewable_get_size (iter->data, &layer_width, &layer_height);

      width  = MAX (width, layer_width);
      height = MAX (height, layer_height);
    }

  if (viewport_width  == image_width &&
      viewport_height == image_height)
    {
      /* if the whole image is visible, act as if there was no viewport */

      viewport_x      = 0;
      viewport_y      = 0;
      viewport_width  = 0;
      viewport_height = 0;
    }

  if (drawable)
    {
      /*  if pasting to a drawable  */

      GimpContainer *children;
      gint           off_x, off_y;
      gint           target_x, target_y;
      gint           target_width, target_height;
      gint           paste_x, paste_y;
      gint           paste_width, paste_height;
      gboolean       have_mask;

      have_mask = ! gimp_channel_is_empty (gimp_image_get_mask (image));

      gimp_item_get_offset (GIMP_ITEM (drawable), &off_x, &off_y);

      children = gimp_viewable_get_children (GIMP_VIEWABLE (drawable));

      if (children && gimp_container_get_n_children (children) == 0)
        {
          /* treat empty layer groups as image-sized, use the selection
           * as target
           */
          gimp_item_bounds (GIMP_ITEM (gimp_image_get_mask (image)),
                            &target_x, &target_y,
                            &target_width, &target_height);
        }
      else
        {
          gimp_item_mask_intersect (GIMP_ITEM (drawable),
                                    &target_x, &target_y,
                                    &target_width, &target_height);
        }

      if (! have_mask         &&    /* if we have no mask */
          viewport_width  > 0 &&    /* and we have a viewport */
          viewport_height > 0 &&
          (width  < target_width || /* and the paste is smaller than the target */
           height < target_height) &&

          /* and the viewport intersects with the target */
          gimp_rectangle_intersect (viewport_x, viewport_y,
                                    viewport_width, viewport_height,
                                    off_x, off_y, /* target_x,y are 0 */
                                    target_width, target_height,
                                    &paste_x, &paste_y,
                                    &paste_width, &paste_height))
        {
          /*  center on the viewport  */

          *offset_x = paste_x + (paste_width - width)  / 2;
          *offset_y = paste_y + (paste_height- height) / 2;
        }
      else
        {
          /*  otherwise center on the target  */

          *offset_x = off_x + target_x + (target_width  - width)  / 2;
          *offset_y = off_y + target_y + (target_height - height) / 2;

          /*  and keep it that way  */
          clamp_to_image = FALSE;
        }
    }
  else if (viewport_width  > 0 &&  /* if we have a viewport */
           viewport_height > 0 &&
           (width  < image_width || /* and the paste is       */
            height < image_height)) /* smaller than the image */
    {
      /*  center on the viewport  */

      *offset_x = viewport_x + (viewport_width  - width)  / 2;
      *offset_y = viewport_y + (viewport_height - height) / 2;
    }
  else
    {
      /*  otherwise center on the image  */

      *offset_x = (image_width  - width)  / 2;
      *offset_y = (image_height - height) / 2;

      /*  and keep it that way  */
      clamp_to_image = FALSE;
    }

  if (clamp_to_image)
    {
      /*  Ensure that the pasted layer is always within the image, if it
       *  fits and aligned at top left if it doesn't. (See bug #142944).
       */
      *offset_x = MIN (*offset_x, image_width  - width);
      *offset_y = MIN (*offset_y, image_height - height);
      *offset_x = MAX (*offset_x, 0);
      *offset_y = MAX (*offset_y, 0);
    }
}

static GList *
gimp_edit_paste_paste (GimpImage     *image,
                       GimpDrawable  *drawable,
                       GList         *layers,
                       GimpPasteType  paste_type,
                       gboolean       use_offset,
                       gint           offset_x,
                       gint           offset_y)
{
  GList *iter;

  gimp_image_undo_group_start (image, GIMP_UNDO_GROUP_EDIT_PASTE,
                               C_("undo-type", "Paste"));

  for (iter = layers; iter; iter = iter->next)
    {
      /* Layers for in-place paste are already translated. */
      if (use_offset)
        gimp_item_translate (GIMP_ITEM (iter->data), offset_x, offset_y, FALSE);

      switch (paste_type)
        {
        case GIMP_PASTE_TYPE_FLOATING:
        case GIMP_PASTE_TYPE_FLOATING_IN_PLACE:
          /*  if there is a selection mask clear it - this might not
           *  always be desired, but in general, it seems like the correct
           *  behavior
           */
          if (! gimp_channel_is_empty (gimp_image_get_mask (image)))
            gimp_channel_clear (gimp_image_get_mask (image), NULL, TRUE);

          /* fall thru */

        case GIMP_PASTE_TYPE_FLOATING_INTO:
        case GIMP_PASTE_TYPE_FLOATING_INTO_IN_PLACE:
          floating_sel_attach (iter->data, drawable);
          break;

        case GIMP_PASTE_TYPE_NEW_LAYER:
        case GIMP_PASTE_TYPE_NEW_LAYER_IN_PLACE:
            {
              GimpLayer *parent   = NULL;
              gint       position = 0;

              /*  always add on top of a passed layer, where we would attach
               *  a floating selection
               */
              if (GIMP_IS_LAYER (drawable))
                {
                  parent   = gimp_layer_get_parent (GIMP_LAYER (drawable));
                  position = gimp_item_get_index (GIMP_ITEM (drawable));
                }

              gimp_image_add_layer (image, iter->data, parent, position, TRUE);
            }
          break;
        }
    }

  gimp_image_undo_group_end (image);

  return layers;
}

GList *
gimp_edit_paste (GimpImage     *image,
                 GimpDrawable  *drawable,
                 GimpObject    *paste,
                 GimpPasteType  paste_type,
                 gint           viewport_x,
                 gint           viewport_y,
                 gint           viewport_width,
                 gint           viewport_height)
{
  GList    *layers;
  gboolean  use_offset = FALSE;
  gint      offset_x   = 0;
  gint      offset_y   = 0;

  g_return_val_if_fail (GIMP_IS_IMAGE (image), NULL);
  g_return_val_if_fail (drawable == NULL || GIMP_IS_DRAWABLE (drawable), NULL);
  g_return_val_if_fail (drawable == NULL ||
                        gimp_item_is_attached (GIMP_ITEM (drawable)), NULL);
  g_return_val_if_fail (GIMP_IS_IMAGE (paste) || GIMP_IS_BUFFER (paste), NULL);

  layers = gimp_edit_paste_get_layers (image, drawable, paste, &paste_type);

  if (! layers)
    return NULL;

  if (gimp_edit_paste_is_in_place (paste_type))
    {
      if (GIMP_IS_BUFFER (paste))
        {
          GimpBuffer *buffer = GIMP_BUFFER (paste);

          use_offset = TRUE;
          offset_x   = buffer->offset_x;
          offset_y   = buffer->offset_y;
        }
    }
  else
    {
      use_offset = TRUE;
      gimp_edit_paste_get_viewport_offset (image, drawable, layers,
                                           viewport_x,
                                           viewport_y,
                                           viewport_width,
                                           viewport_height,
                                           &offset_x,
                                           &offset_y);
    }

  return gimp_edit_paste_paste (image, drawable, layers, paste_type,
                                use_offset, offset_x, offset_y);
}

GimpImage *
gimp_edit_paste_as_new_image (Gimp       *gimp,
                              GimpObject *paste)
{
  GimpImage *image = NULL;

  g_return_val_if_fail (GIMP_IS_GIMP (gimp), NULL);
  g_return_val_if_fail (GIMP_IS_IMAGE (paste) || GIMP_IS_BUFFER (paste), NULL);

  if (GIMP_IS_IMAGE (paste))
    {
      image = gimp_image_duplicate (GIMP_IMAGE (paste));
    }
  else if (GIMP_IS_BUFFER (paste))
    {
      image = gimp_image_new_from_buffer (gimp, GIMP_BUFFER (paste));
    }

  return image;
}

const gchar *
gimp_edit_named_cut (GimpImage     *image,
                     const gchar   *name,
                     GList         *drawables,
                     GimpContext   *context,
                     GError       **error)
{
  GimpBuffer *buffer;

  g_return_val_if_fail (GIMP_IS_IMAGE (image), NULL);
  g_return_val_if_fail (name != NULL, NULL);
  g_return_val_if_fail (GIMP_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  buffer = gimp_edit_extract (image, drawables, context, TRUE, error);

  if (buffer)
    {
      gimp_object_set_name (GIMP_OBJECT (buffer), name);
      gimp_container_add (image->gimp->named_buffers, GIMP_OBJECT (buffer));
      g_object_unref (buffer);

      return gimp_object_get_name (buffer);
    }

  return NULL;
}

const gchar *
gimp_edit_named_copy (GimpImage     *image,
                      const gchar   *name,
                      GList         *drawables,
                      GimpContext   *context,
                      GError       **error)
{
  GimpBuffer *buffer;

  g_return_val_if_fail (GIMP_IS_IMAGE (image), NULL);
  g_return_val_if_fail (name != NULL, NULL);
  g_return_val_if_fail (GIMP_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  buffer = gimp_edit_extract (image, drawables, context, FALSE, error);

  if (buffer)
    {
      gimp_object_set_name (GIMP_OBJECT (buffer), name);
      gimp_container_add (image->gimp->named_buffers, GIMP_OBJECT (buffer));
      g_object_unref (buffer);

      return gimp_object_get_name (buffer);
    }

  return NULL;
}

const gchar *
gimp_edit_named_copy_visible (GimpImage    *image,
                              const gchar  *name,
                              GimpContext  *context,
                              GError      **error)
{
  GimpBuffer *buffer;
  GList      *pickables;

  g_return_val_if_fail (GIMP_IS_IMAGE (image), NULL);
  g_return_val_if_fail (name != NULL, NULL);
  g_return_val_if_fail (GIMP_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  pickables = g_list_prepend (NULL, image);
  buffer = gimp_edit_extract (image, pickables, context, FALSE, error);
  g_list_free (pickables);

  if (buffer)
    {
      gimp_object_set_name (GIMP_OBJECT (buffer), name);
      gimp_container_add (image->gimp->named_buffers, GIMP_OBJECT (buffer));
      g_object_unref (buffer);

      return gimp_object_get_name (buffer);
    }

  return NULL;
}


/*  private functions  */

/**
 * gimp_edit_extract:
 * @image:
 * @pickables:
 * @context:
 * @cut_pixels:
 * @error:
 *
 * Extracts the selected part of @image from the list of @pickables.
 * If @cut_pixels is %TRUE, and there is only one pickable input, and if
 * this pickable is a #GimpDrawable, then the selected pixels will be
 * effectively erased from the input pickable.
 * Otherwise @cut_pixels has no additional effect.
 * Note that all @pickables must belong to the same @image.
 *
 * Returns: a #GimpBuffer of the selected part of @image as if only the
 * selected @pickables were present (composited according to their
 * properties, unless there is only one pickable, in which case direct
 * pixel information is used without composition).
 */
static GimpBuffer *
gimp_edit_extract (GimpImage     *image,
                   GList         *pickables,
                   GimpContext   *context,
                   gboolean       cut_pixels,
                   GError       **error)
{
  GeglBuffer *buffer;
  gint        offset_x;
  gint        offset_y;

  g_return_val_if_fail (g_list_length (pickables) > 0, NULL);

  if (g_list_length (pickables) > 1 ||
      ! GIMP_IS_DRAWABLE (pickables->data))
    cut_pixels = FALSE;

  if (cut_pixels)
    gimp_image_undo_group_start (image, GIMP_UNDO_GROUP_EDIT_CUT,
                                 C_("undo-type", "Cut"));

  /*  Cut/copy the mask portion from the image  */
  buffer = gimp_selection_extract (GIMP_SELECTION (gimp_image_get_mask (image)),
                                   pickables, context,
                                   cut_pixels, FALSE, FALSE,
                                   &offset_x, &offset_y, error);

  if (cut_pixels)
    gimp_image_undo_group_end (image);

  if (buffer)
    {
      GimpBuffer *gimp_buffer;
      gdouble     res_x;
      gdouble     res_y;

      gimp_buffer = gimp_buffer_new (buffer, _("Global Buffer"),
                                     offset_x, offset_y, FALSE);
      g_object_unref (buffer);

      gimp_image_get_resolution (image, &res_x, &res_y);
      gimp_buffer_set_resolution (gimp_buffer, res_x, res_y);
      gimp_buffer_set_unit (gimp_buffer, gimp_image_get_unit (image));

      if (GIMP_IS_COLOR_MANAGED (pickables->data))
        {
          GimpColorProfile *profile =
            gimp_color_managed_get_color_profile (GIMP_COLOR_MANAGED (pickables->data));

          if (profile)
            gimp_buffer_set_color_profile (gimp_buffer, profile);
        }

      return gimp_buffer;
    }

  return NULL;
}
