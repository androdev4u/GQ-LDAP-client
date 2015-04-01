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

/* $Id: dt_oc.c 975 2006-09-07 18:44:41Z herzi $ */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */

/* Doesn't make sense without schema information */
#ifdef HAVE_LDAP_STR2OBJECTCLASS

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "dt_oc.h"

#include "common.h"
#include "gq-tab-browse.h"
#include "util.h"
#include "errorchain.h"
#include "input.h"
#include "tinput.h"
#include "encode.h"
#include "ldif.h" /* for b64_decode */
#include "syntax.h"
#include "schema.h"

struct change_info {
     struct inputform *iform;
     char *oldtext;
     char *newtext;
     GtkWidget *combo;
     int id;
};


static void free_change_info(struct change_info *ci) {

     if (ci->newtext) g_free(ci->newtext);
     if (ci->oldtext) g_free(ci->oldtext);
     ci->newtext = ci->oldtext = NULL;

     if (ci->id) {
	  gtk_timeout_remove(ci->id);
	  ci->id = 0;
     }

     /* XXX do we need to call inputform_free() first? */
     ci->iform = NULL;
     ci->combo = NULL;

     g_free(ci);

}


static gboolean do_change(int error_context, struct change_info *ci);

/* Oh what a dirty hack - in order to allow browse selection to work
   normally we register a timeout job to wait until selection via the
   combo is done. This is sooooo dirty!! */

static int timeout_oc(struct change_info *ci)
{
     GtkCombo *c = GTK_COMBO(ci->combo);
     GtkEntry *e = GTK_ENTRY(c->entry);

/*       printf("idle check %08x %08x %08x %08x\n",  */
/*	    GTK_WIDGET_FLAGS (c->list), */
/*	    GTK_WIDGET_FLAGS (c->popup), */
/*	    GTK_WIDGET_VISIBLE(c->popwin), */
/*	    GTK_WIDGET_FLAGS (c->popwin) */
/*	    ); */

     /* NOTE :: ATTENTION :: NOTE

	Using c->popwin is a NON-DOCUMENTED FEATURE of GTK_COMBO!!!!!!

      */

     if (!GTK_WIDGET_VISIBLE(c->popwin)) {
	  gchar *newtext;
	  gboolean rebuild = FALSE;
	  int ctx;

	  gtk_timeout_remove(ci->id);
	  ci->id = 0;

/*	  printf("working!!\n"); */

	  newtext = gtk_editable_get_chars(GTK_EDITABLE(e), 0, -1);
	  if (ci->newtext) g_free(ci->newtext);
	  ci->newtext = newtext;

	  ctx = error_new_context(_("Changing objectClass attribute"),
				  GTK_WIDGET(c));

	  rebuild = do_change(ctx, ci);
	  if (rebuild) {
	       build_or_update_inputform(ctx, ci->iform, FALSE);
	  }

	  error_flush(ctx);
	  return FALSE;
     }

     /* try again */
     return TRUE;
}


static gboolean check_if_any_oc_needs(GList *oclist, LDAPAttributeType *at,
				      struct server_schema *ss)
{
     int i;
     char **atnames;

     for ( ; oclist ; oclist = g_list_next(oclist)) {
	  LDAPObjectClass *oc = (LDAPObjectClass *) oclist->data;

	  /* walk all mandatory attributes */
	  for(i = 0; oc->oc_at_oids_must && oc->oc_at_oids_must[i]; i++) {
	       for (atnames = at->at_names ; atnames && *atnames ; atnames++) {
		    if (strcasecmp(oc->oc_at_oids_must[i], *atnames) == 0)
			 return TRUE;
	       }
	  }
	  /* walk all optional attributes */
	  for(i = 0; oc->oc_at_oids_may && oc->oc_at_oids_may[i]; i++) {
	       for (atnames = at->at_names ; atnames && *atnames ; atnames++) {
		    if (strcasecmp(oc->oc_at_oids_may[i], *atnames) == 0)
			 return TRUE;
	       }
	  }
     }
     return FALSE;
}

static void new_oc(GtkEditable *entry, struct change_info *ci)
{
     if (ci->id == 0)
	  ci->id = gtk_timeout_add(25,
				   (GtkFunction)timeout_oc,
				   ci);
}

