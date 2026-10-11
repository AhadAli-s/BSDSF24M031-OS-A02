# OS Programming Assignment 02 Report

## Feature-2: ls-v1.1.0 Complete Long Listing Format

### Q1. What is the crucial difference between stat() and lstat()? When is lstat() more appropriate in ls?

`stat()` follows symbolic links and returns the metadata of the file the link points to. `lstat()` does not follow the link and returns the metadata of the link itself, where the type is `S_IFLNK` and `st_size` is the length of the target path.

`lstat()` is the right call for `ls`. The long listing must show a symbolic link as a link, with an `l` in the first permission character and the `name -> target` form, and only `lstat()` gives that information. `lstat()` also works on broken links, where `stat()` fails because the target does not exist. In the recursive `-R` feature it prevents following a link that points back to a parent directory, which would loop forever.

### Q2. How can bitwise operators and macros extract file type and permissions from st_mode?

`st_mode` packs the file type into the upper bits and the permissions into the lower 12 bits: nine `rwx` bits for owner, group and others, plus the setuid, setgid and sticky bits.

To read the file type, mask with `S_IFMT` first and compare the result with a type constant:

    if ((st.st_mode & S_IFMT) == S_IFDIR) { /* directory */ }

The masking step is required because the type constants share bits (`S_IFBLK` includes the bits of `S_IFDIR` and `S_IFCHR`), so a plain `st_mode & S_IFDIR` can give a wrong answer. The macros `S_ISDIR()`, `S_ISREG()` and `S_ISLNK()` do this masking internally.

To read a permission, AND the mode with the permission constant. A non-zero result means the bit is set:

    if (st.st_mode & S_IRUSR) { /* owner can read */ }

The constants are `S_IRUSR`, `S_IWUSR`, `S_IXUSR` for the owner, `S_IRGRP`, `S_IWGRP`, `S_IXGRP` for the group, and `S_IROTH`, `S_IWOTH`, `S_IXOTH` for others. `S_ISUID`, `S_ISGID` and `S_ISVTX` give the special bits, which change the execute character to `s`, `S`, `t` or `T`.

## Feature-3: ls-v1.2.0 Column Display (Down Then Across)

### Q1. What is the logic for "down then across" printing? Why is a single loop insufficient?

All filenames are read into an array first and the longest name is measured. The column width is the longest name plus 2 spaces of gap. Then:

    cols = terminal_width / column_width
    rows = ceil(total_files / cols)

The array is treated as filled column by column. The item printed at row `r` and column `c` is `names[r + c * rows]`. The program loops over rows in the outer loop and over columns in the inner loop, so row 0 prints `names[0]`, `names[rows]`, `names[2*rows]` and so on.

A single loop over the array prints the names in array order, and a terminal prints line by line, so consecutive names would sit side by side. That produces an "across" layout. The down-then-across layout needs the index to jump forward by `rows` between neighbours on the same line, which requires the row and column loops and the total count known in advance.

### Q2. What is the purpose of ioctl here? What are the limits of a fixed 80 column fallback?

`ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws)` asks the terminal driver for the current window size. It fills a `struct winsize`, and `ws.ws_col` holds the number of columns. The program uses it to calculate how many columns of filenames fit on one line.

With only a fixed width of 80, the output ignores the real terminal. On a wide terminal it leaves space unused and prints fewer columns than would fit. On a terminal narrower than 80 columns the lines wrap and the alignment breaks. It also does not react when the user resizes the window. The fixed width is only a fallback for the case where `ioctl` fails, such as when the output goes to a pipe or a file.
