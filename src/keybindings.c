#include <tilda-config.h>

#include <errno.h>

#include "debug.h"
#include "tilda.h"
#include "keybindings.h"
#include "key_grabber.h"

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <glib.h>
#include <glib/gi18n.h>
// Not a huge fan of having this in the module space rather than explicitly passed around
static GtkListStore *keybindings_model = NULL;

static gboolean validate_pulldown_keybinding(const gchar* accel, tilda_window* tw, const gchar* message);
static gboolean validate_keybinding(const gchar* accel, const tilda_window* tw, const gchar* message);
static gboolean validate_keybindings(const tilda_window *tw);
static gpointer wizard_dlg_key_grab (GtkWidget *dialog,
									 GdkEventKey *event,
									 GtkTreeView *tree_view_keybindings);
static gboolean keybindings_button_press_event_cb(GtkWidget *widget,
												  GdkEventButton *event,
												  GtkBuilder *xml);

enum
{
	KB_TREE_ACTION,
	KB_TREE_SHORTCUT,
	KB_TREE_CONFIG_NAME,
	KB_NUM_COLUMNS
};

static gboolean validate_pulldown_keybinding(const gchar* accel, tilda_window* tw, const gchar* action)
{
    /* Try to grab the key. This is a good way to validate it :) */
    gboolean key_is_valid = tilda_keygrabber_bind (accel, tw);

    if (key_is_valid)
        return TRUE;
    else
    {
        /* Show the "invalid keybinding" dialog */
        gchar *message = g_strdup_printf(_("The keybinding you chose for \"%s\" is invalid. Please choose another."), action);
        show_invalid_keybinding_dialog (GTK_WINDOW(tw->wizard_window), action);
        return FALSE;
    }
}

static gboolean validate_keybinding(const gchar* accel, const tilda_window *tw, const gchar* action)
{
    guint accel_key;
    GdkModifierType accel_mods;

    /* Parse the accelerator string. If it parses improperly, both values will be 0.*/
    gtk_accelerator_parse (accel, &accel_key, &accel_mods);
    if (! ((accel_key == 0) && (accel_mods == 0)) ) {
        return TRUE;
    } else {
        gchar *message = g_strdup_printf(_("The keybinding you chose for \"%s\" is invalid. Please choose another."), action);
        show_invalid_keybinding_dialog (GTK_WINDOW(tw->wizard_window), message);
        return FALSE;
    }
}

void apply_keybindings()
{
	GtkTreeIter iter;

	gboolean valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL(keybindings_model), &iter);

	while(valid)
	{
		gchar *action, *config_name, *shortcut, *path;

		gtk_tree_model_get(GTK_TREE_MODEL(keybindings_model), &iter,
						   KB_TREE_ACTION, &action,
						   KB_TREE_CONFIG_NAME, &config_name,
						   KB_TREE_SHORTCUT, &shortcut,
						   -1);

		path = g_strdup_printf("<tilda>/context/%s", action);

		tilda_window_update_keyboard_accelerators(path, config_getstr(config_name));

		g_free(path);

		valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(keybindings_model), &iter);
	}
}

/******************************************************************************/
/*               ALL static callback helpers are below                        */
/******************************************************************************/

/*
 * The functions wizard_dlg_key_grab() and setup_key_grab() are totally inspired by
 * the custom keybindings dialog in denemo. See http://denemo.sourceforge.net/
 *
 * Thanks for the help! :)
 */

/*
 * This function gets called once per key that is pressed. This means we have to
 * manually ignore all modifier keys, and only register "real" keys.
 *
 * We return when the first "real" key is pressed.
 *
 * Note that this is called for the key_press_event for the key-grab dialog, not for the wizard.
 */

static gpointer wizard_dlg_key_grab (GtkWidget *dialog,
									 GdkEventKey *event,
									 GtkTreeView *tree_view_keybindings)
{
    DEBUG_FUNCTION ("wizard_dlg_key_grab");
    DEBUG_ASSERT (tree_view_keybindings != NULL);
    DEBUG_ASSERT (event != NULL);

    gchar *key;

    if (gtk_accelerator_valid (event->keyval, event->state))
    {
		GtkTreeIter iter;
		GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(tree_view_keybindings));
		keybindings_model =
			GTK_LIST_STORE(gtk_tree_view_get_model (tree_view_keybindings));

		GtkTreeModel *model = GTK_TREE_MODEL(keybindings_model);

		if(!gtk_tree_selection_get_selected(selection, &model, &iter))
		{
			return FALSE;
		}

        /* This lets us ignore all ignorable modifier keys, including
         * NumLock and many others. :)
         *
         * The logic is: keep only the important modifiers that were pressed
         * for this event. */
        event->state &= gtk_accelerator_get_default_mod_mask();

        /* Generate the correct name for this key */
        key = gtk_accelerator_name (event->keyval, event->state);

		gtk_list_store_set (GTK_LIST_STORE(keybindings_model), &iter,
						    KB_TREE_SHORTCUT, key, -1);

		gtk_dialog_response (GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

        #ifdef DEBUG
            g_printerr ("KEY GRABBED: %s\n", key);
        #endif

        /* Free the string */
        g_free (key);
    }
    return GDK_EVENT_PROPAGATE;
}


