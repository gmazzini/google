/*
 *
 * List entries in a directory (like a simple "ls").
 * Designed to work well even with very large directories:
 *  - default mode streams output (does NOT store all names in RAM)
 *  - optional --sort uses scandir() (stores entries -> higher memory use)
 *
 * Exit codes:
 *  0 = OK
 *  1 = invalid arguments
 *  2 = cannot open directory
 *  3 = read error while iterating directory
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <getopt.h>

static void usage(const char *prog) {
  fprintf(stderr,
    "Usage: %s [options] <directory>\n"
    "\n"
    "Options:\n"
    "  -a, --all        include hidden entries (starting with '.')\n"
    "  -f, --full-path  print full path (dir/name) instead of just name\n"
    "  -s, --sort       sort by name (uses memory; slower on huge dirs)\n"
    "  -h, --help       show this help\n"
    "\n"
    "Examples:\n"
    "  %s /var/log\n"
    "  %s -a --full-path /etc\n"
    "  %s --sort .\n",
    prog, prog, prog, prog
  );
}

static int is_dot_or_dotdot(const char *name) {
  return (strcmp(name, ".") == 0 || strcmp(name, "..") == 0);
}

static void print_entry(const char *dir, const char *name, int full_path) {
  if (!full_path) {
    puts(name);
    return;
  }

  /* Build dir/name safely */
  size_t dlen = strlen(dir);
  int need_slash = (dlen > 0 && dir[dlen - 1] != '/');

  /* +1 for '/' if needed, +1 for '\0' */
  size_t out_len = dlen + (need_slash ? 1 : 0) + strlen(name) + 1;

  char *out = (char*)malloc(out_len);
  if (!out) {
    /* If we can't allocate, fall back to printing just the name. */
    puts(name);
    return;
  }

  if (need_slash)
    snprintf(out, out_len, "%s/%s", dir, name);
  else
    snprintf(out, out_len, "%s%s", dir, name);

  puts(out);
  free(out);
}

static int list_streaming(const char *dir, int show_all, int full_path) {
  DIR *dp = opendir(dir);
  if (!dp) {
    fprintf(stderr, "Error: cannot open directory '%s': %s\n", dir, strerror(errno));
    return 2;
  }

  errno = 0;
  struct dirent *de;
  while ((de = readdir(dp)) != NULL) {
    const char *name = de->d_name;

    if (is_dot_or_dotdot(name)) continue;
    if (!show_all && name[0] == '.') continue;

    print_entry(dir, name, full_path);
    errno = 0;
  }

  if (errno != 0) {
    fprintf(stderr, "Error: readdir() failed on '%s': %s\n", dir, strerror(errno));
    closedir(dp);
    return 3;
  }

  closedir(dp);
  return 0;
}

static int filter_hidden(const struct dirent *de) {
  /* This filter keeps everything; hidden filtering is handled later
     because we also skip '.' and '..'. */
  (void)de;
  return 1;
}

static int list_sorted(const char *dir, int show_all, int full_path) {
  struct dirent **namelist = NULL;

  int n = scandir(dir, &namelist, filter_hidden, alphasort);
  if (n < 0) {
    fprintf(stderr, "Error: cannot scan directory '%s': %s\n", dir, strerror(errno));
    return 2;
  }

  for (int i = 0; i < n; i++) {
    const char *name = namelist[i]->d_name;

    if (is_dot_or_dotdot(name)) { free(namelist[i]); continue; }
    if (!show_all && name[0] == '.') { free(namelist[i]); continue; }

    print_entry(dir, name, full_path);
    free(namelist[i]);
  }

  free(namelist);
  return 0;
}

int main(int argc, char **argv) {
  int show_all = 0;
  int full_path = 0;
  int do_sort = 0;

  static const struct option long_opts[] = {
    { "all",       no_argument, 0, 'a' },
    { "full-path", no_argument, 0, 'f' },
    { "sort",      no_argument, 0, 's' },
    { "help",      no_argument, 0, 'h' },
    { 0, 0, 0, 0 }
  };

  int c;
  while ((c = getopt_long(argc, argv, "afsh", long_opts, NULL)) != -1) {
    switch (c) {
      case 'a': show_all = 1; break;
      case 'f': full_path = 1; break;
      case 's': do_sort = 1; break;
      case 'h': usage(argv[0]); return 0;
      default:
        usage(argv[0]);
        return 1;
    }
  }

  if (optind != argc - 1) {
    usage(argv[0]);
    return 1;
  }

  const char *dir = argv[optind];

  /* Basic sanity check: directory must exist and be a directory */
  struct stat st;
  if (stat(dir, &st) != 0) {
    fprintf(stderr, "Error: cannot stat '%s': %s\n", dir, strerror(errno));
    return 2;
  }
  if (!S_ISDIR(st.st_mode)) {
    fprintf(stderr, "Error: '%s' is not a directory\n", dir);
    return 2;
  }

  if (do_sort)
    return list_sorted(dir, show_all, full_path);
  else
    return list_streaming(dir, show_all, full_path);
}
