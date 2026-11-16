/*
 * AuralisOS Apprenticeship & Interactive Onboarding Tour: auralis-welcome
 * Liquid Glass Edition (<95MB Idle RAM)
 * Guides new users through system navigation, multi-world app routing,
 * modular hydration, and dual-boot installation.
 */

#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    GtkWidget *stack;
    GtkWidget *btn_prev;
    GtkWidget *btn_next;
    GtkWidget *lbl_step;
    int current_step;
} WelcomeApp;

#define TOTAL_STEPS 5

static const char *step_names[] = {
    "welcome",
    "shortcuts",
    "multiworld",
    "hydration",
    "installation"
};

static void update_navigation_state(WelcomeApp *app) {
    gtk_stack_set_visible_child_name(GTK_STACK(app->stack), step_names[app->current_step]);
    
    char step_text[64];
    snprintf(step_text, sizeof(step_text), "Step %d of %d", app->current_step + 1, TOTAL_STEPS);
    gtk_label_set_text(GTK_LABEL(app->lbl_step), step_text);

    gtk_widget_set_sensitive(app->btn_prev, app->current_step > 0);
    
    if (app->current_step == TOTAL_STEPS - 1) {
        gtk_button_set_label(GTK_BUTTON(app->btn_next), "Finish Tour ✓");
    } else {
        gtk_button_set_label(GTK_BUTTON(app->btn_next), "Next Step →");
    }
}

static void on_prev_clicked(GtkWidget *btn, gpointer data) {
    (void)btn;
    WelcomeApp *app = (WelcomeApp *)data;
    if (app->current_step > 0) {
        app->current_step--;
        update_navigation_state(app);
    }
}

static void on_next_clicked(GtkWidget *btn, gpointer data) {
    (void)btn;
    WelcomeApp *app = (WelcomeApp *)data;
    if (app->current_step < TOTAL_STEPS - 1) {
        app->current_step++;
        update_navigation_state(app);
    } else {
        /* Finished */
        GtkWidget *win = gtk_widget_get_root(btn);
        if (GTK_IS_WINDOW(win)) {
            gtk_window_destroy(GTK_WINDOW(win));
        }
    }
}

static void on_launch_cmd(GtkWidget *btn, gpointer data) {
    (void)btn;
    const char *cmd = (const char *)data;
    if (cmd && strlen(cmd) > 0) {
        char full_cmd[256];
        snprintf(full_cmd, sizeof(full_cmd), "%s &", cmd);
        int ret = system(full_cmd);
        (void)ret;
    }
}

static GtkWidget *create_welcome_page(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_set_margin_start(box, 30);
    gtk_widget_set_margin_end(box, 30);
    gtk_widget_set_margin_top(box, 20);

    GtkWidget *icon_lbl = gtk_label_new("❖");
    gtk_widget_add_css_class(icon_lbl, "tour-hero-icon");
    gtk_box_append(GTK_BOX(box), icon_lbl);

    GtkWidget *title = gtk_label_new("Welcome to AuralisOS Apprenticeship");
    gtk_widget_add_css_class(title, "tour-page-title");
    gtk_box_append(GTK_BOX(box), title);

    GtkWidget *subtitle = gtk_label_new("Liquid Glass Edition • Extreme Efficiency (<95MB Idle RAM)");
    gtk_widget_add_css_class(subtitle, "tour-page-subtitle");
    gtk_box_append(GTK_BOX(box), subtitle);

    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_add_css_class(card, "tour-info-card");

    GtkWidget *p1 = gtk_label_new(
        "• Musl Libc & Alpine Linux Core: Built with zero systemd bloat for lightning-fast responsiveness.\n"
        "• Liquid Glass Compositor: Wayfire Wayland environment featuring signature Dual-Kawase blur.\n"
        "• ZRAM Compressed Memory: Enforces 150% RAM swap with aggressive compaction (vm.swappiness=180).\n"
        "• 4-World Subsystem Gateway: Run Windows (.exe), Android (.apk), and Linux (.deb/.rpm) natively."
    );
    gtk_widget_set_halign(p1, GTK_ALIGN_START);
    gtk_widget_add_css_class(p1, "tour-body-text");
    gtk_box_append(GTK_BOX(card), p1);

    gtk_box_append(GTK_BOX(box), card);
    return box;
}