static gboolean keybindings_button_press_event_cb(GtkWidget *widget,
												  GdkEventButton *event,
												  GtkBuilder *xml)
{
    const GtkTreeView *tree_view_keybindings = GTK_TREE_VIEW(widget);
    const GtkWidget *wizard_window =
        GTK_WIDGET (gtk_builder_get_object (xml, "wizard_window"));

	if(event->button == 1 && event->type == GDK_2BUTTON_PRESS)
	{
		GtkTreeIter iter;
		GtkTreeSelection *selection = gtk_tree_view_get_selection (tree_view_keybindings);
		GtkTreeModel *model = GTK_TREE_MODEL(keybindings_model);

		if(!gtk_tree_selection_get_selected(selection, &model, &iter))
		{
			return FALSE;
		}


		/* Bring up the dialog that will accept the new keybinding */
		GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW(wizard_window),
													GTK_DIALOG_DESTROY_WITH_PARENT,
													GTK_MESSAGE_QUESTION,
													GTK_BUTTONS_CANCEL,
													_("Enter keyboard shortcut"));

		/* Connect the key grabber to the dialog */
		g_signal_connect (G_OBJECT(dialog), "key_press_event",
						  G_CALLBACK(wizard_dlg_key_grab), tree_view_keybindings);

		gtk_window_set_keep_above (GTK_WINDOW(dialog), TRUE);
		gtk_dialog_run (GTK_DIALOG (dialog));

		gtk_widget_destroy(dialog);

		return TRUE;
	}

	return FALSE;
}

static gboolean validate_keybindings(const tilda_window *tw)
{
	GtkTreeIter iter;

	gboolean valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL(keybindings_model), &iter);

	while(valid)
	{
		gchar *action, *config_name, *shortcut;

		gtk_tree_model_get(GTK_TREE_MODEL(keybindings_model), &iter,
						   KB_TREE_ACTION, &action,
						   KB_TREE_CONFIG_NAME, &config_name,
						   KB_TREE_SHORTCUT, &shortcut,
						   -1);

		if(0 == g_strcmp0("key", config_name))
		{
			if (!validate_pulldown_keybinding(shortcut, tw,
											  _("Pull Down Terminal")))
				return FALSE;
		}
		else
		{
			if (!validate_keybinding(shortcut, tw, action))
			{
				return FALSE;
			}

		}

		valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(keybindings_model), &iter);
	}


	return TRUE;
}

gboolean save_keybindings(const tilda_window *tw)
{
	if(!validate_keybindings(tw))
	{
		return FALSE;
	}

	GtkTreeIter iter;

	gboolean valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL(keybindings_model), &iter);

	while(valid)
	{
		gchar* config_name, *shortcut;

		gtk_tree_model_get(GTK_TREE_MODEL(keybindings_model), &iter,
						   KB_TREE_CONFIG_NAME, &config_name,
						   KB_TREE_SHORTCUT, &shortcut,
						   -1);

		config_setstr(config_name, shortcut);

		valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(keybindings_model), &iter);
	}


	return TRUE;
}

void keybindings_model_init(GtkBuilder *xml)
{
	GtkWidget *tree_view_keybindings =
        GTK_WIDGET (gtk_builder_get_object(xml, "tree_view_keybindings"));

	keybindings_model = gtk_list_store_new (KB_NUM_COLUMNS,
											G_TYPE_STRING,
											G_TYPE_STRING,
											G_TYPE_STRING);

	gtk_tree_view_set_model (GTK_TREE_VIEW(tree_view_keybindings),
							 GTK_TREE_MODEL(keybindings_model));


	GtkCellRenderer *renderer = gtk_cell_renderer_text_new ();
	GtkTreeViewColumn *column;
	GtkTreeIter iter;

	column = gtk_tree_view_column_new_with_attributes (_("Action"), renderer,
													   "text", KB_TREE_ACTION, NULL);

	gtk_tree_view_append_column (GTK_TREE_VIEW(tree_view_keybindings), column);

	column = gtk_tree_view_column_new_with_attributes (_("Shortcut"), renderer,
													   "text", KB_TREE_SHORTCUT, NULL);

	gtk_tree_view_append_column (GTK_TREE_VIEW(tree_view_keybindings), column);


	typedef struct keybinding
	{
		gchar* action;
		gchar* config_name;
	} keybinding;

	keybinding bindings[] =
	{
		{"Pull Down Terminal", "key"},
		{"Quit", "quit_key"},
		{"Add Tab", "addtab_key"},
		{"Close Tab", "closetab_key"},
		{"Copy", "copy_key"},
		{"Paste", "paste_key"},
		{"Go To Next Tab", "nexttab_key"},
		{"Go To Previous Tab", "prevtab_key"},
		{"Move Tab Left", "movetableft_key"},
		{"Move Tab Right", "movetabright_key"},
		{"Toggle Fullscreen", "fullscreen_key"},
		{NULL, NULL}
	};

	for(keybinding *current_binding = bindings;; ++current_binding)
	{
		if(current_binding->action == NULL)
		{
			break;
		}

		gtk_list_store_append (keybindings_model, &iter);

		gtk_list_store_set (keybindings_model, &iter,
							KB_TREE_ACTION, current_binding->action,
							KB_TREE_SHORTCUT, config_getstr(current_binding->config_name),
							KB_TREE_CONFIG_NAME, current_binding->config_name,
							-1);

	}

    // Not a huge fan of generating these dynamically, it's not really saving that much Code
    // And it makes it considerably harder to read in order to tell which keys are actually bindable
	for(int i = 1; i <= 10; ++i)
	{
		gchar *config_name = g_strdup_printf("gototab_%i_key", i);
		gchar *action_name = g_strdup_printf("Go To Tab %i", i);

		gtk_list_store_append (keybindings_model, &iter);

		gtk_list_store_set (keybindings_model, &iter,
							KB_TREE_ACTION, action_name,
							KB_TREE_SHORTCUT, config_getstr(config_name),
							KB_TREE_CONFIG_NAME, config_name,
							-1);

		g_free(action_name);
		g_free(config_name);

	}

	g_signal_connect(tree_view_keybindings, "button-press-event",
					 G_CALLBACK(keybindings_button_press_event_cb), xml);

}
