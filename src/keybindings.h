#ifndef KEYBINDINGS_H
#define KEYBINDINGS_H
#include <gtk/gtk.h>
#include "tilda_window.h"

G_BEGIN_DECLS

void keybindings_model_init(GtkBuilder *xml);
gboolean save_keybindings(const tilda_window *tw);
void apply_keybindings();

G_END_DECLS

#endif