static gboolean do_change(int error_context, struct change_info *ci)
{
     gchar *newtext = ci->newtext;
     gchar *oldtext = ci->oldtext;
     struct formfill *form;
     GqServer *server;
     GList *formlist, *tmplist = NULL;
     int i, n = 0;
     struct server_schema *ss;
     LDAPObjectClass *oc;
     LDAPAttributeType *at;
     char **cp;

/*       printf("changed: %s->%s\n", oldtext, newtext); */

     server = ci->iform->server;

     if (strcmp(newtext, oldtext) == 0) {
	  /* nothing changed, fastpath is just to return */
	  return TRUE;
     }

     update_formlist(ci->iform);

     formlist = ci->iform->formlist;

     /* check if parent objectClass is already there, if it isn't pop
        up an error message */

     ss = get_schema(error_context, server);

     if (strlen(newtext) > 0) {
	  form = lookup_attribute_using_schema(formlist,
					       "objectClass",
					       ss, NULL);

	  oc = find_oc_by_oc_name(ss, newtext);
	  if (oc) {
	       for ( cp = oc->oc_sup_oids ; cp && *cp ; cp++) {
		    /* check availability of superior class ... */
		    GList *t;
		    int found = 0;
		    for (t = form->values ; t ; t = g_list_next(t)) {
			 GByteArray *gb = t->data;
			 if (!gb) continue;
			 if (strncasecmp((gchar*)gb->data, *cp,
					 MAX(gb->len, strlen(*cp))) == 0) {
			      /* OK, found */
			      found = 1;
			      break;
			 }
		    }

		    if (!found) {
			 /* this will fire the change signals again, thus
			    the changeinfo data structure may not be freed */
			 gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(ci->combo)->entry), oldtext);
			 g_free(newtext);
			 ci->newtext = NULL;

			 error_push(error_context,
				    _("Missing superior objectClass %s"), *cp);

			 return FALSE;
		    }
	       }
	  }
     }


     /* lookup objectclass */

     if (strlen(oldtext) > 0) {
	  /* removed objectClass */
/*	  printf("del %s\n", oldtext); */
	  oc = find_oc_by_oc_name(ss, oldtext);

	  /* NOTE: cannot remove top objectClass */
	  if (oc || !strcasecmp("top", oldtext)) {
	       /* construct temporary list of objectClasses WITHOUT the one
		  we are about to delete */
	       form = lookup_attribute_using_schema(formlist,
						    "objectClass",
						    ss, NULL);
	       if (form) {
		    GList *t;
		    for (t = form->values ; t ; t = g_list_next(t)) {
			 LDAPObjectClass *oc2;
			 GByteArray *gb = t->data;
			 gchar *c = g_malloc(gb->len + 1);
			 memcpy(c, gb->data, gb->len);
			 c[gb->len] = 0;

			 oc2 = find_oc_by_oc_name(ss, c);

			 /* filter out the to-be-deleted objectClass */
			 if (oc2 == oc) {
			      g_free(c);
			      continue;
			 }

			 tmplist = g_list_append(tmplist, oc2);

			 g_free(c);
		    }
	       }

	       /* Mark no-longer-needed attributes */

	       /* required attributes */
	       for(i = 0; oc->oc_at_oids_must && oc->oc_at_oids_must[i]; i++) {
		    /* does any other objectclass require/allow
		       this attribute? */

		    at = find_canonical_at_by_at(ss, oc->oc_at_oids_must[i]);
		    if (!at) continue;

		    if (!check_if_any_oc_needs(tmplist, at, ss)) {
			 form = lookup_attribute_using_schema(formlist,
							      oc->oc_at_oids_must[i],
							      ss, NULL);
			 if (form) {
			      form->flags |= FLAG_DEL_ME;
			      n++;
			 }
		    }
	       }

	       /* allowed attributes */
	       for(i = 0; oc->oc_at_oids_may && oc->oc_at_oids_may[i]; i++) {
		    /* does any other objectclass require/allow
		       this attribute? */

		    at = find_canonical_at_by_at(ss, oc->oc_at_oids_may[i]);

		    if (!at) continue;
		    if (!check_if_any_oc_needs(tmplist, at, ss)) {
			 form = lookup_attribute_using_schema(formlist,
							      oc->oc_at_oids_may[i],
							      ss, NULL);
			 if (form) {
			      form->flags |= FLAG_DEL_ME;
			      n++;
			 }
		    }
	       }
	       statusbar_msg(_("Marked %d attribute(s) to be obsolete"), n);
	  }
     }

     /* added objectClass */

     n = 0;
     if (strlen(newtext) > 0) {
	  oc = find_oc_by_oc_name(ss, newtext);
	  if (oc) {
	       at = NULL;

	       /* required attributes */
	       for(i = 0; oc->oc_at_oids_must && oc->oc_at_oids_must[i]; i++) {
		    form = lookup_attribute_using_schema(formlist,
							 oc->oc_at_oids_must[i],
							 ss, &at);
		    if (form) {
			 form->flags &= ~FLAG_DEL_ME;
			 continue;
		    }
		    form = new_formfill();
		    g_assert(form);

		    form->server = g_object_ref(server);

		    g_free(form->attrname);
		    form->attrname = g_strdup(oc->oc_at_oids_must[i]);
		    form->flags |= FLAG_MUST_IN_SCHEMA;
		    if (at && at->at_single_value) {
			 form->flags |= FLAG_SINGLE_VALUE;
		    }
		    if (at && at->at_no_user_mod) {
			 form->flags |= FLAG_NO_USER_MOD;
		    }
		    set_displaytype(error_context, server, form);
		    formlist = formlist_append(formlist, form);
		    n++;
	       }

	       /* allowed attributes */
	       for(i = 0; oc->oc_at_oids_may && oc->oc_at_oids_may[i]; i++) {
		    form = lookup_attribute_using_schema(formlist,
							 oc->oc_at_oids_may[i],
							 ss, &at);
		    if (form) {
			 form->flags &= ~FLAG_DEL_ME;
			 continue;
		    }

		    form = new_formfill();
		    g_assert(form);

		    form->server = g_object_ref(server);

		    g_free(form->attrname);
		    form->attrname = g_strdup(oc->oc_at_oids_may[i]);
		    if (at && at->at_single_value) {
			 form->flags |= FLAG_SINGLE_VALUE;
		    }
		    if (at && at->at_no_user_mod) {
			 form->flags |= FLAG_NO_USER_MOD;
		    }
		    set_displaytype(error_context, server, form);
		    formlist = formlist_append(formlist, form);
		    n++;
	       }
	  }
	  ci->iform->formlist = formlist;
     }
     statusbar_msg(_("Added %d attribute(s) from new objectClass"), n);

     g_free(oldtext);
     ci->oldtext = newtext;
     ci->newtext = NULL;

     return TRUE;
}


