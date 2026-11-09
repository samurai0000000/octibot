/*
 * OctiBoard.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "OctiBoard.hxx"
#include "OctiGame.hxx"
#include <algorithm>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>

namespace octi {

OctiBoard::OctiBoard()
    : _mode(GAME_MODE_UNDEF)
    , _width(9)
    , _height(9)
    , _game(nullptr)
    , _suppressNotification(false)
{
    setGameMode(GAME_MODE_2, false, false, GAME_BASES_3);
}

OctiBoard::OctiBoard(OctiGameMode mode,
                     bool useSuperProngs,
                     bool useEdgelessBoard,
                     OctiGameBases bases)
    : _mode(GAME_MODE_UNDEF)
    , _width(9)
    , _height(9)
    , _game(nullptr)
    , _suppressNotification(false)
{
    setGameMode(mode, useSuperProngs, useEdgelessBoard, bases);
}

OctiBoard::~OctiBoard() = default;

void OctiBoard::setGameMode(OctiGameMode mode,
                            bool useSuperProngs,
                            bool useEdgelessBoard,
                            OctiGameBases bases)
{
    if (mode != GAME_MODE_2 && mode != GAME_MODE_4 && mode != GAME_MODE_OFK) {
        throw IllegalMoveException("invalid game mode");
    }

    _mode = mode;
    _suppressNotification = false;

    if (_mode == GAME_MODE_OFK) {
        _width = 6;
        _height = 7;
        bases = GAME_BASES_1;
        useSuperProngs = false;
        useEdgelessBoard = false;
    } else {
        _width = 9;
        _height = 9;
    }

    for (int x = 0; x < 9; x++) {
        for (int y = 0; y < 9; y++) {
            _boardSquares[x][y].configure(_mode, NEUTRAL_SQUARE, x, y);
            _boardSquares[x][y].clear();
        }
    }

    if (_mode == GAME_MODE_2) {
        // Player A bases: (2,2), (4,2), (6,2) (33, 53, 73)
        _boardSquares[2][2].configure(_mode, A_HOME_SQUARE, 2, 2);
        _boardSquares[4][2].configure(_mode, A_HOME_SQUARE, 4, 2);
        _boardSquares[6][2].configure(_mode, A_HOME_SQUARE, 6, 2);

        // Player B bases: (2,6), (4,6), (6,6) (37, 57, 77)
        _boardSquares[2][6].configure(_mode, B_HOME_SQUARE, 2, 6);
        _boardSquares[4][6].configure(_mode, B_HOME_SQUARE, 4, 6);
        _boardSquares[6][6].configure(_mode, B_HOME_SQUARE, 6, 6);
    } else if (_mode == GAME_MODE_4) {
        // Bases: A=(2,2)[33], B=(2,6)[37], C=(6,6)[77], D=(6,2)[73]
        _boardSquares[2][2].configure(_mode, A_HOME_SQUARE, 2, 2);
        _boardSquares[2][6].configure(_mode, B_HOME_SQUARE, 2, 6);
        _boardSquares[6][6].configure(_mode, C_HOME_SQUARE, 6, 6);
        _boardSquares[6][2].configure(_mode, D_HOME_SQUARE, 6, 2);

        // Neutral bases: (4,2)[53], (4,6)[57]
        _boardSquares[4][2].configure(_mode, NEUTRAL_OCTI_SQUARE, 4, 2);
        _boardSquares[4][6].configure(_mode, NEUTRAL_OCTI_SQUARE, 4, 6);
    } else if (_mode == GAME_MODE_OFK) {
        // Bases: A=(1,1),(2,1),(3,1),(4,1) [22,32,42,52]
        _boardSquares[1][1].configure(_mode, A_HOME_SQUARE, 1, 1);
        _boardSquares[2][1].configure(_mode, A_HOME_SQUARE, 2, 1);
        _boardSquares[3][1].configure(_mode, A_HOME_SQUARE, 3, 1);
        _boardSquares[4][1].configure(_mode, A_HOME_SQUARE, 4, 1);

        // Bases: B=(1,5),(2,5),(3,5),(4,5) [26,36,46,56]
        _boardSquares[1][5].configure(_mode, B_HOME_SQUARE, 1, 5);
        _boardSquares[2][5].configure(_mode, B_HOME_SQUARE, 2, 5);
        _boardSquares[3][5].configure(_mode, B_HOME_SQUARE, 3, 5);
        _boardSquares[4][5].configure(_mode, B_HOME_SQUARE, 4, 5);
    }

    std::unique_ptr<OctiGame> oldGame = std::move(_game);
    _game = std::make_unique<OctiGame>(this, _mode, useSuperProngs, useEdgelessBoard, bases);
    notifyGameChanged(oldGame.get(), _game.get());
    _game->reset();
}

void OctiBoard::clear()
{
    for (int x = 0; x < 9; x++) {
        for (int y = 0; y < 9; y++) {
            _boardSquares[x][y].clear();
        }
    }
}

bool OctiBoard::canLiberate(OctiPlayer player) const
{
    if (_mode == GAME_MODE_UNDEF || _mode == GAME_MODE_OFK) {
        return false;
    }

    if (_mode == GAME_MODE_2) {
        if (player == PLAYER_A) {
            return _boardSquares[2][6].isOccupiedBy(PLAYER_A) ||
                   _boardSquares[4][6].isOccupiedBy(PLAYER_A) ||
                   _boardSquares[6][6].isOccupiedBy(PLAYER_A);
        }
        if (player == PLAYER_B) {
            return _boardSquares[2][2].isOccupiedBy(PLAYER_B) ||
                   _boardSquares[4][2].isOccupiedBy(PLAYER_B) ||
                   _boardSquares[6][2].isOccupiedBy(PLAYER_B);
        }
        return false;
    }

    if (_mode == GAME_MODE_4) {
        // Neutral OCTI squares: player must personally occupy
        if (_boardSquares[4][2].isOccupiedBy(player) ||
            _boardSquares[4][6].isOccupiedBy(player)) {
            return true;
        }

        // Enemy home squares: player must personally occupy
        if (player == PLAYER_A || player == PLAYER_C) {
            if (_boardSquares[2][6].isOccupiedBy(player) ||
                _boardSquares[6][2].isOccupiedBy(player)) {
                return true;
            }
        } else if (player == PLAYER_B || player == PLAYER_D) {
            if (_boardSquares[2][2].isOccupiedBy(player) ||
                _boardSquares[6][6].isOccupiedBy(player)) {
                return true;
            }
        }

        return false;
    }

    return false;
}

void OctiBoard::addBoardListener(OctiBoardListener *listener)
{
    if (listener) {
        auto it = std::find(_listeners.begin(), _listeners.end(), listener);
        if (it == _listeners.end()) {
            _listeners.push_back(listener);
        }
    }
}

void OctiBoard::removeBoardListener(OctiBoardListener *listener)
{
    auto it = std::find(_listeners.begin(), _listeners.end(), listener);
    if (it != _listeners.end()) {
        _listeners.erase(it);
    }
}

void OctiBoard::suppressBoardUpdateNotification(bool suppress)
{
    _suppressNotification = suppress;
}

bool OctiBoard::isBoardUpdateNotificationSuppressed() const
{
    return _suppressNotification;
}

void OctiBoard::notifyBoardTurnUpdate(const OctiMove *move)
{
    if (!_suppressNotification) {
        for (auto *l : _listeners) {
            l->boardUpdated(this, move);
        }
    }
}

void OctiBoard::notifyGameChanged(OctiGame *oldGame, OctiGame *newGame)
{
    for (auto *l : _listeners) {
        l->gameChanged(this, oldGame, newGame);
    }
}

std::string OctiBoard::toPositionString() const
{
    std::ostringstream ss;

    // 1. mode
    if (_mode == GAME_MODE_2) {
        ss << "2";
    } else if (_mode == GAME_MODE_4) {
        ss << "4";
    } else if (_mode == GAME_MODE_OFK) {
        ss << "OFK";
    } else {
        ss << "UNDEF";
    }
    ss << ":";

    // 2. bases
    ss << (_game ? static_cast<int>(_game->getBases()) : 3) << ":";

    // 3. superprongs
    ss << ((_game && _game->useSuperProngs()) ? "1" : "0") << ":";

    // 4. edgeless
    ss << ((_game && _game->useEdgelessBoard()) ? "1" : "0") << ":";

    // 5. status
    if (!_game || !_game->isGameStarted()) {
        ss << "N";
    } else if (_game->isGameResigned()) {
        ss << "R";
    } else if (_game->isGameOver()) {
        ss << "O";
    } else {
        ss << "P";
    }
    ss << ":";

    // 6. turn
    if (!_game || !_game->isGameStarted() || _game->isGameOver()) {
        ss << "-";
    } else {
        switch (_game->getTurn()) {
        case PLAYER_A: ss << "A"; break;
        case PLAYER_B: ss << "B"; break;
        case PLAYER_C: ss << "C"; break;
        case PLAYER_D: ss << "D"; break;
        default:       ss << "-"; break;
        }
    }
    ss << ":";

    // 7. winner
    if (!_game || _game->getWinner() == PLAYER_UNDEF) {
        ss << "-";
    } else {
        switch (_game->getWinner()) {
        case PLAYER_A: ss << "A"; break;
        case PLAYER_B: ss << "B"; break;
        case PLAYER_C: ss << "C"; break;
        case PLAYER_D: ss << "D"; break;
        default:       ss << "-"; break;
        }
    }
    ss << ":";

    // 8. elimMask
    int elimMask = 0;
    if (_mode == GAME_MODE_4 && _game) {
        if (_game->isPlayerEliminated(PLAYER_A)) elimMask |= 1;
        if (_game->isPlayerEliminated(PLAYER_B)) elimMask |= 2;
        if (_game->isPlayerEliminated(PLAYER_C)) elimMask |= 4;
        if (_game->isPlayerEliminated(PLAYER_D)) elimMask |= 8;
    }
    ss << elimMask << ":";

    // 9. pNRes
    int numPlayers = (_mode == GAME_MODE_4) ? 4 : 2;
    for (int p = 1; p <= numPlayers; p++) {
        OctiPlayer player = static_cast<OctiPlayer>(p);
        if (_game) {
            ss << _game->getFreePods(player) << ","
               << _game->getCapturedPods(player) << ","
               << _game->getFreeProngs(player) << ","
               << _game->getFreeSuperProngs(player);
        } else {
            ss << "0,0,0,0";
        }
        ss << ":";
    }

    // 10. squares: row-major order (y then x)
    bool firstPod = true;
    for (int y = 0; y < _height; y++) {
        for (int x = 0; x < _width; x++) {
            const OctiSquare &sq = _boardSquares[x][y];
            for (int i = 0; i < sq.getNumPods(); i++) {
                if (!firstPod) {
                    ss << ",";
                }
                firstPod = false;

                const OctiPod &pod = sq.getPodAt(i);
                char owner = '?';
                switch (pod.getType()) {
                case A_POD: owner = 'A'; break;
                case B_POD: owner = 'B'; break;
                case C_POD: owner = 'C'; break;
                case D_POD: owner = 'D'; break;
                default: break;
                }
                ss << owner << (x + 1) << (y + 1) << pod.getConfigString();
            }
        }
    }

    if (firstPod) {
        ss << "-";
    }

    return ss.str();
}

void OctiBoard::fromPositionString(const std::string &pos)
{
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(pos);
    while (std::getline(tokenStream, token, ':')) {
        tokens.push_back(token);
    }

    if (tokens.size() < 11) {
        throw IllegalMoveSyntaxException("malformed OCTI-FEN position string");
    }

    // 1. Mode
    OctiGameMode mode = GAME_MODE_2;
    if (tokens[0] == "2") {
        mode = GAME_MODE_2;
    } else if (tokens[0] == "4") {
        mode = GAME_MODE_4;
    } else if (tokens[0] == "OFK") {
        mode = GAME_MODE_OFK;
    } else {
        throw IllegalMoveSyntaxException("invalid mode in position string");
    }

    int expectedTokens = (mode == GAME_MODE_4) ? 13 : 11;
    if (static_cast<int>(tokens.size()) != expectedTokens) {
        throw IllegalMoveSyntaxException("token count mismatch in position string");
    }

    // 2. Bases
    OctiGameBases bases = (tokens[1] == "1") ? GAME_BASES_1 : GAME_BASES_3;

    // 3. Superprongs
    bool useSuper = (tokens[2] == "1");

    // 4. Edgeless
    bool useEdgeless = (tokens[3] == "1");

    if (mode == GAME_MODE_OFK) {
        if (tokens[1] != "1") {
            throw IllegalMoveSyntaxException("OFK mode requires 1 base");
        }
        if (useSuper) {
            throw IllegalMoveSyntaxException("superprongs not allowed in OCTI for Kids");
        }
        if (useEdgeless) {
            throw IllegalMoveSyntaxException("edgeless board not allowed in OCTI for Kids");
        }
    }

    // Configure board grid and reinitialize game engine
    setGameMode(mode, useSuper, useEdgeless, bases);
    clear();

    // 5. Status
    char statusChar = tokens[4].empty() ? 'P' : tokens[4][0];

    // 6. Turn
    OctiPlayer turn = PLAYER_UNDEF;
    if (!tokens[5].empty() && tokens[5] != "-") {
        switch (tokens[5][0]) {
        case 'A': turn = PLAYER_A; break;
        case 'B': turn = PLAYER_B; break;
        case 'C': turn = PLAYER_C; break;
        case 'D': turn = PLAYER_D; break;
        default:  turn = PLAYER_UNDEF; break;
        }
    }

    // 7. Winner
    OctiPlayer winner = PLAYER_UNDEF;
    if (!tokens[6].empty() && tokens[6] != "-") {
        switch (tokens[6][0]) {
        case 'A': winner = PLAYER_A; break;
        case 'B': winner = PLAYER_B; break;
        case 'C': winner = PLAYER_C; break;
        case 'D': winner = PLAYER_D; break;
        default:  winner = PLAYER_UNDEF; break;
        }
    }

    // 8. Eliminated mask
    int elimMask = std::stoi(tokens[7]);

    // Build snapshot for game restore
    OctiGameSnapshot snap;
    snap.gameStarted = (statusChar != 'N');
    snap.gameResigned = (statusChar == 'R');
    snap.gameOver = (statusChar == 'O' || statusChar == 'R');
    snap.turn = turn;
    snap.winner = winner;

    if (mode == GAME_MODE_4) {
        snap.players[PLAYER_A].eliminated = ((elimMask & 1) != 0);
        snap.players[PLAYER_B].eliminated = ((elimMask & 2) != 0);
        snap.players[PLAYER_C].eliminated = ((elimMask & 4) != 0);
        snap.players[PLAYER_D].eliminated = ((elimMask & 8) != 0);
    }

    // 9. Player resources
    int numPlayers = (mode == GAME_MODE_4) ? 4 : 2;
    for (int p = 1; p <= numPlayers; p++) {
        std::string resStr = tokens[7 + p];
        std::istringstream resStream(resStr);
        std::string val;
        int vals[4] = {0, 0, 0, 0};
        int vi = 0;
        while (std::getline(resStream, val, ',') && vi < 4) {
            vals[vi++] = std::stoi(val);
        }
        if (mode == GAME_MODE_OFK) {
            if (vals[0] > 0) {
                throw IllegalMoveSyntaxException("freePods not allowed in OCTI for Kids");
            }
            if (vals[3] > 0) {
                throw IllegalMoveSyntaxException("freeSuperProngs not allowed in OCTI for Kids");
            }
            if (vals[1] < 0 || vals[1] > 4) {
                throw IllegalMoveSyntaxException("invalid capturedPods in OCTI for Kids");
            }
        }
        snap.players[p].freePods = vals[0];
        snap.players[p].capturedPods = vals[1];
        snap.players[p].freeProngs = vals[2];
        snap.players[p].freeSuperProngs = vals[3];
    }

    // 10. Squares
    std::string squaresStr = tokens[tokens.size() - 1];
    if (!squaresStr.empty() && squaresStr != "-") {
        std::istringstream sqStream(squaresStr);
        std::string podToken;
        while (std::getline(sqStream, podToken, ',')) {
            if (podToken.size() < 3) {
                continue;
            }
            char ownerChar = podToken[0];
            char xChar = podToken[1];
            char yChar = podToken[2];

            OctiPlayer owner = PLAYER_UNDEF;
            switch (ownerChar) {
            case 'A': owner = PLAYER_A; break;
            case 'B': owner = PLAYER_B; break;
            case 'C': owner = PLAYER_C; break;
            case 'D': owner = PLAYER_D; break;
            default:  break;
            }

            int x = xChar - '1';
            int y = yChar - '1';
            if (!isValidCoord(x, y)) {
                throw IllegalMoveSyntaxException("square coordinate out of bounds in position string");
            }

            OctiPod pod(playerToPodType(owner));
            std::size_t idx = 3;
            while (idx < podToken.size()) {
                char dChar = podToken[idx++];
                OctiDir dir = charToDir(dChar);
                if (dir == DIR_UNDEF) {
                    break;
                }
                bool super = false;
                if (idx < podToken.size() && podToken[idx] == '!') {
                    if (mode == GAME_MODE_OFK) {
                        throw IllegalMoveSyntaxException("superprongs not allowed in OCTI for Kids");
                    }
                    super = true;
                    idx++;
                }
                pod.addProng(dir, super);
            }

            if (mode == GAME_MODE_OFK && !squareAt(x, y).isEmpty()) {
                throw IllegalMoveSyntaxException("pod stacking not allowed in OCTI for Kids");
            }

            squareAt(x, y).addPod(pod);
            snap.squares[x][y].push_back(pod);
        }
    }

    if (_game) {
        _game->restoreSnapshot(snap);
    }
}

void OctiBoard::serializeGame(std::ostream &out) const
{
    if (_game) {
        _game->serialize(out);
    }
}

std::string OctiBoard::serializeGame() const
{
    std::ostringstream ss;
    serializeGame(ss);
    return ss.str();
}

std::string OctiBoard::serializeJson(const OctiGameMetadata &meta) const
{
    return _game ? _game->serializeJson(meta) : "";
}

std::string OctiBoard::serializeJson() const
{
    return _game ? _game->serializeJson() : "";
}

std::string OctiBoard::exportCanonicalMovesText() const
{
    return _game ? _game->exportCanonicalMovesText() : "";
}

std::string OctiBoard::computeCanonicalGameId() const
{
    return _game ? _game->computeCanonicalGameId() : "";
}

void OctiBoard::deserializeGame(std::istream &in)
{
    nlohmann::json root;
    try {
        in >> root;
    } catch (...) {
        throw IllegalMoveSyntaxException("invalid JSON transcript");
    }

    if (!root.is_object() || root.value("format", "") != "octi.transcript.v3") {
        throw IllegalMoveSyntaxException("unsupported transcript format; expected octi.transcript.v3");
    }

    std::string modeStr = root.value("mode", "2P");
    OctiGameMode mode = GAME_MODE_2;
    if (modeStr == "2P" || modeStr == "2" || modeStr == "1") {
        mode = GAME_MODE_2;
    } else if (modeStr == "4P" || modeStr == "4") {
        mode = GAME_MODE_4;
    } else if (modeStr == "OFK" || modeStr == "3") {
        mode = GAME_MODE_OFK;
    } else {
        throw IllegalMoveSyntaxException("invalid mode: " + modeStr);
    }

    int basesInt = root.value("bases", 3);
    OctiGameBases bases = (basesInt == 1) ? GAME_BASES_1 : GAME_BASES_3;

    bool superProngs = root.value("superProngs", false);
    bool edgeless = root.value("edgeless", false);

    std::string startPlayerStr = root.value("startPlayer", "A");
    OctiPlayer startPlayer = PLAYER_A;
    if (startPlayerStr == "A" || startPlayerStr == "1") startPlayer = PLAYER_A;
    else if (startPlayerStr == "B" || startPlayerStr == "2") startPlayer = PLAYER_B;
    else if (startPlayerStr == "C" || startPlayerStr == "3") startPlayer = PLAYER_C;
    else if (startPlayerStr == "D" || startPlayerStr == "4") startPlayer = PLAYER_D;

    std::string statusStr = root.value("status", "IN_PROGRESS");
    std::string winnerStr = root.value("winner", "NONE");
    OctiPlayer winner = PLAYER_UNDEF;
    if (winnerStr == "A" || winnerStr == "1") winner = PLAYER_A;
    else if (winnerStr == "B" || winnerStr == "2") winner = PLAYER_B;
    else if (winnerStr == "C" || winnerStr == "3") winner = PLAYER_C;
    else if (winnerStr == "D" || winnerStr == "4") winner = PLAYER_D;

    OctiGameMetadata meta;
    meta.gameId = root.value("gameId", "");
    meta.source = root.value("source", "LOCAL");
    meta.license = root.value("license", "");
    meta.attribution = root.value("attribution", "");
    meta.termination = root.value("termination", "");
    meta.timeControl = root.value("timeControl", "untimed");

    if (root.contains("client") && root["client"].is_object()) {
        const auto &clientObj = root["client"];
        meta.appVersion = clientObj.value("appVersion", "");
        meta.buildType = clientObj.value("buildType", "release");
        meta.platform = clientObj.value("platform", "");
        if (clientObj.contains("extended") && clientObj["extended"].is_object()) {
            const auto &extObj = clientObj["extended"];
            meta.extended.present = true;
            meta.extended.startedAt = extObj.value("startedAt", 0LL);
            meta.extended.endedAt = extObj.value("endedAt", 0LL);
            meta.extended.utcOffsetMin = extObj.value("utcOffsetMin", 0);
            meta.extended.locale = extObj.value("locale", "");
            meta.extended.osVersion = extObj.value("osVersion", "");
            meta.extended.installId = extObj.value("installId", "");
        }
    }

    if (root.contains("players") && root["players"].is_array()) {
        for (const auto &pObj : root["players"]) {
            if (!pObj.is_object()) continue;
            OctiPlayerMetadata p;
            p.seat = pObj.value("seat", 1);
            std::string pStr = pObj.value("player", "A");
            if (pStr == "A") p.player = PLAYER_A;
            else if (pStr == "B") p.player = PLAYER_B;
            else if (pStr == "C") p.player = PLAYER_C;
            else if (pStr == "D") p.player = PLAYER_D;
            p.type = pObj.value("type", "HUMAN");
            p.handle = pObj.value("handle", "Player");
            p.aiDifficulty = pObj.value("aiDifficulty", 0);
            p.aiBudgetMs = pObj.value("aiBudgetMs", 0);
            meta.players.push_back(p);
        }
    }

    setGameMode(mode, superProngs, edgeless, bases);
    clear();
    _game->reset();
    _game->setGameMetadata(meta);

    if (statusStr != "NOT_STARTED") {
        _game->start(startPlayer);
        if (root.contains("plies") && root["plies"].is_array()) {
            for (const auto &plyObj : root["plies"]) {
                if (!plyObj.is_object()) continue;
                std::string moveStr = plyObj.value("move", "");
                if (moveStr.empty()) {
                    throw IllegalMoveSyntaxException("empty move string in ply");
                }
                _game->move(moveStr);

                OctiPlyMetadata plyMeta;
                plyMeta.elapsedMs = plyObj.value("elapsedMs", 0LL);
                plyMeta.clockAfterMs = plyObj.value("clockAfterMs", -1LL);
                if (plyObj.contains("ai") && plyObj["ai"].is_object()) {
                    const auto &aiObj = plyObj["ai"];
                    plyMeta.isAiMove = true;
                    plyMeta.aiDifficulty = aiObj.value("difficulty", 0);
                    plyMeta.aiBudgetMs = aiObj.value("budgetMs", 0);
                    plyMeta.aiDepthReached = aiObj.value("depthReached", 0);
                    plyMeta.aiNodesVisited = aiObj.value("nodesVisited", 0ULL);
                    plyMeta.aiBestScore = aiObj.value("bestScore", 0);
                }
                _game->setLastPlyMetadata(plyMeta);
            }
        }

        if (statusStr == "RESIGNED") {
            if (winner != PLAYER_UNDEF) {
                _game->setResignedWinner(winner);
            } else {
                _game->resign(_game->getTurn());
            }
        }
    }
}

void OctiBoard::deserializeGame(const std::string &transcript)
{
    std::istringstream ss(transcript);
    deserializeGame(ss);
}

std::ostream &operator<<(std::ostream &os, const OctiBoard &board)
{
    int w = board.getBoardWidth();
    int h = board.getBoardHeight();

    os << "  ";
    for (int x = 0; x < w; x++) {
        os << "   " << (x + 1) << "  ";
    }
    os << "\n";

    for (int y = h - 1; y >= 0; y--) {
        os << "  +";
        for (int x = 0; x < w; x++) {
            os << "-----+";
        }
        os << "\n";

        os << (y + 1) << " |";
        for (int x = 0; x < w; x++) {
            const OctiSquare &sq = board.squareAt(x, y);
            if (sq.isEmpty()) {
                if (sq.getType() == A_HOME_SQUARE) {
                    os << " (A) |";
                } else if (sq.getType() == B_HOME_SQUARE) {
                    os << " (B) |";
                } else if (sq.getType() == C_HOME_SQUARE) {
                    os << " (C) |";
                } else if (sq.getType() == D_HOME_SQUARE) {
                    os << " (D) |";
                } else if (sq.getType() == NEUTRAL_OCTI_SQUARE) {
                    os << " (*) |";
                } else {
                    os << "     |";
                }
            } else {
                char owner = ' ';
                switch (sq.getPodAt(sq.getNumPods() - 1).getType()) {
                case A_POD: owner = 'A'; break;
                case B_POD: owner = 'B'; break;
                case C_POD: owner = 'C'; break;
                case D_POD: owner = 'D'; break;
                default: break;
                }
                if (sq.getNumPods() == 1) {
                    std::string cfg = sq.getPodAt(0).getConfigString();
                    if (cfg.size() > 3) cfg = cfg.substr(0, 3);
                    while (cfg.size() < 3) cfg += ' ';
                    os << " " << owner << ":" << cfg << "|";
                } else {
                    os << " " << owner << "[" << sq.getNumPods() << "] |";
                }
            }
        }
        os << " " << (y + 1) << "\n";
    }

    os << "  +";
    for (int x = 0; x < w; x++) {
        os << "-----+";
    }
    os << "\n";

    os << "  ";
    for (int x = 0; x < w; x++) {
        os << "   " << (x + 1) << "  ";
    }
    os << "\n";

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