static GtkWidget *create_shortcuts_page(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_set_margin_start(box, 30);
    gtk_widget_set_margin_end(box, 30);
    gtk_widget_set_margin_top(box, 20);

    GtkWidget *title = gtk_label_new("Mastering the Desktop: Keyboard Cheatsheet");
    gtk_widget_add_css_class(title, "tour-page-title");
    gtk_box_append(GTK_BOX(box), title);

    GtkWidget *subtitle = gtk_label_new("Everything in AuralisOS is reachable in a single instant keypress");
    gtk_widget_add_css_class(subtitle, "tour-page-subtitle");
    gtk_box_append(GTK_BOX(box), subtitle);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 16);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_widget_add_css_class(grid, "tour-info-card");

    const char *shortcuts[][2] = {
        {"Super (Windows Key)", "Toggle Start Menu & Search Launcher"},
        {"Super + D", "Open Real-Time System Dashboard & Telemetry"},
        {"Ctrl + Shift + Esc", "Open Auralis Task Manager & Process Monitor"},
        {"Super + Enter", "Launch Foot Terminal (Ultra-Low 2MB Footprint)"},
        {"Super + 1 / 2", "Switch Virtual Workspaces Instantly"},
        {"Super + B", "Toggle Dual-Kawase Liquid Glass Blur Effect"},
        {"Super + Q / Alt + F4", "Close Active Window"},
        {NULL, NULL}
    };

    for (int i = 0; shortcuts[i][0]; i++) {
        GtkWidget *k_lbl = gtk_label_new(shortcuts[i][0]);
        gtk_widget_add_css_class(k_lbl, "key-badge");
        gtk_widget_set_halign(k_lbl, GTK_ALIGN_START);

        GtkWidget *d_lbl = gtk_label_new(shortcuts[i][1]);
        gtk_widget_add_css_class(d_lbl, "key-desc");
        gtk_widget_set_halign(d_lbl, GTK_ALIGN_START);

        gtk_grid_attach(GTK_GRID(grid), k_lbl, 0, i, 1, 1);
        gtk_grid_attach(GTK_GRID(grid), d_lbl, 1, i, 1, 1);
    }

    gtk_box_append(GTK_BOX(box), grid);
    return box;
}

static GtkWidget *create_multiworld_page(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_set_margin_start(box, 30);
    gtk_widget_set_margin_end(box, 30);
    gtk_widget_set_margin_top(box, 20);

    GtkWidget *title = gtk_label_new("Multi-World Architecture: 4 Operating Systems in 1");
    gtk_widget_add_css_class(title, "tour-page-title");
    gtk_box_append(GTK_BOX(box), title);

    GtkWidget *subtitle = gtk_label_new("Run foreign applications natively without running heavyweight virtual machines");
    gtk_widget_add_css_class(subtitle, "tour-page-subtitle");
    gtk_box_append(GTK_BOX(box), subtitle);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 12);
    gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);

    const char *worlds[][3] = {
        {"Windows (.exe / .msi)", "Native Wayland Wine 9.x Driver", "Direct DirectX/Vulkan execution with zero X11 overhead."},
        {"Android (.apk)", "Waydroid LineageOS Subsystem", "Multi-window Android apps rendered straight to Wayland."},
        {"Linux (.deb / .rpm)", "Distrobox + Podman Containers", "Run software packages from Ubuntu, Debian, Arch & Fedora."},
        {"macOS (Mach-O)", "Darling Translation Subsystem", "Runs command-line Mach-O binaries and Apple frameworks."}
    };

    for (int i = 0; i < 4; i++) {
        GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
        gtk_widget_add_css_class(card, "subsys-world-card");

        GtkWidget *wt = gtk_label_new(worlds[i][0]);
        gtk_widget_add_css_class(wt, "world-title");
        gtk_widget_set_halign(wt, GTK_ALIGN_START);

        GtkWidget *we = gtk_label_new(worlds[i][1]);
        gtk_widget_add_css_class(we, "world-engine");
        gtk_widget_set_halign(we, GTK_ALIGN_START);

        GtkWidget *wd = gtk_label_new(worlds[i][2]);
        gtk_widget_add_css_class(wd, "world-desc");
        gtk_label_set_wrap(GTK_LABEL(wd), TRUE);
        gtk_widget_set_halign(wd, GTK_ALIGN_START);

        gtk_box_append(GTK_BOX(card), wt);
        gtk_box_append(GTK_BOX(card), we);
        gtk_box_append(GTK_BOX(card), wd);

        gtk_grid_attach(GTK_GRID(grid), card, i % 2, i / 2, 1, 1);
    }

    gtk_box_append(GTK_BOX(box), grid);
    return box;
}

