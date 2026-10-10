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
#include <sys/stat.h>

extern int errno;

void do_ls(const char *dir, int long_flag);
void do_ls_long(const char *dir, DIR *dp);
void mode_to_string(mode_t mode, char *str);
int num_width(unsigned long long n);

int main(int argc, char *argv[])
{
    int opt;
    int long_flag = 0;

    while ((opt = getopt(argc, argv, "l")) != -1)
    {
        switch (opt)
        {
        case 'l':
            long_flag = 1;
            break;
        default:
            fprintf(stderr, "Usage: %s [-l] [directory...]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if (optind == argc)
    {
        do_ls(".", long_flag);
    }
    else
    {
        for (int i = optind; i < argc; i++)
        {
            printf("Directory listing of %s : \n", argv[i]);
            do_ls(argv[i], long_flag);
            puts("");
        }
    }
    return 0;
}

void do_ls(const char *dir, int long_flag)
{
    struct dirent *entry;
    DIR *dp = opendir(dir);
    if (dp == NULL)
    {
        fprintf(stderr, "Cannot open directory : %s\n", dir);
        return;
    }

    if (long_flag)
    {
        do_ls_long(dir, dp);
    }
    else
    {
        errno = 0;
        while ((entry = readdir(dp)) != NULL)
        {
            if (entry->d_name[0] == '.')
                continue;
            printf("%s\n", entry->d_name);
        }
        if (errno != 0)
        {
            perror("readdir failed");
        }
    }

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

void do_ls_long(const char *dir, DIR *dp)
{
    struct dirent *entry;
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
    errno = 0;
    while ((entry = readdir(dp)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;
        snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);
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
    rewinddir(dp);
    while ((entry = readdir(dp)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;
        snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);
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

        printf("%s %*lu %-*s %-*s %*lld %s %s",
               perms,
               w_link, (unsigned long)st.st_nlink,
               w_user, uname,
               w_group, gname,
               w_size, (long long)st.st_size,
               timebuf,
               entry->d_name);

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
