/* ==============================================================================
 * AuralisOS Native "Liquid Glass" Desktop Shell
 * Phase 2: Ultra-Low-Memory Native Shell (<95MB Idle RAM)
 * Framework: C11, GTK4, gtk4-layer-shell, musl libc
 * Components: Translucent Taskbar, App Launcher (.desktop parser), SysTray, Popover
 * ============================================================================== */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <gtk/gtk.h>
#include <gtk4-layer-shell/gtk4-layer-shell.h>

/* Embedded Liquid Glass fallback CSS */
static const char *FALLBACK_CSS =
    "* { font-family: 'Noto Sans', sans-serif; font-size: 13px; }"
    "window.auralis-taskbar { background-color: rgba(14, 18, 28, 0.80); border-top: 1px solid rgba(255, 255, 255, 0.16); box-shadow: 0 -4px 18px rgba(0,0,0,0.45); }"
    ".taskbar-box { padding: 4px 8px; }"
    ".btn-start { background: linear-gradient(135deg, rgba(0, 180, 255, 0.35), rgba(0, 100, 220, 0.45)); border: 1px solid rgba(0, 210, 255, 0.55); border-radius: 8px; color: #ffffff; font-weight: bold; padding: 4px 12px; }"
    ".btn-start:hover { background: linear-gradient(135deg, rgba(0, 210, 255, 0.55), rgba(0, 130, 255, 0.65)); border-color: #ffffff; }"
    ".task-item { background-color: rgba(255, 255, 255, 0.05); border: 1px solid rgba(255, 255, 255, 0.08); border-radius: 6px; color: #e0e6ed; padding: 4px 10px; margin: 0 2px; }"
    ".task-item:hover { background-color: rgba(255, 255, 255, 0.15); color: #ffffff; }"
    ".tray-pill { background-color: rgba(255, 255, 255, 0.06); border: 1px solid rgba(255, 255, 255, 0.12); border-radius: 14px; padding: 3px 10px; margin: 0 3px; color: #cdd6e0; font-size: 12px; font-weight: 500; }"
    ".tray-pill.ram-warning { background-color: rgba(255, 160, 0, 0.25); border-color: rgba(255, 180, 0, 0.6); color: #ffca28; }"
    ".tray-pill.ram-critical { background-color: rgba(255, 50, 50, 0.3); border-color: rgba(255, 70, 70, 0.7); color: #ff5252; }"
    ".tray-clock { font-weight: bold; color: #ffffff; }"
    "window.auralis-launcher { background-color: rgba(14, 18, 28, 0.90); border: 1px solid rgba(255, 255, 255, 0.18); border-radius: 14px; box-shadow: 0 16px 40px rgba(0, 0, 0, 0.6); padding: 12px; }"
    ".launcher-search { background-color: rgba(255, 255, 255, 0.08); border: 1px solid rgba(255, 255, 255, 0.2); border-radius: 8px; color: #ffffff; padding: 8px 12px; }"
    ".launcher-search:focus { border-color: #00b4ff; }"
    ".app-row { background-color: transparent; border-radius: 8px; padding: 6px 10px; margin: 2px 0; }"
    ".app-row:hover, .app-row:selected { background-color: rgba(0, 180, 255, 0.22); }"
    ".app-title { color: #f0f4f8; font-weight: 600; font-size: 13px; }"
    ".app-desc { color: #8c9ba8; font-size: 11px; }"
    ".launcher-footer { border-top: 1px solid rgba(255, 255, 255, 0.1); padding-top: 8px; margin-top: 6px; }"
    ".user-badge { color: #8fa0b0; font-size: 11px; }"
    ".control-center-box { background-color: rgba(14, 18, 28, 0.95); border: 1px solid rgba(255, 255, 255, 0.18); border-radius: 12px; padding: 14px; min-width: 260px; }"
    ".control-btn { background-color: rgba(255, 255, 255, 0.08); border: 1px solid rgba(255, 255, 255, 0.15); border-radius: 8px; color: #ffffff; padding: 6px 10px; }"
    ".control-btn:hover { background-color: rgba(0, 180, 255, 0.3); border-color: rgba(0, 200, 255, 0.5); }";

