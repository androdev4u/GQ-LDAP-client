/*
    GQ -- a GTK-based LDAP client
    Copyright (C) 1998-2003 Bert Vermeulen
    Copyright (C) 2002-2003 Peter Stamfest

    This program is released under the Gnu General Public License with
    the additional exemption that compiling, linking, and/or using
    OpenSSL is allowed.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/* $Id: template.c 975 2006-09-07 18:44:41Z herzi $ */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */

#ifdef HAVE_LDAP_STR2OBJECTCLASS

#include <string.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <lber.h>
#include <ldap.h>
#include <ldap_schema.h>

#include "mainwin.h"
#include "configfile.h"
#include "common.h"
#include "gq-tab-schema.h"
#include "util.h"
#include "schema.h"
#include "template.h"
#include "tdefault.h"
#include "errorchain.h"
#include "debug.h"
#include "state.h"


extern GtkWidget *current_template_clist;
static GtkWidget *templatewin;

static void build_used_attrs_list(GtkWidget *window, int context);

/* malloc and init gq_template struct */
struct gq_template *new_template(void)
{
     struct gq_template *tmpl;

     tmpl = g_malloc0(sizeof(struct gq_template));

     tmpl->name = g_strdup("");
     tmpl->objectclasses = NULL;

     return tmpl;
}

void free_template(struct gq_template *tmpl)
{
     g_free(tmpl->name);
     
     if (tmpl->objectclasses) {
	  g_list_foreach(tmpl->objectclasses, (GFunc) g_free, NULL);
     }
     g_list_free(tmpl->objectclasses);
     g_free(tmpl);
}

void create_template_edit_window(GqServer *server, 
				 const char *templatename,
				 GtkWidget *modalFor)
{
     GList *list;
     GString *gmessage;
     GtkWidget *vbox1, *hbox0, *hbox1, *vbox2, *vbox3, *vbox4;
     GtkWidget *hbox2, *hbox3, *hbox4, *scrwin;
     GtkWidget *oclist, *templatelist, *clist, *button, *arrow, *label, *entry;
     struct server_schema *ss;
     struct gq_template *tmpl;
     char *otitle[1] = { _("Available objectclasses") };
     char *ttitle[1] = { _("Template objectclasses") };
     char *rtitle[1] = { _("Required attributes") };
     char *atitle[1] = { _("Allowed attributes") };

     int create_context = error_new_context(_("Opening template window"),
					    modalFor);

     gmessage = NULL;
     ss = get_schema(create_context, server);
     if(ss == NULL) {
	  gmessage = g_string_sized_new(64);
	  g_string_sprintf(gmessage,
			   _("No schema information found for server %s"),
			   server->name);
     }
     else if(ss->oc == NULL) {
	  gmessage = g_string_sized_new(64);
	  g_string_sprintf(gmessage,
			   _("No objectclass information found for server %s"),
			   server->name);
     }
     else if(ss->at == NULL) {
	  gmessage = g_string_sized_new(64);
	  g_string_sprintf(gmessage,
			   _("No attribute type information found for server %s"),
			   server->name);
     }

     if(gmessage) {
	  error_push(create_context, gmessage->str);
	  g_string_free(gmessage, TRUE);

	  goto done;
     }

     templatewin = stateful_gtk_window_new(GTK_WINDOW_TOPLEVEL,
					   "template", 700, 450);
     if (modalFor) {
	  g_assert(GTK_IS_WINDOW(modalFor));
	  gtk_window_set_modal(GTK_WINDOW(templatewin), TRUE);
	  gtk_window_set_transient_for(GTK_WINDOW(templatewin),
				       GTK_WINDOW(modalFor));
     }

     gtk_object_set_data_full(GTK_OBJECT(templatewin), "server",
			      g_object_ref(server), g_object_unref);

     gtk_container_border_width(GTK_CONTAINER(templatewin), 10);
     g_signal_connect(templatewin, "key_press_event",
			G_CALLBACK(close_on_esc), templatewin);

     vbox1 = gtk_vbox_new(FALSE, 0);
     gtk_container_add(GTK_CONTAINER(templatewin), vbox1);
     gtk_widget_show(vbox1);

     hbox0 = gtk_hbox_new(FALSE, 0);
     gtk_widget_show(hbox0);
     gtk_box_pack_start(GTK_BOX(vbox1), hbox0, FALSE, FALSE, 0);

     hbox1 = gtk_hbox_new(FALSE, 20);
     gtk_widget_show(hbox1);
     gtk_box_pack_start(GTK_BOX(vbox1), hbox1, TRUE, TRUE, 5);

     /* Schema objectclasses */
     scrwin = gtk_scrolled_window_new(NULL, NULL);
     gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrwin),
				    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
     gtk_widget_show(scrwin);
     gtk_box_pack_start(GTK_BOX(hbox1), scrwin, TRUE, TRUE, 0);

     oclist = gtk_clist_new_with_titles(1, otitle);
     GTK_CLIST(oclist)->button_actions[1] = GTK_BUTTON_SELECTS;
