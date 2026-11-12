/* ==============================================================================
 * AuralisOS Graphical System Installer: auralis-installer
 * Framework: C11, GTK4, musl libc
 * Desktop Integration: Liquid Glass Frosted UI with Dual-Kawase Blur
 * Modes:
 *   1. "Install Next to Windows" (Zero-Repartitioning Loopback Dual-Boot)
 *   2. "Native Drive Install" (Entire Disk or Dedicated Partition)
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
#include <glib.h>

typedef struct {
    GtkWidget *window;
    GtkWidget *stack;
    
    /* Page 1: Welcome */
    GtkWidget *radio_loopback;
    GtkWidget *radio_native;
    
    /* Page 2: Target Selection */
    GtkWidget *combo_targets;
    GtkWidget *entry_user;
    GtkWidget *entry_pass;
    GtkWidget *entry_host;
    GtkWidget *lbl_target_info;
    
    /* Page 3: Progress */
    GtkWidget *progress_bar;
    GtkWidget *lbl_status;
    GtkWidget *spinner;
    
    /* Page 4: Complete */
    GtkWidget *lbl_complete;
    
    /* State */
    char selected_mode[32];
    char selected_target[128];
    GPid backend_pid;
    GIOChannel *io_channel;
} InstallerApp;

static InstallerApp app_data;

/* Probe block devices and partitions */
static void populate_targets(InstallerApp *app) {
    gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(app->combo_targets));

    FILE *fp = popen("blkid -o device 2>/dev/null", "r");
    if (!fp) {
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(app->combo_targets), "/dev/sda", "Default Disk (/dev/sda)");
        gtk_combo_box_set_active(GTK_COMBO_BOX(app->combo_targets), 0);
        return;
    }

    char line[256];
    int count = 0;
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strlen(line) == 0 || strstr(line, "loop") || strstr(line, "sr0") || strstr(line, "ram"))
            continue;

        char desc[512];
        char fstype[64] = "Unknown";
        char size_buf[64] = "";

        /* Check filesystem type */
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "blkid -s TYPE -o value %s 2>/dev/null", line);
        FILE *fs_fp = popen(cmd, "r");
        if (fs_fp) {
            if (fgets(fstype, sizeof(fstype), fs_fp)) {
                fstype[strcspn(fstype, "\r\n")] = '\0';
            }
            pclose(fs_fp);
        }

        /* Check size */
        char disk_name[64];
        const char *base = strrchr(line, '/');
        if (base) {
            snprintf(disk_name, sizeof(disk_name), "%s", base + 1);
            snprintf(cmd, sizeof(cmd), "lsblk -b -n -o SIZE %s 2>/dev/null | head -n1", line);
            FILE *sz_fp = popen(cmd, "r");
            if (sz_fp) {
                long long bytes = 0;
                if (fscanf(sz_fp, "%lld", &bytes) == 1 && bytes > 0) {
                    double gb = (double)bytes / (1024.0 * 1024.0 * 1024.0);
                    snprintf(size_buf, sizeof(size_buf), " (%.1f GB)", gb);
                }
                pclose(sz_fp);
            }
        }

        snprintf(desc, sizeof(desc), "%s [%s]%s", line, fstype, size_buf);
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(app->combo_targets), line, desc);
        count++;
    }
    pclose(fp);

    if (count > 0) {
        gtk_combo_box_set_active(GTK_COMBO_BOX(app->combo_targets), 0);
    } else {
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(app->combo_targets), "/dev/sda", "Drive /dev/sda (Generic)");
        gtk_combo_box_set_active(GTK_COMBO_BOX(app->combo_targets), 0);
    }
}

/* Background process output watcher */
static gboolean on_backend_output(GIOChannel *source, GIOCondition condition, gpointer data) {
    (void)condition;
    InstallerApp *app = (InstallerApp *)data;
    gchar *line = NULL;
    gsize length = 0;
    GError *error = NULL;

    if (g_io_channel_read_line(source, &line, &length, NULL, &error) == G_IO_STATUS_NORMAL) {
        if (line) {
            line[strcspn(line, "\r\n")] = '\0';
            if (g_str_has_prefix(line, "PROGRESS: ")) {
                int pct = atoi(line + 10);
                gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(app->progress_bar), (double)pct / 100.0);
            } else if (g_str_has_prefix(line, "STATUS: ")) {
                gtk_label_set_text(GTK_LABEL(app->lbl_status), line + 8);
            } else if (g_strcmp0(line, "SUCCESS") == 0) {
                gtk_spinner_stop(GTK_SPINNER(app->spinner));
                gtk_stack_set_visible_child_name(GTK_STACK(app->stack), "complete");
            }
            g_free(line);
        }
        return TRUE;
    }
    return FALSE;
}