/* Data structure for parsed .desktop applications */
typedef struct {
    char *name;
    char *exec;
    char *icon;
    char *comment;
    char *categories;
    GtkWidget *row_widget;
} DesktopApp;

static GList *app_list = NULL;

/* UI Widgets */
static GtkWidget *taskbar_window = NULL;
static GtkWidget *launcher_window = NULL;
static GtkWidget *launcher_search_entry = NULL;
static GtkWidget *launcher_listbox = NULL;
static GtkWidget *lbl_ram = NULL;
static GtkWidget *lbl_net = NULL;
static GtkWidget *lbl_clock = NULL;
static GtkWidget *control_popover = NULL;

/* ------------------------------------------------------------------------------
 * Helper: Strip field codes from Desktop Exec keys (%f, %u, etc.)
 * ------------------------------------------------------------------------------ */
static char *clean_exec_command(const char *raw_exec) {
    if (!raw_exec) return NULL;
    GString *clean = g_string_new("");
    const char *p = raw_exec;
    while (*p) {
        if (*p == '%' && *(p + 1)) {
            p += 2; /* Skip %u, %U, %f, %F, %i, %c, %k */
            continue;
        }
        g_string_append_c(clean, *p);
        p++;
    }
    return g_string_free(clean, FALSE);
}

/* ------------------------------------------------------------------------------
 * Launch Application Command
 * ------------------------------------------------------------------------------ */
static void launch_command(const char *cmd) {
    if (!cmd || strlen(cmd) == 0) return;
    GError *error = NULL;
    if (!g_spawn_command_line_async(cmd, &error)) {
        g_warning("AURALIS-SHELL: Failed to launch '%s': %s", cmd, error ? error->message : "unknown error");
        g_clear_error(&error);
    }
}

/* ------------------------------------------------------------------------------
 * Desktop Entry Parser
 * Scans directories for .desktop files and loads application metadata
 * ------------------------------------------------------------------------------ */
static void scan_desktop_directory(const char *dir_path) {
    GDir *dir = g_dir_open(dir_path, 0, NULL);
    if (!dir) return;

    const char *filename = NULL;
    while ((filename = g_dir_read_name(dir)) != NULL) {
        if (!g_str_has_suffix(filename, ".desktop")) continue;

        char *full_path = g_build_filename(dir_path, filename, NULL);
        GKeyFile *kf = g_key_file_new();

        if (g_key_file_load_from_file(kf, full_path, G_KEY_FILE_NONE, NULL)) {
            /* Check if entry should be displayed */
            gboolean no_display = g_key_file_get_boolean(kf, "Desktop Entry", "NoDisplay", NULL);
            gboolean hidden = g_key_file_get_boolean(kf, "Desktop Entry", "Hidden", NULL);

            if (!no_display && !hidden) {
                char *name = g_key_file_get_locale_string(kf, "Desktop Entry", "Name", NULL, NULL);
                char *raw_exec = g_key_file_get_string(kf, "Desktop Entry", "Exec", NULL);
                char *icon = g_key_file_get_string(kf, "Desktop Entry", "Icon", NULL);
                char *comment = g_key_file_get_locale_string(kf, "Desktop Entry", "Comment", NULL, NULL);
                char *categories = g_key_file_get_string(kf, "Desktop Entry", "Categories", NULL);

                if (name && raw_exec) {
                    DesktopApp *app = g_new0(DesktopApp, 1);
                    app->name = name;
                    app->exec = clean_exec_command(raw_exec);
                    app->icon = icon ? icon : g_strdup("application-x-executable");
                    app->comment = comment ? comment : g_strdup("");
                    app->categories = categories ? categories : g_strdup("");
                    app_list = g_list_append(app_list, app);
                    g_free(raw_exec);
                } else {
                    g_free(name);
                    g_free(raw_exec);
                    g_free(icon);
                    g_free(comment);
                    g_free(categories);
                }
            }
        }
        g_key_file_free(kf);
        g_free(full_path);
    }
    g_dir_close(dir);
}

