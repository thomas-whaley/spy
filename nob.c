#define NOB_IMPLEMENTATION

#include <thirdparty/nob.h>

#define BUILD_FOLDER "build/"
#define SRC_FOLDER "src/"
#define THIRDPARTY_FOLDER "thirdparty/"

const char *tools[] = {
    "txt2bpe",
    "bpe2dot",
    "bpe2bpe",
    "bpe_inspect",
    "bpe_gen",
    "tkn_inspect",
};

void build_tool_async(Nob_Cmd *cmd, Nob_Procs *procs, const char *bin_path, const char *src_path)
{
    nob_cmd_append(cmd, "gcc", "-Wall", "-Wextra", "-ggdb", "-I" THIRDPARTY_FOLDER, "-march=native", "-Ofast", "-o", bin_path, src_path, SRC_FOLDER "spy.c");
    nob_da_append(procs, nob_cmd_run_async_and_reset(cmd));
}

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);

    Nob_Cmd cmd = {0};
    Nob_Procs procs = {0};

    const char *program_name = nob_shift(argv, argc);

    if (!nob_mkdir_if_not_exists(BUILD_FOLDER))
        return 1;

    if (argc <= 0)
    {
        for (size_t i = 0; i < NOB_ARRAY_LEN(tools); ++i)
        {
            const char *bin_path = nob_temp_sprintf(BUILD_FOLDER "%s", tools[i]);
            const char *src_path = nob_temp_sprintf(SRC_FOLDER "%s.c", tools[i]);
            nob_build_tool_async(&cmd, &procs, bin_path, src_path);
        }
        if (!nob_procs_wait_and_reset(&procs))
            return 1;
        return 0;
    }

    const char *tool_name = nob_shift(argv, argc);

    for (size_t i = 0; i < NOB_ARRAY_LEN(tools); ++i)
    {
        if (strcmp(tool_name, tools[i]) == 0)
        {
            const char *bin_path = nob_temp_sprintf(BUILD_FOLDER "%s", tools[i]);
            const char *src_path = nob_temp_sprintf(SRC_FOLDER "%s.c", tools[i]);
            nob_build_tool_async(&cmd, &procs, bin_path, src_path);
            if (!nob_procs_wait_and_reset(&procs))
                return 1;
            nob_cmd_append(&cmd, bin_path);
            nob_da_append_many(&cmd, argv, argc);
            if (!nob_cmd_run_sync_and_reset(&cmd))
                return 1;
            return 0;
        }
    }

    nob_log(NOB_ERROR, "Unknown tool `%s`", tool_name);
    nob_log(NOB_ERROR, "Available tools:");
    for (size_t i = 0; i < NOB_ARRAY_LEN(tools); ++i)
    {
        nob_log(NOB_ERROR, "  %s", tools[i]);
    }

    return 0;
}