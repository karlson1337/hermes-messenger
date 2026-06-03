#include "../hermes_protocols.h"
#include "globals.h"

#include <ncurses.h>

#define FRIEND_W 24

static WINDOW *friend_win, *header_win, *chat_win, *input_win;
static pthread_mutex_t ui_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool ui_ready = false;

void redraw_chat();
int chatdb_get_count(const char *user_a, const char *user_b);

static void create_windows(int h, int w) {
    int cw = w - FRIEND_W;
    int ih = 3;
    int ch = h - ih - 2;
    if (ch < 1) ch = 1;

    friend_win = newwin(h,   FRIEND_W, 0,      0);
    header_win = newwin(2,   cw,       0,      FRIEND_W);
    chat_win   = newwin(ch,  cw,       2,      FRIEND_W);
    input_win  = newwin(ih,  cw,       h - ih, FRIEND_W);
}

void ui_init()
{
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    start_color();
    init_pair(1, COLOR_GREEN,  COLOR_BLACK);
    init_pair(2, COLOR_YELLOW, COLOR_BLACK);
    init_pair(3, COLOR_CYAN,   COLOR_BLACK);

    create_windows(LINES, COLS);

    if (!friend_win || !header_win || !chat_win || !input_win) {
        endwin();
        fprintf(stderr, "Terminal too small\n");
        exit(1);
    }

    scrollok(chat_win, TRUE);
    idlok(chat_win, TRUE);
    keypad(input_win, TRUE);

    box(friend_win, 0, 0);
    box(input_win,  0, 0);
    mvwprintw(friend_win, 0, 1, " Friends ");
    mvwprintw(input_win, 0, 1, " Message (Type /help for a list of commands) ");

    char logged_in_str[64];
    snprintf(logged_in_str, sizeof(logged_in_str), " Logged in as: %s ", self);
    mvwprintw(input_win, 0, getmaxx(input_win) - strlen(logged_in_str) - 2, "%s", logged_in_str);

    wrefresh(friend_win);
    wrefresh(header_win);
    wrefresh(chat_win);
    wrefresh(input_win);

    ui_ready = true;
}

static void print_word_wrapped(WINDOW *win, int start_y, int start_x, int width, const char *str) 
{
    char *text = malloc(strlen(str) + 1);
    if (text) {
        strcpy(text, str);
    }
    if (!text) return;

    int y = start_y;
    int x = start_x;
    int max_x = start_x + width;

    char *word = strtok(text, " \n");
    while (word != NULL) {
        int len = strlen(word);

        if (x + len > max_x && x != start_x) {
            y++;
            x = start_x;
        }

        mvwaddstr(win, y, x, word);
        x += len + 1;

        word = strtok(NULL, " \n");
    }
    
    free(text);
}

void ui_set_header(const char *name)
{
    pthread_mutex_lock(&ui_mutex);
    wclear(header_win);
    wattron(header_win, COLOR_PAIR(3) | A_BOLD);
    mvwprintw(header_win, 0, 1, " %s ", name);
    wattroff(header_win, COLOR_PAIR(3) | A_BOLD);
    mvwhline(header_win, 1, 0, ACS_HLINE, COLS - FRIEND_W);
    wrefresh(header_win);
    pthread_mutex_unlock(&ui_mutex);
}

void ui_print_message(int64_t timestamp, const char *sender, const unsigned char *msg)
{
    bool is_self;
    if(strcmp(self, sender) == 0) {is_self = true;}
    else is_self = false;
    int64_t now = time(NULL);
    struct tm now_tm, msg_tm;
    time_t ts = (time_t)timestamp;

    struct tm *tmp = localtime(&now);
    now_tm = *tmp;
    tmp = localtime(&ts);
    msg_tm = *tmp;

    //print year if not current year
    char buf[32];
    if (msg_tm.tm_year != now_tm.tm_year)
        strftime(buf, sizeof(buf), "%b %d %Y %H:%M", &msg_tm);
    else
        strftime(buf, sizeof(buf), " %b %d %H:%M", &msg_tm);

    pthread_mutex_lock(&ui_mutex);
    if (!is_self) wattron(chat_win, COLOR_PAIR(1));
    wprintw(chat_win, "[%s] (%s): %s\n\n", buf, is_self ? "you" : sender, (const char *)msg);
    if (!is_self) wattroff(chat_win, COLOR_PAIR(1));
    wrefresh(chat_win);

    touchwin(input_win);
    wrefresh(input_win);

    pthread_mutex_unlock(&ui_mutex);
}

