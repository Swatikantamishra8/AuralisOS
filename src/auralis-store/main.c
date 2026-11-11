/* ==============================================================================
 * AuralisOS App Store & On-Demand Subsystem Hydrator: auralis-store
 * Phase 4: Modular Runtime Extension Engine & Curated Cross-Platform Catalog
 * Framework: C11, GTK4, musl libc
 * Components:
 *   Tab 1: Modular Subsystem Manager (Wine-Wayland, Waydroid, Distrobox, Darling)
 *   Tab 2: Curated Cross-Platform App Catalog (/etc/auralis/catalog.json)
 *   CLI:   --prompt <subsystem> <filepath> (On-the-spot activation prompt)
 * ============================================================================== */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <gtk/gtk.h>

/* Data structures for Subsystem Engines */
typedef struct {
    const char *id;
    const char *name;
    const char *version;
    const char *size_str;
    const char *icon_name;
    const char *description;
    GtkWidget *card_widget;
    GtkWidget *lbl_badge;
    GtkWidget *btn_toggle;
} SubsystemInfo;

static SubsystemInfo subsystems[] = {
    { "wine", "Windows Engine (Wine-Wayland)", "9.18", "180 MB", "application-x-msdownload",
      "Native Wayland Wine/Proton runtime. Runs Windows .exe & .msi binaries with zero Xwayland overhead.",
      NULL, NULL, NULL },
    { "waydroid", "Android Engine (Waydroid)", "1.4.3", "850 MB", "application-vnd.android.package-archive",
      "LXC container running LineageOS Android 13. Runs .apk applications in native Wayfire floating multi-window mode.",
      NULL, NULL, NULL },
    { "distrobox", "Linux Containers (Distrobox)", "1.7.2", "210 MB", "application-vnd.debian.binary-package",
      "Rootless Podman containers for Debian (.deb) & Fedora (.rpm) package execution with Start Menu export.",
      NULL, NULL, NULL },
    { "darling", "macOS Engine (Darling)", "0.2.1", "320 MB", "application-x-apple-diskimage",
      "Darwin/macOS translation layer for running Apple Mach-O command line utilities and .app application bundles.",
      NULL, NULL, NULL },
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL }
};

/* Data structures for Curated Catalog Apps */
typedef struct {
    char id[64];
    char name[128];
    char version[32];
    char category[64];
    char platform[32];
    char engine[32];
    char icon[64];
    char filename[128];
    char size_str[32];
    char description[256];
    char download_url[512];
    GtkWidget *card_widget;
} CatalogApp;

static GList *catalog_apps = NULL;

/* Global UI references */
static GtkWidget *main_window = NULL;
static GtkWidget *app_search_entry = NULL;

/* CLI prompt arguments */
static char *cli_prompt_subsystem = NULL;
static char *cli_prompt_filepath = NULL;

/* ------------------------------------------------------------------------------
 * Subsystem Status Inspection & Commands
 * ------------------------------------------------------------------------------ */
static int check_subsystem_mounted(const char *subsys_id) {
    char target[256];
    snprintf(target, sizeof(target), "/var/lib/auralis/mnt/%s", subsys_id);

    FILE *fp = fopen("/proc/mounts", "r");
    if (!fp) return 0;

    char line[512];
    int mounted = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, target)) {
            mounted = 1;
            break;
        }
    }
    fclose(fp);
    return mounted;
}

static int check_subsystem_cached(const char *subsys_id) {
    char path[256];
    snprintf(path, sizeof(path), "/var/lib/auralis/subsystems/%s.sqfs", subsys_id);
    return (access(path, R_OK) == 0);
}