static GtkWidget *create_hydration_page(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_set_margin_start(box, 30);
    gtk_widget_set_margin_end(box, 30);
    gtk_widget_set_margin_top(box, 20);

    GtkWidget *title = gtk_label_new("Modular Hydration: 0 MB Idle RAM");
    gtk_widget_add_css_class(title, "tour-page-title");
    gtk_box_append(GTK_BOX(box), title);

    GtkWidget *subtitle = gtk_label_new("Subsystems sleep in compressed layers and hydrate only when needed");
    gtk_widget_add_css_class(subtitle, "tour-page-subtitle");
    gtk_box_append(GTK_BOX(box), subtitle);

    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_add_css_class(card, "tour-info-card");

    GtkWidget *p1 = gtk_label_new(
        "Traditional Linux desktops keep dozens of daemons running, wasting 1-2 GB of RAM.\n\n"
        "AuralisOS uses Modular Hydration:\n"
        "• Standby Mode: Runtimes reside as compressed SquashFS layers in /var/lib/auralis/subsystems.\n"
        "• Instant Hydration: Launching an app mounts the subsystem in under 0.5s via loopback.\n"
        "• Automatic Reaping: The background reaper unmounts idle layers after applications close.\n\n"
        "You can manage and download runtime packages anytime inside the Auralis Store."
    );
    gtk_widget_set_halign(p1, GTK_ALIGN_START);
    gtk_widget_add_css_class(p1, "tour-body-text");
    gtk_box_append(GTK_BOX(card), p1);

    gtk_box_append(GTK_BOX(box), card);
    return box;
}

static GtkWidget *create_installation_page(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_set_margin_start(box, 30);
    gtk_widget_set_margin_end(box, 30);
    gtk_widget_set_margin_top(box, 20);

    GtkWidget *title = gtk_label_new("Ready to Install AuralisOS?");
    gtk_widget_add_css_class(title, "tour-page-title");
    gtk_box_append(GTK_BOX(box), title);

    GtkWidget *subtitle = gtk_label_new("Choose safe Windows NTFS loopback or full bare-metal installation");
    gtk_widget_add_css_class(subtitle, "tour-page-subtitle");
    gtk_box_append(GTK_BOX(box), subtitle);

    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_add_css_class(card, "tour-info-card");

    GtkWidget *p1 = gtk_label_new(
        "1. Windows Loopback Co-Existence (Safe & Recommended):\n"
        "   Installs inside a 24 GB sparse file at C:\\AuralisOS\\rootfs.img without repartitioning.\n"
        "   Your Windows drive remains completely untouched and safe.\n\n"
        "2. Native Bare-Metal Installation:\n"
        "   Formats the target drive with GPT, EFI System Partition, and ext4 rootfs for maximum speed."
    );
    gtk_widget_set_halign(p1, GTK_ALIGN_START);
    gtk_widget_add_css_class(p1, "tour-body-text");
    gtk_box_append(GTK_BOX(card), p1);

    /* Quick Launch Row */
    GtkWidget *btn_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_top(btn_row, 10);

    GtkWidget *btn_inst = gtk_button_new_with_label("💿 Launch Installer");
    gtk_widget_add_css_class(btn_inst, "btn-tour-launch-inst");
    g_signal_connect(btn_inst, "clicked", G_CALLBACK(on_launch_cmd), "/usr/local/bin/auralis-installer");
    gtk_box_append(GTK_BOX(btn_row), btn_inst);

    GtkWidget *btn_dash = gtk_button_new_with_label("❖ Open Dashboard");
    gtk_widget_add_css_class(btn_dash, "btn-tour-launch");
    g_signal_connect(btn_dash, "clicked", G_CALLBACK(on_launch_cmd), "/usr/local/bin/auralis-dashboard");
    gtk_box_append(GTK_BOX(btn_row), btn_dash);

    GtkWidget *btn_store = gtk_button_new_with_label("📦 Open Store");
    gtk_widget_add_css_class(btn_store, "btn-tour-launch");
    g_signal_connect(btn_store, "clicked", G_CALLBACK(on_launch_cmd), "/usr/local/bin/auralis-store");
    gtk_box_append(GTK_BOX(btn_row), btn_store);

    gtk_box_append(GTK_BOX(card), btn_row);

    gtk_box_append(GTK_BOX(box), card);
    return box;
}

