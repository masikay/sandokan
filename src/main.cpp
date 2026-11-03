#include <cstdlib>
#include <cstring>

#include "core/Application.h"

// --editor                start directly in the level editor
// --select N              (with --editor) select level object N and focus the camera on it
// --screenshot FILE       save a screenshot after a few frames and exit (for checking visuals)
int main(int argc, char** argv)
{
    LaunchOptions options;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--editor") == 0) options.startInEditor = true;
        else if (std::strcmp(argv[i], "--select") == 0 && i + 1 < argc) options.selectObject = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) options.screenshotPath = argv[++i];
    }

    Application app(1280, 720, "Sandokan", options);
    app.Run();
    return 0;
}
