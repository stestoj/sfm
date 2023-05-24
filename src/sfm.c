#include <ncurses.h>
#include <locale.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <magic.h>
#include <sys/mman.h>
#include "file.h"
#include "sort.h"

#define UP 'k'
#define DOWN 'j'
#define LEFT 'h'
#define RIGHT 'l'
#define FIRST_ENTRY 'g'
#define LAST_ENTRY 'G'

#define ORIGIN_X 0
#define ORIGIN_Y 0
#define SIZE 4096
#define END_OF_LINE -1
#define TERM_COLOR -1
#define HIGHLIGHT 1

enum mime_index {
    BINARY,
    TXT,
    C_TXT,
    C_PLUS_TXT,
    PYTHON,
    EXECUTABLE,
    SHELL_TXT,
    HTML_TXT,
    PNG,
    PDF,
    JPEG
};

const char *mime_types[] = {"application/octet-stream","text/plain","text/x-c", "text/x-c++", "text/x-script.python", "application/x-pie-executable", "text/x-shellscript", "text/html", "image/png", "application/pdf", "image/jpeg"};

int dir_count = 0;
int scroll_pos = 0;
int size = 0;

void init_ncurses(){
    initscr();
    if(!has_colors()){
        endwin();
        printf("This terminal does not support colors.\n");
        exit(1);
    }
    start_color();
    use_default_colors();
    cbreak();
    noecho();
}

void print_path(WINDOW *window_path, char *current_path){
    wmove(window_path, 0, 0);
    wclrtoeol(window_path);
    wprintw(window_path, "[");
    wprintw(window_path, current_path);
    wprintw(window_path, "]");
    wnoutrefresh(window_path);
}

void redraw_window(struct file *files, char *current_path, int cursor_y, WINDOW *window, WINDOW *window_path, WINDOW *window_border){
    window_path = newwin(0, COLS, 0, 0);
    window_border = newwin(LINES, COLS, 1, 0);
    window = newwin(LINES, COLS - 2, 2, 1);

    box(window_border, 0, 0);
    print_path(window_path, current_path);
    wnoutrefresh(window_border);


    if(scroll_pos >= dir_count){
        int temp_y = dir_count - 1;
        int temp_pos = scroll_pos;
        while(temp_y >= ORIGIN_Y){
            wmove(window, temp_y, ORIGIN_X);
            wprintw(window, files[temp_pos].filename);
            --temp_y;
            --temp_pos;
        }

    } else {
        int temp_y = ORIGIN_Y;
        for (int i = 0; i < dir_count; ++i){
            wmove(window, temp_y, ORIGIN_X);
            wprintw(window, files[i].filename);
            ++temp_y;
        }
    }


    wmove(window, cursor_y, ORIGIN_X);
    wnoutrefresh(window);

}

void print_files(WINDOW *win, char *cwd, struct file *files, int cursor_y){
    DIR *dirp = opendir(cwd);
    struct dirent *dir = readdir(dirp);


    // Remove old entries
    if(dir_count > 0){
        cursor_y = ORIGIN_Y;
        wmove(win, cursor_y, ORIGIN_X);
        werase(win);
    }

    dir_count = 0;
    size = 0;
    cursor_y = ORIGIN_Y;

    memset(files, 0, SIZE);

    while((dir = readdir(dirp))){
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0){
            continue;
        } else {
            strcpy(files[size].filename, dir->d_name);
            files[size].is_dir = (dir->d_type == DT_DIR) ? true : false;
            ++size;
        }
    }

    qsort(files, size, sizeof(struct file), &dir_cmp);

    // Populate window
    while(dir_count < size && dir_count < (LINES - 2)){
        wmove(win, cursor_y, ORIGIN_X);
        wprintw(win, files[dir_count].filename);
        ++dir_count;
        ++cursor_y;
    }


    scroll_pos = dir_count - 1;

    wmove(win, ORIGIN_Y, ORIGIN_X);
    wnoutrefresh(win);

}

void append_file(struct file *f, char *new_path, char *current_path){
        char file[256] = "/";
        strcpy(new_path, current_path);
        strcat(file, f->filename);
        strcat(new_path, file);

}

