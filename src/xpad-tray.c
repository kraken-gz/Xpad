/*

Copyright (c) 2002 Jamis Buck
Copyright (c) 2003-2007 Michael Terry
Copyright (c) 2013-2014 Arthur Borsboom

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/

#include "../config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#ifdef HAVE_APP_INDICATOR
#include <libappindicator/app-indicator.h>
#endif

#include "xpad-tray.h"
#include "xpad-app.h"
#include "xpad-pad.h"
#include "xpad-pad-group.h"
#include "xpad-preferences.h"
#include "xpad-settings.h"
#include "fio.h"
#include "help.h"

enum
{
	DO_NOTHING,
	TOGGLE_SHOW_ALL,
	LIST_OF_PADS,
	NEW_PAD
};

static void xpad_tray_open (XpadSettings *settings);
static void xpad_tray_close ();
static void xpad_tray_toggle (XpadSettings *settings);
/* static void xpad_tray_activate_cb (GtkStatusIcon *icon, XpadSettings *settings); */
/* static void xpad_tray_show_windows_list (GtkStatusIcon *icon); */
static GtkWidget* xpad_tray_create_menu(XpadSettings *settings);
static AppIndicator* xpad_tray_app_indicator_new (XpadSettings *settings);

static AppIndicator *app_indicator = NULL;

#define ICON_NAME "xpad"
#define TRAY_ICON "xpad-panel"

void xpad_tray_init (XpadSettings *settings) {
	gboolean tray_enabled;
	g_object_get (settings, "tray-enabled", &tray_enabled, NULL);

	if (tray_enabled) {
        app_indicator = xpad_tray_app_indicator_new(settings);
        xpad_tray_open(settings);
        g_signal_connect (settings, "notify::tray-enabled", G_CALLBACK (xpad_tray_toggle), NULL);
    }
}

static char const* getIconName(void)
{
    char const* icon_name;

    GtkIconTheme* theme = gtk_icon_theme_get_default();

    /* If the tray's icon is a 48x48 file, use it. Otherwise, use the fallback builtin icon. */
    if (!gtk_icon_theme_has_icon(theme, TRAY_ICON)) {
        icon_name = ICON_NAME;
    } else {
        GtkIconInfo* icon_info = gtk_icon_theme_lookup_icon(theme, TRAY_ICON, 48, GTK_ICON_LOOKUP_USE_BUILTIN);
        gboolean const icon_is_builtin = gtk_icon_info_get_filename(icon_info) == NULL;
        g_object_unref(icon_info);
        icon_name = icon_is_builtin ? ICON_NAME : TRAY_ICON;
    }

    return icon_name;
}

static void xpad_tray_toggle (XpadSettings *settings) {
	gboolean tray_enabled;
	g_object_get (settings, "tray-enabled", &tray_enabled, NULL);

	if (tray_enabled) {
		xpad_tray_open (settings);
	} else {
		xpad_tray_close ();
    }
}

static void xpad_tray_open (XpadSettings *settings)
{
	GtkWidget* tray_menu = xpad_tray_create_menu(settings);
	app_indicator_set_menu(app_indicator, GTK_MENU(tray_menu));
}

static AppIndicator* xpad_tray_app_indicator_new (XpadSettings *settings)
{
	char const* icon_name = getIconName();
	AppIndicator* indicator = app_indicator_new(ICON_NAME, icon_name, APP_INDICATOR_CATEGORY_SYSTEM_SERVICES);
	app_indicator_set_status(indicator, APP_INDICATOR_STATUS_ACTIVE);
	app_indicator_set_title(indicator, g_get_application_name());
	return indicator;
}

static void xpad_tray_close ()
{
    if (app_indicator)
        g_clear_object (&app_indicator);
}

void xpad_tray_dispose (XpadSettings *settings) {
	if (settings)
		g_signal_handlers_disconnect_by_func (settings, xpad_tray_toggle, NULL);
	xpad_tray_close ();
}

gboolean xpad_tray_is_open ()
{
	if (app_indicator)
		return TRUE;
	else
		return FALSE;
}

static void menu_spawn (XpadSettings *settings)
{
	GtkWidget *pad = xpad_pad_new (xpad_app_get_pad_group (), settings);
	gtk_widget_show (pad);
}

static GtkWidget* xpad_tray_create_menu(XpadSettings *settings) {
	XpadPadGroup *group = xpad_app_get_pad_group ();
	gboolean no_pads = !xpad_pad_group_has_pads (group);

	GtkWidget *menu = gtk_menu_new ();
	GtkWidget *item;

	item = gtk_menu_item_new_with_mnemonic (_("_New"));
	g_signal_connect_swapped (item, "activate", G_CALLBACK (menu_spawn), settings);
	gtk_container_add (GTK_CONTAINER (menu), item);

	item = gtk_separator_menu_item_new ();
	gtk_container_add (GTK_CONTAINER (menu), item);

	item = gtk_menu_item_new_with_mnemonic (_("_Show All"));
	g_signal_connect_swapped (item, "activate", G_CALLBACK (xpad_pad_group_show_all), group);
	gtk_container_add (GTK_CONTAINER (menu), item);

	if (no_pads)
		gtk_widget_set_sensitive (item, FALSE);

	item = gtk_menu_item_new_with_mnemonic (_("_Close All"));
	g_signal_connect_swapped (item, "activate", G_CALLBACK (xpad_pad_group_close_all), group);
	gtk_container_add (GTK_CONTAINER (menu), item);

	if (no_pads)
		gtk_widget_set_sensitive (item, FALSE);

	item = gtk_separator_menu_item_new ();
	gtk_container_add (GTK_CONTAINER (menu), item);

	xpad_pad_append_pad_titles_to_menu (menu);

	item = gtk_separator_menu_item_new ();
	gtk_container_add (GTK_CONTAINER (menu), item);

	item = gtk_menu_item_new_with_mnemonic (_("_Preferences"));
	g_signal_connect_swapped (item, "activate", G_CALLBACK (xpad_preferences_open), settings);
	gtk_container_add (GTK_CONTAINER (menu), item);

	item = gtk_menu_item_new_with_mnemonic (_("_Help"));
	g_signal_connect_swapped (item, "activate", G_CALLBACK (show_help), settings);
	gtk_container_add (GTK_CONTAINER (menu), item);

	item = gtk_menu_item_new_with_mnemonic (_("_Quit"));
	g_signal_connect (item, "activate", G_CALLBACK (xpad_app_quit), NULL);
	gtk_container_add (GTK_CONTAINER (menu), item);
    
    gtk_widget_show_all (menu);
    
    return menu;
}

/*
static void xpad_tray_activate_cb (GtkStatusIcon *icon, XpadSettings *settings) {
	XpadPadGroup *group = xpad_app_get_pad_group ();

	guint tray_click_configuration;
	g_object_get (settings, "tray-click-configuration", &tray_click_configuration, NULL);

	switch (tray_click_configuration)
	{
		case TOGGLE_SHOW_ALL:
			xpad_pad_group_toggle_hide (group);
			break;
		case LIST_OF_PADS:
			xpad_tray_show_windows_list (icon);
			break;
		case NEW_PAD:
			menu_spawn (settings);
			break;
	}
}
*/

/*
static void xpad_tray_show_windows_list (GtkStatusIcon *icon)
{
	GtkWidget* menu = gtk_menu_new ();
	xpad_pad_append_pad_titles_to_menu (menu);
	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, gtk_status_icon_position_menu, icon, 0, gtk_get_current_event_time());
}
*/
