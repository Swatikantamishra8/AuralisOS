/*
 * AuralisOS System Dashboard: auralis-dashboard
 * Liquid Glass Edition (<95MB Idle RAM)
 * Provides real-time hardware telemetry, Multi-World subsystem toggles,
 * and quick system controls.
 */

#include <gtk/gtk.h>
#include <stdio exciting>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/statvfs.h>

typedef struct {
    GtkWidget *lbl_ram;
    GtkWidget *bar_ram;
    GtkWidget *lbl_cpu;
    GtkWidget *bar_cpu;
    GtkWidget *lbl_zram;
    GtkWidget *bar_zram;
    GtkWidget *lbl_disk;
    GtkWidget *bar_disk;
    GtkWidget *lbl_uptime;
    GtkWidget *lbl_subsys_wine;
    GtkWidget *lbl_subsys_android;
    GtkWidget *lbl_subsys_linux;
    GtkWidget *lbl_subsys_macos;
    
    unsigned long long prev_idle;
    unsigned long long prev_total;
} DashboardApp;

static void read_ram_info(unsigned long *used_mb, unsigned long *total_mb, double *frac) {
    *used_mb = 0;
    *total_mb = 1;
    *frac = 0.0;
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) return;
    char line[256];
    unsigned long mem_total = 0, mem_free = 0, mem_avail = 0, buffers = 0, cached = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "MemTotal:", 9) == 0) sscanf(line + 9, "%lu", &mem_total);
        else if (strncmp(line, "MemFree:", 8) == 0) sscanf(line + 8, "%lu", &mem_free);
        else if (strncmp(line, "MemAvailable:", 13) == 0) sscanf(line + 13, "%lu", &mem_avail);
        else if (strncmp(line, "Buffers:", 8) == 0) sscanf(line + 8, "%lu", &buffers);
        else if (strncmp(line, "Cached:", 7) == 0) sscanf(line + 7, "%lu", &cached);
    }
    fclose(f);
    if (mem_avail == 0) mem_avail = mem_free + buffers + cached;
    unsigned long used = (mem_total > mem_avail) ? (mem_total - mem_avail) : 0;
    *used_mb = used / 1024;
    *total_mb = mem_total / 1024;
    if (*total_mb > 0) *frac = (double)(*used_mb) / (double)(*total_mb);
}

static void read_cpu_load(DashboardApp *app, double *load) {
    *load = 0.0;
    FILE *f = fopen("/proc/stat", "r");
    if (!f) return;
    char line[256];
    if (fgets(line, sizeof(line), f)) {
        unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
        if (sscanf(line, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
                   &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal) >= 4) {
            unsigned long long idle_all = idle + iowait;
            unsigned long long total = user + nice + system + idle + iowait + irq + softirq + steal;
            if (app->prev_total > 0 && total > app->prev_total) {
                unsigned long long diff_idle = idle_all - app->prev_idle;
                unsigned long long diff_total = total - app->prev_total;
                if (diff_total > 0 && diff_idle <= diff_total) {
                    *load = (double)(diff_total - diff_idle) / (double)diff_total;
                }
            }
            app->prev_idle = idle_all;
            app->prev_total = total;
        }
    }
    fclose(f);
}

static void read_disk_info(double *frac, unsigned long *avail_gb, unsigned long *total_gb) {
    *frac = 0.0;
    *avail_gb = 0;
    *total_gb = 1;
    struct statvfs s;
    if (statvfs("/", &s) == 0) {
        unsigned long long total = (unsigned long long)s.f_blocks * s.f_frsize;
        unsigned long long free_bytes = (unsigned long long)s.f_bavail * s.f_frsize;
        unsigned long long used = (total > free_bytes) ? (total - free_bytes) : 0;
        *total_gb = total / (1024 * 1024 * 1024);
        *avail_gb = free_bytes / (1024 * 1024 * 1024);
        if (total > 0) *frac = (double)used / (double)total;
    }
}