#ifdef OLD_FOCUS_HANDLING
     GTK_WIDGET_UNSET_FLAGS(oclist, GTK_CAN_FOCUS);
#endif
     gtk_object_set_data_full(GTK_OBJECT(oclist), "server",
			      g_object_ref(server), g_object_unref);

     gtk_object_set_data(GTK_OBJECT(templatewin), "oclist", oclist);
     gtk_clist_set_compare_func(GTK_CLIST(oclist), clistcasecmp);
     gtk_clist_column_titles_passive(GTK_CLIST(oclist));
     g_signal_connect(oclist, "select_row",
                        G_CALLBACK(select_oc_from_clist), NULL);
     gtk_widget_show(oclist);
     gtk_container_add(GTK_CONTAINER(scrwin), oclist);

     /* the two vboxes take care of placing the arrows *just so* */
     vbox2 = gtk_vbox_new(FALSE, 0);
     gtk_widget_show(vbox2);
     gtk_box_pack_start(GTK_BOX(hbox1), vbox2, FALSE, FALSE, 0);

     vbox3 = gtk_vbox_new(FALSE, 0);
     gtk_widget_show(vbox3);
     gtk_box_pack_start(GTK_BOX(vbox2), vbox3, TRUE, FALSE, 0);

     /* right arrow */
     button = gtk_button_new();
     gtk_widget_set_usize(button, 20, 30);
#ifdef OLD_FOCUS_HANDLING
     GTK_WIDGET_UNSET_FLAGS(button, GTK_CAN_FOCUS);
#endif
     gtk_object_set_data(GTK_OBJECT(button), "direction", "right");
     arrow = gtk_arrow_new(GTK_ARROW_RIGHT, GTK_SHADOW_OUT);
     gtk_widget_show(arrow);
     gtk_container_add(GTK_CONTAINER(button), arrow);
     g_signal_connect(button, "clicked",
			G_CALLBACK(arrow_button_callback), templatewin);
     gtk_widget_show(button);
     gtk_box_pack_start(GTK_BOX(vbox3), button, TRUE, TRUE, 10);

     /* left arrow */
     button = gtk_button_new();
     gtk_widget_set_usize(button, 20, 30);
#ifdef OLD_FOCUS_HANDLING
     GTK_WIDGET_UNSET_FLAGS(button, GTK_CAN_FOCUS);
#endif
     gtk_object_set_data(GTK_OBJECT(button), "direction", "left");
     arrow = gtk_arrow_new(GTK_ARROW_LEFT, GTK_SHADOW_OUT);
     gtk_widget_show(arrow);
     gtk_container_add(GTK_CONTAINER(button), arrow);
     g_signal_connect(button, "clicked",
			G_CALLBACK(arrow_button_callback), templatewin);
     gtk_widget_show(button);
     gtk_box_pack_start(GTK_BOX(vbox3), button, TRUE, TRUE, 10);

     /* Template objectclasses */
     scrwin = gtk_scrolled_window_new(NULL, NULL);
     gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrwin),
				    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
     gtk_widget_show(scrwin);
     gtk_box_pack_start(GTK_BOX(hbox1), scrwin, TRUE, TRUE, 0);

     templatelist = gtk_clist_new_with_titles(1, ttitle);
     GTK_CLIST(templatelist)->button_actions[1] = GTK_BUTTON_SELECTS;
#ifdef OLD_FOCUS_HANDLING
     GTK_WIDGET_UNSET_FLAGS(templatelist, GTK_CAN_FOCUS);