static void load_all_applications(void) {
    /* Standard XDG applications search paths */
    scan_desktop_directory("/usr/share/applications");
    scan_desktop_directory("/usr/local/share/applications");

    const char *user_data = g_get_user_data_dir();
    char *user_apps = g_build_filename(user_data, "applications", NULL);
    scan_desktop_directory(user_apps);
    g_free(user_apps);

    /* If app list is empty, inject essential Auralis system utilities */
    if (!app_list) {
        DesktopApp *t = g_new0(DesktopApp, 1);
        t->name = g_strdup("Terminal (Foot)");
        t->exec = g_strdup("foot");
        t->icon = g_strdup("utilities-terminal");
        t->comment = g_strdup("Ultra-lightweight Wayland terminal");
        app_list = g_list_append(app_list, t);

        DesktopApp *tm = g_new0(DesktopApp, 1);
        tm->name = g_strdup("Auralis Task Manager");
        tm->exec = g_strdup("/usr/local/bin/auralis-taskmgr");
        tm->icon = g_strdup("utilities-system-monitor");
        tm->comment = g_strdup("Real-time RAM & CPU monitor");
        app_list = g_list_append(app_list, tm);
    }
}

/* ------------------------------------------------------------------------------
 * Launcher Visibility & Filtering
 * ------------------------------------------------------------------------------ */
static void toggle_launcher(void) {
    if (!launcher_window) return;
    gboolean visible = gtk_widget_is_visible(launcher_window);
    if (visible) {
        gtk_widget_set_visible(launcher_window, FALSE);
    } else {
        gtk_widget_set_visible(launcher_window, TRUE);
        gtk_editable_set_text(GTK_EDITABLE(launcher_search_entry), "");
        gtk_widget_grab_focus(launcher_search_entry);
    }
}

static void on_app_row_activated(GtkListBox *box, GtkListBoxRow *row, gpointer user_data) {
    (void)box; (void)user_data;
    DesktopApp *app = (DesktopApp *)g_object_get_data(G_OBJECT(row), "desktop-app");
    if (app && app->exec) {
        launch_command(app->exec);
        gtk_widget_set_visible(launcher_window, FALSE);
    }
}

static gboolean app_filter_func(GtkListBoxRow *row, gpointer user_data) {
    const char *search_text = gtk_editable_get_text(GTK_EDITABLE(user_data));
    if (!search_text || strlen(search_text) == 0) return TRUE;

    DesktopApp *app = (DesktopApp *)g_object_get_data(G_OBJECT(row), "desktop-app");
    if (!app) return TRUE;

    char *term = g_utf8_casefold(search_text, -1);
    char *name_fold = g_utf8_casefold(app->name, -1);
    char *desc_fold = g_utf8_casefold(app->comment, -1);

    gboolean match = (strstr(name_fold, term) != NULL) || (strstr(desc_fold, term) != NULL);

    g_free(term);
    g_free(name_fold);
    g_free(desc_fold);
    return match;
}

static void on_search_changed(GtkSearchEntry *entry, gpointer user_data) {
    (void)entry;
    GtkListBox *listbox = GTK_LIST_BOX(user_data);
    gtk_list_box_invalidate_filter(listbox);
}

/* ------------------------------------------------------------------------------
 * System Monitoring Backend: RAM, Network, Clock
 * ------------------------------------------------------------------------------ */