GtkWidget *dt_oc_get_widget(int error_context,
			    struct formfill *form, GByteArray *data,
			    GCallback *activatefunc,
			    gpointer funcdata)
{
     GtkWidget *combo;
     GList *oclist;
     GList *pop = NULL;
     GqServer *server;
     struct server_schema *ss;
     struct change_info *ci;
     struct inputform *iform;
     char *octmp = NULL;

     iform = (struct inputform *) funcdata;
     server = iform->server;
     combo = gtk_combo_new();
     gtk_combo_set_value_in_list(GTK_COMBO(combo), TRUE, TRUE);

     pop = NULL;
     pop = g_list_append(pop, "");

     ss = get_schema(error_context, server);

     if (ss) {
	  /* turn data->data into a nul terminated string ... */

	  if (data) {
	       octmp = g_malloc0( data->len + 1);
	       strncpy(octmp, (gchar*)data->data, data->len); /* UTF-8 OK */
	  }
	  oclist = ss->oc;
	  while(oclist) {
	       LDAPObjectClass *oc = (LDAPObjectClass *) oclist->data;
	       if (oc && oc->oc_names) {
		    if (octmp && strcasecmp(oc->oc_names[0], octmp) == 0) {
			 /* add the current objectClass as the first
			    element to the dropdown list.

			    NOTE: we lookup the objectclass from the list,
			    as we want to use a fixed string for the list -
			    we cannot just strdup the data->data stuff, as
			    we would never be able to free it!!! */

			 pop = g_list_insert(pop, oc->oc_names[0], 0);
		    } else {
			 pop = g_list_append(pop, oc->oc_names[0]);
		    }
	       }
	       oclist = g_list_next(oclist);
	  }

	  if (octmp) g_free(octmp);

	  gtk_combo_set_popdown_strings(GTK_COMBO(combo),
				   pop);

	  if (pop) g_list_free(pop);

	  gtk_editable_set_editable(GTK_EDITABLE(GTK_COMBO(combo)->entry),
				    FALSE);
     } else {
	  /* make the combobox editable if we do not have the last
	     resort schema server around */
	  gtk_editable_set_editable(GTK_EDITABLE(GTK_COMBO(combo)->entry),
				    TRUE);
     }

     ci = (struct change_info*) g_malloc(sizeof(struct change_info));
     ci->iform = iform;
     ci->newtext = NULL;
     ci->oldtext = NULL;
     ci->combo = combo;
     ci->id = 0;

     gtk_object_set_data_full(GTK_OBJECT(combo), "ci", ci,
			      (GtkDestroyNotify) free_change_info);

     dt_oc_set_data(form, data, combo);

     g_signal_connect_after(GTK_COMBO(combo)->entry,
			      "changed",
			      G_CALLBACK(new_oc),
			      ci);

     return combo;
}