#endif
     gtk_object_set_data_full(GTK_OBJECT(templatelist), "server",
			      g_object_ref(server), g_object_unref);

     gtk_object_set_data(GTK_OBJECT(templatewin), "templatelist", templatelist);
     gtk_clist_column_titles_passive(GTK_CLIST(templatelist));
     g_signal_connect(templatelist, "select_row",
                        G_CALLBACK(select_oc_from_clist), NULL);
     gtk_widget_show(templatelist);
     gtk_container_add(GTK_CONTAINER(scrwin), templatelist);

     /* vbox to hold required and allowed attributes clists */
     vbox4 = gtk_vbox_new(FALSE, 0);
     gtk_widget_show(vbox4);
     gtk_box_pack_start(GTK_BOX(hbox1), vbox4, TRUE, TRUE, 0);

     /* Required attributes */
     scrwin = gtk_scrolled_window_new(NULL, NULL);
     gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrwin),
				    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
     gtk_widget_show(scrwin);
     clist = gtk_clist_new_with_titles(1, rtitle);
     GTK_CLIST(clist)->button_actions[1] = GTK_BUTTON_SELECTS;
#ifdef OLD_FOCUS_HANDLING
     GTK_WIDGET_UNSET_FLAGS(clist, GTK_CAN_FOCUS);
#endif
     gtk_object_set_data_full(GTK_OBJECT(clist), "server",
			      g_object_ref(server), g_object_unref);

     gtk_object_set_data(GTK_OBJECT(templatewin), "reqdattr", clist);
     gtk_clist_column_titles_passive(GTK_CLIST(clist));
     g_signal_connect(clist, "select_row",
                        G_CALLBACK(select_at_from_clist), NULL);
     gtk_widget_show(clist);
     gtk_container_add(GTK_CONTAINER(scrwin), clist);
     gtk_box_pack_start(GTK_BOX(vbox4), scrwin, TRUE, TRUE, 0);

     /* Allowed attributes */
     scrwin = gtk_scrolled_window_new(NULL, NULL);
     gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrwin),
				    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
     gtk_widget_show(scrwin);
     clist = gtk_clist_new_with_titles(1, atitle);
     GTK_CLIST(clist)->button_actions[1] = GTK_BUTTON_SELECTS;
#ifdef OLD_FOCUS_HANDLING
     GTK_WIDGET_UNSET_FLAGS(clist, GTK_CAN_FOCUS);
#endif
     gtk_object_set_data_full(GTK_OBJECT(clist), "server",
			      g_object_ref(server), g_object_unref);

     gtk_object_set_data(GTK_OBJECT(templatewin), "allowedattr", clist);
     gtk_clist_column_titles_passive(GTK_CLIST(clist));
     g_signal_connect(clist, "select_row",
                        G_CALLBACK(select_at_from_clist), NULL);
     gtk_widget_show(clist);
     gtk_container_add(GTK_CONTAINER(scrwin), clist);
     gtk_box_pack_start(GTK_BOX(vbox4), scrwin, TRUE, TRUE, 0);

     hbox2 = gtk_hbox_new(FALSE, 0);
     gtk_widget_show(hbox2);
     gtk_box_pack_start(GTK_BOX(vbox1), hbox2, FALSE, FALSE, 10);

#if EXPERIMENTAL_TEMPLATE_DEFAULTS
     /* Input defaults */
     button = gtk_button_new_with_label("  Edit input defaults  ");
     GTK_WIDGET_UNSET_FLAGS(button, GTK_CAN_DEFAULT);
#ifdef OLD_FOCUS_HANDLING
     GTK_WIDGET_UNSET_FLAGS(button, GTK_CAN_FOCUS);
#endif
     g_signal_connect(button, "clicked",
			G_CALLBACK(create_tdefault_edit_window),
			templatewin);
     gtk_widget_show(button);
     gtk_box_pack_start(GTK_BOX(hbox2), button, FALSE, FALSE, 0);