void ui_draw_friends()
{
    if (!ui_ready) return;

    pthread_mutex_lock(&ui_mutex);
    wclear(friend_win);
    box(friend_win, 0, 0);
    mvwprintw(friend_win, 0, 1, " Friends ");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(friend_list,
        "SELECT username, unread FROM friends ORDER BY username;", -1, &stmt, NULL);
    int row = 1;
    while (sqlite3_step(stmt) == SQLITE_ROW && row < LINES - 1) {
        const char *name   = (const char *)sqlite3_column_text(stmt, 0);
        int         unread = sqlite3_column_int(stmt, 1);
        wmove(friend_win, row++, 1);
        if (unread > 0) {
            wattron(friend_win, COLOR_PAIR(2));
            wprintw(friend_win, "%-14s (%d)", name, unread);
            wattroff(friend_win, COLOR_PAIR(2));
        } else {
            wprintw(friend_win, "%s", name);
        }
    }
    sqlite3_finalize(stmt);
    wrefresh(friend_win);
    
    touchwin(input_win);
    wrefresh(input_win);
    
    pthread_mutex_unlock(&ui_mutex);
}

void handle_resize()
{
    pthread_mutex_lock(&ui_mutex); // protect ncurses state during changes
    endwin();
    refresh();
    clear();

    int h = LINES, w = COLS;
    int cw = w - FRIEND_W;
    int ih = 3;
    int ch = h - ih - 2;
    if (ch < 1) ch = 1;

    wresize(friend_win, h,  FRIEND_W);
    wresize(header_win, 2,  cw);
    wresize(chat_win,   ch, cw);
    wresize(input_win,  ih, cw);

    mvwin(friend_win, 0,      0);
    mvwin(header_win, 0,      FRIEND_W);
    mvwin(chat_win,   2,      FRIEND_W);
    mvwin(input_win,  h - ih, FRIEND_W);
    pthread_mutex_unlock(&ui_mutex);

    ui_draw_friends();
    ui_set_header(recipient[0] ? recipient : "(no chat open)");
    redraw_chat();
    
    pthread_mutex_lock(&ui_mutex);
    wclear(input_win);
    box(input_win, 0, 0);
    mvwprintw(input_win, 0, 1, " Message (Type /help for a list of commands) ");

    char logged_in_str[64];
    snprintf(logged_in_str, sizeof(logged_in_str), " Logged in as: %s ", self);
    mvwprintw(input_win, 0, getmaxx(input_win) - strlen(logged_in_str) - 2, "%s", logged_in_str); 

    wrefresh(input_win);
    pthread_mutex_unlock(&ui_mutex);
}

void ui_print_system_message(const char *msg)
{
    pthread_mutex_lock(&ui_mutex);
    wattron(chat_win, COLOR_PAIR(2) | A_BOLD);
    wprintw(chat_win, "=== SYSTEM: %s ===\n\n", msg);
    wattroff(chat_win, COLOR_PAIR(2) | A_BOLD);
    wrefresh(chat_win);

    touchwin(input_win);
    wrefresh(input_win);

    pthread_mutex_unlock(&ui_mutex);
}

void ui_show_help()
{
    int h = 14, w = 50;
    bool keep_open = true;

    while (keep_open) {
        int y = (LINES - h) / 2;
        int x = (COLS  - w) / 2;
        if (y < 0) y = 0;
        if (x < 0) x = 0;

        WINDOW *popup = newwin(h, w, y, x);
        if (!popup) return;
        
        keypad(popup, TRUE); 
        box(popup, 0, 0);

        mvwaddstr(popup, 0, 1, " Help ");
        
        int current_row = 1; 
        for (int i = 0; i < NUM_COMMANDS; i++) {
            if (current_row >= h - 3) break; 

            int max_wrap_width = w - 4; 

            int approx_lines = (strlen(commands[i]) / max_wrap_width) + 1;

            print_word_wrapped(popup, current_row, 2, max_wrap_width, commands[i]);
            current_row += approx_lines;
        }

        mvwprintw(popup, h - 2, 2, "Press any key to close...");
        wrefresh(popup);

        int ch = wgetch(popup);
        
        if (ch == KEY_RESIZE) {
            delwin(popup);     
            handle_resize();   
        } else {
            delwin(popup);
            keep_open = false; 
        }
    }

    pthread_mutex_lock(&ui_mutex);
    clear();   
    refresh(); 
    
    touchwin(friend_win);
    touchwin(header_win);
    touchwin(chat_win);
    touchwin(input_win);
    
    wrefresh(friend_win);
    wrefresh(header_win);
    wrefresh(chat_win);
    pthread_mutex_unlock(&ui_mutex);

    ui_draw_friends();
    ui_set_header(recipient[0] ? recipient : "(no chat open)");

    pthread_mutex_lock(&ui_mutex);
    wclear(input_win);
    box(input_win, 0, 0);
    mvwprintw(input_win, 0, 1, " Message (Type /help for a list of commands) ");
    wrefresh(input_win);
    pthread_mutex_unlock(&ui_mutex);
}