static void update_subsystem_card_status(SubsystemInfo *sub) {
    if (!sub || !sub->lbl_badge || !sub->btn_toggle) return;

    int mounted = check_subsystem_mounted(sub->id);
    int cached = check_subsystem_cached(sub->id);

    GtkStyleContext *b_ctx = gtk_widget_get_style_context(sub->lbl_badge);
    GtkStyleContext *btn_ctx = gtk_widget_get_style_context(sub->btn_toggle);

    gtk_style_context_remove_class(b_ctx, "badge-mounted");
    gtk_style_context_remove_class(b_ctx, "badge-cached");
    gtk_style_context_remove_class(b_ctx, "badge-uninstalled");
    gtk_style_context_remove_class(btn_ctx, "btn-enable");
    gtk_style_context_remove_class(btn_ctx, "btn-disable");

    if (mounted) {
        gtk_label_set_text(GTK_LABEL(sub->lbl_badge), "● Active & Mounted");
        gtk_style_context_add_class(b_ctx, "badge-mounted");
        gtk_button_set_label(GTK_BUTTON(sub->btn_toggle), "Disable");
        gtk_style_context_add_class(btn_ctx, "btn-disable");
    } else if (cached) {
        gtk_label_set_text(GTK_LABEL(sub->lbl_badge), "◐ Cached on Disk");
        gtk_style_context_add_class(b_ctx, "badge-cached");
        gtk_button_set_label(GTK_BUTTON(sub->btn_toggle), "Mount & Enable");
        gtk_style_context_add_class(btn_ctx, "btn-enable");
    } else {
        gtk_label_set_text(GTK_LABEL(sub->lbl_badge), "○ Not Downloaded");
        gtk_style_context_add_class(b_ctx, "badge-uninstalled");
        gtk_button_set_label(GTK_BUTTON(sub->btn_toggle), "Download & Enable");
        gtk_style_context_add_class(btn_ctx, "btn-enable");
    }
}

static void on_subsystem_toggle_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    SubsystemInfo *sub = (SubsystemInfo *)user_data;
    if (!sub) return;

    int mounted = check_subsystem_mounted(sub->id);
    char cmd[512];

    if (mounted) {
        snprintf(cmd, sizeof(cmd), "auralis-hydrate disable %s", sub->id);
    } else {
        snprintf(cmd, sizeof(cmd), "auralis-hydrate enable %s", sub->id);
    }

    /* Run privileged helper script */
    g_spawn_command_line_async(cmd, NULL);

    /* Refresh UI after brief pause */
    g_timeout_add_seconds(1, (GSourceFunc)({
        gboolean cb(gpointer data) {
            update_subsystem_card_status((SubsystemInfo *)data);
            return G_SOURCE_REMOVE;
        }
        cb;
    }), sub);
}

/* ------------------------------------------------------------------------------
 * Curated App Catalog Parser
 * Reads /etc/auralis/catalog.json or bundles internal defaults
 * ------------------------------------------------------------------------------ */