static gboolean update_telemetry(gpointer data) {
    DashboardApp *app = (DashboardApp *)data;
    
    /* 1. RAM */
    unsigned long ram_used, ram_total;
    double ram_frac;
    read_ram_info(&ram_used, &ram_total, &ram_frac);
    char ram_str[128];
    snprintf(ram_str, sizeof(ram_str), "%lu MB / %lu MB", ram_used, ram_total);
    gtk_label_set_text(GTK_LABEL(app->lbl_ram), ram_str);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(app->bar_ram), ram_frac);
    
    /* 2. CPU */
    double cpu_load;
    read_cpu_load(app, &cpu_load);
    char cpu_str[64];
    snprintf(cpu_str, sizeof(cpu_str), "%.1f %% Load", cpu_load * 100.0);
    gtk_label_set_text(GTK_LABEL(app->lbl_cpu), cpu_str);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(app->bar_cpu), cpu_load);
    
    /* 3. Disk */
    double disk_frac;
    unsigned long avail_gb, total_gb;
    read_disk_info(&disk_frac, &avail_gb, &total_gb);
    char disk_str[128];
    snprintf(disk_str, sizeof(disk_str), "%lu GB Free of %lu GB", avail_gb, total_gb);
    gtk_label_set_text(GTK_LABEL(app->lbl_disk), disk_str);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(app->bar_disk), disk_frac);
    
    /* 4. Subsystem Mount States */
    FILE *fmount = fopen("/proc/mounts", "r");
    gboolean wine_on = FALSE, android_on = FALSE, linux_on = FALSE, macos_on = FALSE;
    if (fmount) {
        char mline[512];
        while (fgets(mline, sizeof(mline), fmount)) {
            if (strstr(mline, "/var/lib/auralis/mnt/wine")) wine_on = TRUE;
            if (strstr(mline, "/var/lib/auralis/mnt/waydroid")) android_on = TRUE;
            if (strstr(mline, "/var/lib/auralis/mnt/distrobox")) linux_on = TRUE;
            if (strstr(mline, "/var/lib/auralis/mnt/darling")) macos_on = TRUE;
        }
        fclose(fmount);
    }
    
    gtk_label_set_text(GTK_LABEL(app->lbl_subsys_wine), wine_on ? "● Mounted (Active)" : "○ Standby (0MB Idle)");
    gtk_label_set_text(GTK_LABEL(app->lbl_subsys_android), android_on ? "● Mounted (Active)" : "○ Standby (0MB Idle)");
    gtk_label_set_text(GTK_LABEL(app->lbl_subsys_linux), linux_on ? "● Mounted (Active)" : "○ Standby (0MB Idle)");
    gtk_label_set_text(GTK_LABEL(app->lbl_subsys_macos), macos_on ? "● Mounted (Active)" : "○ Standby (0MB Idle)");
    
    return G_SOURCE_CONTINUE;
}

static void on_launch_app(GtkWidget *btn, gpointer data) {
    (void)btn;
    const char *cmd = (const char *)data;
    if (cmd && strlen(cmd) > 0) {
        char full_cmd[256];
        snprintf(full_cmd, sizeof(full_cmd), "%s &", cmd);
        int ret = system(full_cmd);
        (void)ret;
    }
}

static void on_cycle_wallpaper(GtkWidget *btn, gpointer data) {
    (void)btn;
    (void)data;
    int ret = system("/usr/local/bin/auralis-wallpaper random &");
    (void)ret;
}