static gboolean update_clock(gpointer user_data) {
    (void)user_data;
    time_t rawtime;
    struct tm *timeinfo;
    char time_str[32];
    char date_str[64];

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    strftime(time_str, sizeof(time_str), "%I:%M %p", timeinfo);
    strftime(date_str, sizeof(date_str), "%A, %B %d, %Y", timeinfo);

    if (lbl_clock) {
        gtk_label_set_text(GTK_LABEL(lbl_clock), time_str);
        gtk_widget_set_tooltip_text(lbl_clock, date_str);
    }
    return G_SOURCE_CONTINUE;
}

static gboolean update_system_tray(gpointer user_data) {
    (void)user_data;

    /* 1. RAM Usage from /proc/meminfo */
    FILE *fp = fopen("/proc/meminfo", "r");
    if (fp) {
        long mem_total = 0, mem_avail = 0;
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            if (sscanf(line, "MemTotal: %ld kB", &mem_total) == 1) continue;
            if (sscanf(line, "MemAvailable: %ld kB", &mem_avail) == 1) continue;
        }
        fclose(fp);

        if (mem_total > 0) {
            long mem_used = mem_total - mem_avail;
            long used_mb = mem_used / 1024;
            int pct = (int)((mem_used * 100) / mem_total);

            char ram_buf[64];
            snprintf(ram_buf, sizeof(ram_buf), "RAM: %ld MB (%d%%)", used_mb, pct);
            gtk_label_set_text(GTK_LABEL(lbl_ram), ram_buf);

            /* Dynamic Alert Styling */
            GtkStyleContext *ctx = gtk_widget_get_style_context(lbl_ram);
            gtk_style_context_remove_class(ctx, "ram-warning");
            gtk_style_context_remove_class(ctx, "ram-critical");

            if (pct >= 90) {
                gtk_style_context_add_class(ctx, "ram-critical");
            } else if (pct >= 75) {
                gtk_style_context_add_class(ctx, "ram-warning");
            }
        }
    }

    /* 2. Network Interface Status from /sys/class/net */
    DIR *net_dir = opendir("/sys/class/net");
    gboolean net_up = FALSE;
    char active_iface[32] = "none";

    if (net_dir) {
        struct dirent *entry;
        while ((entry = readdir(net_dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0 || strcmp(entry->d_name, "lo") == 0)
                continue;

            char state_path[256];
            snprintf(state_path, sizeof(state_path), "/sys/class/net/%s/operstate", entry->d_name);
            FILE *sfp = fopen(state_path, "r");
            if (sfp) {
                char state[32];
                if (fgets(state, sizeof(state), sfp)) {
                    if (strncmp(state, "up", 2) == 0) {
                        net_up = TRUE;
                        strncpy(active_iface, entry->d_name, sizeof(active_iface) - 1);
                        fclose(sfp);
                        break;
                    }
                }
                fclose(sfp);
            }
        }
        closedir(net_dir);
    }

    if (lbl_net) {
        char net_buf[64];
        if (net_up) {
            snprintf(net_buf, sizeof(net_buf), "NET ● %s", active_iface);
        } else {
            snprintf(net_buf, sizeof(net_buf), "NET ○ OFF");
        }
        gtk_label_set_text(GTK_LABEL(lbl_net), net_buf);
    }

    return G_SOURCE_CONTINUE;
}

/* ------------------------------------------------------------------------------
 * Control Center Actions: RAM Optimization & Power Management
 * ------------------------------------------------------------------------------ */
static void on_optimize_ram(GtkButton *btn, gpointer user_data) {
    (void)btn; (void)user_data;
    /* Flush memory cache: sync && echo 3 > /proc/sys/vm/drop_caches */
    sync();
    int fd = open("/proc/sys/vm/drop_caches", O_WRONLY);
    if (fd >= 0) {
        write(fd, "3\n", 2);
        close(fd);
    }
    update_system_tray(NULL);
}

