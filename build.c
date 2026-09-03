/* GURD Build Tool - <https://github.com/Thepigcat76/gurd> */

#include "gurd.h"

#define COMPILER "clang"
#define STANDARD "gnu23"
#define DEBUG true
#define OUT_NAME "build/gurd-prototype"

#define LIB_LILC "lilc"

static Cmd cmd = {0};

static void visit_entry(struct file_entry entry) {
  if (entry.file_ext == NULL || strcmp(entry.file_ext, "c") != 0)
    return;

  cmd_appendf(&cmd, "%s", entry.path);
}

int main(int argc, char **argv) {
  // Remove old build files
  remove_dir_recursive("build", false);

  // The compiler to use
  cmd_appendf(&cmd, COMPILER);
  // Flags
  cmd_appendf(&cmd, "-std=%s", STANDARD);
  if (DEBUG) {
    cmd_appendf(&cmd, "-g");
  }
  // Output location
  cmd_appendf(&cmd, "-o");
  cmd_appendf(&cmd, OUT_NAME);

  // Adding src files
  walk_dir("src", visit_entry);

  // Libraries
  cmd_appendf(&cmd, "-l" LIB_LILC);

  // Make sure out path exists
  ensure_parent_dirs(OUT_NAME, 0755);

  // Run the command
  cmd_execute(&cmd);

  bool run = arg_eq(argc, argv, 1, "r");

  bool debug = run && (args_contains(argc, argv, "--debug") != -1 ||
                       args_contains(argc, argv, "-d") != -1);

  int args_arg_idx = args_contains(argc, argv, "--args");

  if (run) {
    char args[1024] = {'\0'};

    if (args_arg_idx != -1 && args_arg_idx + 1 < argc) {
      for (int i = args_arg_idx + 1; i < argc; i++) {
        strcat(args, argv[i]);
        if (i + 1 < argc) {
          strcat(args, " ");
        }
      }
    }

    char exec_cmd[1024];
    sprintf(exec_cmd, "./%s", OUT_NAME);

    if (debug) {
      sprintf(exec_cmd, "gdb --args ./%s", OUT_NAME);
    } else if (args_contains(argc, argv, "-vg") != -1) {
      sprintf(exec_cmd, "valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --errors-for-leak-kinds=all ./%s", OUT_NAME);
    }

    return WEXITSTATUS(systemf("%s %s", exec_cmd, args));
  }

}
