#include "ui.h"

#include "storage.h"
#include "toggle_ipc.h"
#include "util.h"

#include <ctype.h>
#include <gtk/gtk.h>
#include <pango/pango.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

typedef struct {
    RcopyConfig cfg;
    ClipList list;
    GtkWidget *window;
    GtkWidget *search;
    GtkWidget *listbox;
    int server_fd;
    time_t index_mtime;
    off_t index_size;
} AppState;

static void rebuild_list(AppState *state);

static const char *pick_best_mime(char **types, size_t count) {
    static const char *preferred[] = {
        "image/png",
        "image/jpeg",
        "image/webp",
        "text/plain;charset=utf-8",
        "text/plain",
    };
    size_t i;
    size_t j;

    for (i = 0; i < sizeof(preferred) / sizeof(preferred[0]); i++) {
        for (j = 0; j < count; j++) {
            if (strcmp(preferred[i], types[j]) == 0) {
                return types[j];
            }
        }
    }

    return (count > 0) ? types[0] : NULL;
}

static void sync_latest_clipboard(AppState *state) {
    char **types = NULL;
    size_t count = 0;
    const char *mime_type = "text/plain";
    char *current = NULL;
    size_t current_len = 0;
    ClipItem last;
    int changed = 1;

    memset(&last, 0, sizeof(last));

    if (util_list_clipboard_types(&types, &count) == 0 && count > 0) {
        const char *picked = pick_best_mime(types, count);
        if (picked != NULL) {
            mime_type = picked;
            if (util_read_wl_paste_type(mime_type, &current, &current_len) != 0 || current_len == 0) {
                free(current);
                current = NULL;
                current_len = 0;
            }
        }
    }

    if (current == NULL || current_len == 0) {
        mime_type = "text/plain";
        if (util_read_wl_paste(&current, &current_len) != 0 || current_len == 0) {
            free(current);
            util_free_string_list(types, count);
            return;
        }
    }

    if (storage_get_last(&state->cfg, &last) == 0 && last.content != NULL && last.mime_type != NULL) {
        if (strcmp(last.mime_type, mime_type) == 0 &&
            last.content_len == current_len &&
            memcmp(last.content, current, current_len) == 0) {
            changed = 0;
        }
    }

    if (changed) {
        storage_save(&state->cfg, mime_type, current, current_len);
    }

    storage_free_item(&last);
    free(current);
    util_free_string_list(types, count);
}

static int read_index_signature(AppState *state, time_t *mtime, off_t *size) {
    struct stat st;
    if (stat(state->cfg.index_file, &st) != 0) {
        return -1;
    }
    *mtime = st.st_mtime;
    *size = st.st_size;
    return 0;
}

static void load_entries(AppState *state) {
    storage_free_list(&state->list);
    memset(&state->list, 0, sizeof(state->list));
    storage_load_all(&state->cfg, &state->list, (size_t)state->cfg.max_items);

    if (read_index_signature(state, &state->index_mtime, &state->index_size) != 0) {
        state->index_mtime = 0;
        state->index_size = 0;
    }
}

static void refresh_entries(AppState *state) {
    load_entries(state);
    rebuild_list(state);
}

static void maybe_refresh_entries(AppState *state) {
    time_t mtime = 0;
    off_t size = 0;

    if (read_index_signature(state, &mtime, &size) != 0) {
        return;
    }

    if (mtime != state->index_mtime || size != state->index_size) {
        refresh_entries(state);
    }
}

static void hide_window(AppState *state) {
    gtk_widget_hide(state->window);
}

static void show_window(AppState *state) {
    sync_latest_clipboard(state);
    maybe_refresh_entries(state);
    gtk_widget_show_all(state->window);
    gtk_window_present(GTK_WINDOW(state->window));
    gtk_widget_grab_focus(state->listbox);
}

static void toggle_window(AppState *state) {
    if (gtk_widget_get_visible(state->window)) {
        hide_window(state);
    } else {
        show_window(state);
    }
}