static void apply_css(void) {
    GtkCssProvider *provider = gtk_css_provider_new();
    const char *paths[] = {
        "/usr/share/auralis-welcome/style.css",
        "style.css",
        "src/auralis-welcome/style.css",
        NULL
    };
    for (int i = 0; paths[i]; i++) {
        if (g_file_test(paths[i], G_FILE_TEST_EXISTS)) {
            gtk_css_provider_load_from_path(provider, paths[i]);
            gtk_style_context_add_provider_for_display(
                gdk_display_get_default(),
                GTK_STYLE_PROVIDER(provider),
                GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
            );
            break;
        }
    }
}

static void on_activate(GtkApplication *gtk_app, gpointer user_data) {
    (void)user_data;
    apply_css();

    WelcomeApp *app = g_new0(WelcomeApp, 1);
    app->current_step = 0;

    GtkWidget *window = gtk_application_window_new(gtk_app);
    gtk_window_set_title(GTK_WINDOW(window), "Auralis Apprenticeship: Interactive Tour");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 560);
    gtk_widget_add_css_class(window, "tour-window");

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    /* Stack for Tour Pages */
    app->stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(app->stack), GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
    gtk_stack_set_transition_duration(GTK_STACK(app->stack), 250);
    gtk_widget_set_vexpand(app->stack, TRUE);

    gtk_stack_add_named(GTK_STACK(app->stack), create_welcome_page(), "welcome");
    gtk_stack_add_named(GTK_STACK(app->stack), create_shortcuts_page(), "shortcuts");
    gtk_stack_add_named(GTK_STACK(app->stack), create_multiworld_page(), "multiworld");
    gtk_stack_add_named(GTK_STACK(app->stack), create_hydration_page(), "hydration");
    gtk_stack_add_named(GTK_STACK(app->stack), create_installation_page(), "installation");

    gtk_box_append(GTK_BOX(main_box), app->stack);

    /* Bottom Navigation Bar */
    GtkWidget *nav_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_add_css_class(nav_bar, "tour-nav-bar");
    gtk_widget_set_margin_start(nav_bar, 24);
    gtk_widget_set_margin_end(nav_bar, 24);
    gtk_widget_set_margin_top(nav_bar, 12);
    gtk_widget_set_margin_bottom(nav_bar, 16);

    app->btn_prev = gtk_button_new_with_label("← Previous");
    gtk_widget_add_css_class(app->btn_prev, "btn-nav-prev");
    g_signal_connect(app->btn_prev, "clicked", G_CALLBACK(on_prev_clicked), app);
    gtk_box_append(GTK_BOX(nav_bar), app->btn_prev);

    GtkWidget *nav_spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(nav_spacer, TRUE);
    gtk_box_append(GTK_BOX(nav_bar), nav_spacer);

    app->lbl_step = gtk_label_new("Step 1 of 5");
    gtk_widget_add_css_class(app->lbl_step, "tour-step-badge");
    gtk_box_append(GTK_BOX(nav_bar), app->lbl_step);

    GtkWidget *nav_spacer2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(nav_spacer2, TRUE);
    gtk_box_append(GTK_BOX(nav_bar), nav_spacer2);

    app->btn_next = gtk_button_new_with_label("Next Step →");
    gtk_widget_add_css_class(app->btn_next, "btn-nav-next");
    g_signal_connect(app->btn_next, "clicked", G_CALLBACK(on_next_clicked), app);
    gtk_box_append(GTK_BOX(nav_bar), app->btn_next);

    gtk_box_append(GTK_BOX(main_box), nav_bar);

    gtk_window_set_child(GTK_WINDOW(window), main_box);

    update_navigation_state(app);
    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("org.auralis.welcome", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
