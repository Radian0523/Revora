#include "../Engine/Core/Application.h"

int main(int /*argc*/, char* /*argv*/[]) {
    Revora::Application app;

    if (!app.Initialize()) {
        return 1;
    }

    app.Run();
    app.Shutdown();

    return 0;
}