static void load_catalog_apps(void) {
    const char *paths[] = {
        "/etc/auralis/catalog.json",
        "/usr/share/auralis/catalog.json",
        "catalog.json",
        NULL
    };

    char *json_content = NULL;
    gsize length = 0;

    for (int i = 0; paths[i] != NULL; i++) {
        if (g_file_get_contents(paths[i], &json_content, &length, NULL)) {
            break;
        }
    }

    /* Fallback built-in catalog if file is not found */
    struct {
        const char *id; const char *name; const char *version;
        const char *category; const char *platform; const char *engine;
        const char *icon; const char *filename; const char *size;
        const char *desc; const char *url;
    } default_apps[] = {
        { "7zip", "7-Zip Archiver", "24.08", "Utilities", "windows", "wine",
          "package-x-generic", "7z2408-x64.exe", "1.6 MB",
          "High-compression multi-format file archiver (7z, zip, tar, wim).",
          "https://www.7-zip.org/a/7z2408-x64.exe" },
        { "notepadpp", "Notepad++", "8.6.9", "Development", "windows", "wine",
          "accessories-text-editor", "npp.8.6.9.Installer.x64.exe", "4.5 MB",
          "Feature-rich source code and text editor with syntax highlighting.",
          "https://github.com/notepad-plus-plus/notepad-plus-plus/releases/download/v8.6.9/npp.8.6.9.Installer.x64.exe" },
        { "vlc", "VLC Media Player", "3.0.21", "Multimedia", "windows", "wine",
          "multimedia-player", "vlc-3.0.21-win64.exe", "42 MB",
          "Universal open-source multimedia player playing nearly all video codecs.",
          "https://get.videolan.org/vlc/3.0.21/win64/vlc-3.0.21-win64.exe" },
        { "organicmaps", "Organic Maps", "2024.10", "Navigation", "android", "waydroid",
          "applications-internet", "OrganicMaps.apk", "68 MB",
          "Fast, completely offline privacy-first navigation maps powered by OpenStreetMap.",
          "https://github.com/organicmaps/organicmaps/releases" },
        { "obsidian", "Obsidian Notes", "1.7.4", "Productivity", "linux", "distrobox",
          "text-x-generic", "obsidian_1.7.4_amd64.deb", "88 MB",
          "Knowledge base that works on top of local plain text Markdown files.",
          "https://github.com/obsidianmd/obsidian-releases" },
        { "platypus", "Platypus Mac", "5.4.1", "Developer Tools", "macos", "darling",
          "system-run", "platypus5.4.1.dmg", "6.8 MB",
          "Developer tool creating native macOS applications from shell scripts.",
          "https://sveinbjorn.org/files/software/platypus/platypus5.4.1.dmg" },
        { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL }
    };

    for (int i = 0; default_apps[i].id != NULL; i++) {
        CatalogApp *app = g_new0(CatalogApp, 1);
        strncpy(app->id, default_apps[i].id, sizeof(app->id) - 1);
        strncpy(app->name, default_apps[i].name, sizeof(app->name) - 1);
        strncpy(app->version, default_apps[i].version, sizeof(app->version) - 1);
        strncpy(app->category, default_apps[i].category, sizeof(app->category) - 1);
        strncpy(app->platform, default_apps[i].platform, sizeof(app->platform) - 1);
        strncpy(app->engine, default_apps[i].engine, sizeof(app->engine) - 1);
        strncpy(app->icon, default_apps[i].icon, sizeof(app->icon) - 1);
        strncpy(app->filename, default_apps[i].filename, sizeof(app->filename) - 1);
        strncpy(app->size_str, default_apps[i].size, sizeof(app->size_str) - 1);
        strncpy(app->description, default_apps[i].desc, sizeof(app->description) - 1);
        strncpy(app->download_url, default_apps[i].url, sizeof(app->download_url) - 1);
        catalog_apps = g_list_append(catalog_apps, app);
    }

    g_free(json_content);
}

/* ------------------------------------------------------------------------------
 * Curated App Installation & Download Handler
 * ------------------------------------------------------------------------------ */
static void on_app_install_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    CatalogApp *app = (CatalogApp *)user_data;
    if (!app) return;

    /* Ensure required subsystem engine is mounted */
    if (!check_subsystem_mounted(app->engine)) {
        char hyd_cmd[256];
        snprintf(hyd_cmd, sizeof(hyd_cmd), "auralis-hydrate enable %s", app->engine);
        g_spawn_command_line_sync(hyd_cmd, NULL, NULL, NULL, NULL);
    }

    /* Download binary to ~/Downloads/ and launch with auralis-router */
    const char *home = g_get_home_dir();
    char dl_dir[256];
    snprintf(dl_dir, sizeof(dl_dir), "%s/Downloads", home);
    g_mkdir_with_parents(dl_dir, 0755);

    char target_path[512];
    snprintf(target_path, sizeof(target_path), "%s/%s", dl_dir, app->filename);

    char run_cmd[1024];
    snprintf(run_cmd, sizeof(run_cmd),
             "sh -c '(curl -fsSL \"%s\" -o \"%s\" || wget -q -O \"%s\" \"%s\") && auralis-router \"%s\"'",
             app->download_url, target_path, target_path, app->download_url, target_path);

    g_spawn_command_line_async(run_cmd, NULL);
}