static void on_power_reboot(GtkButton *btn, gpointer user_data) {
    (void)btn; (void)user_data;
    launch_command("reboot");
}

static void on_power_shutdown(GtkButton *btn, gpointer user_data) {
    (void)btn; (void)user_data;
    launch_command("poweroff");
}

static void on_launch_taskmgr(GtkButton *btn, gpointer user_data) {
    (void)btn; (void)user_data;
    launch_command("/usr/local/bin/auralis-taskmgr");
    if (control_popover) gtk_popover_popdown(GTK_POPOVER(control_popover));
}

/* ------------------------------------------------------------------------------
 * UI Construction: Quick Control Center Popover
 * ------------------------------------------------------------------------------ */
static GtkWidget *create_control_center_popover(GtkWidget *parent_btn) {
    GtkWidget *popover = gtk_popover_new();
    gtk_popover_set_position(GTK_POPOVER(popover), GTK_POS_TOP);
    gtk_widget_set_parent(popover, parent_btn);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_add_css_class(vbox, "control-center-box");

    /* Title */
    GtkWidget *lbl_title = gtk_label_new("Auralis Control Center");
    gtk_widget_add_css_class(lbl_title, "control-title");
    gtk_box_append(GTK_BOX(vbox), lbl_title);

    /* Quick Memory Optimization */
    GtkWidget *btn_opt = gtk_button_new_with_label("⚡ Drop Cache (Free RAM)");
    gtk_widget_add_css_class(btn_opt, "control-btn");
    g_signal_connect(btn_opt, "clicked", G_CALLBACK(on_optimize_ram), NULL);
    gtk_box_append(GTK_BOX(vbox), btn_opt);

    /* Task Manager Shortcut */
    GtkWidget *btn_tm = gtk_button_new_with_label("📊 Task Manager");
    gtk_widget_add_css_class(btn_tm, "control-btn");
    g_signal_connect(btn_tm, "clicked", G_CALLBACK(on_launch_taskmgr), NULL);
    gtk_box_append(GTK_BOX(vbox), btn_tm);

    /* Audio Volume Slider */
    GtkWidget *lbl_vol = gtk_label_new("Master Volume");
    gtk_widget_set_halign(lbl_vol, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(vbox), lbl_vol);
    GtkWidget *slider_vol = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 5);
    gtk_range_set_value(GTK_RANGE(slider_vol), 75);
    gtk_box_append(GTK_BOX(vbox), slider_vol);

    /* Display Brightness Slider */
    GtkWidget *lbl_bri = gtk_label_new("Display Brightness");
    gtk_widget_set_halign(lbl_bri, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(vbox), lbl_bri);
    GtkWidget *slider_bri = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 10, 100, 5);
    gtk_range_set_value(GTK_RANGE(slider_bri), 85);
    gtk_box_append(GTK_BOX(vbox), slider_bri);

    /* Separator */
    gtk_box_append(GTK_BOX(vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

    /* Power Actions Box */
    GtkWidget *pwr_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_halign(pwr_box, GTK_ALIGN_CENTER);

    GtkWidget *btn_reb = gtk_button_new_with_label("🔄 Reboot");
    gtk_widget_add_css_class(btn_reb, "control-btn");
    g_signal_connect(btn_reb, "clicked", G_CALLBACK(on_power_reboot), NULL);
    gtk_box_append(GTK_BOX(pwr_box), btn_reb);

    GtkWidget *btn_off = gtk_button_new_with_label("⏻ Power Off");
    gtk_widget_add_css_class(btn_off, "control-btn");
    gtk_widget_add_css_class(btn_off, "control-btn-danger");
    g_signal_connect(btn_off, "clicked", G_CALLBACK(on_power_shutdown), NULL);
    gtk_box_append(GTK_BOX(pwr_box), btn_off);

    gtk_box_append(GTK_BOX(vbox), pwr_box);

    gtk_popover_set_child(GTK_POPOVER(popover), vbox);
    return popover;
}

static void on_control_center_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn; (void)user_data;
    if (control_popover) {
        if (gtk_widget_is_visible(control_popover)) {
            gtk_popover_popdown(GTK_POPOVER(control_popover));
        } else {
            gtk_popover_popup(GTK_POPOVER(control_popover));
        }
    }
}

