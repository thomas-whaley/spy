#define NOB_IMPLEMENTATION

#include "thirdparty/nob.h"

#define BUILD_FOLDER "build/"
#define SRC_FOLDER "src/"
#define THIRDPARTY_FOLDER "thirdparty/"

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);

    Nob_Cmd cmd = {0};
    Nob_Procs procs = {0};

    const char *program_name = nob_shift(argv, argc);

    if (!nob_mkdir_if_not_exists(BUILD_FOLDER))
        return 1;

    nob_cmd_append(&cmd, "gcc", "-Wall", "-Wextra", "-I" THIRDPARTY_FOLDER, "-O0", "-o", BUILD_FOLDER "spy", SRC_FOLDER "spy.c");
    nob_da_append(&procs, nob_cmd_run_async_and_reset(&cmd));
    return 0;
}