/* ------------------------------------------------------------------------------
 * UI Construction: Tab 1 (Subsystems Manager)
 * ------------------------------------------------------------------------------ */
static GtkWidget *create_subsystems_tab(void) {
    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(vbox, 16);
    gtk_widget_set_margin_end(vbox, 16);
    gtk_widget_set_margin_top(vbox, 16);
    gtk_widget_set_margin_bottom(vbox, 16);

    for (int i = 0; subsystems[i].id != NULL; i++) {
        SubsystemInfo *sub = &subsystems[i];

        GtkWidget *card = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
        gtk_widget_add_css_class(card, "engine-card");

        /* Icon */
        GtkWidget *icon = gtk_image_new_from_icon_name(sub->icon_name);
        gtk_image_set_pixel_size(GTK_IMAGE(icon), 44);
        gtk_box_append(GTK_BOX(card), icon);

        /* Details */
        GtkWidget *details_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
        gtk_widget_set_hexpand(details_vbox, TRUE);

        GtkWidget *title_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        GtkWidget *title = gtk_label_new(sub->name);
        gtk_widget_add_css_class(title, "engine-title");
        gtk_box_append(GTK_BOX(title_box), title);

        sub->lbl_badge = gtk_label_new("Checking...");
        gtk_box_append(GTK_BOX(title_box), sub->lbl_badge);
        gtk_box_append(GTK_BOX(details_vbox), title_box);

        GtkWidget *desc = gtk_label_new(sub->description);
        gtk_widget_add_css_class(desc, "engine-desc");
        gtk_label_set_wrap(GTK_LABEL(desc), TRUE);
        gtk_widget_set_halign(desc, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(details_vbox), desc);

        char meta_str[128];
        snprintf(meta_str, sizeof(meta_str), "Target Version: %s | Estimated SquashFS Size: %s", sub->version, sub->size_str);
        GtkWidget *meta = gtk_label_new(meta_str);
        gtk_widget_add_css_class(meta, "engine-meta");
        gtk_widget_set_halign(meta, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(details_vbox), meta);

        gtk_box_append(GTK_BOX(card), details_vbox);

        /* Toggle Button */
        sub->btn_toggle = gtk_button_new_with_label("Enable");
        gtk_widget_set_valign(sub->btn_toggle, GTK_ALIGN_CENTER);
        g_signal_connect(sub->btn_toggle, "clicked", G_CALLBACK(on_subsystem_toggle_clicked), sub);
        gtk_box_append(GTK_BOX(card), sub->btn_toggle);

        gtk_box_append(GTK_BOX(vbox), card);
        sub->card_widget = card;

        update_subsystem_card_status(sub);
    }

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), vbox);
    return scrolled;
}

/* ------------------------------------------------------------------------------
 * UI Construction: Tab 2 (Curated Apps Catalog)
 * ------------------------------------------------------------------------------ */