/* ------------------------------------------------------------------------------
 * UI Construction: Translucent Start Menu Launcher Window
 * ------------------------------------------------------------------------------ */
static void create_launcher_window(GtkApplication *app) {
    launcher_window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(launcher_window), "Auralis Launcher");
    gtk_widget_add_css_class(launcher_window, "auralis-launcher");
    gtk_window_set_default_size(GTK_WINDOW(launcher_window), 380, 500);

    /* Layer Shell Setup */
    gtk_layer_init_for_window(GTK_WINDOW(launcher_window));
    gtk_layer_set_layer(GTK_WINDOW(launcher_window), GTK_LAYER_SHELL_LAYER_TOP);
    gtk_layer_set_anchor(GTK_WINDOW(launcher_window), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(launcher_window), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    gtk_layer_set_margin(GTK_WINDOW(launcher_window), GTK_LAYER_SHELL_EDGE_BOTTOM, 48);
    gtk_layer_set_margin(GTK_WINDOW(launcher_window), GTK_LAYER_SHELL_EDGE_LEFT, 10);
    gtk_layer_set_keyboard_mode(GTK_WINDOW(launcher_window), GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND);
    gtk_layer_set_namespace(GTK_WINDOW(launcher_window), "auralis-shell-launcher");

    GtkWidget *main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);

    /* Search Bar */
    launcher_search_entry = gtk_search_entry_new();
    gtk_widget_add_css_class(launcher_search_entry, "launcher-search");
    gtk_search_entry_set_key_capture_widget(GTK_SEARCH_ENTRY(launcher_search_entry), launcher_window);
    gtk_box_append(GTK_BOX(main_vbox), launcher_search_entry);

    /* Applications List in Scrolled Window */
    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    launcher_listbox = gtk_list_box_new();
    gtk_widget_add_css_class(launcher_listbox, "launcher-list");
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(launcher_listbox), GTK_SELECTION_SINGLE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), launcher_listbox);
    gtk_box_append(GTK_BOX(main_vbox), scrolled);

    /* Populate ListBox with parsed .desktop items */
    for (GList *l = app_list; l != NULL; l = l->next) {
        DesktopApp *item = (DesktopApp *)l->data;
        GtkWidget *row = gtk_list_box_row_new();
        gtk_widget_add_css_class(row, "app-row");
        g_object_set_data(G_OBJECT(row), "desktop-app", item);

        GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);

        /* Icon */
        GtkWidget *img = gtk_image_new_from_icon_name(item->icon);
        gtk_image_set_pixel_size(GTK_IMAGE(img), 28);
        gtk_box_append(GTK_BOX(hbox), img);

        /* Text Details */
        GtkWidget *text_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        GtkWidget *name_lbl = gtk_label_new(item->name);
        gtk_widget_set_halign(name_lbl, GTK_ALIGN_START);
        gtk_widget_add_css_class(name_lbl, "app-title");
        gtk_box_append(GTK_BOX(text_vbox), name_lbl);

        if (item->comment && strlen(item->comment) > 0) {
            GtkWidget *desc_lbl = gtk_label_new(item->comment);
            gtk_widget_set_halign(desc_lbl, GTK_ALIGN_START);
            gtk_widget_add_css_class(desc_lbl, "app-desc");
            gtk_label_set_ellipsize(GTK_LABEL(desc_lbl), PANGO_ELLIPSIZE_END);
            gtk_box_append(GTK_BOX(text_vbox), desc_lbl);
        }

        gtk_box_append(GTK_BOX(hbox), text_vbox);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), hbox);
        gtk_list_box_append(GTK_LIST_BOX(launcher_listbox), row);
        item->row_widget = row;
    }

    /* Set dynamic search filtering */
    gtk_list_box_set_filter_func(GTK_LIST_BOX(launcher_listbox), app_filter_func, launcher_search_entry, NULL);
    g_signal_connect(launcher_search_entry, "search-changed", G_CALLBACK(on_search_changed), launcher_listbox);
    g_signal_connect(launcher_listbox, "row-activated", G_CALLBACK(on_app_row_activated), NULL);

    /* Footer: User profile and quick power triggers */
    GtkWidget *footer_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(footer_box, "launcher-footer");

    GtkWidget *user_lbl = gtk_label_new("❖ root @ auralis (<95MB Idle)");
    gtk_widget_add_css_class(user_lbl, "user-badge");
    gtk_box_append(GTK_BOX(footer_box), user_lbl);

    GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(spacer, TRUE);
    gtk_box_append(GTK_BOX(footer_box), spacer);

    GtkWidget *quick_tm = gtk_button_new_with_label("TaskMgr");
    gtk_widget_add_css_class(quick_tm, "control-btn");
    g_signal_connect(quick_tm, "clicked", G_CALLBACK(on_launch_taskmgr), NULL);
    gtk_box_append(GTK_BOX(footer_box), quick_tm);

    GtkWidget *quick_pwr = gtk_button_new_with_label("⏻");
    gtk_widget_add_css_class(quick_pwr, "control-btn");
    g_signal_connect(quick_pwr, "clicked", G_CALLBACK(on_power_shutdown), NULL);
    gtk_box_append(GTK_BOX(footer_box), quick_pwr);

    gtk_box_append(GTK_BOX(main_vbox), footer_box);

    gtk_window_set_child(GTK_WINDOW(launcher_window), main_vbox);
}

