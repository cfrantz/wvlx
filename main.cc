#include <cstdio>
#include <string>
#include <vector>

#include "app.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/log/log.h"
#include "util/config.h"

const char kUsage[] =
R"ZZZ(<optional flags>

Description:
  An empty project.

Flags:
  --hidpi <n>         Set the scaling factor on hidpi displays (try 2.0).
)ZZZ";

int main(int argc, char *argv[]) {
    absl::SetProgramUsageMessage(kUsage);
    std::vector<char*> args = absl::ParseCommandLine(argc, argv);
    if (args.size() > 2) {
        LOG(QFATAL) << "Too many positional args";
    }

    project::App app("Empty Project");
    app.Init();
    if (args.size() > 1) {
        app.Load(args[1]);
    }
    app.Run();
    return 0;
}
