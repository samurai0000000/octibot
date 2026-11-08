/*
 * OctiExceptions.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef OCTI_EXCEPTIONS_HXX
#define OCTI_EXCEPTIONS_HXX

#include <stdexcept>
#include <string>

namespace octi {

class IllegalMoveSyntaxException : public std::runtime_error {
public:
    explicit IllegalMoveSyntaxException(const std::string &message)
        : std::runtime_error(message)
    {
    }

    explicit IllegalMoveSyntaxException(const char *message)
        : std::runtime_error(message ? message : "Illegal move syntax")
    {
    }
};

class IllegalMoveException : public std::runtime_error {
public:
    explicit IllegalMoveException(const std::string &message)
        : std::runtime_error(message)
    {
    }

    explicit IllegalMoveException(const char *message)
        : std::runtime_error(message ? message : "Illegal move")
    {
    }
};

} // namespace octi

#endif // OCTI_EXCEPTIONS_HXX

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