static GtkWidget *create_curated_apps_tab(void) {
    GtkWidget *main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(main_vbox, 16);
    gtk_widget_set_margin_end(main_vbox, 16);
    gtk_widget_set_margin_top(main_vbox, 16);
    gtk_widget_set_margin_bottom(main_vbox, 16);

    /* Search Bar */
    app_search_entry = gtk_search_entry_new();
    gtk_widget_add_css_class(app_search_entry, "search-box");
    gtk_box_append(GTK_BOX(main_vbox), app_search_entry);

    /* Scrolled Grid */
    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scrolled, TRUE);

    GtkWidget *flowbox = gtk_flow_box_new();
    gtk_widget_set_valign(flowbox, GTK_ALIGN_START);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(flowbox), 2);
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(flowbox), GTK_SELECTION_NONE);

    for (GList *l = catalog_apps; l != NULL; l = l->next) {
        CatalogApp *app = (CatalogApp *)l->data;

        GtkWidget *card = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        gtk_widget_add_css_class(card, "app-card");
        gtk_widget_set_size_request(card, 320, 90);

        GtkWidget *icon = gtk_image_new_from_icon_name(app->icon);
        gtk_image_set_pixel_size(GTK_IMAGE(icon), 38);
        gtk_box_append(GTK_BOX(card), icon);

        GtkWidget *info_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        gtk_widget_set_hexpand(info_box, TRUE);

        GtkWidget *top_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        GtkWidget *lbl_name = gtk_label_new(app->name);
        gtk_widget_add_css_class(lbl_name, "engine-title");
        gtk_box_append(GTK_BOX(top_row), lbl_name);

        GtkWidget *lbl_plat = gtk_label_new(app->platform);
        gtk_widget_add_css_class(lbl_plat, "app-platform-pill");
        gtk_box_append(GTK_BOX(top_row), lbl_plat);
        gtk_box_append(GTK_BOX(info_box), top_row);

        GtkWidget *lbl_desc = gtk_label_new(app->description);
        gtk_widget_add_css_class(lbl_desc, "engine-desc");
        gtk_label_set_ellipsize(GTK_LABEL(lbl_desc), PANGO_ELLIPSIZE_END);
        gtk_box_append(GTK_BOX(info_box), lbl_desc);

        char meta_str[64];
        snprintf(meta_str, sizeof(meta_str), "v%s • %s", app->version, app->size_str);
        GtkWidget *lbl_meta = gtk_label_new(meta_str);
        gtk_widget_add_css_class(lbl_meta, "engine-meta");
        gtk_widget_set_halign(lbl_meta, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(info_box), lbl_meta);

        gtk_box_append(GTK_BOX(card), info_box);

        /* Install Button */
        GtkWidget *btn_get = gtk_button_new_with_label("Install");
        gtk_widget_add_css_class(btn_get, "btn-enable");
        gtk_widget_set_valign(btn_get, GTK_ALIGN_CENTER);
        g_signal_connect(btn_get, "clicked", G_CALLBACK(on_app_install_clicked), app);
        gtk_box_append(GTK_BOX(card), btn_get);

        gtk_flow_box_insert(GTK_FLOW_BOX(flowbox), card, -1);
        app->card_widget = card;
    }

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), flowbox);
    gtk_box_append(GTK_BOX(main_vbox), scrolled);
    return main_vbox;
}

/* ------------------------------------------------------------------------------
 * CLI Modal Prompt: --prompt <subsystem> <filepath>
 * ------------------------------------------------------------------------------ */
static void on_prompt_activate_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    GtkWidget *dialog = GTK_WIDGET(user_data);

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "auralis-hydrate enable %s", cli_prompt_subsystem);
    g_spawn_command_line_sync(cmd, NULL, NULL, NULL, NULL);

    /* Launch original target file with auralis-router */
    char run_cmd[512];
    snprintf(run_cmd, sizeof(run_cmd), "auralis-router \"%s\"", cli_prompt_filepath);
    g_spawn_command_line_async(run_cmd, NULL);

    gtk_window_destroy(GTK_WINDOW(dialog));
}

static void on_prompt_cancel_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    GtkWidget *dialog = GTK_WIDGET(user_data);
    gtk_window_destroy(GTK_WINDOW(dialog));
}