static void apply_css(void) {
    GtkCssProvider *provider = gtk_css_provider_new();
    const char *css =
        "window#rcopy-window {"
        "  background: #0f1115;"
        "  color: #f2f3f6;"
        "}"
        "#search-box {"
        "  background: #171a21;"
        "  color: #e8ebf2;"
        "  border-radius: 10px;"
        "  border: 1px solid #2b3140;"
        "  padding: 8px 10px;"
        "}"
        "list#clip-list {"
        "  background: transparent;"
        "}"
        "row.clip-row {"
        "  background: #151922;"
        "  border-radius: 12px;"
        "  border: 1px solid #2a3242;"
        "  margin: 6px 2px;"
        "  padding: 8px;"
        "}"
        "row.clip-row:selected {"
        "  background: #1f2940;"
        "  border-color: #5ea1ff;"
        "}"
        "label.clip-text {"
        "  color: #edf1fb;"
        "}"
        "label.clip-meta {"
        "  color: #98a6c1;"
        "  font-size: 11px;"
        "}"
        "separator.clip-sep {"
        "  color: #2b3140;"
        "  margin-top: 6px;"
        "}";

    gtk_css_provider_load_from_data(provider, css, -1, NULL);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    g_object_unref(provider);
}

static int contains_case_insensitive(const char *haystack, const char *needle) {
    size_t i, j;
    size_t nlen;

    if (needle == NULL || needle[0] == '\0') {
        return 1;
    }

    if (haystack == NULL) {
        return 0;
    }

    nlen = strlen(needle);
    for (i = 0; haystack[i] != '\0'; i++) {
        for (j = 0; j < nlen; j++) {
            char a = haystack[i + j];
            char b = needle[j];
            if (a == '\0') {
                return 0;
            }
            if (tolower((unsigned char)a) != tolower((unsigned char)b)) {
                break;
            }
        }
        if (j == nlen) {
            return 1;
        }
    }
    return 0;
}

static void activate_selected_and_hide(AppState *state) {
    GtkListBoxRow *row = gtk_list_box_get_selected_row(GTK_LIST_BOX(state->listbox));
    if (row != NULL) {
        const char *content = g_object_get_data(G_OBJECT(row), "content");
        const char *mime_type = g_object_get_data(G_OBJECT(row), "mime_type");
        gpointer length_ptr = g_object_get_data(G_OBJECT(row), "content_len");
        size_t len = (size_t)GPOINTER_TO_SIZE(length_ptr);
        if (content != NULL && mime_type != NULL) {
            util_write_wl_copy_type(mime_type, content, len);
            util_run_shell(state->cfg.paste_command);
        }
    }
    hide_window(state);
}

static gboolean on_window_key(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    AppState *state = user_data;

    if (event->keyval == GDK_KEY_Escape) {
        hide_window(state);
        return TRUE;
    }

    if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter) {
        activate_selected_and_hide(state);
        return TRUE;
    }

    if (event->keyval == GDK_KEY_Down || event->keyval == GDK_KEY_Up) {
        gtk_widget_grab_focus(state->listbox);
    }

    (void)widget;
    return FALSE;
}

static void rebuild_list(AppState *state) {
    const char *query = gtk_entry_get_text(GTK_ENTRY(state->search));
    GList *children = gtk_container_get_children(GTK_CONTAINER(state->listbox));
    GList *it;
    size_t i;

    for (it = children; it != NULL; it = it->next) {
        gtk_widget_destroy(GTK_WIDGET(it->data));
    }
    g_list_free(children);

    for (i = 0; i < state->list.count; i++) {
        ClipItem *item = &state->list.items[i];
        int is_image = (item->mime_type != NULL && strncmp(item->mime_type, "image/", 6) == 0);
        GtkWidget *row;
        GtkWidget *box;
        GtkWidget *label;
        int matches = 0;

        if (contains_case_insensitive(item->mime_type, query)) {
            matches = 1;
        }
        if (!is_image && contains_case_insensitive(item->content, query)) {
            matches = 1;
        }
        if (!matches) {
            continue;
        }

        row = gtk_list_box_row_new();
        gtk_style_context_add_class(gtk_widget_get_style_context(row), "clip-row");
        box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
        gtk_container_add(GTK_CONTAINER(row), box);

        if (is_image) {
            GInputStream *stream;
            GdkPixbuf *pixbuf;
            char meta[128];
            GtkWidget *image;

            stream = g_memory_input_stream_new_from_data(item->content, (gssize)item->content_len, NULL);
            pixbuf = gdk_pixbuf_new_from_stream_at_scale(stream, 380, 240, TRUE, NULL, NULL);
            if (pixbuf != NULL) {
                image = gtk_image_new_from_pixbuf(pixbuf);
                gtk_box_pack_start(GTK_BOX(box), image, FALSE, FALSE, 0);
                g_object_unref(pixbuf);
            }
            g_object_unref(stream);

            snprintf(meta, sizeof(meta), "%s  (%zu bytes)", item->mime_type, item->content_len);
            label = gtk_label_new(meta);
            gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
            gtk_style_context_add_class(gtk_widget_get_style_context(label), "clip-meta");
            gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
        } else {
            label = gtk_label_new(item->content != NULL ? item->content : "");
            gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
            gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
            gtk_label_set_line_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD_CHAR);
            gtk_style_context_add_class(gtk_widget_get_style_context(label), "clip-text");
            gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
        }

        label = gtk_label_new(item->mime_type != NULL ? item->mime_type : "unknown");
        gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
        gtk_style_context_add_class(gtk_widget_get_style_context(label), "clip-meta");
        gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);

        {
            GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
            gtk_style_context_add_class(gtk_widget_get_style_context(sep), "clip-sep");
            gtk_box_pack_start(GTK_BOX(box), sep, FALSE, FALSE, 0);
        }

        gtk_widget_show_all(row);

        g_object_set_data(G_OBJECT(row), "content", item->content);
        g_object_set_data(G_OBJECT(row), "content_len", GSIZE_TO_POINTER(item->content_len));
        g_object_set_data(G_OBJECT(row), "mime_type", item->mime_type);

        gtk_container_add(GTK_CONTAINER(state->listbox), row);
    }

    children = gtk_container_get_children(GTK_CONTAINER(state->listbox));
    if (children != NULL) {
        gtk_list_box_select_row(GTK_LIST_BOX(state->listbox), GTK_LIST_BOX_ROW(children->data));
        g_list_free(children);
    }
}