/* ------------------------------------------------------------------------------
 * UI Construction: Bottom Translucent Glass Taskbar
 * ------------------------------------------------------------------------------ */
static void on_task_pinned_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    const char *cmd = (const char *)user_data;
    launch_command(cmd);
}

static void create_taskbar_window(GtkApplication *app) {
    taskbar_window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(taskbar_window), "Auralis Taskbar");
    gtk_widget_add_css_class(taskbar_window, "auralis-taskbar");

    /* Layer Shell Setup for Bottom Bar */
    gtk_layer_init_for_window(GTK_WINDOW(taskbar_window));
    gtk_layer_set_layer(GTK_WINDOW(taskbar_window), GTK_LAYER_SHELL_LAYER_TOP);
    gtk_layer_auto_exclusive_zone_enable(GTK_WINDOW(taskbar_window));
    gtk_layer_set_anchor(GTK_WINDOW(taskbar_window), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(taskbar_window), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(taskbar_window), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
    gtk_layer_set_namespace(GTK_WINDOW(taskbar_window), "auralis-shell-panel");

    /* Main Container */
    GtkWidget *bar_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(bar_box, "taskbar-box");

    /* 1. Start Button */
    GtkWidget *btn_start = gtk_button_new_with_label("❖ Auralis");
    gtk_widget_add_css_class(btn_start, "btn-start");
    g_signal_connect(btn_start, "clicked", G_CALLBACK(toggle_launcher), NULL);
    gtk_box_append(GTK_BOX(bar_box), btn_start);

    /* Separator */
    gtk_box_append(GTK_BOX(bar_box), gtk_separator_new(GTK_ORIENTATION_VERTICAL));

    /* 2. Pinned Task / App Launchers */
    struct {
        const char *label;
        const char *cmd;
    } pinned_apps[] = {
        { "Terminal", "foot" },
        { "Store", "/usr/local/bin/auralis-store" },
        { "TaskMgr", "/usr/local/bin/auralis-taskmgr" },
        { "Status", "/usr/local/bin/auralis-status" },
        { NULL, NULL }
    };

    for (int i = 0; pinned_apps[i].label != NULL; i++) {
        GtkWidget *pbtn = gtk_button_new_with_label(pinned_apps[i].label);
        gtk_widget_add_css_class(pbtn, "task-item");
        g_signal_connect(pbtn, "clicked", G_CALLBACK(on_task_pinned_clicked), (gpointer)pinned_apps[i].cmd);
        gtk_box_append(GTK_BOX(bar_box), pbtn);
    }

    /* Expanding spacer pushing tray to right edge */
    GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(spacer, TRUE);
    gtk_box_append(GTK_BOX(bar_box), spacer);

    /* 3. System Tray Elements */
    /* RAM Indicator */
    lbl_ram = gtk_label_new("RAM: ... MB");
    gtk_widget_add_css_class(lbl_ram, "tray-pill");
    gtk_box_append(GTK_BOX(bar_box), lbl_ram);

    /* Network Indicator */
    lbl_net = gtk_label_new("NET ●");
    gtk_widget_add_css_class(lbl_net, "tray-pill");
    gtk_box_append(GTK_BOX(bar_box), lbl_net);

    /* Clock */
    lbl_clock = gtk_label_new("--:--");
    gtk_widget_add_css_class(lbl_clock, "tray-pill");
    gtk_widget_add_css_class(lbl_clock, "tray-clock");
    gtk_box_append(GTK_BOX(bar_box), lbl_clock);

    /* Quick Control Center Trigger */
    GtkWidget *btn_cc = gtk_button_new_with_label("⚡");
    gtk_widget_add_css_class(btn_cc, "btn-control-center");
    control_popover = create_control_center_popover(btn_cc);
    g_signal_connect(btn_cc, "clicked", G_CALLBACK(on_control_center_clicked), NULL);
    gtk_box_append(GTK_BOX(bar_box), btn_cc);

    gtk_window_set_child(GTK_WINDOW(taskbar_window), bar_box);
    gtk_widget_set_visible(taskbar_window, TRUE);
}

