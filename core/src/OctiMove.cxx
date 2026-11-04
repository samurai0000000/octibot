/*
 * OctiMove.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "OctiMove.hxx"
#include <cctype>
#include <ostream>

namespace octi {

static std::string trimWhitespace(const std::string &str)
{
    std::size_t start = 0;
    while (start < str.size() && std::isspace(static_cast<unsigned char>(str[start]))) {
        start++;
    }
    if (start == str.size()) {
        return "";
    }
    std::size_t end = str.size();
    while (end > start && std::isspace(static_cast<unsigned char>(str[end - 1]))) {
        end--;
    }
    return str.substr(start, end - start);
}

static OctiMoveOp parseSingleRegularOp(const std::string &s)
{
    if (s.size() < 2) {
        throw IllegalMoveSyntaxException("move operation too short");
    }

    char c0 = s[0];
    char c1 = s[1];
    if (c0 < '1' || c0 > '9' || c1 < '1' || c1 > '9') {
        throw IllegalMoveSyntaxException("invalid start coordinate digits");
    }

    OctiMoveOp op;
    op.x0 = c0 - '1';
    op.y0 = c1 - '1';

    if (s.size() == 2) {
        op.addPod = true;
        op.liberate = false;
        return op;
    }

    if (s.size() == 3 && s[2] == '*') {
        op.addPod = true;
        op.liberate = true;
        return op;
    }

    // Parse pod config
    std::size_t idx = 2;
    bool seenDir[DIR_MAX] = {false};

    while (idx < s.size()) {
        char c = s[idx];
        OctiDir dir = charToDir(c);
        if (dir == DIR_UNDEF) {
            break;
        }

        if (seenDir[dir]) {
            throw IllegalMoveSyntaxException("duplicate direction in pod configuration");
        }
        seenDir[dir] = true;
        idx++;

        bool isSuper = false;
        if (idx < s.size() && s[idx] == '!') {
            isSuper = true;
            idx++;
        }
        op.pod.addProng(dir, isSuper);
    }

    if (idx >= s.size()) {
        throw IllegalMoveSyntaxException("incomplete move syntax after pod config");
    }

    char action = s[idx];
    if (action == '+') {
        idx++;
        if (idx >= s.size()) {
            throw IllegalMoveSyntaxException("expected direction after '+'");
        }
        OctiDir addDir = charToDir(s[idx]);
        if (addDir == DIR_UNDEF) {
            throw IllegalMoveSyntaxException("invalid direction after '+'");
        }
        idx++;

        bool addSuper = false;
        if (idx < s.size() && s[idx] == '!') {
            addSuper = true;
            idx++;
        }

        if (idx == s.size()) {
            // Prong add
            if (op.pod.prongAt(addDir) != NO_PRONG) {
                throw IllegalMoveSyntaxException("cannot add prong that already exists");
            }
            op.addProngDir = addDir;
            op.superProng = addSuper;
            return op;
        }

        if (s[idx] == '-') {
            // Reposition format: +addDir-rmDir
            idx++;
            if (idx >= s.size()) {
                throw IllegalMoveSyntaxException("expected direction after '-' in reposition");
            }
            OctiDir rmDir = charToDir(s[idx]);
            if (rmDir == DIR_UNDEF) {
                throw IllegalMoveSyntaxException("invalid remove direction in reposition");
            }
            idx++;

            if (idx < s.size() && s[idx] == '!') {
                throw IllegalMoveSyntaxException("superprong cannot be removed in reposition");
            }
            if (addSuper) {
                throw IllegalMoveSyntaxException("superprong cannot be added in reposition");
            }
            if (idx != s.size()) {
                throw IllegalMoveSyntaxException("extra trailing characters after reposition");
            }

            if (addDir == rmDir) {
                throw IllegalMoveSyntaxException("reposition with identical add and remove directions");
            }
            if (op.pod.prongAt(rmDir) == NO_PRONG) {
                throw IllegalMoveSyntaxException("cannot remove prong that does not exist");
            }
            if (op.pod.prongAt(rmDir) == SUPER_PRONG) {
                throw IllegalMoveSyntaxException("cannot reposition a superprong");
            }
            if (op.pod.prongAt(addDir) != NO_PRONG) {
                throw IllegalMoveSyntaxException("cannot add prong that already exists");
            }

            op.addProngDir = addDir;
            op.rmProngDir = rmDir;
            op.superProng = false;
            return op;
        }

        throw IllegalMoveSyntaxException("unexpected character after prong add direction");
    }

    if (action == '-') {
        idx++;
        if (idx >= s.size()) {
            throw IllegalMoveSyntaxException("unexpected end of string after '-'");
        }

        char nextC = s[idx];
        if ((nextC >= 'A' && nextC <= 'H') || (nextC >= 'a' && nextC <= 'h')) {
            // Reposition format: -rmDir+addDir
            OctiDir rmDir = charToDir(nextC);
            if (rmDir == DIR_UNDEF) {
                throw IllegalMoveSyntaxException("invalid remove direction in reposition");
            }
            idx++;

            if (idx < s.size() && s[idx] == '!') {
                throw IllegalMoveSyntaxException("superprong cannot be removed in reposition");
            }
            if (idx >= s.size() || s[idx] != '+') {
                throw IllegalMoveSyntaxException("expected '+' after remove direction in reposition");
            }
            idx++;

            if (idx >= s.size()) {
                throw IllegalMoveSyntaxException("expected add direction after '+' in reposition");
            }
            OctiDir addDir = charToDir(s[idx]);
            if (addDir == DIR_UNDEF) {
                throw IllegalMoveSyntaxException("invalid add direction in reposition");
            }
            idx++;

            if (idx < s.size() && s[idx] == '!') {
                throw IllegalMoveSyntaxException("superprong cannot be added in reposition");
            }
            if (idx != s.size()) {
                throw IllegalMoveSyntaxException("extra trailing characters after reposition");
            }

            if (addDir == rmDir) {
                throw IllegalMoveSyntaxException("reposition with identical add and remove directions");
            }
            if (op.pod.prongAt(rmDir) == NO_PRONG) {
                throw IllegalMoveSyntaxException("cannot remove prong that does not exist");
            }
            if (op.pod.prongAt(rmDir) == SUPER_PRONG) {
                throw IllegalMoveSyntaxException("cannot reposition a superprong");
            }
            if (op.pod.prongAt(addDir) != NO_PRONG) {
                throw IllegalMoveSyntaxException("cannot add prong that already exists");
            }

            op.addProngDir = addDir;
            op.rmProngDir = rmDir;
            op.superProng = false;
            return op;
        }

        if (nextC >= '1' && nextC <= '9') {
            // Hop chain
            while (true) {
                if (idx + 1 >= s.size()) {
                    throw IllegalMoveSyntaxException("truncated hop destination coordinate");
                }
                char hx = s[idx];
                char hy = s[idx + 1];
                if (hx < '1' || hx > '9' || hy < '1' || hy > '9') {
                    throw IllegalMoveSyntaxException("invalid hop destination coordinate digits");
                }
                int hopX = hx - '1';
                int hopY = hy - '1';
                idx += 2;

                bool capture = false;
                if (idx < s.size() && (s[idx] == 'x' || s[idx] == 'X')) {
                    capture = true;
                    idx++;
                    if (idx < s.size() && (s[idx] == 'x' || s[idx] == 'X')) {
                        throw IllegalMoveSyntaxException("duplicate capture marker");
                    }
                }

                OctiHop hop;
                hop.x = hopX;
                hop.y = hopY;
                hop.capture = capture;
                op.hops.push_back(hop);

                if (idx >= s.size()) {
                    break;
                }
                if (s[idx] != '-') {
                    throw IllegalMoveSyntaxException("unexpected character in hop chain");
                }
                idx++;
            }

            if (op.hops.empty()) {
                throw IllegalMoveSyntaxException("hop chain has no hops");
            }
            return op;
        }

        throw IllegalMoveSyntaxException("invalid character following '-'");
    }

    throw IllegalMoveSyntaxException("unrecognized move operation character");
}

static OctiMoveOp parseSingleHopChain(const std::string &s)
{
    OctiMoveOp op = parseSingleRegularOp(s);
    if (op.hops.empty() || op.addPod || op.addProngDir != DIR_UNDEF ||
        op.rmProngDir != DIR_UNDEF) {
        throw IllegalMoveSyntaxException("expected hop chain operation");
    }
    return op;
}

void OctiMove::parseMoveString(const std::string &str, std::vector<OctiMoveOp> &outOps)
{
    outOps.clear();
    std::string s = trimWhitespace(str);
    if (s.empty()) {
        throw IllegalMoveSyntaxException("empty move string");
    }

    // Split by ';' into superprong prelude parts and regular move part
    std::vector<std::string> semiParts;
    std::size_t start = 0;
    while (true) {
        std::size_t pos = s.find(';', start);
        if (pos == std::string::npos) {
            semiParts.push_back(s.substr(start));
            break;
        }
        semiParts.push_back(s.substr(start, pos - start));
        start = pos + 1;
    }

    if (semiParts.size() > 3) {
        throw IllegalMoveSyntaxException("more than 2 superprong prelude operations");
    }

    for (std::size_t i = 0; i < semiParts.size(); ++i) {
        semiParts[i] = trimWhitespace(semiParts[i]);
        if (semiParts[i].empty()) {
            throw IllegalMoveSyntaxException("empty segment around ';'");
        }
    }

    // Prelude parts: all but the last
    for (std::size_t i = 0; i + 1 < semiParts.size(); ++i) {
        if (semiParts[i].find(',') != std::string::npos) {
            throw IllegalMoveSyntaxException("superprong prelude move cannot be composite");
        }
        OctiMoveOp op = parseSingleHopChain(semiParts[i]);
        op.isSuperProngBonusMove = true;
        outOps.push_back(op);
    }

    // Regular part
    const std::string &regStr = semiParts.back();
    if (regStr.find(',') != std::string::npos) {
        std::vector<std::string> commaParts;
        std::size_t cStart = 0;
        while (true) {
            std::size_t cPos = regStr.find(',', cStart);
            if (cPos == std::string::npos) {
                commaParts.push_back(regStr.substr(cStart));
                break;
            }
            commaParts.push_back(regStr.substr(cStart, cPos - cStart));
            cStart = cPos + 1;
        }

        int commonX0 = -1;
        int commonY0 = -1;

        for (std::size_t i = 0; i < commaParts.size(); ++i) {
            commaParts[i] = trimWhitespace(commaParts[i]);
            if (commaParts[i].empty()) {
                throw IllegalMoveSyntaxException("empty composite operation around ','");
            }
            OctiMoveOp op = parseSingleHopChain(commaParts[i]);
            op.isSuperProngBonusMove = false;
            if (i == 0) {
                commonX0 = op.x0;
                commonY0 = op.y0;
            } else {
                if (op.x0 != commonX0 || op.y0 != commonY0) {
                    throw IllegalMoveSyntaxException(
                        "composite move operations must share the same starting coordinate");
                }
            }
            outOps.push_back(op);
        }
    } else {
        OctiMoveOp op = parseSingleRegularOp(regStr);
        op.isSuperProngBonusMove = false;
        outOps.push_back(op);
    }
}

std::string OctiMove::formatOps(const std::vector<OctiMoveOp> &ops)
{
    if (ops.empty()) {
        return "";
    }

    std::string result;
    for (std::size_t i = 0; i < ops.size(); ++i) {
        const OctiMoveOp &op = ops[i];

        result.push_back(static_cast<char>('1' + op.x0));
        result.push_back(static_cast<char>('1' + op.y0));

        if (op.addPod) {
            if (op.liberate) {
                result.push_back('*');
            }
        } else if (op.addProngDir != DIR_UNDEF && op.rmProngDir != DIR_UNDEF) {
            result += op.pod.getConfigString();
            result.push_back('+');
            result.push_back(dirToChar(op.addProngDir));
            result.push_back('-');
            result.push_back(dirToChar(op.rmProngDir));
        } else if (op.addProngDir != DIR_UNDEF) {
            result += op.pod.getConfigString();
            result.push_back('+');
            result.push_back(dirToChar(op.addProngDir));
            if (op.superProng) {
                result.push_back('!');
            }
        } else if (!op.hops.empty()) {
            result += op.pod.getConfigString();
            for (const auto &h : op.hops) {
                result.push_back('-');
                result.push_back(static_cast<char>('1' + h.x));
                result.push_back(static_cast<char>('1' + h.y));
                if (h.capture) {
                    result.push_back('x');
                }
            }
        }

        if (i + 1 < ops.size()) {
            if (op.isSuperProngBonusMove) {
                result.push_back(';');
            } else {
                result.push_back(',');
            }
        }
    }

    return result;
}

OctiMove::OctiMove()
{
}

OctiMove::OctiMove(const std::string &str)
{
    parseMoveString(str, _ops);
    _str = formatOps(_ops);
}

OctiMove::OctiMove(const char *str)
{
    if (str == nullptr) {
        throw IllegalMoveSyntaxException("null move string");
    }
    parseMoveString(std::string(str), _ops);
    _str = formatOps(_ops);
}

OctiMove::OctiMove(const std::vector<OctiMoveOp> &ops)
    : _ops(ops)
{
    if (_ops.empty()) {
        _str = "";
        return;
    }

    // Validate ops
    bool seenRegular = false;
    int bonusCount = 0;
    int firstRegX0 = -1;
    int firstRegY0 = -1;
    int regularCount = 0;

    for (std::size_t i = 0; i < _ops.size(); ++i) {
        const OctiMoveOp &op = _ops[i];

        if (op.x0 < 0 || op.x0 > 8 || op.y0 < 0 || op.y0 > 8) {
            throw IllegalMoveSyntaxException("op coordinate out of bounds");
        }

        if (op.isSuperProngBonusMove) {
            if (seenRegular) {
                throw IllegalMoveSyntaxException(
                    "superprong bonus move cannot appear after regular move");
            }
            bonusCount++;
            if (bonusCount > 2) {
                throw IllegalMoveSyntaxException("too many superprong bonus moves");
            }
            if (op.hops.empty() || op.addPod || op.addProngDir != DIR_UNDEF ||
                op.rmProngDir != DIR_UNDEF) {
                throw IllegalMoveSyntaxException(
                    "superprong bonus move must be a hop chain");
            }
        } else {
            seenRegular = true;
            regularCount++;
            if (regularCount == 1) {
                firstRegX0 = op.x0;
                firstRegY0 = op.y0;
            } else {
                if (op.x0 != firstRegX0 || op.y0 != firstRegY0) {
                    throw IllegalMoveSyntaxException(
                        "composite regular moves must share the same starting coordinate");
                }
                if (op.hops.empty() || op.addPod || op.addProngDir != DIR_UNDEF ||
                    op.rmProngDir != DIR_UNDEF) {
                    throw IllegalMoveSyntaxException(
                        "composite moves must consist solely of hop chains");
                }
            }
        }

        if (op.addPod) {
            if (!op.hops.empty() || op.addProngDir != DIR_UNDEF ||
                op.rmProngDir != DIR_UNDEF) {
                throw IllegalMoveSyntaxException("invalid pod placement op structure");
            }
        } else if (op.addProngDir != DIR_UNDEF && op.rmProngDir != DIR_UNDEF) {
            if (op.addProngDir == op.rmProngDir || op.superProng ||
                !op.hops.empty()) {
                throw IllegalMoveSyntaxException("invalid reposition op structure");
            }
            if (op.pod.prongAt(op.rmProngDir) != NORMAL_PRONG) {
                throw IllegalMoveSyntaxException(
                    "cannot reposition non-existent or superprong");
            }
            if (op.pod.prongAt(op.addProngDir) != NO_PRONG) {
                throw IllegalMoveSyntaxException(
                    "cannot reposition to already occupied prong slot");
            }
        } else if (op.addProngDir != DIR_UNDEF) {
            if (!op.hops.empty()) {
                throw IllegalMoveSyntaxException("invalid prong add op structure");
            }
            if (op.pod.prongAt(op.addProngDir) != NO_PRONG) {
                throw IllegalMoveSyntaxException(
                    "cannot add prong to already occupied slot");
            }
        } else if (!op.hops.empty()) {
            for (const auto &h : op.hops) {
                if (h.x < 0 || h.x > 8 || h.y < 0 || h.y > 8) {
                    throw IllegalMoveSyntaxException("hop coordinate out of bounds");
                }
            }
        } else {
            throw IllegalMoveSyntaxException("unrecognized move op structure");
        }
    }

    if (regularCount == 0 && bonusCount > 0) {
        throw IllegalMoveSyntaxException("superprong prelude without regular move");
    }

    _str = formatOps(_ops);
}

const std::vector<OctiMoveOp> &OctiMove::getOps() const
{
    return _ops;
}

std::size_t OctiMove::size() const
{
    return _ops.size();
}

bool OctiMove::isEmpty() const
{
    return _ops.empty();
}

std::string OctiMove::asString() const
{
    return _str;
}

bool OctiMove::operator==(const OctiMove &other) const
{
    return _ops == other._ops;
}

bool OctiMove::operator!=(const OctiMove &other) const
{
    return !(*this == other);
}

std::ostream &operator<<(std::ostream &os, const OctiMove &move)
{
    os << move._str;
    return os;
}

} // namespace octi

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