GByteArray *dt_oc_get_data(struct formfill *form, GtkWidget *widget)
{
	GQTypeDisplayClass* klass;
    GByteArray *copy;
    int l;

    gchar *content = gtk_editable_get_chars(GTK_EDITABLE(GTK_COMBO(widget)->entry), 0, -1);
    if (!content) return NULL;

    l = strlen(content);
    if (l == 0) {
	g_free(content);
	return NULL;
    }

    klass = g_type_class_ref(form->dt_handler);
    copy = DT_ENTRY(klass)->decode(content, strlen(content));
    g_type_class_unref(klass);

/*      printf("getting orgtext '%s' text '%s' orglen=%d len1=%d len2=%d\n", */
/*	   content, */
/*	   copy->data, */
/*	   strlen(content), */
/*	   copy->len, strlen(copy->data)); */

    g_free(content);

    return copy;
}

void dt_oc_set_data(struct formfill *form, GByteArray *data,
		    GtkWidget *widget)
{
     struct change_info *ci = gtk_object_get_data(GTK_OBJECT(widget), "ci");

     if (ci->oldtext) g_free(ci->oldtext);
     if(data) {
          GQDisplayOCClass* klass = g_type_class_ref(form->dt_handler);
	  guchar g = '\0';

	  GByteArray *encoded = DT_ENTRY(klass)->encode((gchar*)data->data, data->len);
	  /*	printf("setting text '%s' orglen=%d len1=%d len2=%d\n", */
	  /*	       encoded->data, */
	  /*	       data->len, */
	  /*	       encoded->len, strlen(encoded->data)); */
	  gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(widget)->entry),
			     (gchar*)encoded->data);
	  g_byte_array_append(encoded, &g, 1);

	  ci->oldtext = (gchar*)encoded->data;

	  g_byte_array_free(encoded, FALSE);
	  g_type_class_unref(klass);
     } else {
	  gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(widget)->entry), "");
	  ci->oldtext = g_strdup("");
     }
}

/* GType */
G_DEFINE_TYPE(GQDisplayOC, gq_display_oc, GQ_TYPE_DISPLAY_ENTRY);

static void
gq_display_oc_init(GQDisplayOC* self) {}

static void
gq_display_oc_class_init(GQDisplayOCClass* self_class) {
	GQTypeDisplayClass * gtd_class = GQ_TYPE_DISPLAY_CLASS(self_class);
	GQDisplayEntryClass* gde_class = GQ_DISPLAY_ENTRY_CLASS(self_class);

	gtd_class->name = Q_("displaytype|Object Class");
	gtd_class->selectable = FALSE;
	gtd_class->show_in_search_result = TRUE;
	gtd_class->get_widget = dt_oc_get_widget;
	gtd_class->get_data = dt_oc_get_data;
	gtd_class->set_data = dt_oc_set_data;
	gtd_class->buildLDAPMod = bervalLDAPMod;

	gde_class->encode = identity;
	gde_class->decode = identity;
};

#endif /* HAVE_LDAP_STR2OBJECTCLASS */