/* ------------------------------------------------------------------------------
 * GTK Application Activation & Entrypoint
 * ------------------------------------------------------------------------------ */
static void on_app_activate(GtkApplication *app, gpointer user_data) {
    (void)user_data;

    /* Initialize GTK Theme and Liquid Glass CSS */
    GtkCssProvider *provider = gtk_css_provider_new();
    const char *css_paths[] = {
        "/usr/share/auralis-shell/style.css",
        "/etc/auralis-shell/style.css",
        "style.css",
        NULL
    };

    gboolean css_loaded = FALSE;
    for (int i = 0; css_paths[i] != NULL; i++) {
        if (access(css_paths[i], R_OK) == 0) {
            gtk_css_provider_load_from_path(provider, css_paths[i]);
            css_loaded = TRUE;
            break;
        }
    }

    if (!css_loaded) {
        gtk_css_provider_load_from_data(provider, FALLBACK_CSS, -1);
    }

    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    g_object_unref(provider);

    /* Parse all .desktop applications */
    load_all_applications();

    /* Construct UI */
    create_taskbar_window(app);
    create_launcher_window(app);

    /* Initial status update and register polling timers */
    update_system_tray(NULL);
    update_clock(NULL);

    g_timeout_add_seconds(1, update_clock, NULL);
    g_timeout_add_seconds(2, update_system_tray, NULL);
}

int main(int argc, char *argv[]) {
    /* Handle standalone toggle flag if called from Wayfire shortcut */
    if (argc > 1 && strcmp(argv[1], "--toggle-launcher") == 0) {
        /* When invoked with --toggle-launcher, pass signal or handled via DBus/App */
    }

    GtkApplication *app = gtk_application_new("org.auralis.shell", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(on_app_activate), NULL);

    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    return status;
}