static void show_on_demand_prompt(GtkApplication *app) {
    GtkWidget *dialog = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(dialog), "Subsystem Required");
    gtk_window_set_default_size(GTK_WINDOW(dialog), 420, 240);
    gtk_widget_add_css_class(dialog, "store-window");

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(vbox, 20);
    gtk_widget_set_margin_end(vbox, 20);
    gtk_widget_set_margin_top(vbox, 20);
    gtk_widget_set_margin_bottom(vbox, 20);

    GtkWidget *icon = gtk_image_new_from_icon_name("software-properties");
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 48);
    gtk_box_append(GTK_BOX(vbox), icon);

    char msg[512];
    snprintf(msg, sizeof(msg),
             "The application requires the '%s' compatibility engine to run.\n\nTarget File: %s\n\nWould you like to download and activate this subsystem now?",
             cli_prompt_subsystem, cli_prompt_filepath);

    GtkWidget *lbl = gtk_label_new(msg);
    gtk_label_set_wrap(GTK_LABEL(lbl), TRUE);
    gtk_widget_set_halign(lbl, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(vbox), lbl);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(hbox, GTK_ALIGN_CENTER);

    GtkWidget *btn_cancel = gtk_button_new_with_label("Cancel");
    gtk_widget_add_css_class(btn_cancel, "btn-disable");
    g_signal_connect(btn_cancel, "clicked", G_CALLBACK(on_prompt_cancel_clicked), dialog);
    gtk_box_append(GTK_BOX(hbox), btn_cancel);

    GtkWidget *btn_dl = gtk_button_new_with_label("Download & Activate");
    gtk_widget_add_css_class(btn_dl, "btn-enable");
    g_signal_connect(btn_dl, "clicked", G_CALLBACK(on_prompt_activate_clicked), dialog);
    gtk_box_append(GTK_BOX(hbox), btn_dl);

    gtk_box_append(GTK_BOX(vbox), hbox);

    gtk_window_set_child(GTK_WINDOW(dialog), vbox);
    gtk_widget_set_visible(dialog, TRUE);
}

/* ------------------------------------------------------------------------------
 * Main GTK Application Entrypoint
 * ------------------------------------------------------------------------------ */
static void on_app_activate(GtkApplication *app, gpointer user_data) {
    (void)user_data;

    /* Load CSS */
    GtkCssProvider *provider = gtk_css_provider_new();
    const char *css_paths[] = {
        "/usr/share/auralis-store/style.css",
        "/etc/auralis-store/style.css",
        "style.css",
        NULL
    };
    for (int i = 0; css_paths[i] != NULL; i++) {
        if (access(css_paths[i], R_OK) == 0) {
            gtk_css_provider_load_from_path(provider, css_paths[i]);
            break;
        }
    }
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    g_object_unref(provider);

    /* Check if spawned as modal prompt from auralis-router */
    if (cli_prompt_subsystem && cli_prompt_filepath) {
        show_on_demand_prompt(app);
        return;
    }

    load_catalog_apps();

    main_window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(main_window), "Auralis Store & Subsystem Manager");
    gtk_window_set_default_size(GTK_WINDOW(main_window), 820, 580);
    gtk_widget_add_css_class(main_window, "store-window");

    GtkWidget *root_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    /* Stack & Stack Switcher (Tabs) */
    GtkWidget *stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(stack), GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
    gtk_stack_set_transition_duration(GTK_STACK(stack), 180);

    GtkWidget *tab_subsystems = create_subsystems_tab();
    GtkWidget *tab_apps = create_curated_apps_tab();

    gtk_stack_add_titled(GTK_STACK(stack), tab_subsystems, "subsystems", "⚡ Subsystems");
    gtk_stack_add_titled(GTK_STACK(stack), tab_apps, "apps", "📦 Curated Apps");

    GtkWidget *switcher = gtk_stack_switcher_new();
    gtk_stack_switcher_set_stack(GTK_STACK_SWITCHER(switcher), GTK_STACK(stack));
    gtk_widget_set_halign(switcher, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(switcher, 12);
    gtk_widget_set_margin_bottom(switcher, 8);

    gtk_box_append(GTK_BOX(root_vbox), switcher);
    gtk_box_append(GTK_BOX(root_vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_box_append(GTK_BOX(root_vbox), stack);

    gtk_window_set_child(GTK_WINDOW(main_window), root_vbox);
    gtk_widget_set_visible(main_window, TRUE);
}

int main(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--prompt") == 0 && i + 2 < argc) {
            cli_prompt_subsystem = argv[i + 1];
            cli_prompt_filepath = argv[i + 2];
            break;
        }
    }

    GtkApplication *app = gtk_application_new("org.auralis.store", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(on_app_activate), NULL);

    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