static void apply_css(void) {
    GtkCssProvider *provider = gtk_css_provider_new();
    const char *paths[] = {
        "/usr/share/auralis-dashboard/style.css",
        "style.css",
        "src/auralis-dashboard/style.css",
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

    DashboardApp *app = g_new0(DashboardApp, 1);

    GtkWidget *window = gtk_application_window_new(gtk_app);
    gtk_window_set_title(GTK_WINDOW(window), "Auralis System Dashboard");
    gtk_window_set_default_size(GTK_WINDOW(window), 820, 620);
    gtk_widget_add_css_class(window, "dashboard-window");

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_set_margin_start(main_box, 20);
    gtk_widget_set_margin_end(main_box, 20);
    gtk_widget_set_margin_top(main_box, 18);
    gtk_widget_set_margin_bottom(main_box, 18);

    /* 1. Top Header */
    GtkWidget *header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *title_lbl = gtk_label_new("❖ AURALIS DASHBOARD");
    gtk_widget_add_css_class(title_lbl, "dashboard-title");
    gtk_box_append(GTK_BOX(header_box), title_lbl);

    GtkWidget *tag_lbl = gtk_label_new("Liquid Glass Edition • <95MB Idle RAM");
    gtk_widget_add_css_class(tag_lbl, "dashboard-tag");
    gtk_box_append(GTK_BOX(header_box), tag_lbl);

    GtkWidget *hdr_spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(hdr_spacer, TRUE);
    gtk_box_append(GTK_BOX(header_box), hdr_spacer);

    GtkWidget *btn_tour = gtk_button_new_with_label("✦ Apprenticeship Tour");
    gtk_widget_add_css_class(btn_tour, "btn-action-primary");
    g_signal_connect(btn_tour, "clicked", G_CALLBACK(on_launch_app), "/usr/local/bin/auralis-welcome");
    gtk_box_append(GTK_BOX(header_box), btn_tour);

    gtk_box_append(GTK_BOX(main_box), header_box);

    /* 2. System Telemetry Grid (RAM, CPU, ZRAM, Storage) */
    GtkWidget *grid_metrics = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid_metrics), 12);
    gtk_grid_set_row_spacing(GTK_GRID(grid_metrics), 12);
    gtk_grid_set_column_homogeneous(GTK_GRID(grid_metrics), TRUE);

    /* Card 1: RAM */
    GtkWidget *card_ram = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_add_css_class(card_ram, "metric-card");
    GtkWidget *lbl_ram_title = gtk_label_new("MEMORY FOOTPRINT");
    gtk_widget_add_css_class(lbl_ram_title, "metric-title");
    gtk_widget_set_halign(lbl_ram_title, GTK_ALIGN_START);
    app->lbl_ram = gtk_label_new("Scanning...");
    gtk_widget_add_css_class(app->lbl_ram, "metric-value");
    gtk_widget_set_halign(app->lbl_ram, GTK_ALIGN_START);
    app->bar_ram = gtk_progress_bar_new();
    gtk_widget_add_css_class(app->bar_ram, "metric-progress");
    GtkWidget *lbl_ram_sub = gtk_label_new("Target: <95 MB Idle (Active Optimization)");
    gtk_widget_add_css_class(lbl_ram_sub, "metric-sub");
    gtk_widget_set_halign(lbl_ram_sub, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(card_ram), lbl_ram_title);
    gtk_box_append(GTK_BOX(card_ram), app->lbl_ram);
    gtk_box_append(GTK_BOX(card_ram), app->bar_ram);
    gtk_box_append(GTK_BOX(card_ram), lbl_ram_sub);
    gtk_grid_attach(GTK_GRID(grid_metrics), card_ram, 0, 0, 1, 1);

    /* Card 2: CPU */
    GtkWidget *card_cpu = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_add_css_class(card_cpu, "metric-card");
    GtkWidget *lbl_cpu_title = gtk_label_new("PROCESSOR LOAD");
    gtk_widget_add_css_class(lbl_cpu_title, "metric-title");
    gtk_widget_set_halign(lbl_cpu_title, GTK_ALIGN_START);
    app->lbl_cpu = gtk_label_new("Measuring...");
    gtk_widget_add_css_class(app->lbl_cpu, "metric-value");
    gtk_widget_set_halign(app->lbl_cpu, GTK_ALIGN_START);
    app->bar_cpu = gtk_progress_bar_new();
    gtk_widget_add_css_class(app->bar_cpu, "metric-progress");
    GtkWidget *lbl_cpu_sub = gtk_label_new("Low-Latency Wayland Compositor");
    gtk_widget_add_css_class(lbl_cpu_sub, "metric-sub");
    gtk_widget_set_halign(lbl_cpu_sub, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(card_cpu), lbl_cpu_title);
    gtk_box_append(GTK_BOX(card_cpu), app->lbl_cpu);
    gtk_box_append(GTK_BOX(card_cpu), app->bar_cpu);
    gtk_box_append(GTK_BOX(card_cpu), lbl_cpu_sub);
    gtk_grid_attach(GTK_GRID(grid_metrics), card_cpu, 1, 0, 1, 1);

    /* Card 3: ZRAM Swap */
    GtkWidget *card_zram = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_add_css_class(card_zram, "metric-card");
    GtkWidget *lbl_zram_title = gtk_label_new("ZRAM COMPRESSED SWAP");
    gtk_widget_add_css_class(lbl_zram_title, "metric-title");
    gtk_widget_set_halign(lbl_zram_title, GTK_ALIGN_START);
    app->lbl_zram = gtk_label_new("150% RAM (zstd algo)");
    gtk_widget_add_css_class(app->lbl_zram, "metric-value");
    gtk_widget_set_halign(app->lbl_zram, GTK_ALIGN_START);
    app->bar_zram = gtk_progress_bar_new();
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(app->bar_zram), 0.15);
    gtk_widget_add_css_class(app->bar_zram, "metric-progress");
    GtkWidget *lbl_zram_sub = gtk_label_new("Swappiness: 180 (Instant Reclamation)");
    gtk_widget_add_css_class(lbl_zram_sub, "metric-sub");
    gtk_widget_set_halign(lbl_zram_sub, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(card_zram), lbl_zram_title);
    gtk_box_append(GTK_BOX(card_zram), app->lbl_zram);
    gtk_box_append(GTK_BOX(card_zram), app->bar_zram);
    gtk_box_append(GTK_BOX(card_zram), lbl_zram_sub);
    gtk_grid_attach(GTK_GRID(grid_metrics), card_zram, 2, 0, 1, 1);

    /* Card 4: Storage */
    GtkWidget *card_disk = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_add_css_class(card_disk, "metric-card");
    GtkWidget *lbl_disk_title = gtk_label_new("SYSTEM STORAGE");
    gtk_widget_add_css_class(lbl_disk_title, "metric-title");
    gtk_widget_set_halign(lbl_disk_title, GTK_ALIGN_START);
    app->lbl_disk = gtk_label_new("Detecting...");
    gtk_widget_add_css_class(app->lbl_disk, "metric-value");
    gtk_widget_set_halign(app->lbl_disk, GTK_ALIGN_START);
    app->bar_disk = gtk_progress_bar_new();
    gtk_widget_add_css_class(app->bar_disk, "metric-progress");
    GtkWidget *lbl_disk_sub = gtk_label_new("Virtual Loopback & Native Root");
    gtk_widget_add_css_class(lbl_disk_sub, "metric-sub");
    gtk_widget_set_halign(lbl_disk_sub, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(card_disk), lbl_disk_title);
    gtk_box_append(GTK_BOX(card_disk), app->lbl_disk);
    gtk_box_append(GTK_BOX(card_disk), app->bar_disk);
    gtk_box_append(GTK_BOX(card_disk), lbl_disk_sub);
    gtk_grid_attach(GTK_GRID(grid_metrics), card_disk, 3, 0, 1, 1);

    gtk_box_append(GTK_BOX(main_box), grid_metrics);

    /* 3. Section Header: Multi-World Subsystems */
    GtkWidget *subsys_hdr = gtk_label_new("MULTI-WORLD UNIVERSAL RUNTIMES (ON-DEMAND MODULAR HYDRATION)");
    gtk_widget_add_css_class(subsys_hdr, "section-header");
    gtk_widget_set_halign(subsys_hdr, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(main_box), subsys_hdr);

    /* Subsystems Grid */
    GtkWidget *grid_subsys = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid_subsys), 12);
    gtk_grid_set_row_spacing(GTK_GRID(grid_subsys), 12);
    gtk_grid_set_column_homogeneous(GTK_GRID(grid_subsys), TRUE);

    /* 1. Windows */
    GtkWidget *tile_wine = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_add_css_class(tile_wine, "subsys-tile");
    GtkWidget *twine_title = gtk_label_new("Windows Engine (.exe)");
    gtk_widget_add_css_class(twine_title, "subsys-title");
    gtk_widget_set_halign(twine_title, GTK_ALIGN_START);
    app->lbl_subsys_wine = gtk_label_new("○ Standby (0MB Idle)");
    gtk_widget_add_css_class(app->lbl_subsys_wine, "subsys-badge");
    gtk_widget_set_halign(app->lbl_subsys_wine, GTK_ALIGN_START);
    GtkWidget *twine_desc = gtk_label_new("Native Wayland Wine 9.x runner");
    gtk_widget_add_css_class(twine_desc, "subsys-desc");
    gtk_widget_set_halign(twine_desc, GTK_ALIGN_START);
    GtkWidget *btn_wine_act = gtk_button_new_with_label("Manage in Store");
    gtk_widget_add_css_class(btn_wine_act, "btn-subsys");
    g_signal_connect(btn_wine_act, "clicked", G_CALLBACK(on_launch_app), "/usr/local/bin/auralis-store");
    gtk_box_append(GTK_BOX(tile_wine), twine_title);
    gtk_box_append(GTK_BOX(tile_wine), app->lbl_subsys_wine);
    gtk_box_append(GTK_BOX(tile_wine), twine_desc);
    gtk_box_append(GTK_BOX(tile_wine), btn_wine_act);
    gtk_grid_attach(GTK_GRID(grid_subsys), tile_wine, 0, 0, 1, 1);

    /* 2. Android */
    GtkWidget *tile_droid = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_add_css_class(tile_droid, "subsys-tile");
    GtkWidget *tdroid_title = gtk_label_new("Android Engine (.apk)");
    gtk_widget_add_css_class(tdroid_title, "subsys-title");
    gtk_widget_set_halign(tdroid_title, GTK_ALIGN_START);
    app->lbl_subsys_android = gtk_label_new("○ Standby (0MB Idle)");
    gtk_widget_add_css_class(app->lbl_subsys_android, "subsys-badge");
    gtk_widget_set_halign(app->lbl_subsys_android, GTK_ALIGN_START);
    GtkWidget *tdroid_desc = gtk_label_new("Waydroid LineageOS container");
    gtk_widget_add_css_class(tdroid_desc, "subsys-desc");
    gtk_widget_set_halign(tdroid_desc, GTK_ALIGN_START);
    GtkWidget *btn_droid_act = gtk_button_new_with_label("Manage in Store");
    gtk_widget_add_css_class(btn_droid_act, "btn-subsys");
    g_signal_connect(btn_droid_act, "clicked", G_CALLBACK(on_launch_app), "/usr/local/bin/auralis-store");
    gtk_box_append(GTK_BOX(tile_droid), tdroid_title);
    gtk_box_append(GTK_BOX(tile_droid), app->lbl_subsys_android);
    gtk_box_append(GTK_BOX(tile_droid), tdroid_desc);
    gtk_box_append(GTK_BOX(tile_droid), btn_droid_act);
    gtk_grid_attach(GTK_GRID(grid_subsys), tile_droid, 1, 0, 1, 1);

    /* 3. Linux */
    GtkWidget *tile_lin = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_add_css_class(tile_lin, "subsys-tile");
    GtkWidget *tlin_title = gtk_label_new("Linux Containers (.deb/.rpm)");
    gtk_widget_add_css_class(tlin_title, "subsys-title");
    gtk_widget_set_halign(tlin_title, GTK_ALIGN_START);
    app->lbl_subsys_linux = gtk_label_new("○ Standby (0MB Idle)");
    gtk_widget_add_css_class(app->lbl_subsys_linux, "subsys-badge");
    gtk_widget_set_halign(app->lbl_subsys_linux, GTK_ALIGN_START);
    GtkWidget *tlin_desc = gtk_label_new("Distrobox & Podman rootless layer");
    gtk_widget_add_css_class(tlin_desc, "subsys-desc");
    gtk_widget_set_halign(tlin_desc, GTK_ALIGN_START);
    GtkWidget *btn_lin_act = gtk_button_new_with_label("Manage in Store");
    gtk_widget_add_css_class(btn_lin_act, "btn-subsys");
    g_signal_connect(btn_lin_act, "clicked", G_CALLBACK(on_launch_app), "/usr/local/bin/auralis-store");
    gtk_box_append(GTK_BOX(tile_lin), tlin_title);
    gtk_box_append(GTK_BOX(tile_lin), app->lbl_subsys_linux);
    gtk_box_append(GTK_BOX(tile_lin), tlin_desc);
    gtk_box_append(GTK_BOX(tile_lin), btn_lin_act);
    gtk_grid_attach(GTK_GRID(grid_subsys), tile_lin, 2, 0, 1, 1);

    /* 4. macOS */
    GtkWidget *tile_mac = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_add_css_class(tile_mac, "subsys-tile");
    GtkWidget *tmac_title = gtk_label_new("macOS Engine (Mach-O)");
    gtk_widget_add_css_class(tmac_title, "subsys-title");
    gtk_widget_set_halign(tmac_title, GTK_ALIGN_START);
    app->lbl_subsys_macos = gtk_label_new("○ Standby (0MB Idle)");
    gtk_widget_add_css_class(app->lbl_subsys_macos, "subsys-badge");
    gtk_widget_set_halign(app->lbl_subsys_macos, GTK_ALIGN_START);
    GtkWidget *tmac_desc = gtk_label_new("Darling Mach-O translation");
    gtk_widget_add_css_class(tmac_desc, "subsys-desc");
    gtk_widget_set_halign(tmac_desc, GTK_ALIGN_START);
    GtkWidget *btn_mac_act = gtk_button_new_with_label("Manage in Store");
    gtk_widget_add_css_class(btn_mac_act, "btn-subsys");
    g_signal_connect(btn_mac_act, "clicked", G_CALLBACK(on_launch_app), "/usr/local/bin/auralis-store");
    gtk_box_append(GTK_BOX(tile_mac), tmac_title);
    gtk_box_append(GTK_BOX(tile_mac), app->lbl_subsys_macos);
    gtk_box_append(GTK_BOX(tile_mac), tmac_desc);
    gtk_box_append(GTK_BOX(tile_mac), btn_mac_act);
    gtk_grid_attach(GTK_GRID(grid_subsys), tile_mac, 3, 0, 1, 1);

    gtk_box_append(GTK_BOX(main_box), grid_subsys);

    /* 4. Quick Action Tool Buttons */
    GtkWidget *actions_hdr = gtk_label_new("QUICK ACTIONS & SYSTEM CONTROLS");
    gtk_widget_add_css_class(actions_hdr, "section-header");
    gtk_widget_set_halign(actions_hdr, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(main_box), actions_hdr);

    GtkWidget *act_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);

    GtkWidget *btn_tm = gtk_button_new_with_label("⚡ Task Manager");
    gtk_widget_add_css_class(btn_tm, "btn-control");
    g_signal_connect(btn_tm, "clicked", G_CALLBACK(on_launch_app), "/usr/local/bin/auralis-taskmgr");
    gtk_box_append(GTK_BOX(act_bar), btn_tm);

    GtkWidget *btn_store = gtk_button_new_with_label("📦 App Store");
    gtk_widget_add_css_class(btn_store, "btn-control");
    g_signal_connect(btn_store, "clicked", G_CALLBACK(on_launch_app), "/usr/local/bin/auralis-store");
    gtk_box_append(GTK_BOX(act_bar), btn_store);

    GtkWidget *btn_term = gtk_button_new_with_label(" Terminal");
    gtk_widget_add_css_class(btn_term, "btn-control");
    g_signal_connect(btn_term, "clicked", G_CALLBACK(on_launch_app), "foot");
    gtk_box_append(GTK_BOX(act_bar), btn_term);

    GtkWidget *btn_bg = gtk_button_new_with_label("🖼 Change Wallpaper");
    gtk_widget_add_css_class(btn_bg, "btn-control");
    g_signal_connect(btn_bg, "clicked", G_CALLBACK(on_cycle_wallpaper), NULL);
    gtk_box_append(GTK_BOX(act_bar), btn_bg);

    GtkWidget *btn_inst = gtk_button_new_with_label("💿 Install AuralisOS");
    gtk_widget_add_css_class(btn_inst, "btn-control-highlight");
    g_signal_connect(btn_inst, "clicked", G_CALLBACK(on_launch_app), "/usr/local/bin/auralis-installer");
    gtk_box_append(GTK_BOX(act_bar), btn_inst);

    gtk_box_append(GTK_BOX(main_box), act_bar);

    gtk_window_set_child(GTK_WINDOW(window), main_box);

    /* Initial update and periodic polling every 1.5 seconds */
    update_telemetry(app);
    g_timeout_add(1500, update_telemetry, app);

    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("org.auralis.dashboard", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