static void on_install_now_clicked(GtkWidget *btn, gpointer data) {
    (void)btn;
    InstallerApp *app = (InstallerApp *)data;

    const char *target = gtk_combo_box_get_active_id(GTK_COMBO_BOX(app->combo_targets));
    if (!target || strlen(target) == 0) target = "/dev/sda";
    snprintf(app->selected_target, sizeof(app->selected_target), "%s", target);

    const char *user = gtk_editable_get_text(GTK_EDITABLE(app->entry_user));
    const char *pass = gtk_editable_get_text(GTK_EDITABLE(app->entry_pass));
    const char *host = gtk_editable_get_text(GTK_EDITABLE(app->entry_host));

    if (!user || strlen(user) == 0) user = "auralis";
    if (!pass || strlen(pass) == 0) pass = "auralis";
    if (!host || strlen(host) == 0) host = "auralis";

    /* Switch to progress page */
    gtk_stack_set_visible_child_name(GTK_STACK(app->stack), "progress");
    gtk_spinner_start(GTK_SPINNER(app->spinner));
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(app->progress_bar), 0.05);
    gtk_label_set_text(GTK_LABEL(app->lbl_status), "Starting installation process...");

    /* Launch backend helper script */
    char *argv[] = {
        "/bin/sh",
        "/usr/local/bin/auralis-installer-backend",
        app->selected_mode,
        app->selected_target,
        (char *)host,
        (char *)user,
        (char *)pass,
        NULL
    };

    gint out_fd = -1;
    GError *err = NULL;
    if (g_spawn_async_with_pipes(NULL, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL,
                                 &app->backend_pid, NULL, &out_fd, NULL, &err)) {
        app->io_channel = g_io_channel_unix_new(out_fd);
        g_io_channel_set_encoding(app->io_channel, NULL, NULL);
        g_io_add_watch(app->io_channel, G_IO_IN | G_IO_HUP, on_backend_output, app);
    } else {
        gtk_label_set_text(GTK_LABEL(app->lbl_status), "Error: Failed to launch installer backend.");
    }
}

static void on_continue_clicked(GtkWidget *btn, gpointer data) {
    (void)btn;
    InstallerApp *app = (InstallerApp *)data;

    if (gtk_check_button_get_active(GTK_CHECK_BUTTON(app->radio_loopback))) {
        strcpy(app->selected_mode, "loopback");
        gtk_label_set_text(GTK_LABEL(app->lbl_target_info),
            "Mode: Install Next to Windows (Loopback)\n"
            "Creates \\AuralisOS\\rootfs.img (24 GB) on your selected Windows C: NTFS partition.\n"
            "Zero disk repartitioning required. 100% reversible.");
    } else {
        strcpy(app->selected_mode, "native");
        gtk_label_set_text(GTK_LABEL(app->lbl_target_info),
            "Mode: Native Drive Install (Entire Disk or Partition)\n"
            "Formats the selected drive with GPT, EFI System Partition, and ext4 rootfs.\n"
            "Warning: Back up existing data on the target partition before proceeding.");
    }

    populate_targets(app);
    gtk_stack_set_visible_child_name(GTK_STACK(app->stack), "target");
}

static void on_back_clicked(GtkWidget *btn, gpointer data) {
    (void)btn;
    InstallerApp *app = (InstallerApp *)data;
    gtk_stack_set_visible_child_name(GTK_STACK(app->stack), "welcome");
}

static void on_try_live_clicked(GtkWidget *btn, gpointer data) {
    (void)btn;
    InstallerApp *app = (InstallerApp *)data;
    gtk_window_close(GTK_WINDOW(app->window));
}

