/*
 * Seahorse
 *
 * Copyright (C) 2006 Nate Nielsen
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include <gnome.h>

#include "seahorse-ssh-source.h"
#include "seahorse-ssh-key.h"
#include "seahorse-ssh-operation.h"
#include "seahorse-widget.h"
#include "seahorse-util.h"
#include "seahorse-key-dialogs.h"
#include "seahorse-progress.h"
#include "seahorse-gtkstock.h"

static void
completion_handler (SeahorseOperation *op, gpointer data)
{
    GError *error = NULL;
    if (!seahorse_operation_is_successful (op)) {
        seahorse_operation_copy_error (op, &error);
        seahorse_util_handle_error (error, _("Couldn't generate SSH key"));
    }
}

static void
upload_handler (SeahorseOperation *op, gpointer data)
{
    SeahorseSSHKey *skey;
    GList *keys;
    
    if (!seahorse_operation_is_successful (op) ||
        seahorse_operation_is_cancelled (op))
        return;
    
    skey = SEAHORSE_SSH_KEY (seahorse_operation_get_result (op));
    g_return_if_fail (SEAHORSE_IS_SSH_KEY (skey));
    
    keys = g_list_append (NULL, skey);
    seahorse_ssh_upload_prompt (keys);
    g_list_free (keys);
}

static void
on_response (GtkDialog *dialog, guint response, SeahorseWidget *swidget)
{
    SeahorseSSHSource *src;
    SeahorseOperation *op;
    GtkWidget *widget;
    const gchar *email;
    const gchar *t;
    gboolean upload;
    guint type;
    guint bits;
    
    if (response == GTK_RESPONSE_HELP) {
        seahorse_widget_show_help (swidget);
        return;
    }
    
    if (response != GTK_RESPONSE_OK) {
        seahorse_widget_destroy (swidget);
        return;
    }
    
    /* The email address */
    widget = seahorse_widget_get_widget (swidget, "email-entry");
    g_return_if_fail (widget != NULL);
    email = gtk_entry_get_text (GTK_ENTRY (widget));
    
    /* The 'configure' option */
    widget = seahorse_widget_get_widget (swidget, "configure-option");
    g_return_if_fail (widget != NULL);
    upload = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

    /* The algorithm */
    widget = seahorse_widget_get_widget (swidget, "algorithm-choice");
    g_return_if_fail (widget != NULL);
    t = gtk_combo_box_get_active_text (GTK_COMBO_BOX (widget));
    if (t && strcasecmp (t, "DSA"))
        type = SSH_ALGO_DSA;
    else
        type = SSH_ALGO_RSA;
    
    /* The number of bits */
    widget = seahorse_widget_get_widget (swidget, "bits-entry");
    g_return_if_fail (widget != NULL);
    bits = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (widget));
    if (bits < 512 || bits > 8192) {
        g_message ("invalid key size: %s defaulting to 2048", t);
        bits = 2048;
    }
    
    src = SEAHORSE_SSH_SOURCE (g_object_get_data (G_OBJECT (swidget), "key-source"));
    g_return_if_fail (SEAHORSE_IS_SSH_SOURCE (src));
    
    /* We start creation */
    op = seahorse_ssh_operation_generate (src, email, type, bits);
    g_return_if_fail (op != NULL);
    
    seahorse_widget_destroy (swidget);
    
    /* Watch for errors so we can display */
    g_signal_connect (op, "done", G_CALLBACK (completion_handler), NULL);
    
    /* When completed upload */
    if (upload)
        g_signal_connect (op, "done", G_CALLBACK (upload_handler), NULL);
    
    seahorse_progress_show (op, _("Creating SSH Key"), TRUE);
}

void
seahorse_ssh_generate_show (SeahorseSSHSource *src)
{
    SeahorseWidget *swidget;
    GtkWidget *widget;
    
    swidget = seahorse_widget_new ("ssh-generate");
    
    /* Widget already present */
    if (swidget == NULL)
        return;
    
    widget = seahorse_widget_get_widget (swidget, "ssh-image");
    g_return_if_fail (widget != NULL);
    gtk_image_set_from_stock (GTK_IMAGE (widget), SEAHORSE_STOCK_KEY_SSH, GTK_ICON_SIZE_DIALOG);
    
    widget = seahorse_widget_get_widget (swidget, "algorithm-choice");
    g_return_if_fail (widget != NULL);
    gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
    
    g_object_ref (src);
    g_object_set_data_full (G_OBJECT (swidget), "key-source", src, g_object_unref);
    
    g_signal_connect (seahorse_widget_get_top (swidget), "response", 
                    G_CALLBACK (on_response), swidget);
}