void handle_chld(int sig){
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

void open_file(struct file *f, char *editor, char *term, char *current_path){
    const char *mime;
    char file_path[PATH_MAX];
    magic_t magic;

    pid_t pid = fork();

    // child
    if(pid == 0){
        append_file(f, file_path, current_path);
        magic = magic_open(MAGIC_MIME_TYPE);
        magic_load(magic, NULL);
        mime = magic_file(magic, file_path);
        int mime_size = sizeof(mime_types) / sizeof(char *);
        for(int x = 0; x < mime_size; ++x){
            if((strcmp(mime, mime_types[x])) == 0){
                switch(x){
                    case BINARY: case TXT: case C_TXT: case C_PLUS_TXT:
                    case PYTHON: case EXECUTABLE: case SHELL_TXT: case HTML_TXT:
                        execlp(term, term, "-e", editor, file_path, NULL);
                        x = mime_size;
                        break;
                    default:
                        execlp("xdg-open", "xdg-open", file_path, NULL);
                        x = mime_size;
                        break;
                }
            }

        }
    }

}

int main(){
    int cursor_y = ORIGIN_Y;
    int character = 0, scroll_y = ORIGIN_Y;
    struct file files[SIZE] = {0};
    char current_path[PATH_MAX];
    char *editor = getenv("EDITOR");
    char *term = getenv("TERM");

    signal(SIGCHLD, handle_chld);
    setlocale(LC_ALL, "");
    getcwd(current_path, PATH_MAX);
    init_ncurses();
    init_pair(HIGHLIGHT, TERM_COLOR, COLOR_BLUE);

    WINDOW *window_path = newwin(0, COLS, 0, 0);
    WINDOW *window_border = newwin(LINES, COLS, 1, 0);
    WINDOW *window = newwin(LINES, COLS - 2, 2, 1);

    box(window_border, 0, 0);
    print_path(window_path, current_path);
    wnoutrefresh(window_border);
    print_files(window, current_path, files, cursor_y);
    wchgat(window, COLS - 2, A_NORMAL, HIGHLIGHT, NULL);

    /* wgetch internally refreshes the window */
    while((character = wgetch(window)) != 'q'){
        if (character == KEY_RESIZE){
            wresize(window_path, 0, COLS);
            wresize(window_border, LINES, COLS);
            wresize(window, LINES, COLS - 2);
            wnoutrefresh(window_path);
            wnoutrefresh(window_border);
            wnoutrefresh(window);
            redraw_window(files, current_path, cursor_y, window, window_path, window_border);
        }

        wchgat(window, COLS - 2, A_NORMAL, 0, NULL);

        if(character == UP){
            if(cursor_y == ORIGIN_Y && scroll_pos >= dir_count){
                --scroll_pos;
                --scroll_y;
                int temp = scroll_pos;
                cursor_y = (dir_count - 1);
                wmove(window, ORIGIN_Y, ORIGIN_X);
                werase(window);
                while(cursor_y >= ORIGIN_Y){
                    wmove(window, cursor_y, ORIGIN_X);
                    wprintw(window, files[temp].filename);
                    --cursor_y;
                    --temp;
                }
                cursor_y = ORIGIN_Y;
                wmove(window, cursor_y, ORIGIN_X);

            } else if(cursor_y == ORIGIN_Y){
                cursor_y = (dir_count - 1);
                scroll_y += (dir_count - 1);
                wmove(window, cursor_y, ORIGIN_X);
            } else {
                --cursor_y;
                --scroll_y;
                wmove(window, cursor_y, ORIGIN_X);
            }

        }

        if(character == DOWN){
            if(cursor_y == (dir_count - 1) && scroll_pos < size - 1 ){
                ++scroll_pos;
                ++scroll_y;
                int temp = scroll_pos;
                wmove(window, ORIGIN_Y, ORIGIN_X);
                werase(window);
                while(cursor_y >= ORIGIN_Y){
                    wmove(window, cursor_y, ORIGIN_X);
                    wprintw(window, files[temp].filename);
                    --cursor_y;
                    --temp;
                }
                cursor_y = dir_count - 1;
                wmove(window, cursor_y, ORIGIN_X);
            } else if (cursor_y == (dir_count - 1)){
                cursor_y = ORIGIN_Y;
                scroll_y -= (dir_count - 1);
                wmove(window, cursor_y, ORIGIN_X);
            } else {
                ++cursor_y;
                ++scroll_y;
                wmove(window, cursor_y, ORIGIN_X);
            }

        }

        if(character == LEFT){
            chdir("..");
            getcwd(current_path, PATH_MAX);
            cursor_y = ORIGIN_Y;
            scroll_y = ORIGIN_Y;
            print_path(window_path, current_path);
            print_files(window, current_path, files, cursor_y);
        }

        if(character == RIGHT){
            if(files[scroll_y].is_dir){
                chdir(files[scroll_y].filename);
                getcwd(current_path, PATH_MAX);
                cursor_y = ORIGIN_Y;
                scroll_y = ORIGIN_Y;
                print_path(window_path, current_path);
                print_files(window, current_path, files, cursor_y);
            } else {
                open_file(&files[scroll_y], editor, term, current_path);
            }
        }

        if(character == 'g'){
            character = wgetch(window);
            if(character == FIRST_ENTRY){
                if(dir_count < size){
                    cursor_y = ORIGIN_Y;
                    wmove(window, cursor_y, ORIGIN_X);
                    werase(window);
                    while(cursor_y <= dir_count - 1){
                        wmove(window, cursor_y, ORIGIN_X);
                        wprintw(window, files[cursor_y].filename);
                        ++cursor_y;
                    }
                }
                scroll_pos = dir_count - 1;
                scroll_y = ORIGIN_Y;
                cursor_y = ORIGIN_Y;
                wmove(window, cursor_y, ORIGIN_X);
            }
        }

        if(character == LAST_ENTRY){
            if(dir_count < size){
                scroll_y = size - 1;
                cursor_y = dir_count - 1;
                wmove(window, ORIGIN_Y, ORIGIN_X);
                werase(window);
                while(cursor_y >= ORIGIN_X){
                    wmove(window, cursor_y, ORIGIN_X);
                    wprintw(window, files[scroll_y].filename);
                    --scroll_y;
                    --cursor_y;
                }
                scroll_y = size - 1;

            } else {
                scroll_y = dir_count - 1;
            }
            scroll_pos = size - 1;
            cursor_y = dir_count - 1;
            wmove(window, cursor_y, ORIGIN_X);
        }

        wchgat(window, COLS - 2, A_NORMAL, HIGHLIGHT, NULL);

    };

    wattroff(window, COLOR_PAIR(HIGHLIGHT));
    delwin(window_path);
    delwin(window_border);
    delwin(window);
    endwin();

    return 0;
}