static void on_reboot_clicked(GtkWidget *btn, gpointer data) {
    (void)btn;
    (void)data;
    if (fork() == 0) {
        execl("/sbin/reboot", "reboot", NULL);
        exit(0);
    }
}

/* UI Construction */
static void activate(GtkApplication *gtk_app, gpointer user_data) {
    (void)user_data;
    InstallerApp *app = &app_data;

    app->window = gtk_application_window_new(gtk_app);
    gtk_window_set_title(GTK_WINDOW(app->window), "AuralisOS System Installer");
    gtk_window_set_default_size(GTK_WINDOW(app->window), 720, 520);
    gtk_window_set_resizable(GTK_WINDOW(app->window), FALSE);

    /* Apply CSS Styling */
    GtkCssProvider *css = gtk_css_provider_new();
    const char *style_path = "/usr/share/auralis-installer/style.css";
    if (access(style_path, R_OK) == 0) {
        gtk_css_provider_load_from_path(css, style_path);
    } else if (access("style.css", R_OK) == 0) {
        gtk_css_provider_load_from_path(css, "style.css");
    }
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(css),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    app->stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(app->stack), GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
    gtk_window_set_child(GTK_WINDOW(app->window), app->stack);

    /* --------------------------------------------------------------------------
     * Page 1: Welcome & Mode Selection
     * -------------------------------------------------------------------------- */
    GtkWidget *box_welcome = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_start(box_welcome, 36);
    gtk_widget_set_margin_end(box_welcome, 36);
    gtk_widget_set_margin_top(box_welcome, 32);
    gtk_widget_set_margin_bottom(box_welcome, 32);

    GtkWidget *lbl_title = gtk_label_new("Welcome to AuralisOS");
    gtk_widget_add_css_class(lbl_title, "installer-title");
    gtk_box_append(GTK_BOX(box_welcome), lbl_title);

    GtkWidget *lbl_subtitle = gtk_label_new("Ultra-Low-Memory Linux (<95 MB Idle RAM) with Liquid Glass Wayland Desktop");
    gtk_widget_add_css_class(lbl_subtitle, "installer-subtitle");
    gtk_box_append(GTK_BOX(box_welcome), lbl_subtitle);

    /* Card 1: Loopback Dual-Boot */
    GtkWidget *card_loop = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_add_css_class(card_loop, "mode-card");
    app->radio_loopback = gtk_check_button_new_with_label("Install Next to Windows (Safe Co-Existence Dual-Boot)");
    gtk_widget_add_css_class(app->radio_loopback, "mode-radio");
    gtk_check_button_set_active(GTK_CHECK_BUTTON(app->radio_loopback), TRUE);
    GtkWidget *lbl_loop_desc = gtk_label_new("Installs inside a 24 GB sparse file (C:\\AuralisOS\\rootfs.img) on your existing Windows NTFS drive.\nZero disk repartitioning. Completely safe and reversible with 1-click.");
    gtk_label_set_wrap(GTK_LABEL(lbl_loop_desc), TRUE);
    gtk_widget_add_css_class(lbl_loop_desc, "mode-desc");
    gtk_box_append(GTK_BOX(card_loop), app->radio_loopback);
    gtk_box_append(GTK_BOX(card_loop), lbl_loop_desc);
    gtk_box_append(GTK_BOX(box_welcome), card_loop);

    /* Card 2: Native Drive Install */
    GtkWidget *card_native = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_add_css_class(card_native, "mode-card");
    app->radio_native = gtk_check_button_new_with_label("Native Disk Installation (Entire Drive or Partition)");
    gtk_widget_add_css_class(app->radio_native, "mode-radio");
    gtk_check_button_set_group(GTK_CHECK_BUTTON(app->radio_native), GTK_CHECK_BUTTON(app->radio_loopback));
    GtkWidget *lbl_native_desc = gtk_label_new("Installs natively to an entire hard drive, SSD, or dedicated partition. Formats as ext4 with EFI bootloader.");
    gtk_label_set_wrap(GTK_LABEL(lbl_native_desc), TRUE);
    gtk_widget_add_css_class(lbl_native_desc, "mode-desc");
    gtk_box_append(GTK_BOX(card_native), app->radio_native);
    gtk_box_append(GTK_BOX(card_native), lbl_native_desc);
    gtk_box_append(GTK_BOX(box_welcome), card_native);

    /* Buttons */
    GtkWidget *btn_box_1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_halign(btn_box_1, GTK_ALIGN_END);
    gtk_widget_set_margin_top(btn_box_1, 16);

    GtkWidget *btn_try_live = gtk_button_new_with_label("Try Live Desktop First");
    gtk_widget_add_css_class(btn_try_live, "btn-secondary");
    g_signal_connect(btn_try_live, "clicked", G_CALLBACK(on_try_live_clicked), app);

    GtkWidget *btn_continue = gtk_button_new_with_label("Continue to Install →");
    gtk_widget_add_css_class(btn_continue, "btn-primary");
    g_signal_connect(btn_continue, "clicked", G_CALLBACK(on_continue_clicked), app);

    gtk_box_append(GTK_BOX(btn_box_1), btn_try_live);
    gtk_box_append(GTK_BOX(btn_box_1), btn_continue);
    gtk_box_append(GTK_BOX(box_welcome), btn_box_1);

    gtk_stack_add_named(GTK_STACK(app->stack), box_welcome, "welcome");

    /* --------------------------------------------------------------------------
     * Page 2: Target Selection & Credentials
     * -------------------------------------------------------------------------- */
    GtkWidget *box_target = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_set_margin_start(box_target, 36);
    gtk_widget_set_margin_end(box_target, 36);
    gtk_widget_set_margin_top(box_target, 28);
    gtk_widget_set_margin_bottom(box_target, 28);

    GtkWidget *lbl_target_hdr = gtk_label_new("Target Configuration");
    gtk_widget_add_css_class(lbl_target_hdr, "installer-title");
    gtk_box_append(GTK_BOX(box_target), lbl_target_hdr);

    app->lbl_target_info = gtk_label_new("");
    gtk_widget_add_css_class(app->lbl_target_info, "info-banner");
    gtk_box_append(GTK_BOX(box_target), app->lbl_target_info);

    /* Target Drive Selector */
    GtkWidget *lbl_drive = gtk_label_new("Select Target Partition or Drive:");
    gtk_widget_set_halign(lbl_drive, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(box_target), lbl_drive);

    app->combo_targets = gtk_combo_box_text_new();
    gtk_widget_add_css_class(app->combo_targets, "target-dropdown");
    gtk_box_append(GTK_BOX(box_target), app->combo_targets);

    /* User details grid */
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_widget_set_margin_top(grid, 8);

    GtkWidget *lbl_u = gtk_label_new("Username:");
    gtk_widget_set_halign(lbl_u, GTK_ALIGN_END);
    app->entry_user = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(app->entry_user), "auralis");

    GtkWidget *lbl_p = gtk_label_new("Password:");
    gtk_widget_set_halign(lbl_p, GTK_ALIGN_END);
    app->entry_pass = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(app->entry_pass), FALSE);
    gtk_editable_set_text(GTK_EDITABLE(app->entry_pass), "auralis");

    GtkWidget *lbl_h = gtk_label_new("Hostname:");
    gtk_widget_set_halign(lbl_h, GTK_ALIGN_END);
    app->entry_host = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(app->entry_host), "auralis");

    gtk_grid_attach(GTK_GRID(grid), lbl_u, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), app->entry_user, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), lbl_p, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), app->entry_pass, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), lbl_h, 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), app->entry_host, 1, 2, 1, 1);
    gtk_box_append(GTK_BOX(box_target), grid);

    /* Buttons */
    GtkWidget *btn_box_2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_halign(btn_box_2, GTK_ALIGN_END);
    gtk_widget_set_margin_top(btn_box_2, 16);

    GtkWidget *btn_back = gtk_button_new_with_label("← Back");
    gtk_widget_add_css_class(btn_back, "btn-secondary");
    g_signal_connect(btn_back, "clicked", G_CALLBACK(on_back_clicked), app);

    GtkWidget *btn_install = gtk_button_new_with_label("Install AuralisOS Now");
    gtk_widget_add_css_class(btn_install, "btn-primary");
    g_signal_connect(btn_install, "clicked", G_CALLBACK(on_install_now_clicked), app);

    gtk_box_append(GTK_BOX(btn_box_2), btn_back);
    gtk_box_append(GTK_BOX(btn_box_2), btn_install);
    gtk_box_append(GTK_BOX(box_target), btn_box_2);

    gtk_stack_add_named(GTK_STACK(app->stack), box_target, "target");

    /* --------------------------------------------------------------------------
     * Page 3: Progress
     * -------------------------------------------------------------------------- */
    GtkWidget *box_prog = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_set_margin_start(box_prog, 48);
    gtk_widget_set_margin_end(box_prog, 48);
    gtk_widget_set_margin_top(box_prog, 64);
    gtk_widget_set_valign(box_prog, GTK_ALIGN_CENTER);

    GtkWidget *lbl_prog_hdr = gtk_label_new("Installing AuralisOS...");
    gtk_widget_add_css_class(lbl_prog_hdr, "installer-title");
    gtk_box_append(GTK_BOX(box_prog), lbl_prog_hdr);

    app->spinner = gtk_spinner_new();
    gtk_widget_set_size_request(app->spinner, 48, 48);
    gtk_box_append(GTK_BOX(box_prog), app->spinner);

    app->progress_bar = gtk_progress_bar_new();
    gtk_widget_set_size_request(app->progress_bar, -1, 14);
    gtk_widget_add_css_class(app->progress_bar, "custom-progress");
    gtk_box_append(GTK_BOX(box_prog), app->progress_bar);

    app->lbl_status = gtk_label_new("Preparing target drive...");
    gtk_widget_add_css_class(app->lbl_status, "status-text");
    gtk_box_append(GTK_BOX(box_prog), app->lbl_status);

    gtk_stack_add_named(GTK_STACK(app->stack), box_prog, "progress");

    /* --------------------------------------------------------------------------
     * Page 4: Complete / Success
     * -------------------------------------------------------------------------- */
    GtkWidget *box_comp = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_set_margin_start(box_comp, 48);
    gtk_widget_set_margin_end(box_comp, 48);
    gtk_widget_set_margin_top(box_comp, 64);
    gtk_widget_set_valign(box_comp, GTK_ALIGN_CENTER);

    GtkWidget *lbl_comp_hdr = gtk_label_new("🎉 Installation Complete!");
    gtk_widget_add_css_class(lbl_comp_hdr, "installer-title");
    gtk_box_append(GTK_BOX(box_comp), lbl_comp_hdr);

    GtkWidget *lbl_comp_msg = gtk_label_new(
        "AuralisOS has been installed successfully.\n"
        "You can restart your machine now to boot into your new operating system,\n"
        "or continue exploring the Live desktop.");
    gtk_label_set_justify(GTK_LABEL(lbl_comp_msg), GTK_JUSTIFY_CENTER);
    gtk_widget_add_css_class(lbl_comp_msg, "installer-subtitle");
    gtk_box_append(GTK_BOX(box_comp), lbl_comp_msg);

    GtkWidget *btn_box_3 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_widget_set_halign(btn_box_3, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(btn_box_3, 24);

    GtkWidget *btn_stay = gtk_button_new_with_label("Stay in Live Desktop");
    gtk_widget_add_css_class(btn_stay, "btn-secondary");
    g_signal_connect(btn_stay, "clicked", G_CALLBACK(on_try_live_clicked), app);

    GtkWidget *btn_reboot = gtk_button_new_with_label("Restart Computer Now ↻");
    gtk_widget_add_css_class(btn_reboot, "btn-primary");
    g_signal_connect(btn_reboot, "clicked", G_CALLBACK(on_reboot_clicked), app);

    gtk_box_append(GTK_BOX(btn_box_3), btn_stay);
    gtk_box_append(GTK_BOX(btn_box_3), btn_reboot);
    gtk_box_append(GTK_BOX(box_comp), btn_box_3);

    gtk_stack_add_named(GTK_STACK(app->stack), box_comp, "complete");

    /* Show window */
    gtk_window_present(GTK_WINDOW(app->window));
}

int main(int argc, char *argv[]) {
    GtkApplication *app = gtk_application_new("org.auralis.installer", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