#endif

     hbox3 = gtk_hbox_new(FALSE, 0);
     gtk_widget_show(hbox3);
     gtk_box_pack_start(GTK_BOX(vbox1), hbox3, FALSE, FALSE, 10);

     /* Template name */
     label = gtk_label_new(_("Template name"));
     gtk_widget_show(label);
     gtk_box_pack_start(GTK_BOX(hbox3), label, FALSE, FALSE, 0);

     entry = gtk_entry_new();
     gtk_entry_set_max_length(GTK_ENTRY(entry), 127);
     gtk_object_set_data(GTK_OBJECT(templatewin), "templatenamebox", entry);
     g_signal_connect(entry, "activate",
			G_CALLBACK(save_template_callback),
			templatewin);
     gtk_widget_show(entry);
     gtk_box_pack_start(GTK_BOX(hbox3), entry, FALSE, FALSE, 10);

     hbox4 = gtk_hbutton_box_new();
     gtk_widget_show(hbox4);
     gtk_box_pack_start(GTK_BOX(vbox1), hbox4, FALSE, FALSE, 10);

     /* Save */
     button = gtk_button_new_from_stock(GTK_STOCK_SAVE);
     GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
     g_signal_connect(button, "clicked",
			G_CALLBACK(save_template_callback),
			templatewin);
     gtk_widget_show(button);
     gtk_box_pack_start(GTK_BOX(hbox4), button, FALSE, FALSE, 0);
     gtk_widget_grab_default(button);

     /* Cancel */
     button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
     g_signal_connect_swapped(button, "clicked",
			       G_CALLBACK(gtk_widget_destroy),
			       templatewin);
     gtk_widget_show(button);
     gtk_box_pack_end(GTK_BOX(hbox4), button, FALSE, FALSE, 0);

     fill_new_template(templatewin);

     /* default title, in case editing doesn't work out */
     gtk_window_set_title(GTK_WINDOW(templatewin),
			  _("GQ: create new template"));

     if(templatename) {
	  gtk_window_set_title(GTK_WINDOW(templatewin),
			       _("GQ: edit template"));
	  gtk_object_set_data_full(GTK_OBJECT(templatewin),
				   "templatename", 
				   g_strdup(templatename), g_free);
	  tmpl = find_template_by_name(templatename);
	  if(tmpl) {
	       gtk_entry_set_text(GTK_ENTRY(entry), tmpl->name);
	       list = tmpl->objectclasses;
	       while(list) {
		    /* a bit slow maybe */
		    move_objectclass(oclist, templatelist, 
				     (const char *) list->data);
		    build_used_attrs_list(templatewin, create_context);
		    list = list->next;
	       }
	  }
     }
     else {
	  /* a new template has the "top" objectclass selected
	     by default (LDAP V3 requirement) */
	  move_objectclass(oclist, templatelist, "top");
	  build_used_attrs_list(templatewin, create_context);
     }

     gtk_widget_show(templatewin);

     gtk_widget_grab_focus(entry);

 done:
     error_flush(create_context);
}


/*
 * initialize template editor
 */
void fill_new_template(GtkWidget *window)
{
     GList *list;
     GtkWidget *oclist, *templatelist;
     LDAPObjectClass *oc;
     GqServer *server;
     char *ocname[1];

     if( (oclist = gtk_object_get_data(GTK_OBJECT(window), "oclist")) == NULL)
	  return;

     if( (templatelist = gtk_object_get_data(GTK_OBJECT(window), "templatelist")) == NULL)
	  return;

     if( (server = gtk_object_get_data(GTK_OBJECT(window), "server")) == NULL)
	  return;

     gtk_clist_freeze(GTK_CLIST(oclist));
     gtk_clist_clear(GTK_CLIST(oclist));
     list = server->ss->oc;
     while(list) {
	  oc = list->data;
	  if(oc) {
	       if(oc->oc_names && oc->oc_names[0])
		    ocname[0] = oc->oc_names[0];
	       else
		    ocname[0] = oc->oc_oid;
	       gtk_clist_append(GTK_CLIST(oclist), ocname);
	  }
	  list = list->next;
     }
     gtk_clist_thaw(GTK_CLIST(oclist));

}


/*
 * move objectclass selected in "Objectclasses" clist to "Template" clist
 */
void arrow_button_callback(GtkWidget *widget, GtkWidget *window)
{
     GtkWidget *oclist, *templatelist;
     char *direction, *objectclass;
     int context;

     if( (oclist = gtk_object_get_data(GTK_OBJECT(window), "oclist")) == NULL)
	  return;

     if( (templatelist = gtk_object_get_data(GTK_OBJECT(window), "templatelist")) == NULL)
	  return;

     if( (direction = gtk_object_get_data(GTK_OBJECT(widget), "direction")) == NULL)
	  return;

     if(!strcmp(direction, "right")) {
	  objectclass = get_clist_selection(oclist);
	  move_objectclass(oclist, templatelist, objectclass);
     }
     else {
	  objectclass = get_clist_selection(templatelist);
	  move_objectclass(templatelist, oclist, objectclass);
	  gtk_clist_sort(GTK_CLIST(oclist));
     }

     context = 
	  error_new_context(_("Adding/deleting objectclass to/from template"),
			    window);

     build_used_attrs_list(window, context);

     error_flush(context);
}


/*
 * default clist sort function is case-sensitive, this does a case-insensitive
 * sort instead
 */
