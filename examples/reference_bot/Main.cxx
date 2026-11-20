/*
 * Main.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#if defined(_WIN32)
#include <windows.h>
#endif
#include "OctiBotRunner.hxx"

int main(int argc, char **argv) {
#if defined(_WIN32)
    ::SetConsoleOutputCP(CP_UTF8);
    ::SetConsoleCP(CP_UTF8);
#endif
    return octi::runFleetCli(argc, argv);
}

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
