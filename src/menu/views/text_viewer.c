/**
 * @file text_viewer.c
 * @brief Text Viewer component implementation
 * @ingroup ui_components
 */

#include <stdio.h>
#include <sys/stat.h>

#include "../ui_components/constants.h"
#include "../fonts.h"
#include "../sound.h"
#include "utils/utils.h"
#include "views.h"

#define MAX_FILE_SIZE KiB(128)

/** @brief Text file structure */
typedef struct {
    FILE *f; /**< File pointer */
    char *contents; /**< File contents */
    size_t length; /**< File length */
    int lines; /**< Number of lines */
    int current_line; /**< Current line */
    int offset; /**< Offset in the file */
    bool vertical_scroll_possible; /**< Flag indicating if vertical scroll is possible */
} text_file_t;

static text_file_t *text;

/**
 * @brief Perform vertical scroll in the text file.
 * 
 * @param lines Number of lines to scroll.
 */
static void perform_vertical_scroll (int lines) {
    if (!text->vertical_scroll_possible) {
        return;
    }

    int direction = (lines < 0) ? -1 : 1;
    int next_offset = text->offset;

    for (int i = 0; i < abs(lines); i++) {
        while (true) {
            next_offset += direction;
            if (next_offset <= 0) {
                text->current_line = 0;
                text->offset = 0;
                return;
            }
            if (next_offset > text->length) {
                return;
            }
            if (text->contents[next_offset - 1] == '\n') {
                break;
            }
        }
        text->current_line += direction;
        text->offset = next_offset;
    }
}

/**
 * @brief Process user actions for the text viewer.
 * 
 * @param menu Pointer to the menu structure.
 */
static void process (menu_t *menu) {
    if (menu->actions.back) {
        sound_play_effect(SFX_EXIT);
        menu->next_mode = MENU_MODE_BROWSER;
    } else if (text) {
        if (menu->actions.go_up) {
            perform_vertical_scroll(menu->actions.go_fast ? -10 : -1);
        } else if (menu->actions.go_down) {
            perform_vertical_scroll(menu->actions.go_fast ? 10 : 1);
        }
    }
}

/**
 * @brief Draw the text viewer.
 * 
 * @param menu Pointer to the menu structure.
 * @param d Pointer to the display surface.
 */
static void draw (menu_t *menu, surface_t *d) {
    rdpq_attach(d, NULL);

    ui_components_background_draw();

    ui_components_layout_draw();

    ui_components_main_text_draw(
        ALIGN_LEFT, VALIGN_TOP,
        "%s\n",
        text->contents + text->offset
    );

    ui_components_list_scrollbar_draw(text->current_line, text->lines, LIST_ENTRIES);

    ui_components_actions_bar_text_draw(
        ALIGN_LEFT, VALIGN_TOP,
        "^%02XUp / Down: Scroll^00\n"
        "B: Back",
        text->vertical_scroll_possible ? STL_DEFAULT : STL_GRAY
    );

    rdpq_detach_show();
}

/**
 * @brief Deinitialize the text viewer.
 */
static void deinit (void) {
    if (text) {
        if (text->f) {
            fclose(text->f);
        }
        if (text->contents) {
            free(text->contents);
        }
        free(text);
        text = NULL;
    }
}

/**
 * @brief Initialize the text viewer.
 * 
 * @param menu Pointer to the menu structure.
 */
void view_text_viewer_init (menu_t *menu) {
    if ((text = calloc(1, sizeof(text_file_t))) == NULL) {
        return menu_show_error(menu, "Couldn't allocate memory for the text file");
    }

    path_t *path = path_clone_push(menu->browser.directory, menu->browser.entry->name);
    text->f = fopen(path_get(path), "r");
    path_free(path);

    if (text->f == NULL) {
        deinit();
        return menu_show_error(menu, "Couldn't open text file");
    }

    struct stat st;
    if (fstat(fileno(text->f), &st)) {
        deinit();
        return menu_show_error(menu, "Couldn't get text file size");
    }
    text->length = st.st_size;

    if (text->length <= 0) {
        deinit();
        return menu_show_error(menu, "Text file is empty");
    }

    if (text->length > MAX_FILE_SIZE) {
        deinit();
        return menu_show_error(menu, "Text file is too big to be displayed");
    }

    if ((text->contents = malloc((text->length + 1) * sizeof(char))) == NULL) {
        deinit();
        return menu_show_error(menu, "Couldn't allocate memory for the text file contents");
    }

    if (fread(text->contents, text->length, 1, text->f) != 1) {
        deinit();
        return menu_show_error(menu, "Couldn't read text file contents");
    }
    text->contents[text->length] = '\0';

    if (fclose(text->f)) {
        deinit();
        return menu_show_error(menu, "Couldn't close text file");
    }
    text->f = NULL;

    text->lines = 1;
    for (size_t i = 0; i < text->length; i++) {
        if (text->contents[i] == '\n') {
            text->lines += 1;
        }
    }

    text->vertical_scroll_possible = (text->lines > LIST_ENTRIES);
}

/**
 * @brief Display the text viewer.
 * 
 * @param menu Pointer to the menu structure.
 * @param display Pointer to the display surface.
 */
void view_text_viewer_display (menu_t *menu, surface_t *display) {
    process(menu);

    draw(menu, display);

    if (menu->next_mode != MENU_MODE_TEXT_VIEWER) {
        deinit();
    }
}