static void render_live_input(WINDOW *win, const char *text) 
{
    int text_w = getmaxx(win) - 4;
    int max_y = getmaxy(win) - 2;
    
    int len = strlen(text);
    
    int cursor_line, cursor_col;
    if (len > 0 && len % text_w == 0) {
        cursor_line = (len / text_w) - 1;
        cursor_col = text_w;
    } else {
        cursor_line = len / text_w;
        cursor_col = len % text_w;
    }
    
    int total_lines = cursor_line + 1;
    int start_line = 0;
    
    if (total_lines > max_y) {
        start_line = total_lines - max_y;
    }

    for (int i = 0; i < len; i++) {
        int char_line = i / text_w;
        if (char_line >= start_line && char_line < start_line + max_y) {
            int draw_y = 1 + (char_line - start_line);
            int draw_x = 2 + (i % text_w);
            mvwaddch(win, draw_y, draw_x, text[i]);
        }
    }
    
    int final_y = 1 + (cursor_line - start_line);
    int final_x = 2 + cursor_col;
    
    wmove(win, final_y, final_x);
}

char *ui_get_input()
{
    static char buf[MSG_BODY_SIZE];
    int pos = 0;
    memset(buf, 0, MSG_BODY_SIZE);

    pthread_mutex_lock(&ui_mutex);
    wclear(input_win);
    box(input_win, 0, 0);
    mvwprintw(input_win, 0, 1, " Message (Type /help for a list of commands) ");

    char logged_in_str[64];
    snprintf(logged_in_str, sizeof(logged_in_str), " Logged in as: %s ", self);
    mvwprintw(input_win, 0, getmaxx(input_win) - strlen(logged_in_str) - 2, "%s", logged_in_str);

    scrollok(input_win, TRUE);
    wmove(input_win, 1, 2);
    wrefresh(input_win);
    pthread_mutex_unlock(&ui_mutex);

    int ch;
    while ((ch = wgetch(input_win)) != '\n') {
        
        if (ch == KEY_RESIZE) { 
            handle_resize(); 
            pthread_mutex_lock(&ui_mutex);
            wclear(input_win);
            box(input_win, 0, 0);
            render_live_input(input_win, buf);
            wrefresh(input_win);
            pthread_mutex_unlock(&ui_mutex);
            continue; 
        }
        
        //CHAT HISTORY SCROLLING
        if (ch == KEY_UP) { 
            if (recipient[0] != '\0') {
                int total_msgs = chatdb_get_count(self, recipient);
                if (chat_scroll_offset < total_msgs - 1) {
                    chat_scroll_offset++;
                    redraw_chat(); 
                    pthread_mutex_lock(&ui_mutex);
                    wclear(input_win);
                    box(input_win, 0, 0);
                    render_live_input(input_win, buf);
                    wrefresh(input_win);
                    pthread_mutex_unlock(&ui_mutex);
                }
            }
            continue; 
        }
        if (ch == KEY_DOWN) { 
            if (chat_scroll_offset > 0) {
                chat_scroll_offset--;
                redraw_chat(); 
                pthread_mutex_lock(&ui_mutex);
                wclear(input_win);
                box(input_win, 0, 0);
                render_live_input(input_win, buf);
                wrefresh(input_win);
                pthread_mutex_unlock(&ui_mutex);
            }
            continue; 
        }

        //BACKSPACE
        if (ch == KEY_BACKSPACE || ch == 127) {
            if (pos > 0) {
                buf[--pos] = '\0';
                pthread_mutex_lock(&ui_mutex);
                wclear(input_win);
                box(input_win, 0, 0);
                mvwprintw(input_win, 0, 1, " Message (Type /help for a list of commands) ");
                
                char logged_in_str[64];
                snprintf(logged_in_str, sizeof(logged_in_str), " Logged in as: %s ", self);
                mvwprintw(input_win, 0, getmaxx(input_win) - strlen(logged_in_str) - 2, "%s", logged_in_str);

                render_live_input(input_win, buf);
                wrefresh(input_win);
                pthread_mutex_unlock(&ui_mutex);
            }
            continue;
        }

        //TEXT INPUT
        if (ch >= 32 && ch < 127 && pos < MSG_BODY_SIZE - 1) {
            buf[pos++] = (char)ch;
            buf[pos] = '\0'; 

            pthread_mutex_lock(&ui_mutex);
            wclear(input_win);
            box(input_win, 0, 0);
            mvwprintw(input_win, 0, 1, " Message (Type /help for a list of commands) ");
            
            char logged_in_str[64];
            snprintf(logged_in_str, sizeof(logged_in_str), " Logged in as: %s ", self);
            mvwprintw(input_win, 0, getmaxx(input_win) - strlen(logged_in_str) - 2, "%s", logged_in_str);

            render_live_input(input_win, buf);
            wrefresh(input_win);
            pthread_mutex_unlock(&ui_mutex);
        }
    }
    buf[pos] = '\0';
    return buf;
}

void ui_clear_chat()
{
    pthread_mutex_lock(&ui_mutex);
    wclear(chat_win);
    wrefresh(chat_win);
    
    touchwin(input_win);
    wrefresh(input_win);
    pthread_mutex_unlock(&ui_mutex);
}

void ui_cleanup()
{
    ui_ready = false;
    delwin(friend_win);
    delwin(header_win);
    delwin(chat_win);
    delwin(input_win);
    endwin();
}