static void on_search_changed(GtkEditable *editable, gpointer user_data) {
    AppState *state = user_data;
    (void)editable;
    rebuild_list(state);
}

static void on_row_activated(GtkListBox *box, GtkListBoxRow *row, gpointer user_data) {
    AppState *state = user_data;
    (void)box;
    (void)row;
    activate_selected_and_hide(state);
}

static gboolean on_ipc_tick(gpointer user_data) {
    AppState *state = user_data;
    if (toggle_server_poll(state->server_fd)) {
        toggle_window(state);
    }
    return TRUE;
}

static gboolean on_window_delete(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
    AppState *state = user_data;
    (void)widget;
    (void)event;
    hide_window(state);
    return TRUE;
}

static void on_window_destroy(GtkWidget *widget, gpointer user_data) {
    AppState *state = user_data;
    (void)widget;
    toggle_server_stop(&state->cfg, state->server_fd);
    storage_free_list(&state->list);
    gtk_main_quit();
}

int ui_run(const RcopyConfig *cfg, int start_hidden) {
    AppState state;
    GtkWidget *vbox;
    GtkWidget *scroll;

    memset(&state, 0, sizeof(state));
    state.cfg = *cfg;
    state.server_fd = -1;
    state.index_mtime = 0;
    state.index_size = 0;

    if (storage_init(&state.cfg) != 0) {
        return 1;
    }

    load_entries(&state);

    gtk_init(NULL, NULL);
    apply_css();

    state.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_name(state.window, "rcopy-window");
    gtk_window_set_title(GTK_WINDOW(state.window), "rcopy");
    gtk_window_set_default_size(GTK_WINDOW(state.window), 760, 540);

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);
    gtk_container_add(GTK_CONTAINER(state.window), vbox);

    state.search = gtk_search_entry_new();
    gtk_widget_set_name(state.search, "search-box");
    gtk_box_pack_start(GTK_BOX(vbox), state.search, FALSE, FALSE, 0);

    scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);

    state.listbox = gtk_list_box_new();
    gtk_widget_set_name(state.listbox, "clip-list");
    gtk_list_box_set_activate_on_single_click(GTK_LIST_BOX(state.listbox), FALSE);
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(state.listbox), GTK_SELECTION_SINGLE);
    gtk_container_add(GTK_CONTAINER(scroll), state.listbox);

    rebuild_list(&state);

    g_signal_connect(state.search, "changed", G_CALLBACK(on_search_changed), &state);
    g_signal_connect(state.listbox, "row-activated", G_CALLBACK(on_row_activated), &state);
    g_signal_connect(state.window, "key-press-event", G_CALLBACK(on_window_key), &state);
    g_signal_connect(state.window, "delete-event", G_CALLBACK(on_window_delete), &state);
    g_signal_connect(state.window, "destroy", G_CALLBACK(on_window_destroy), &state);

    if (toggle_server_start(&state.cfg, &state.server_fd) == 0) {
        g_timeout_add(50, on_ipc_tick, &state);
    }

    if (start_hidden) {
        /* Realize and keep resident so first toggle is instant. */
        gtk_widget_show_all(state.window);
        gtk_widget_hide(state.window);
    } else {
        show_window(&state);
    }

    gtk_main();
    return 0;
}
