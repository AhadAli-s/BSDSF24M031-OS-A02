/*
 * Programming Assignment 02: lsv1.1.0
 * Adds the -l long listing format to the base version 1.0.0
 * Usage:
 *       $ ls
 *       $ ls -l
 *       $ ls -l /home /etc
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

extern int errno;

/* ANSI escape codes */
#define COLOR_RESET   "\033[0m"
#define COLOR_BLUE    "\033[0;34m"
#define COLOR_GREEN   "\033[0;32m"
#define COLOR_RED     "\033[0;31m"
#define COLOR_PINK    "\033[0;35m"
#define COLOR_REVERSE "\033[7m"

enum display_mode { MODE_DEFAULT, MODE_LONG, MODE_HORIZONTAL };

void do_ls(const char *dir, int mode);
void do_ls_long(const char *dir, char **names, size_t count);
int compare_names(const void *a, const void *b);
char **read_names(DIR *dp, size_t *count, size_t *maxlen);
size_t get_term_width(void);
void print_vertical(const char *dir, char **names, size_t count, size_t maxlen);
void print_horizontal(const char *dir, char **names, size_t count, size_t maxlen);
int has_suffix(const char *name, const char *suffix);
const char *get_color(const char *dir, const char *name);
void print_name(const char *dir, const char *name);
void mode_to_string(mode_t mode, char *str);
int num_width(unsigned long long n);