int clistcasecmp(GtkCList *clist, gconstpointer ptr1, gconstpointer ptr2)
{
     GtkCListRow *row1, *row2;

     row1 = (GtkCListRow *) ptr1;
     row2 = (GtkCListRow *) ptr2;

     return(strcasecmp(GTK_CELL_TEXT(row1->cell[0])->text,
		       GTK_CELL_TEXT(row2->cell[0])->text));
}


/*
 * move item by name from source clist to destination clist
 */
void move_objectclass(GtkWidget *source, GtkWidget *destination, 
		      const char *objectclass)
{
     int row;
     char *dummy[1];

     if(objectclass == NULL)
	  return;

     row = get_clist_row_by_text(source, objectclass);
     if(row == -1)
	  return;

     dummy[0] = g_strdup(objectclass);
     gtk_clist_remove(GTK_CLIST(source), row);

     gtk_clist_append(GTK_CLIST(destination), dummy);
     g_free(dummy[0]);

}


/*
 * build "Required attributes" and "Allowed attributes" clists from
 * current objectclasses in "Template" clist
 */
static void build_used_attrs_list(GtkWidget *window, int context)
{
     LDAPObjectClass *oc;
     GtkWidget *templatelist, *reqdlist, *allowedlist;
     GqServer *server;
     struct server_schema *ss;
     int i, attr;
     char *objectclass, *dummy[1];

     if( (server = gtk_object_get_data(GTK_OBJECT(window), "server")) == NULL)
	  return;

     if( (templatelist = gtk_object_get_data(GTK_OBJECT(window), "templatelist")) == NULL)
	  return;

     if( (reqdlist = gtk_object_get_data(GTK_OBJECT(window), "reqdattr")) == NULL)
	  return;

     if( (allowedlist = gtk_object_get_data(GTK_OBJECT(window), "allowedattr")) == NULL)
	  return;

     ss = get_server_schema(context, server);

     gtk_clist_freeze(GTK_CLIST(reqdlist));
     gtk_clist_clear(GTK_CLIST(reqdlist));
     gtk_clist_freeze(GTK_CLIST(allowedlist));
     gtk_clist_clear(GTK_CLIST(allowedlist));
     for(i = 0; i < GTK_CLIST(templatelist)->rows; i++) {
	  gtk_clist_get_text(GTK_CLIST(templatelist), i, 0, &objectclass);
	  oc = find_oc_by_oc_name(ss, objectclass);
	  if(oc) {

	       /* Required attributes */
	       if(oc->oc_at_oids_must)
		    for(attr = 0; oc->oc_at_oids_must[attr]; attr++)
			 if(get_clist_row_by_text(reqdlist, oc->oc_at_oids_must[attr]) == -1) {
			      dummy[0] = oc->oc_at_oids_must[attr];
			      gtk_clist_append(GTK_CLIST(reqdlist), dummy);
			 }

	       /* Allowed attributes */
	       if(oc->oc_at_oids_may)
		    for(attr = 0; oc->oc_at_oids_may[attr]; attr++)
			 if(get_clist_row_by_text(allowedlist, oc->oc_at_oids_may[attr]) == -1) {
			      dummy[0] = oc->oc_at_oids_may[attr];
			      gtk_clist_append(GTK_CLIST(allowedlist), dummy);
			 }

	  }

     }
     gtk_clist_thaw(GTK_CLIST(reqdlist));
     gtk_clist_thaw(GTK_CLIST(allowedlist));

}


/*
 * save new template or update existing template. Also saves to .gq
 * and destroys window
 */
