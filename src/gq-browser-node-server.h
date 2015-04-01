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

#ifndef GQ_SERVER_BROWSE_H_INCLUDED
#define GQ_SERVER_BROWSE_H_INCLUDED

#include "gq-browser-node.h"
#include "gq-server.h"

G_BEGIN_DECLS

typedef struct _GqBrowserNodeServer GqBrowserNodeServer;
typedef GqBrowserNodeClass          GqBrowserNodeServerClass;

#define GQ_TYPE_BROWSER_NODE_SERVER         (gq_browser_node_server_get_type())
#define GQ_BROWSER_NODE_SERVER(i)           (G_TYPE_CHECK_INSTANCE_CAST((i), GQ_TYPE_BROWSER_NODE_SERVER, GqBrowserNodeServer))
#define GQ_BROWSER_NODE_SERVER_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST((c), GQ_TYPE_BROWSER_NODE_SERVER, GqBrowserNodeServerClass))
#define GQ_IS_BROWSER_NODE_SERVER(i)        (G_TYPE_CHECK_INSTANCE_TYPE((i), GQ_TYPE_BROWSER_NODE_SERVER))
#define GQ_IS_BROWSER_NODE_SERVER_CLASS(c)  (G_TYPE_CHECK_CLASS_CAST((i), GQ_TYPE_BROWSER_NODE_SERVER))
#define GQ_BROWSER_NODE_SERVER_GET_CLASS(i) (G_TYPE_INSTANCE_GET_CLASS((i), GQ_TYPE_BROWSER_NODE_SERVER, GqBrowserNodeServerClass))

GType gq_browser_node_server_get_type(void);

struct _GqBrowserNodeServer {
	GqBrowserNode base_instance;

	/* specific */
	GqServer *server;
	int once_expanded;
};

GqBrowserNode *gq_browser_node_server_new(GqServer *server);

G_END_DECLS

#endif


/*
   Local Variables:
   c-basic-offset: 5
   End:
 */

