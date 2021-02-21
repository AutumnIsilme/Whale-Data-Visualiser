#include <Windows.h>

extern int main(int argc, char** argv);

int CALLBACK
WinMain(HINSTANCE Instance,
        HINSTANCE PrevInstance,
        LPSTR CmdLine,
        int CmdShow) {
    main(0, nullptr);
}