void save_template_callback(GtkWidget *widget, GtkWidget *window)
{
     struct gq_template *old_tmpl, *tmpl;
     char *old_template_name;
     int error_context;
     gboolean save_ok = FALSE;

     error_context = error_new_context(_("Saving template"), widget);

     tmpl = window2template(window);
     if(tmpl == NULL) {
	  /* window2template() failed */
	  error_push(error_context, _("Could not create new template."));
	  goto done;
     }

     if(strlen(tmpl->name) == 0) {
	  /* need a name for this template, warning popup */
	  error_push(error_context,
		     _("You need to enter a name for the new template."));
	  free_template(tmpl);
	  goto done;
     }

     if( (old_template_name = gtk_object_get_data(GTK_OBJECT(window), "templatename")) == NULL) {
	  /* add new template to list, but make sure the template name is unique */
	  if( (find_template_by_name(tmpl->name)) == NULL) {
	       config->templates = g_list_append(config->templates, tmpl);
	  } else {
	       error_push(error_context,
			  _("A template with that name already exists!"));
	       free_template(tmpl);
	       goto done;
	  }
	  save_ok = save_config_ext(error_context);
	  if (!save_ok) {
	       /* save failed - undo changes */
	       config->templates = g_list_remove(config->templates, tmpl);
	       free_template(tmpl);
	  }
     } else {
	  GList *l;
	  /* update existing template */
	  old_tmpl = find_template_by_name(old_template_name);
	  
	  /* In order to encapsulate knowledge about the internal
	     structure of a template object, updating is done via
	     delete/insert. This is less optimal than the old
	     behaviour to change the filter in-place

	     (especially in case some other part of gq holds a pointer
	     to the template)
	  */
	  if(old_tmpl) {
	       l = g_list_find(config->templates, old_tmpl);
	       l->data = tmpl;

	       save_ok = save_config_ext(error_context);
	       if (save_ok) {
		    free_template(old_tmpl);
	       } else {
		    l->data = old_tmpl;
		    free_template(tmpl);
	       }
	  } else {
	       /* hmmmm... template was deleted while editing it. Just
		  add it in then. */
	       config->templates = g_list_append(config->templates, tmpl);
	       save_ok = save_config_ext(error_context);
	       if (!save_ok) {
		    config->templates = g_list_remove(config->templates, tmpl);
		    free_template(tmpl);
	       }
	  }
     }

     if (save_ok) {
	  fill_clist_templates(current_template_clist);
	  gtk_widget_destroy(window);
     }

 done:
     error_flush(error_context);
}


/*
 * extract template objectclasses clist from window, and build
 * a (malloc'ed) gq_template struct with it
 */
struct gq_template *window2template(GtkWidget *window)
{
     GList *list;
     GtkWidget *templatenamebox, *templatelist;
     struct gq_template *tmpl;
     int i;
     char *objectclass;
     const char *c;

     if( (templatelist = gtk_object_get_data(GTK_OBJECT(window), "templatelist")) == NULL)
	  return(NULL);

     if( (templatenamebox = gtk_object_get_data(GTK_OBJECT(window), "templatenamebox")) == NULL)
	  return(NULL);

     if( (tmpl = new_template()) == NULL)
	  return(NULL);

     
     c = gtk_entry_get_text(GTK_ENTRY(templatenamebox));
     g_free_and_dup(tmpl->name, c);

     list = NULL;
     for(i = 0; i < GTK_CLIST(templatelist)->rows; i++) {
	  gtk_clist_get_text(GTK_CLIST(templatelist), i, 0, &objectclass);
	  list = g_list_append(list, g_strdup(objectclass));

     }
     tmpl->objectclasses = list;

     return(tmpl);
}


/*
 * fill clist with names of templates
 */
void fill_clist_templates(GtkWidget *clist)
{
     GList *list;
     struct gq_template *tmpl;
     char *dummy[1];

     gtk_clist_freeze(GTK_CLIST(clist));
     gtk_clist_clear(GTK_CLIST(clist));

     list = config->templates;
     while(list) {
	  if( (tmpl = (struct gq_template *) list->data)) {
	       dummy[0] = tmpl->name;
	       gtk_clist_append(GTK_CLIST(clist), dummy);
	  }
	  list = list->next;
     }

     gtk_clist_thaw(GTK_CLIST(clist));

}


/*
 * find selected item in clist by clist->data (char *)
 * return clist row string
 */
char *get_clist_selection(GtkWidget *clist)
{
     GList *list;
     char *objectclass;

     objectclass = NULL;
     if( (list = GTK_CLIST(clist)->selection))
	  gtk_clist_get_text(GTK_CLIST(clist), GPOINTER_TO_INT(list->data),
			     0, &objectclass);

     return(objectclass);
}


/*
 * find selected item in clist by row string
 * return row number
 */
int get_clist_row_by_text(GtkWidget *clist, const char *text)
{
     int i, row;
     char *str;

     row = -1;
     for(i = 0; i < GTK_CLIST(clist)->rows; i++) {
	  gtk_clist_get_text(GTK_CLIST(clist), i, 0, &str);
	  if(!strcasecmp(str, text)) {
	       row = i;
	       break;
	  }
     }

     return(row);
}


#endif /* HAVE_LDAP_STR2OBJECTCLASS */

/* 
   Local Variables:
   c-basic-offset: 5
   End:
 */