int main(int argc, char *argv[])
{
    int opt;
    int mode = MODE_DEFAULT;

    while ((opt = getopt(argc, argv, "lx")) != -1)
    {
        switch (opt)
        {
        case 'l':
            mode = MODE_LONG;
            break;
        case 'x':
            mode = MODE_HORIZONTAL;
            break;
        default:
            fprintf(stderr, "Usage: %s [-l | -x] [directory...]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if (optind == argc)
    {
        do_ls(".", mode);
    }
    else
    {
        for (int i = optind; i < argc; i++)
        {
            printf("Directory listing of %s : \n", argv[i]);
            do_ls(argv[i], mode);
            puts("");
        }
    }
    return 0;
}

void do_ls(const char *dir, int mode)
{
    DIR *dp = opendir(dir);
    if (dp == NULL)
    {
        fprintf(stderr, "Cannot open directory : %s\n", dir);
        return;
    }

    size_t count = 0, maxlen = 0;
    char **names = read_names(dp, &count, &maxlen);

    /* Sort once, every display mode then prints the sorted array */
    if (count > 1)
        qsort(names, count, sizeof(char *), compare_names);

    if (mode == MODE_LONG)
        do_ls_long(dir, names, count);
    else if (mode == MODE_HORIZONTAL)
        print_horizontal(dir, names, count, maxlen);
    else
        print_vertical(dir, names, count, maxlen);

    for (size_t i = 0; i < count; i++)
        free(names[i]);
    free(names);

    closedir(dp);
}

/* Number of characters needed to print n in decimal */
int num_width(unsigned long long n)
{
    char buf[32];
    return snprintf(buf, sizeof(buf), "%llu", n);
}

/* Convert st_mode into a 10 character string such as -rwxr-xr-x */
void mode_to_string(mode_t mode, char *str)
{
    if (S_ISREG(mode))
        str[0] = '-';
    else if (S_ISDIR(mode))
        str[0] = 'd';
    else if (S_ISLNK(mode))
        str[0] = 'l';
    else if (S_ISCHR(mode))
        str[0] = 'c';
    else if (S_ISBLK(mode))
        str[0] = 'b';
    else if (S_ISFIFO(mode))
        str[0] = 'p';
    else if (S_ISSOCK(mode))
        str[0] = 's';
    else
        str[0] = '?';

    str[1] = (mode & S_IRUSR) ? 'r' : '-';
    str[2] = (mode & S_IWUSR) ? 'w' : '-';
    str[3] = (mode & S_IXUSR) ? 'x' : '-';
    str[4] = (mode & S_IRGRP) ? 'r' : '-';
    str[5] = (mode & S_IWGRP) ? 'w' : '-';
    str[6] = (mode & S_IXGRP) ? 'x' : '-';
    str[7] = (mode & S_IROTH) ? 'r' : '-';
    str[8] = (mode & S_IWOTH) ? 'w' : '-';
    str[9] = (mode & S_IXOTH) ? 'x' : '-';
    str[10] = '\0';

    /* Special bits: setuid, setgid, sticky */
    if (mode & S_ISUID)
        str[3] = (mode & S_IXUSR) ? 's' : 'S';
    if (mode & S_ISGID)
        str[6] = (mode & S_IXGRP) ? 's' : 'S';
    if (mode & S_ISVTX)
        str[9] = (mode & S_IXOTH) ? 't' : 'T';
}

void do_ls_long(const char *dir, char **names, size_t count)
{
    struct stat st;
    struct passwd *pw;
    struct group *gr;
    char path[PATH_MAX];
    char perms[11];
    char ubuf[32], gbuf[32];
    const char *uname, *gname;
    int w_link = 1, w_user = 1, w_group = 1, w_size = 1;
    long long total = 0;

    /* Pass 1: find the widest value in each column and the block total */
    for (size_t i = 0; i < count; i++)
    {
        snprintf(path, sizeof(path), "%s/%s", dir, names[i]);
        if (lstat(path, &st) == -1)
        {
            perror(path);
            continue;
        }
        total += st.st_blocks;

        pw = getpwuid(st.st_uid);
        if (pw)
            uname = pw->pw_name;
        else
        {
            snprintf(ubuf, sizeof(ubuf), "%u", st.st_uid);
            uname = ubuf;
        }
        gr = getgrgid(st.st_gid);
        if (gr)
            gname = gr->gr_name;
        else
        {
            snprintf(gbuf, sizeof(gbuf), "%u", st.st_gid);
            gname = gbuf;
        }

        if (num_width(st.st_nlink) > w_link)
            w_link = num_width(st.st_nlink);
        if ((int)strlen(uname) > w_user)
            w_user = strlen(uname);
        if ((int)strlen(gname) > w_group)
            w_group = strlen(gname);
        if (num_width(st.st_size) > w_size)
            w_size = num_width(st.st_size);
    }

    /* st_blocks counts 512 byte units, ls prints 1024 byte units */
    printf("total %lld\n", total / 2);

    /* Pass 2: print every entry using the widths found above */
    for (size_t i = 0; i < count; i++)
    {
        snprintf(path, sizeof(path), "%s/%s", dir, names[i]);
        if (lstat(path, &st) == -1)
            continue;

        pw = getpwuid(st.st_uid);
        if (pw)
            uname = pw->pw_name;
        else
        {
            snprintf(ubuf, sizeof(ubuf), "%u", st.st_uid);
            uname = ubuf;
        }
        gr = getgrgid(st.st_gid);
        if (gr)
            gname = gr->gr_name;
        else
        {
            snprintf(gbuf, sizeof(gbuf), "%u", st.st_gid);
            gname = gbuf;
        }

        mode_to_string(st.st_mode, perms);

        /* ctime gives "Thu Nov 24 18:22:48 1986\n"
         * Recent files show "Nov 24 18:22", old or future files show "Nov 24  1986" */
        char timebuf[16];
        char *ct = ctime(&st.st_mtime);
        double age = difftime(time(NULL), st.st_mtime);
        if (age > 15778476 || age < 0)
            snprintf(timebuf, sizeof(timebuf), "%.7s %.4s", ct + 4, ct + 20);
        else
            snprintf(timebuf, sizeof(timebuf), "%.12s", ct + 4);

        printf("%s %*lu %-*s %-*s %*lld %s ",
               perms,
               w_link, (unsigned long)st.st_nlink,
               w_user, uname,
               w_group, gname,
               w_size, (long long)st.st_size,
               timebuf);
        print_name(dir, names[i]);

        if (S_ISLNK(st.st_mode))
        {
            char target[PATH_MAX];
            ssize_t len = readlink(path, target, sizeof(target) - 1);
            if (len != -1)
            {
                target[len] = '\0';
                printf(" -> %s", target);
            }
        }
        printf("\n");
    }
}

/* Returns 1 if name ends with suffix */
int has_suffix(const char *name, const char *suffix)
{
    size_t n = strlen(name), m = strlen(suffix);
    return n >= m && strcmp(name + n - m, suffix) == 0;
}

/* Pick the color for a file from its type, empty string means no color */
const char *get_color(const char *dir, const char *name)
{
    char path[PATH_MAX];
    struct stat st;

    if (!isatty(STDOUT_FILENO))
        return "";

    snprintf(path, sizeof(path), "%s/%s", dir, name);
    if (lstat(path, &st) == -1)
        return "";

    if (S_ISLNK(st.st_mode))
        return COLOR_PINK;
    if (S_ISDIR(st.st_mode))
        return COLOR_BLUE;
    if (S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode) ||
        S_ISFIFO(st.st_mode) || S_ISSOCK(st.st_mode))
        return COLOR_REVERSE;
    if (has_suffix(name, ".tar") || has_suffix(name, ".gz") || has_suffix(name, ".zip"))
        return COLOR_RED;
    if (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))
        return COLOR_GREEN;
    return "";
}

/* Print a filename in its color, the padding is added by the caller */
void print_name(const char *dir, const char *name)
{
    const char *color = get_color(dir, name);
    if (color[0] != '\0')
        printf("%s%s%s", color, name, COLOR_RESET);
    else
        printf("%s", name);
}

/* qsort comparison function.
 * qsort passes pointers to the array elements, and each element is a char *,
 * so a and b are really char ** hidden behind const void * */
int compare_names(const void *a, const void *b)
{
    const char *s1 = *(const char **)a;
    const char *s2 = *(const char **)b;
    return strcmp(s1, s2);
}

/* Read all visible filenames into a dynamic array and track the longest name */
char **read_names(DIR *dp, size_t *count, size_t *maxlen)
{
    struct dirent *entry;
    char **names = NULL;
    size_t capacity = 0;

    *count = 0;
    *maxlen = 0;

    errno = 0;
    while ((entry = readdir(dp)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;

        if (*count == capacity)
        {
            capacity = capacity ? capacity * 2 : 64;
            char **tmp = realloc(names, capacity * sizeof(char *));
            if (tmp == NULL)
            {
                perror("realloc");
                break;
            }
            names = tmp;
        }

        names[*count] = strdup(entry->d_name);
        if (names[*count] == NULL)
        {
            perror("strdup");
            break;
        }
        if (strlen(names[*count]) > *maxlen)
            *maxlen = strlen(names[*count]);
        (*count)++;
    }
    if (errno != 0)
        perror("readdir failed");

    return names;
}

/* Terminal width from ioctl, fall back to 80 columns */
size_t get_term_width(void)
{
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
        return ws.ws_col;
    return 80;
}

/* Default display: "down then across" columns that fit the terminal width */
void print_vertical(const char *dir, char **names, size_t count, size_t maxlen)
{
    if (count == 0)
        return;

    size_t col_width = maxlen + 2;
    size_t cols = get_term_width() / col_width;
    if (cols < 1)
        cols = 1;
    size_t rows = (count + cols - 1) / cols;

    /* Print row by row, item index = row + col * rows */
    for (size_t r = 0; r < rows; r++)
    {
        for (size_t c = 0; c < cols; c++)
        {
            size_t idx = r + c * rows;
            if (idx >= count)
                break;
            print_name(dir, names[idx]);
            if (idx + rows < count && c + 1 < cols)
                printf("%*s", (int)(col_width - strlen(names[idx])), "");
        }
        printf("\n");
    }
}

/* -x display: left to right, wrap to a new line when the next item will not fit */
void print_horizontal(const char *dir, char **names, size_t count, size_t maxlen)
{
    if (count == 0)
        return;

    size_t col_width = maxlen + 2;
    size_t term_width = get_term_width();
    size_t pos = 0; /* horizontal position where the current item starts */

    for (size_t i = 0; i < count; i++)
    {
        print_name(dir, names[i]);
        if (i + 1 == count)
            break;

        if (pos + 2 * col_width <= term_width)
        {
            printf("%*s", (int)(col_width - strlen(names[i])), "");
            pos += col_width;
        }
        else
        {
            printf("\n");
            pos = 0;
        }
    }
    printf("\n");
}
