/*
 * OctiGame.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "OctiGame.hxx"
#include "OctiBoard.hxx"
#include "OctiBotEngine.hxx"
#include "OctiCrypto.hxx"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <iostream>
#include <nlohmann/json.hpp>
#include <set>
#include <sstream>

namespace octi {

// Ghost (vacated) pods exist only on the moving stack's own starting square.
// Every other square is judged by plain emptiness so that an own pod with
// identical prongs elsewhere on the board is never mistaken for a vacated pod.
static inline bool isEmptyForJump(const OctiSquare &sq, int x, int y,
                                  int startX, int startY,
                                  const std::vector<OctiPod> &ghostPods)
{
    if (x == startX && y == startY) {
        return sq.isEmptyWithout(ghostPods);
    }
    return sq.isEmpty();
}

static inline bool findStepDelta(int cx, int cy, int tx, int ty,
                                 bool edgeless, int width, int height,
                                 int &outDx, int &outDy)
{
    if (!edgeless) {
        outDx = tx - cx;
        outDy = ty - cy;
        return true;
    }

    bool foundX = false;
    int bestDx = 0;
    for (int d = -2; d <= 2; d++) {
        if (((cx + d) % width + width) % width == tx) {
            bestDx = d;
            foundX = true;
            break;
        }
    }
    if (!foundX) {
        return false;
    }

    bool foundY = false;
    int bestDy = 0;
    for (int d = -2; d <= 2; d++) {
        if (((cy + d) % height + height) % height == ty) {
            bestDy = d;
            foundY = true;
            break;
        }
    }
    if (!foundY) {
        return false;
    }

    outDx = bestDx;
    outDy = bestDy;
    return true;
}

OctiGame::OctiGame(OctiBoard *board,
                   OctiGameMode mode,
                   bool useSuperProngs,
                   bool useEdgelessBoard,
                   OctiGameBases bases)
    : _board(board)
    , _mode(mode)
    , _bases(mode == GAME_MODE_OFK ? GAME_BASES_1 : bases)
    , _useSuperProngs(mode == GAME_MODE_OFK ? false : useSuperProngs)
    , _useEdgelessBoard(mode == GAME_MODE_OFK ? false : useEdgelessBoard)
    , _gameStarted(false)
    , _gameOver(false)
    , _gameResigned(false)
    , _startPlayer(PLAYER_A)
    , _turn(PLAYER_UNDEF)
    , _winner(PLAYER_UNDEF)
    , _hasPreparedMove(false)
{
}

OctiGame::~OctiGame() = default;

void OctiGame::addGameListener(OctiGameListener *listener)
{
    if (listener) {
        auto it = std::find(_listeners.begin(), _listeners.end(), listener);
        if (it == _listeners.end()) {
            _listeners.push_back(listener);
        }
    }
}

void OctiGame::removeGameListener(OctiGameListener *listener)
{
    auto it = std::find(_listeners.begin(), _listeners.end(), listener);
    if (it != _listeners.end()) {
        _listeners.erase(it);
    }
}

void OctiGame::notifyGameUpdated()
{
    for (auto *l : _listeners) {
        l->gameUpdated(this);
    }
}

void OctiGame::notifyUndoRedoStatusChanged()
{
    for (auto *l : _listeners) {
        l->UndoRedoStatusChanged(this, canUndo(), canRedo());
    }
}

OctiBoard *OctiGame::getBoard()
{
    return _board;
}

const OctiBoard *OctiGame::getBoard() const
{
    return _board;
}

OctiGameMode OctiGame::getMode() const
{
    return _mode;
}

OctiGameBases OctiGame::getBases() const
{
    return _bases;
}

bool OctiGame::useSuperProngs() const
{
    return _useSuperProngs;
}

bool OctiGame::useEdgelessBoard() const
{
    return _useEdgelessBoard;
}

bool OctiGame::isGameStarted() const
{
    return _gameStarted;
}

bool OctiGame::isGameOver() const
{
    return _gameOver;
}

bool OctiGame::isGameResigned() const
{
    return _gameResigned;
}

OctiPlayer OctiGame::getTurn() const
{
    return _turn;
}

OctiPlayer OctiGame::getStartPlayer() const
{
    return _startPlayer;
}

OctiPlayer OctiGame::getWinner() const
{
    return _winner;
}

bool OctiGame::isPlayerEliminated(OctiPlayer p) const
{
    if (p < PLAYER_A || p > PLAYER_D) {
        return false;
    }
    return _players[p].eliminated;
}

int OctiGame::getFreePods(OctiPlayer p) const
{
    if (p < PLAYER_A || p > PLAYER_D) return 0;
    return _players[p].freePods;
}

int OctiGame::getCapturedPods(OctiPlayer p) const
{
    if (p < PLAYER_A || p > PLAYER_D) return 0;
    return _players[p].capturedPods;
}

int OctiGame::getFreeProngs(OctiPlayer p) const
{
    if (p < PLAYER_A || p > PLAYER_D) return 0;
    return _players[p].freeProngs;
}

int OctiGame::getFreeSuperProngs(OctiPlayer p) const
{
    if (p < PLAYER_A || p > PLAYER_D) return 0;
    return _players[p].freeSuperProngs;
}

long OctiGame::getTimeRemaining(OctiPlayer p) const
{
    if (p < PLAYER_A || p > PLAYER_D) return 0;
    return _players[p].timeRemainingMs;
}

void OctiGame::setTimeRemaining(OctiPlayer p, long ms)
{
    if (p >= PLAYER_A && p <= PLAYER_D) {
        _players[p].timeRemainingMs = ms;
    }
}

int OctiGame::getNumMoves() const
{
    return static_cast<int>(_history.size()) + _searchPlyOffset;
}

void OctiGame::setSearchMode(bool searchMode)
{
    _searchMode = searchMode;
    if (!searchMode) {
        _nullMoveDepth = 0;
    }
}

bool OctiGame::isSearchMode() const
{
    return _searchMode;
}

bool OctiGame::makeNullMove(std::uint64_t *inOutHash)
{
    if (!_searchMode || !_gameStarted || _gameOver) {
        return false;
    }
    if (_mode != GAME_MODE_2 && _mode != GAME_MODE_OFK) {
        return false;
    }
    if (_turn != PLAYER_A && _turn != PLAYER_B) {
        return false;
    }

    OctiPlayer prevTurn = _turn;
    OctiPlayer nextTurn = (_turn == PLAYER_A) ? PLAYER_B : PLAYER_A;
    _turn = nextTurn;
    ++_searchPlyOffset;
    ++_nullMoveDepth;
    if (inOutHash != nullptr) {
        *inOutHash = OctiBotEngine::toggleTurnHashStatic(
            *inOutHash, prevTurn, nextTurn);
    }
    return true;
}

bool OctiGame::unmakeNullMove(std::uint64_t *inOutHash)
{
    if (!_searchMode || _nullMoveDepth <= 0) {
        return false;
    }
    if (_mode != GAME_MODE_2 && _mode != GAME_MODE_OFK) {
        return false;
    }
    if (_turn != PLAYER_A && _turn != PLAYER_B) {
        return false;
    }

    OctiPlayer prevTurn = _turn;
    OctiPlayer restoredTurn = (_turn == PLAYER_A) ? PLAYER_B : PLAYER_A;
    _turn = restoredTurn;
    --_searchPlyOffset;
    --_nullMoveDepth;
    if (inOutHash != nullptr) {
        *inOutHash = OctiBotEngine::toggleTurnHashStatic(
            *inOutHash, prevTurn, restoredTurn);
    }
    return true;
}

bool OctiGame::isInNullMove() const
{
    return _nullMoveDepth > 0;
}

void OctiGame::getMoves(std::vector<std::pair<OctiPlayer, OctiMove>> &history) const
{
    history = _history;
}

bool OctiGame::canUndo() const
{
    return !_undoStack.empty();
}

bool OctiGame::canRedo() const
{
    return !_redoStack.empty();
}

void OctiGame::reset()
{
    _undoStack.clear();
    _redoStack.clear();
    _history.clear();
    _plyMetadata.clear();

    _gameStarted = false;
    _gameOver = false;
    _gameResigned = false;
    _turn = PLAYER_UNDEF;
    _winner = PLAYER_UNDEF;
    _hasPreparedMove = false;
    _prepOps.clear();
    _searchPlyOffset = 0;
    _nullMoveDepth = 0;

    for (int p = 0; p < 5; p++) {
        _players[p] = OctiPlayerState();
    }

    if (_board) {
        _board->clear();

        if (_mode == GAME_MODE_2) {
            _board->squareAt(2, 2).addPod(OctiPod(A_POD));
            _board->squareAt(4, 2).addPod(OctiPod(A_POD));
            _board->squareAt(6, 2).addPod(OctiPod(A_POD));

            _board->squareAt(2, 6).addPod(OctiPod(B_POD));
            _board->squareAt(4, 6).addPod(OctiPod(B_POD));
            _board->squareAt(6, 6).addPod(OctiPod(B_POD));

            for (int p : {PLAYER_A, PLAYER_B}) {
                _players[p].freePods = 4;
                _players[p].capturedPods = 0;
                if (_useSuperProngs) {
                    _players[p].freeProngs = 24;
                    _players[p].freeSuperProngs = 1;
                } else {
                    _players[p].freeProngs = 25;
                    _players[p].freeSuperProngs = 0;
                }
            }
        } else if (_mode == GAME_MODE_4) {
            _board->squareAt(2, 2).addPod(OctiPod(A_POD));
            _board->squareAt(2, 6).addPod(OctiPod(B_POD));
            _board->squareAt(6, 6).addPod(OctiPod(C_POD));
            _board->squareAt(6, 2).addPod(OctiPod(D_POD));

            for (int p : {PLAYER_A, PLAYER_B, PLAYER_C, PLAYER_D}) {
                _players[p].freePods = 3;
                _players[p].capturedPods = 0;
                if (_useSuperProngs) {
                    _players[p].freeProngs = 11;
                    _players[p].freeSuperProngs = 1;
                } else {
                    _players[p].freeProngs = 12;
                    _players[p].freeSuperProngs = 0;
                }
                _players[p].eliminated = false;
            }
        } else if (_mode == GAME_MODE_OFK) {
            _useSuperProngs = false;
            _useEdgelessBoard = false;
            _bases = GAME_BASES_1;

            _board->squareAt(1, 1).addPod(OctiPod(A_POD));
            _board->squareAt(2, 1).addPod(OctiPod(A_POD));
            _board->squareAt(3, 1).addPod(OctiPod(A_POD));
            _board->squareAt(4, 1).addPod(OctiPod(A_POD));

            _board->squareAt(1, 5).addPod(OctiPod(B_POD));
            _board->squareAt(2, 5).addPod(OctiPod(B_POD));
            _board->squareAt(3, 5).addPod(OctiPod(B_POD));
            _board->squareAt(4, 5).addPod(OctiPod(B_POD));

            for (int p : {PLAYER_A, PLAYER_B}) {
                _players[p].freePods = 0;
                _players[p].capturedPods = 0;
                _players[p].freeProngs = 12;
                _players[p].freeSuperProngs = 0;
                _players[p].eliminated = false;
            }
        }
    }

    notifyGameUpdated();
    notifyUndoRedoStatusChanged();
}

void OctiGame::start(OctiPlayer startPlayer)
{
    _startPlayer = startPlayer;
    _turn = startPlayer;
    _gameStarted = true;
    _gameOver = false;
    _gameResigned = false;
    _winner = PLAYER_UNDEF;
    _nullMoveDepth = 0;

    notifyGameUpdated();
    notifyUndoRedoStatusChanged();
}

void OctiGame::randomize()
{
    if (_board && _mode != GAME_MODE_2) {
        _board->setGameMode(GAME_MODE_2);
    }
    reset();
    start(PLAYER_A);

    const char *moves[] = {
        "33+b",
        "37+e",
        "33",
        "37",
        "33+c",
        "37+d",
        "53+a",
        "57+f",
        "33b+g",
        "37d+f",
        "73+h",
        "37df-46,37e-36",
        "53a+f",
        "57f-35",
        "33c-43,33bg-44",
        "36e+c",
        "53af-54",
        "35f+d"
    };

    for (const char *m : moves) {
        move(m);
    }
}

void OctiGame::resign(OctiPlayer player)
{
    if (!_gameStarted || _gameOver) {
        throw IllegalMoveException("game is not in progress");
    }

    _gameResigned = true;
    _gameOver = true;

    if (_mode == GAME_MODE_2 || _mode == GAME_MODE_OFK) {
        _winner = (player == PLAYER_A) ? PLAYER_B : PLAYER_A;
    } else if (_mode == GAME_MODE_4) {
        if (player == PLAYER_A || player == PLAYER_C) {
            _winner = PLAYER_B;
        } else {
            _winner = PLAYER_A;
        }
    }

    notifyGameUpdated();
    notifyUndoRedoStatusChanged();
}

void OctiGame::move(const std::string &moveStr)
{
    move(OctiMove(moveStr));
}

void OctiGame::move(const char *moveStr)
{
    move(OctiMove(moveStr));
}

void OctiGame::move(const std::vector<OctiMoveOp> &ops)
{
    move(OctiMove(ops));
}

void OctiGame::move(OctiPlayer player, const OctiMove &m)
{
    if (player != _turn) {
        throw IllegalMoveException("not player's turn");
    }
    move(m);
}

void OctiGame::move(OctiPlayer player, const std::string &moveStr)
{
    move(player, OctiMove(moveStr));
}

void OctiGame::move(const OctiMove &m)
{
    executeMoveInternal(m, false);
}

void OctiGame::executeMoveInternal(const OctiMove &m, bool isRedo)
{
    if (!_gameStarted) {
        throw IllegalMoveException("game has not started");
    }
    if (_gameOver) {
        throw IllegalMoveException("game is already over");
    }
    if (_turn == PLAYER_UNDEF) {
        throw IllegalMoveException("turn is undefined");
    }
    if (m.isEmpty()) {
        throw IllegalMoveException("move is empty");
    }

    if (_searchMode) {
        OctiPlayer mover = _turn;
        applyMoveOps(m.getOps());
        check4PlayerElimination();
        ++_searchPlyOffset;
        updateGameOverAndWinner();
        if (!_gameOver) {
            advanceTurn();
            if (_mode == GAME_MODE_2 || _mode == GAME_MODE_OFK) {
                if (!hasAnyLegalMove()) {
                    _gameOver = true;
                    _winner = (_turn == PLAYER_A) ? PLAYER_B : PLAYER_A;
                }
            } else if (_mode == GAME_MODE_4) {
                int seatsChecked = 0;
                while (seatsChecked < 4 && !_players[_turn].eliminated && !hasAnyLegalMove()) {
                    advanceTurn();
                    seatsChecked++;
                }
                if (seatsChecked >= 4) {
                    _gameOver = true;
                    _winner = (mover == PLAYER_A || mover == PLAYER_C) ? PLAYER_A : PLAYER_B;
                }
            }
        }
        return;
    }

    OctiGameSnapshot snapBefore = captureSnapshot();

    try {
        applyMoveOps(m.getOps());
        check4PlayerElimination();
    } catch (...) {
        restoreSnapshot(snapBefore);
        throw;
    }

    if (!isRedo) {
        snapBefore.lastMove = m;
        snapBefore.lastPlayer = snapBefore.turn;
        _undoStack.push_back(snapBefore);
        _redoStack.clear();
        _history.push_back({snapBefore.turn, m});
        _plyMetadata.push_back(OctiPlyMetadata{});
    }

    OctiPlayer mover = snapBefore.turn;
    updateGameOverAndWinner();
    if (!_gameOver) {
        advanceTurn();
        if (_mode == GAME_MODE_2 || _mode == GAME_MODE_OFK) {
            if (!hasAnyLegalMove()) {
                _gameOver = true;
                _winner = (_turn == PLAYER_A) ? PLAYER_B : PLAYER_A;
            }
        } else if (_mode == GAME_MODE_4) {
            int seatsChecked = 0;
            while (seatsChecked < 4 && !_players[_turn].eliminated && !hasAnyLegalMove()) {
                advanceTurn();
                seatsChecked++;
            }
            if (seatsChecked >= 4) {
                _gameOver = true;
                _winner = (mover == PLAYER_A || mover == PLAYER_C) ? PLAYER_A : PLAYER_B;
            }
        }
    }

    if (_board) {
        _board->notifyBoardTurnUpdate(&m);
    }
    notifyGameUpdated();
    notifyUndoRedoStatusChanged();
}

void OctiGame::applyMoveOps(const std::vector<OctiMoveOp> &ops, bool allowPreludeOnly)
{
    if (ops.empty()) {
        throw IllegalMoveException("no operations in move");
    }

    // Separate bonus ops and regular ops
    std::vector<OctiMoveOp> bonusOps;
    std::vector<OctiMoveOp> regularOps;
    bool seenRegular = false;
    for (const auto &op : ops) {
        if (op.isSuperProngBonusMove) {
            if (seenRegular) {
                throw IllegalMoveException(
                    "superprong bonus move cannot appear after regular move");
            }
            bonusOps.push_back(op);
        } else {
            seenRegular = true;
            regularOps.push_back(op);
        }
    }

    if (_mode == GAME_MODE_OFK && regularOps.size() > 1) {
        throw IllegalMoveException("composite moves not allowed in OCTI for Kids");
    }

    if (!bonusOps.empty()) {
        if (!_useSuperProngs) {
            throw IllegalMoveException("superprongs not enabled");
        }
        if (bonusOps.size() > 2) {
            throw IllegalMoveException("exceeded maximum superprong bonus moves");
        }

        // Execute bonus ops
        std::set<std::pair<int, int>> bonusCaptures;
        const OctiPodType turnPodType = playerToPodType(_turn);
        int p1EndX = -1;
        int p1EndY = -1;
        std::uint32_t p1Cfg = 0;
        OctiDir p1SuperDir = DIR_UNDEF;
        int sameCfgCountAfterP1 = 0;

        for (std::size_t bIdx = 0; bIdx < bonusOps.size(); bIdx++) {
            const auto &bOp = bonusOps[bIdx];
            if (bOp.hops.empty()) {
                throw IllegalMoveException("superprong bonus move must contain hops");
            }
            if (bOp.addPod || bOp.addProngDir != DIR_UNDEF || bOp.rmProngDir != DIR_UNDEF) {
                throw IllegalMoveException("superprong bonus move must be a hop chain");
            }
            if (!_board->isValidCoord(bOp.x0, bOp.y0)) {
                throw IllegalMoveException("superprong bonus starting square out of bounds");
            }
            OctiSquare &sq = _board->squareAt(bOp.x0, bOp.y0);
            int podIdx = -1;
            for (int i = 0; i < sq.getNumPods(); i++) {
                if (sq.getPodAt(i).getType() == turnPodType &&
                    (sq.getPodAt(i).rawConfig() & 0xFFFFu) == (bOp.pod.rawConfig() & 0xFFFFu)) {
                    podIdx = i;
                    break;
                }
            }
            if (podIdx == -1) {
                throw IllegalMoveException("pod for superprong bonus move not found");
            }
            OctiPod pod = sq.getPodAt(podIdx);
            const std::uint32_t opCfg = pod.rawConfig() & 0xFFFFu;
            sq.removePod(pod);

            int cx = bOp.x0;
            int cy = bOp.y0;
            OctiDir opSuperDir = DIR_UNDEF;
            std::set<std::pair<int, int>> jumpedSquares;

            for (std::size_t hIdx = 0; hIdx < bOp.hops.size(); hIdx++) {
                const auto &hop = bOp.hops[hIdx];
                if (!_board->isValidCoord(hop.x, hop.y)) {
                    throw IllegalMoveException("superprong bonus hop coordinate out of bounds");
                }
                int dx = 0, dy = 0;
                if (!findStepDelta(cx, cy, hop.x, hop.y, _useEdgelessBoard,
                                   _board->getBoardWidth(), _board->getBoardHeight(),
                                   dx, dy)) {
                    throw IllegalMoveException("invalid displacement in bonus move");
                }
                bool isDist1 = (std::max(std::abs(dx), std::abs(dy)) == 1 &&
                                (std::abs(dx) == 1 || std::abs(dy) == 1));
                if (isDist1) {
                    if (bOp.hops.size() != 1 || hop.capture) {
                        throw IllegalMoveException("invalid distance-1 bonus hop");
                    }
                    OctiDir dir = deltaToDir(dx, dy);
                    if (dir == DIR_UNDEF || pod.prongAt(dir) != SUPER_PRONG) {
                        throw IllegalMoveException("bonus move must follow superprong direction");
                    }
                    opSuperDir = dir;
                    if (bIdx == 1 &&
                        bOp.x0 == p1EndX &&
                        bOp.y0 == p1EndY &&
                        opCfg == p1Cfg &&
                        opSuperDir == p1SuperDir &&
                        sameCfgCountAfterP1 < 2) {
                        throw IllegalMoveException(
                            "superprong socket already used in bonus prelude");
                    }
                    OctiSquare &destSq = _board->squareAt(hop.x, hop.y);
                    if (destSq.hasEnemyOf(_turn)) {
                        throw IllegalMoveException("cannot move onto enemy square in bonus move");
                    }
                    cx = hop.x;
                    cy = hop.y;
                } else {
                    bool validJump = (std::abs(dx) == 0 || std::abs(dx) == 2) &&
                                     (std::abs(dy) == 0 || std::abs(dy) == 2) &&
                                     (std::abs(dx) == 2 || std::abs(dy) == 2) &&
                                     (std::abs(dx) == 0 || std::abs(dy) == 0 || std::abs(dx) == std::abs(dy));
                    if (!validJump) {
                        throw IllegalMoveException("invalid jump geometry in bonus move");
                    }
                    OctiDir dir = deltaToDir(dx / 2, dy / 2);
                    if (dir == DIR_UNDEF || pod.prongAt(dir) != SUPER_PRONG) {
                        throw IllegalMoveException("bonus jump must follow superprong direction");
                    }
                    if (hIdx == 0) {
                        opSuperDir = dir;
                        if (bIdx == 1 &&
                            bOp.x0 == p1EndX &&
                            bOp.y0 == p1EndY &&
                            opCfg == p1Cfg &&
                            opSuperDir == p1SuperDir &&
                            sameCfgCountAfterP1 < 2) {
                            throw IllegalMoveException(
                                "superprong socket already used in bonus prelude");
                        }
                    } else if (dir != opSuperDir) {
                        throw IllegalMoveException(
                            "bonus jump must follow single superprong direction");
                    }
                    int jx = _useEdgelessBoard ? (((cx + dx / 2) % _board->getBoardWidth() + _board->getBoardWidth()) % _board->getBoardWidth()) : (cx + dx / 2);
                    int jy = _useEdgelessBoard ? (((cy + dy / 2) % _board->getBoardHeight() + _board->getBoardHeight()) % _board->getBoardHeight()) : (cy + dy / 2);
                    std::pair<int, int> jCoord = {jx, jy};
                    if (jumpedSquares.count(jCoord) > 0) {
                        throw IllegalMoveException("cannot jump same square twice in bonus move");
                    }
                    jumpedSquares.insert(jCoord);

                    OctiSquare &jumpedSq = _board->squareAt(jx, jy);
                    if (jumpedSq.isEmpty()) {
                        throw IllegalMoveException("cannot jump empty square in bonus move");
                    }
                    if (hop.capture) {
                        bonusCaptures.insert(jCoord);
                    }
                    OctiSquare &landingSq = _board->squareAt(hop.x, hop.y);
                    if (!landingSq.isEmpty()) {
                        throw IllegalMoveException("landing square must be empty in bonus move");
                    }
                    cx = hop.x;
                    cy = hop.y;
                }
            }
            _board->squareAt(cx, cy).addPod(pod);

            if (bIdx == 0) {
                p1EndX = cx;
                p1EndY = cy;
                p1Cfg = opCfg;
                p1SuperDir = opSuperDir;
                sameCfgCountAfterP1 = 0;
                const OctiSquare &landSq = _board->squareAt(cx, cy);
                for (int i = 0; i < landSq.getNumPods(); i++) {
                    const OctiPod &lp = landSq.getPodAt(i);
                    if (lp.getType() == turnPodType &&
                        (lp.rawConfig() & 0xFFFFu) == p1Cfg) {
                        sameCfgCountAfterP1++;
                    }
                }
            }
        }

        // Process bonus captures
        for (const auto &coord : bonusCaptures) {
            OctiSquare &sq = _board->squareAt(coord.first, coord.second);
            for (int i = 0; i < sq.getNumPods(); i++) {
                const OctiPod &sqPod = sq.getPodAt(i);
                OctiPlayer owner = podTypeToPlayer(sqPod.getType());
                _players[_turn].freeProngs += sqPod.getNumNormalProngs();
                _players[_turn].freeSuperProngs += sqPod.getNumSuperProngs();
                _players[owner].capturedPods++;
            }
            sq.clear();
        }
    }

    if (regularOps.empty()) {
        if (allowPreludeOnly && !bonusOps.empty()) {
            return;
        }
        throw IllegalMoveException("move must include a regular turn action");
    }

    // Option V: Pod placement / liberation
    if (regularOps[0].addPod) {
        if (regularOps.size() != 1) {
            throw IllegalMoveException("pod placement cannot be combined with other actions");
        }
        if (_mode == GAME_MODE_OFK) {
            throw IllegalMoveException("pod placement forbidden in OCTI for Kids");
        }
        const auto &op = regularOps[0];
        if (!op.hops.empty() || op.addProngDir != DIR_UNDEF || op.rmProngDir != DIR_UNDEF) {
            throw IllegalMoveException("invalid pod placement op structure");
        }
        if (!_board->isValidCoord(op.x0, op.y0)) {
            throw IllegalMoveException("pod placement square out of bounds");
        }
        OctiSquare &sq = _board->squareAt(op.x0, op.y0);
        if (sq.getType() == NEUTRAL_SQUARE) {
            throw IllegalMoveException("pod placement only allowed on OCTI squares");
        }
        OctiSquareType st = sq.getType();
        bool isFriendlyHome = false;
        if (_board->getNumPlayers() == 2) {
            isFriendlyHome = (_turn == PLAYER_A && st == A_HOME_SQUARE) ||
                             (_turn == PLAYER_B && st == B_HOME_SQUARE);
        } else if (_board->getNumPlayers() == 4) {
            isFriendlyHome = (st == static_cast<OctiSquareType>(_turn));
        }

        if (isFriendlyHome) {
            if (sq.hasEnemyOf(_turn)) {
                throw IllegalMoveException("pod placement square occupied by enemy pod");
            }
        } else {
            if (!sq.isOccupiedBy(_turn)) {
                throw IllegalMoveException("Cannot place pod on unoccupied or partner-only non-home square");
            }
        }

        if (_mode == GAME_MODE_2) {
            bool isFriendlyBase = (sq.getType() == (_turn == PLAYER_A ? A_HOME_SQUARE : B_HOME_SQUARE));
            if (isFriendlyBase) {
                if (op.liberate) {
                    if (!_board->canLiberate(_turn) || _players[_turn].capturedPods <= 0) {
                        throw IllegalMoveException("liberation condition not met on friendly base");
                    }
                    if (!sq.isOccupiedBy(_turn)) {
                        throw IllegalMoveException("liberated pod must be stacked on an occupied OCTI square");
                    }
                    _players[_turn].capturedPods--;
                    sq.addPod(OctiPod(playerToPodType(_turn)));
                } else {
                    if (_players[_turn].freePods > 0) {
                        _players[_turn].freePods--;
                        sq.addPod(OctiPod(playerToPodType(_turn)));
                    } else if (_board->canLiberate(_turn) && _players[_turn].capturedPods > 0 && sq.isOccupiedBy(_turn)) {
                        _players[_turn].capturedPods--;
                        sq.addPod(OctiPod(playerToPodType(_turn)));
                    } else {
                        throw IllegalMoveException("no free or capturable pods available to place");
                    }
                }
            } else {
                // Enemy base
                if (op.liberate) {
                    if (_players[_turn].capturedPods <= 0) {
                        throw IllegalMoveException("no captured pods to liberate");
                    }
                    _players[_turn].capturedPods--;
                    sq.addPod(OctiPod(playerToPodType(_turn)));
                } else {
                    if (_players[_turn].freePods > 0) {
                        _players[_turn].freePods--;
                        sq.addPod(OctiPod(playerToPodType(_turn)));
                    } else if (_players[_turn].capturedPods > 0) {
                        _players[_turn].capturedPods--;
                        sq.addPod(OctiPod(playerToPodType(_turn)));
                    } else {
                        throw IllegalMoveException("no free or capturable pods available to place");
                    }
                }
            }
        } else if (_mode == GAME_MODE_4) {
            OctiSquareType homeType = (_turn == PLAYER_A ? A_HOME_SQUARE :
                                       _turn == PLAYER_B ? B_HOME_SQUARE :
                                       _turn == PLAYER_C ? C_HOME_SQUARE : D_HOME_SQUARE);
            OctiPlayer partner = (_turn == PLAYER_A ? PLAYER_C :
                                  _turn == PLAYER_C ? PLAYER_A :
                                  _turn == PLAYER_B ? PLAYER_D : PLAYER_B);
            OctiSquareType partnerHome = (partner == PLAYER_A ? A_HOME_SQUARE :
                                          partner == PLAYER_B ? B_HOME_SQUARE :
                                          partner == PLAYER_C ? C_HOME_SQUARE : D_HOME_SQUARE);

            if (sq.getType() == homeType) {
                if (op.liberate) {
                    if (!sq.isOccupiedBy(_turn) || !_board->canLiberate(_turn) || _players[_turn].capturedPods <= 0) {
                        throw IllegalMoveException("cannot liberate pod on own base without clearance and captured pods");
                    }
                    _players[_turn].capturedPods--;
                    sq.addPod(OctiPod(playerToPodType(_turn)));
                } else {
                    if (_players[_turn].freePods > 0) {
                        _players[_turn].freePods--;
                        sq.addPod(OctiPod(playerToPodType(_turn)));
                    } else if (sq.isOccupiedBy(_turn) && _board->canLiberate(_turn) && _players[_turn].capturedPods > 0) {
                        _players[_turn].capturedPods--;
                        sq.addPod(OctiPod(playerToPodType(_turn)));
                    } else {
                        throw IllegalMoveException("cannot place reserve pod on own base: no pods available");
                    }
                }
            } else if (sq.getType() == partnerHome) {
                if (_players[partner].eliminated) {
                    if (_players[partner].capturedPods <= 0) {
                        throw IllegalMoveException("cannot revive partner: partner has no captured pods");
                    }
                    _players[partner].eliminated = false;
                    _players[partner].capturedPods--;
                    sq.addPod(OctiPod(playerToPodType(partner)));
                } else {
                    if (!op.liberate || !_board->canLiberate(_turn) || _players[_turn].capturedPods <= 0) {
                        throw IllegalMoveException("cannot place pod on partner base unless liberating own captured pod with liberation clearance");
                    }
                    _players[_turn].capturedPods--;
                    sq.addPod(OctiPod(playerToPodType(_turn)));
                }
            } else if (sq.getType() == NEUTRAL_OCTI_SQUARE) {
                if (!op.liberate) {
                    throw IllegalMoveException("cannot place reserve pod on neutral OCTI square");
                }
                if (_players[_turn].capturedPods <= 0) {
                    throw IllegalMoveException("no captured pods to liberate on neutral OCTI square");
                }
                _players[_turn].capturedPods--;
                sq.addPod(OctiPod(playerToPodType(_turn)));
            } else {
                // Enemy base
                if (op.liberate) {
                    if (_players[_turn].capturedPods <= 0) {
                        throw IllegalMoveException("no captured pods to liberate");
                    }
                    _players[_turn].capturedPods--;
                    sq.addPod(OctiPod(playerToPodType(_turn)));
                } else {
                    if (_players[_turn].freePods > 0) {
                        _players[_turn].freePods--;
                        sq.addPod(OctiPod(playerToPodType(_turn)));
                    } else if (_players[_turn].capturedPods > 0) {
                        _players[_turn].capturedPods--;
                        sq.addPod(OctiPod(playerToPodType(_turn)));
                    } else {
                        throw IllegalMoveException("no free or capturable pods available to place");
                    }
                }
            }
        }
        return;
    }

    // Option I: Add Prong
    if (regularOps[0].addProngDir != DIR_UNDEF && regularOps[0].rmProngDir == DIR_UNDEF) {
        if (regularOps.size() != 1) {
            throw IllegalMoveException("prong addition cannot be combined with other actions");
        }
        const auto &op = regularOps[0];
        if (!op.hops.empty() || op.addProngDir < DIR_N || op.addProngDir >= DIR_MAX) {
            throw IllegalMoveException("invalid prong add op structure");
        }
        if (!_board->isValidCoord(op.x0, op.y0)) {
            throw IllegalMoveException("prong addition square out of bounds");
        }
        OctiSquare &sq = _board->squareAt(op.x0, op.y0);
        int podIdx = -1;
        for (int i = 0; i < sq.getNumPods(); i++) {
            if (sq.getPodAt(i).getType() == playerToPodType(_turn) &&
                (sq.getPodAt(i).rawConfig() & 0xFFFFu) == (op.pod.rawConfig() & 0xFFFFu)) {
                podIdx = i;
                break;
            }
        }
        if (podIdx == -1) {
            throw IllegalMoveException("matching pod for prong addition not found on square");
        }
        if (sq.podAt(podIdx).prongAt(op.addProngDir) != NO_PRONG) {
            throw IllegalMoveException("slot already has a prong");
        }
        if (op.superProng) {
            if (!_useSuperProngs || _players[_turn].freeSuperProngs <= 0) {
                throw IllegalMoveException("no free superprongs available");
            }
            _players[_turn].freeSuperProngs--;
            sq.podAt(podIdx).addProng(op.addProngDir, true);
        } else {
            if (_players[_turn].freeProngs <= 0) {
                throw IllegalMoveException("no free normal prongs available");
            }
            _players[_turn].freeProngs--;
            sq.podAt(podIdx).addProng(op.addProngDir, false);
        }
        return;
    }

    // Option II: Reposition Prong
    if (regularOps[0].addProngDir != DIR_UNDEF && regularOps[0].rmProngDir != DIR_UNDEF) {
        if (regularOps.size() != 1) {
            throw IllegalMoveException("prong repositioning cannot be combined with other actions");
        }
        if (_mode == GAME_MODE_OFK) {
            throw IllegalMoveException("prong repositioning forbidden in OCTI for Kids");
        }
        const auto &op = regularOps[0];
        if (op.superProng) {
            throw IllegalMoveException("cannot add superprong during repositioning");
        }
        if (!op.hops.empty() || op.addProngDir == op.rmProngDir ||
            op.addProngDir < DIR_N || op.addProngDir >= DIR_MAX ||
            op.rmProngDir < DIR_N || op.rmProngDir >= DIR_MAX) {
            throw IllegalMoveException("invalid reposition op structure");
        }
        if (!_board->isValidCoord(op.x0, op.y0)) {
            throw IllegalMoveException("prong repositioning square out of bounds");
        }
        OctiSquare &sq = _board->squareAt(op.x0, op.y0);
        int podIdx = -1;
        for (int i = 0; i < sq.getNumPods(); i++) {
            if (sq.getPodAt(i).getType() == playerToPodType(_turn) &&
                (sq.getPodAt(i).rawConfig() & 0xFFFFu) == (op.pod.rawConfig() & 0xFFFFu)) {
                podIdx = i;
                break;
            }
        }
        if (podIdx == -1) {
            throw IllegalMoveException("matching pod for prong repositioning not found on square");
        }
        if (sq.podAt(podIdx).prongAt(op.rmProngDir) != NORMAL_PRONG) {
            throw IllegalMoveException("can only reposition normal prongs");
        }
        if (sq.podAt(podIdx).prongAt(op.addProngDir) != NO_PRONG) {
            throw IllegalMoveException("target prong slot must be empty");
        }
        sq.podAt(podIdx).removeProng(op.rmProngDir);
        sq.podAt(podIdx).addProng(op.addProngDir, false);
        return;
    }

    // Options III & IV: Movement and Jumps (single or composite stack move)
    int sx = regularOps[0].x0;
    int sy = regularOps[0].y0;
    if (!_board->isValidCoord(sx, sy)) {
        throw IllegalMoveException("starting square out of bounds");
    }

    for (const auto &op : regularOps) {
        if (op.x0 != sx || op.y0 != sy) {
            throw IllegalMoveException("composite move operations must share the same starting square");
        }
        if (op.addPod || op.addProngDir != DIR_UNDEF || op.rmProngDir != DIR_UNDEF) {
            throw IllegalMoveException("movement operation cannot mix pod or prong edits");
        }
        if (op.hops.empty()) {
            throw IllegalMoveException("movement operation must contain at least one hop");
        }
        for (const auto &hop : op.hops) {
            if (!_board->isValidCoord(hop.x, hop.y)) {
                throw IllegalMoveException("hop coordinate out of bounds");
            }
        }
    }

    OctiSquare &startSq = _board->squareAt(sx, sy);
    std::vector<OctiPod> ghostPods = startSq.pods();
    std::vector<bool> claimed(ghostPods.size(), false);
    std::vector<OctiPod> movingPods;

    for (const auto &op : regularOps) {
        int foundIdx = -1;
        for (std::size_t i = 0; i < ghostPods.size(); i++) {
            if (!claimed[i] &&
                ghostPods[i].getType() == playerToPodType(_turn) &&
                (ghostPods[i].rawConfig() & 0xFFFFu) == (op.pod.rawConfig() & 0xFFFFu)) {
                foundIdx = static_cast<int>(i);
                break;
            }
        }
        if (foundIdx == -1) {
            throw IllegalMoveException("matching pod not found in starting stack");
        }
        claimed[static_cast<std::size_t>(foundIdx)] = true;
        movingPods.push_back(ghostPods[static_cast<std::size_t>(foundIdx)]);
    }

    // Remove moving pods from start square
    startSq.clear();
    for (std::size_t i = 0; i < ghostPods.size(); i++) {
        if (!claimed[i]) {
            startSq.addPod(ghostPods[i]);
        }
    }

    struct LandedPod {
        OctiPod pod;
        int finalX = -1;
        int finalY = -1;
    };

    std::vector<LandedPod> landedPods;
    std::vector<std::pair<int, int>> capturedSquaresEndOfTurn;

    for (std::size_t k = 0; k < regularOps.size(); k++) {
        const auto &op = regularOps[k];
        const auto &pod = movingPods[k];
        int cx = sx;
        int cy = sy;
        std::vector<std::pair<int, int>> jumpedSquaresThisPod;

        int dx = 0, dy = 0;
        if (!findStepDelta(cx, cy, op.hops[0].x, op.hops[0].y, _useEdgelessBoard,
                           _board->getBoardWidth(), _board->getBoardHeight(),
                           dx, dy)) {
            throw IllegalMoveException("invalid displacement for first hop");
        }

        bool isDist1 = (std::max(std::abs(dx), std::abs(dy)) == 1 &&
                        (std::abs(dx) == 1 || std::abs(dy) == 1));

        if (isDist1) {
            if (op.hops.size() != 1) {
                throw IllegalMoveException("chaining distance-1 moves is forbidden");
            }
            if (op.hops[0].capture) {
                throw IllegalMoveException("capture not allowed on distance-1 move");
            }
            OctiDir dir = deltaToDir(dx, dy);
            if (dir == DIR_UNDEF || pod.prongAt(dir) == NO_PRONG) {
                throw IllegalMoveException("pod lacks prong in distance-1 move direction");
            }
            OctiSquare &destSq = _board->squareAt(op.hops[0].x, op.hops[0].y);
            if (_mode == GAME_MODE_OFK && !destSq.isEmpty()) {
                throw IllegalMoveException("pod stacking forbidden in OCTI for Kids");
            }
            if (_mode != GAME_MODE_OFK && destSq.hasEnemyOf(_turn)) {
                throw IllegalMoveException("cannot move onto enemy square");
            }
            landedPods.push_back({pod, op.hops[0].x, op.hops[0].y});
        } else {
            // Distance-2 jumps
            for (const auto &hop : op.hops) {
                int jdx = 0, jdy = 0;
                if (!findStepDelta(cx, cy, hop.x, hop.y, _useEdgelessBoard,
                                   _board->getBoardWidth(), _board->getBoardHeight(),
                                   jdx, jdy)) {
                    throw IllegalMoveException("invalid displacement for jump hop");
                }
                bool validJump = (std::abs(jdx) == 0 || std::abs(jdx) == 2) &&
                                 (std::abs(jdy) == 0 || std::abs(jdy) == 2) &&
                                 (std::abs(jdx) == 2 || std::abs(jdy) == 2) &&
                                 (std::abs(jdx) == 0 || std::abs(jdy) == 0 || std::abs(jdx) == std::abs(jdy));
                if (!validJump) {
                    throw IllegalMoveException("invalid jump geometry");
                }
                OctiDir dir = deltaToDir(jdx / 2, jdy / 2);
                if (dir == DIR_UNDEF || pod.prongAt(dir) == NO_PRONG) {
                    throw IllegalMoveException("pod lacks prong in jump direction");
                }
                int jx = _useEdgelessBoard ? (((cx + jdx / 2) % _board->getBoardWidth() + _board->getBoardWidth()) % _board->getBoardWidth()) : (cx + jdx / 2);
                int jy = _useEdgelessBoard ? (((cy + jdy / 2) % _board->getBoardHeight() + _board->getBoardHeight()) % _board->getBoardHeight()) : (cy + jdy / 2);
                std::pair<int, int> jCoord = {jx, jy};

                if (std::find(jumpedSquaresThisPod.begin(), jumpedSquaresThisPod.end(), jCoord) != jumpedSquaresThisPod.end()) {
                    throw IllegalMoveException("cannot jump same square twice in single pod chain");
                }
                jumpedSquaresThisPod.push_back(jCoord);

                OctiSquare &jumpedSq = _board->squareAt(jx, jy);
                if (isEmptyForJump(jumpedSq, jx, jy, sx, sy, ghostPods)) {
                    throw IllegalMoveException("cannot jump square without stationary pods");
                }

                if (hop.capture) {
                    if (std::find(capturedSquaresEndOfTurn.begin(), capturedSquaresEndOfTurn.end(), jCoord) == capturedSquaresEndOfTurn.end()) {
                        capturedSquaresEndOfTurn.push_back(jCoord);
                    }
                }

                OctiSquare &landingSq = _board->squareAt(hop.x, hop.y);
                if (!isEmptyForJump(landingSq, hop.x, hop.y, sx, sy, ghostPods)) {
                    throw IllegalMoveException("landing square must be empty of stationary pods");
                }

                cx = hop.x;
                cy = hop.y;
            }
            landedPods.push_back({pod, cx, cy});
        }
    }

    // Place all landed pods on destination squares
    for (const auto &lp : landedPods) {
        OctiSquare &destSq = _board->squareAt(lp.finalX, lp.finalY);
        if (_mode == GAME_MODE_OFK && !destSq.isEmpty()) {
            throw IllegalMoveException("pod stacking forbidden in OCTI for Kids");
        }
        destSq.addPod(lp.pod);
    }

    // Process end-of-turn captured pods
    for (const auto &coord : capturedSquaresEndOfTurn) {
        OctiSquare &sq = _board->squareAt(coord.first, coord.second);
        for (int pIdx = 0; pIdx < sq.getNumPods(); pIdx++) {
            const OctiPod &sqPod = sq.getPodAt(pIdx);
            OctiPlayer owner = podTypeToPlayer(sqPod.getType());
            _players[_turn].freeProngs += sqPod.getNumNormalProngs();
            _players[_turn].freeSuperProngs += sqPod.getNumSuperProngs();
            _players[owner].capturedPods++;
        }
        sq.clear();
    }
}

void OctiGame::check4PlayerElimination()
{
    if (_mode != GAME_MODE_4) {
        return;
    }

    const std::pair<int, int> homeCoords[5] = {
        {-1, -1}, {2, 2}, {2, 6}, {6, 6}, {6, 2}
    };
    for (int p = 1; p <= 4; p++) {
        OctiPlayer player = static_cast<OctiPlayer>(p);
        if (!_players[p].eliminated) {
            const OctiSquare &hsq = _board->squareAt(homeCoords[p].first, homeCoords[p].second);
            if (hsq.isCaptured()) {
                _players[p].eliminated = true;
                int bw = _board->getBoardWidth();
                int bh = _board->getBoardHeight();
                for (int x = 0; x < bw; x++) {
                    for (int y = 0; y < bh; y++) {
                        OctiSquare &s = _board->squareAt(x, y);
                        std::vector<OctiPod> keep;
                        for (int pi = 0; pi < s.getNumPods(); pi++) {
                            const OctiPod &pod = s.getPodAt(pi);
                            if (podTypeToPlayer(pod.getType()) == player) {
                                _players[_turn].freeProngs += pod.getNumNormalProngs();
                                _players[_turn].freeSuperProngs += pod.getNumSuperProngs();
                                _players[p].capturedPods++;
                            } else {
                                keep.push_back(pod);
                            }
                        }
                        s.clear();
                        for (const auto &kp : keep) {
                            s.addPod(kp);
                        }
                    }
                }
            }
        }
    }
}

void OctiGame::updateGameOverAndWinner()
{
    if (_mode == GAME_MODE_2) {
        int aOccupiesB = 0;
        int bOccupiesA = 0;

        if (_board->squareAt(2, 6).isCapturedBy(PLAYER_A)) aOccupiesB++;
        if (_board->squareAt(4, 6).isCapturedBy(PLAYER_A)) aOccupiesB++;
        if (_board->squareAt(6, 6).isCapturedBy(PLAYER_A)) aOccupiesB++;

        if (_board->squareAt(2, 2).isCapturedBy(PLAYER_B)) bOccupiesA++;
        if (_board->squareAt(4, 2).isCapturedBy(PLAYER_B)) bOccupiesA++;
        if (_board->squareAt(6, 2).isCapturedBy(PLAYER_B)) bOccupiesA++;

        int neededBases = (_bases == GAME_BASES_1) ? 1 : 3;
        if (aOccupiesB >= neededBases) {
            _gameOver = true;
            _winner = PLAYER_A;
            return;
        }
        if (bOccupiesA >= neededBases) {
            _gameOver = true;
            _winner = PLAYER_B;
            return;
        }

        // Pod count check
        int podsA = 0, podsB = 0;
        int bw = _board->getBoardWidth();
        int bh = _board->getBoardHeight();
        for (int x = 0; x < bw; x++) {
            for (int y = 0; y < bh; y++) {
                const OctiSquare &sq = _board->squareAt(x, y);
                for (int i = 0; i < sq.getNumPods(); i++) {
                    if (sq.getPodAt(i).getType() == A_POD) podsA++;
                    else if (sq.getPodAt(i).getType() == B_POD) podsB++;
                }
            }
        }

        if (podsB == 0 && _players[PLAYER_B].freePods == 0) {
            _gameOver = true;
            _winner = PLAYER_A;
            return;
        }
        if (podsA == 0 && _players[PLAYER_A].freePods == 0) {
            _gameOver = true;
            _winner = PLAYER_B;
            return;
        }
    } else if (_mode == GAME_MODE_OFK) {
        // Base occupation (any 1 of 4)
        for (int x = 1; x <= 4; x++) {
            if (_board->squareAt(x, 5).isCapturedBy(PLAYER_A)) {
                _gameOver = true;
                _winner = PLAYER_A;
                return;
            }
            if (_board->squareAt(x, 1).isCapturedBy(PLAYER_B)) {
                _gameOver = true;
                _winner = PLAYER_B;
                return;
            }
        }

        // Total pod capture check
        int podsA = 0, podsB = 0;
        for (int x = 0; x < _board->getBoardWidth(); x++) {
            for (int y = 0; y < _board->getBoardHeight(); y++) {
                const OctiSquare &sq = _board->squareAt(x, y);
                for (int i = 0; i < sq.getNumPods(); i++) {
                    if (sq.getPodAt(i).getType() == A_POD) podsA++;
                    else if (sq.getPodAt(i).getType() == B_POD) podsB++;
                }
            }
        }

        if (podsB == 0 && _players[PLAYER_B].freePods == 0) {
            _gameOver = true;
            _winner = PLAYER_A;
            return;
        }
        if (podsA == 0 && _players[PLAYER_A].freePods == 0) {
            _gameOver = true;
            _winner = PLAYER_B;
            return;
        }
    } else if (_mode == GAME_MODE_4) {
        // Team A/C wins if B's home (2,6) and D's home (6,2) captured by Team A/C
        bool acBases = _board->squareAt(2, 6).isCapturedByTeamOf(PLAYER_A) &&
                       _board->squareAt(6, 2).isCapturedByTeamOf(PLAYER_A);
        if (acBases) {
            _gameOver = true;
            _winner = PLAYER_A;
            return;
        }

        // Team B/D wins if A's home (2,2) and C's home (6,6) captured by Team B/D
        bool bdBases = _board->squareAt(2, 2).isCapturedByTeamOf(PLAYER_B) &&
                       _board->squareAt(6, 6).isCapturedByTeamOf(PLAYER_B);
        if (bdBases) {
            _gameOver = true;
            _winner = PLAYER_B;
            return;
        }

        // Pods count
        int pods[5] = {0};
        int bw4 = _board->getBoardWidth();
        int bh4 = _board->getBoardHeight();
        for (int x = 0; x < bw4; x++) {
            for (int y = 0; y < bh4; y++) {
                const OctiSquare &sq = _board->squareAt(x, y);
                for (int i = 0; i < sq.getNumPods(); i++) {
                    OctiPlayer p = podTypeToPlayer(sq.getPodAt(i).getType());
                    if (p >= 1 && p <= 4) pods[p]++;
                }
            }
        }

        bool bdElim = (_players[PLAYER_B].eliminated || (pods[PLAYER_B] == 0 && _players[PLAYER_B].freePods == 0)) &&
                      (_players[PLAYER_D].eliminated || (pods[PLAYER_D] == 0 && _players[PLAYER_D].freePods == 0));
        if (bdElim) {
            _gameOver = true;
            _winner = PLAYER_A;
            return;
        }

        bool acElim = (_players[PLAYER_A].eliminated || (pods[PLAYER_A] == 0 && _players[PLAYER_A].freePods == 0)) &&
                      (_players[PLAYER_C].eliminated || (pods[PLAYER_C] == 0 && _players[PLAYER_C].freePods == 0));
        if (acElim) {
            _gameOver = true;
            _winner = PLAYER_B;
            return;
        }
    }
}

void OctiGame::advanceTurn()
{
    if (_mode == GAME_MODE_2 || _mode == GAME_MODE_OFK) {
        _turn = (_turn == PLAYER_A) ? PLAYER_B : PLAYER_A;
    } else if (_mode == GAME_MODE_4) {
        int current = static_cast<int>(_turn);
        for (int step = 1; step <= 4; step++) {
            int nextP = (current - 1 + step) % 4 + 1;
            if (!_players[nextP].eliminated) {
                _turn = static_cast<OctiPlayer>(nextP);
                break;
            }
        }
    }
}

void OctiGame::undo()
{
    if (_undoStack.empty()) {
        return;
    }

    OctiGameSnapshot currentSnap = captureSnapshot();
    if (!_history.empty()) {
        currentSnap.lastPlayer = _history.back().first;
        currentSnap.lastMove = _history.back().second;
        _history.pop_back();
    }
    if (!_plyMetadata.empty()) {
        currentSnap.lastPlyMeta = _plyMetadata.back();
        _plyMetadata.pop_back();
    }
    OctiGameSnapshot prevSnap = _undoStack.back();
    _undoStack.pop_back();
    _redoStack.push_back(currentSnap);

    restoreSnapshot(prevSnap);

    if (_board) {
        _board->notifyBoardTurnUpdate(nullptr);
    }
    notifyGameUpdated();
    notifyUndoRedoStatusChanged();
}

void OctiGame::redo()
{
    if (_redoStack.empty()) {
        return;
    }

    OctiGameSnapshot targetSnap = _redoStack.back();
    _redoStack.pop_back();
    OctiGameSnapshot currentSnap = captureSnapshot();
    _undoStack.push_back(currentSnap);

    if (targetSnap.lastPlayer != PLAYER_UNDEF && !targetSnap.lastMove.isEmpty()) {
        _history.push_back({targetSnap.lastPlayer, targetSnap.lastMove});
        _plyMetadata.push_back(targetSnap.lastPlyMeta);
    }

    restoreSnapshot(targetSnap);

    if (_board) {
        _board->notifyBoardTurnUpdate(nullptr);
    }
    notifyGameUpdated();
    notifyUndoRedoStatusChanged();
}

void OctiGame::prepareAddPod(int x, int y, bool liberate)
{
    if (!_hasPreparedMove) {
        _prepSnapshot = captureSnapshot();
        _prepOps.clear();
        _hasPreparedMove = true;
    }

    OctiMoveOp op;
    op.x0 = x;
    op.y0 = y;
    op.addPod = true;
    op.liberate = liberate;
    _prepOps.push_back(op);
}

void OctiGame::prepareAddProng(int x, int y, OctiPod pod, OctiDir dir, bool superProng)
{
    if (!_hasPreparedMove) {
        _prepSnapshot = captureSnapshot();
        _prepOps.clear();
        _hasPreparedMove = true;
    }

    OctiMoveOp op;
    op.x0 = x;
    op.y0 = y;
    op.pod = pod;
    op.addProngDir = dir;
    op.superProng = superProng;
    _prepOps.push_back(op);
}

void OctiGame::prepareReposition(int x, int y, OctiPod pod, OctiDir srcDir, OctiDir tgtDir)
{
    if (!_hasPreparedMove) {
        _prepSnapshot = captureSnapshot();
        _prepOps.clear();
        _hasPreparedMove = true;
    }

    OctiMoveOp op;
    op.x0 = x;
    op.y0 = y;
    op.pod = pod;
    op.rmProngDir = srcDir;
    op.addProngDir = tgtDir;
    _prepOps.push_back(op);
}

void OctiGame::prepareHops(int x, int y, OctiPod pod, const std::vector<OctiHop> &hops)
{
    if (!_hasPreparedMove) {
        _prepSnapshot = captureSnapshot();
        _prepOps.clear();
        _hasPreparedMove = true;
    }

    OctiMoveOp op;
    op.x0 = x;
    op.y0 = y;
    op.pod = pod;
    op.hops = hops;
    _prepOps.push_back(op);
}

void OctiGame::prepareCapture(int x, int y)
{
    if (!_prepOps.empty() && !_prepOps.back().hops.empty()) {
        for (auto &h : _prepOps.back().hops) {
            if (h.x == x && h.y == y) {
                h.capture = true;
                break;
            }
        }
    }
}

void OctiGame::commitPreparedMove()
{
    if (!_hasPreparedMove) {
        throw IllegalMoveException("no prepared move to commit");
    }

    std::vector<OctiMoveOp> ops = _prepOps;
    rollbackPreparedMove();

    OctiMove m(ops);
    move(m);
}

void OctiGame::rollbackPreparedMove()
{
    if (_hasPreparedMove) {
        restoreSnapshot(_prepSnapshot);
        _hasPreparedMove = false;
        _prepOps.clear();
        notifyGameUpdated();
    }
}

bool OctiGame::hasPreparedMove() const
{
    return _hasPreparedMove;
}

std::string OctiGame::getPreparedMoveString() const
{
    if (!_hasPreparedMove || _prepOps.empty()) {
        return "";
    }
    return OctiMove::formatOps(_prepOps);
}

void OctiGame::applyUncommittedTurnForPreview(const std::vector<OctiMoveOp> &ops)
{
    if (!_gameStarted) {
        throw IllegalMoveException("game has not started");
    }
    if (_gameOver) {
        throw IllegalMoveException("game is already over");
    }
    if (_turn == PLAYER_UNDEF || !_board) {
        throw IllegalMoveException("turn is undefined");
    }
    if (ops.empty()) {
        throw IllegalMoveException("no operations in move");
    }

    OctiGameSnapshot snapBefore = captureSnapshot();
    try {
        applyMoveOps(ops, true);
    } catch (...) {
        restoreSnapshot(snapBefore);
        throw;
    }
}

static void dfsJumpChains(const OctiBoard *board,
                          bool edgeless,
                          OctiPlayer turn,
                          int cx, int cy,
                          const OctiPod &pod,
                          const std::vector<OctiPod> &ghostPods,
                          std::vector<std::pair<int, int>> &jumpedSquares,
                          std::vector<OctiHop> &currentHops,
                          int sx, int sy,
                          std::vector<OctiMove> &outMoves)
{
    int w = board->getBoardWidth();
    int h = board->getBoardHeight();

    for (int d = 0; d < DIR_MAX; d++) {
        OctiDir dir = static_cast<OctiDir>(d);
        if (pod.prongAt(dir) == NO_PRONG) {
            continue;
        }

        int dx = dirDeltaX(dir) * 2;
        int dy = dirDeltaY(dir) * 2;
        int jdx = dirDeltaX(dir);
        int jdy = dirDeltaY(dir);

        int jx = edgeless ? (((cx + jdx) % w + w) % w) : (cx + jdx);
        int jy = edgeless ? (((cy + jdy) % h + h) % h) : (cy + jdy);

        int lx = edgeless ? (((cx + dx) % w + w) % w) : (cx + dx);
        int ly = edgeless ? (((cy + dy) % h + h) % h) : (cy + dy);

        if (!edgeless) {
            if (jx < 0 || jx >= w || jy < 0 || jy >= h) continue;
            if (lx < 0 || lx >= w || ly < 0 || ly >= h) continue;
        }

        std::pair<int, int> jCoord = {jx, jy};
        if (std::find(jumpedSquares.begin(), jumpedSquares.end(), jCoord) != jumpedSquares.end()) {
            continue;
        }

        const OctiSquare &jumpedSq = board->squareAt(jx, jy);
        if (isEmptyForJump(jumpedSq, jx, jy, sx, sy, ghostPods)) {
            continue;
        }

        const OctiSquare &landSq = board->squareAt(lx, ly);
        if (!isEmptyForJump(landSq, lx, ly, sx, sy, ghostPods)) {
            continue;
        }

        // Branch 1: Without capture
        {
            OctiHop hop;
            hop.x = lx;
            hop.y = ly;
            hop.capture = false;

            currentHops.push_back(hop);
            jumpedSquares.push_back(jCoord);

            OctiMoveOp op;
            op.x0 = sx;
            op.y0 = sy;
            op.pod = pod;
            op.hops = currentHops;
            outMoves.push_back(OctiMove::fromValidatedSingleOp(std::move(op)));

            // Recurse further along jump chain
            dfsJumpChains(board, edgeless, turn, lx, ly, pod, ghostPods,
                          jumpedSquares, currentHops, sx, sy, outMoves);

            jumpedSquares.pop_back();
            currentHops.pop_back();
        }

        // Branch 2: With capture
        {
            OctiHop hop;
            hop.x = lx;
            hop.y = ly;
            hop.capture = true;

            currentHops.push_back(hop);
            jumpedSquares.push_back(jCoord);

            OctiMoveOp op;
            op.x0 = sx;
            op.y0 = sy;
            op.pod = pod;
            op.hops = currentHops;
            outMoves.push_back(OctiMove::fromValidatedSingleOp(std::move(op)));

            dfsJumpChains(board, edgeless, turn, lx, ly, pod, ghostPods,
                          jumpedSquares, currentHops, sx, sy, outMoves);

            jumpedSquares.pop_back();
            currentHops.pop_back();
        }
    }
}

static void generateZeroPreludeMoves(const OctiBoard *_board,
                                     const OctiPlayerState _players[5],
                                     OctiGameMode _mode,
                                     OctiPlayer _turn,
                                     bool _useSuperProngs,
                                     bool _useEdgelessBoard,
                                     std::vector<OctiMove> &outMoves)
{
    outMoves.clear();
    outMoves.reserve(64);

    int w = _board->getBoardWidth();
    int h = _board->getBoardHeight();

    // 1. Pod placements & liberations
    if (_mode != GAME_MODE_OFK) {
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                const OctiSquare &sq = _board->squareAt(x, y);
                if (sq.getType() == NEUTRAL_SQUARE) {
                    continue;
                }
                OctiSquareType st = sq.getType();
                bool isFriendlyHome = false;
                if (_board->getNumPlayers() == 2) {
                    isFriendlyHome = (_turn == PLAYER_A && st == A_HOME_SQUARE) ||
                                     (_turn == PLAYER_B && st == B_HOME_SQUARE);
                } else if (_board->getNumPlayers() == 4) {
                    isFriendlyHome = (st == static_cast<OctiSquareType>(_turn));
                }

                if (isFriendlyHome) {
                    if (sq.hasEnemyOf(_turn)) {
                        continue;
                    }
                } else {
                    if (!sq.isOccupiedBy(_turn)) {
                        continue;
                    }
                }

                if (_mode == GAME_MODE_2) {
                    bool isFriendlyBase = (sq.getType() == (_turn == PLAYER_A ? A_HOME_SQUARE : B_HOME_SQUARE));
                    if (isFriendlyBase) {
                        if (_players[_turn].freePods > 0) {
                            OctiMoveOp op;
                            op.x0 = x; op.y0 = y; op.addPod = true; op.liberate = false;
                            outMoves.push_back(OctiMove::fromValidatedSingleOp(std::move(op)));
                        }
                        if (_board->canLiberate(_turn) && _players[_turn].capturedPods > 0 && sq.isOccupiedBy(_turn)) {
                            OctiMoveOp op;
                            op.x0 = x; op.y0 = y; op.addPod = true; op.liberate = true;
                            outMoves.push_back(OctiMove::fromValidatedSingleOp(std::move(op)));
                        }
                    } else {
                        // Enemy base
                        if (_players[_turn].freePods > 0) {
                            OctiMoveOp op;
                            op.x0 = x; op.y0 = y; op.addPod = true; op.liberate = false;
                            outMoves.push_back(OctiMove::fromValidatedSingleOp(std::move(op)));
                        }
                        if (_players[_turn].capturedPods > 0) {
                            OctiMoveOp op;
                            op.x0 = x; op.y0 = y; op.addPod = true; op.liberate = true;
                            outMoves.push_back(OctiMove::fromValidatedSingleOp(std::move(op)));
                        }
                    }
                } else if (_mode == GAME_MODE_4) {
                    OctiSquareType homeType = (_turn == PLAYER_A ? A_HOME_SQUARE :
                                               _turn == PLAYER_B ? B_HOME_SQUARE :
                                               _turn == PLAYER_C ? C_HOME_SQUARE : D_HOME_SQUARE);
                    OctiPlayer partner = (_turn == PLAYER_A ? PLAYER_C :
                                          _turn == PLAYER_C ? PLAYER_A :
                                          _turn == PLAYER_B ? PLAYER_D : PLAYER_B);
                    OctiSquareType partnerHome = (partner == PLAYER_A ? A_HOME_SQUARE :
                                                  partner == PLAYER_B ? B_HOME_SQUARE :
                                                  partner == PLAYER_C ? C_HOME_SQUARE : D_HOME_SQUARE);

                    if (sq.getType() == homeType) {
                        if (_players[_turn].freePods > 0) {
                            OctiMoveOp op;
                            op.x0 = x; op.y0 = y; op.addPod = true; op.liberate = false;
                            outMoves.push_back(OctiMove::fromValidatedSingleOp(std::move(op)));
                        }
                        if (sq.isOccupiedBy(_turn) && _board->canLiberate(_turn) && _players[_turn].capturedPods > 0) {
                            OctiMoveOp op;
                            op.x0 = x; op.y0 = y; op.addPod = true; op.liberate = true;
                            outMoves.push_back(OctiMove::fromValidatedSingleOp(std::move(op)));
                        }
                    } else if (sq.getType() == partnerHome) {
                        // Never allow placing freePods on partnerHome
                        if (_players[partner].eliminated) {
                            if (_players[partner].capturedPods > 0) {
                                OctiMoveOp op;
                                op.x0 = x; op.y0 = y; op.addPod = true; op.liberate = true;
                                outMoves.push_back(OctiMove::fromValidatedSingleOp(std::move(op)));
                            }
                        } else {
                            if (_board->canLiberate(_turn) && _players[_turn].capturedPods > 0) {
                                OctiMoveOp op;
                                op.x0 = x; op.y0 = y; op.addPod = true; op.liberate = true;
                                outMoves.push_back(OctiMove::fromValidatedSingleOp(std::move(op)));
                            }
                        }
                    } else if (sq.getType() == NEUTRAL_OCTI_SQUARE) {
                        // Never allow placing freePods on NEUTRAL_OCTI_SQUARE
                        if (_players[_turn].capturedPods > 0) {
                            OctiMoveOp op;
                            op.x0 = x; op.y0 = y; op.addPod = true; op.liberate = true;
                            outMoves.push_back(OctiMove::fromValidatedSingleOp(std::move(op)));
                        }
                    } else {
                        // Enemy base
                        if (_players[_turn].freePods > 0) {
                            OctiMoveOp op;
                            op.x0 = x; op.y0 = y; op.addPod = true; op.liberate = false;
                            outMoves.push_back(OctiMove::fromValidatedSingleOp(std::move(op)));
                        }
                        if (_players[_turn].capturedPods > 0) {
                            OctiMoveOp op;
                            op.x0 = x; op.y0 = y; op.addPod = true; op.liberate = true;
                            outMoves.push_back(OctiMove::fromValidatedSingleOp(std::move(op)));
                        }
                    }
                }
            }
        }
    }

    // 2. Prong Additions, 3. Prong Repositions, 4. Distance-1 Moves, 5. Jumps
    OctiPodType turnPodType = playerToPodType(_turn);
    std::vector<std::pair<int, int>> jumpedSquares;
    std::vector<OctiHop> currentHops;
    jumpedSquares.reserve(16);
    currentHops.reserve(16);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            const OctiSquare &sq = _board->squareAt(x, y);
            int numPods = sq.getNumPods();
            if (numPods == 0) {
                continue;
            }
            std::uint32_t seenConfigs[32];
            int numSeen = 0;

            for (int i = 0; i < numPods; i++) {
                const OctiPod &pod = sq.getPodAt(i);
                if (pod.getType() != turnPodType) {
                    continue;
                }
                std::uint32_t cfg = pod.rawConfig() & 0xFFFFu;
                bool dupCfg = false;
                for (int s = 0; s < numSeen; ++s) {
                    if (seenConfigs[s] == cfg) {
                        dupCfg = true;
                        break;
                    }
                }
                if (dupCfg) {
                    continue;
                }
                if (numSeen < 32) {
                    seenConfigs[numSeen++] = cfg;
                }

                // 2. Prong Additions
                for (int d = 0; d < DIR_MAX; d++) {
                    OctiDir dir = static_cast<OctiDir>(d);
                    if (pod.prongAt(dir) == NO_PRONG) {
                        if (_players[_turn].freeProngs > 0) {
                            OctiMoveOp op;
                            op.x0 = x; op.y0 = y;
                            op.pod = pod;
                            op.addProngDir = dir;
                            op.superProng = false;
                            outMoves.push_back(OctiMove::fromValidatedSingleOp(std::move(op)));
                        }
                        if (_useSuperProngs && _players[_turn].freeSuperProngs > 0) {
                            OctiMoveOp op;
                            op.x0 = x; op.y0 = y;
                            op.pod = pod;
                            op.addProngDir = dir;
                            op.superProng = true;
                            outMoves.push_back(OctiMove::fromValidatedSingleOp(std::move(op)));
                        }
                    }
                }

                // 3. Prong Repositions
                if (_mode != GAME_MODE_OFK && cfg != 0u) {
                    for (int rm = 0; rm < DIR_MAX; rm++) {
                        OctiDir rmDir = static_cast<OctiDir>(rm);
                        if (pod.prongAt(rmDir) == NORMAL_PRONG) {
                            for (int add = 0; add < DIR_MAX; add++) {
                                OctiDir addDir = static_cast<OctiDir>(add);
                                if (addDir != rmDir && pod.prongAt(addDir) == NO_PRONG) {
                                    OctiMoveOp op;
                                    op.x0 = x; op.y0 = y;
                                    op.pod = pod;
                                    op.rmProngDir = rmDir;
                                    op.addProngDir = addDir;
                                    op.superProng = false;
                                    outMoves.push_back(OctiMove::fromValidatedSingleOp(std::move(op)));
                                }
                            }
                        }
                    }
                }

                // 4. Distance-1 Moves & 5. Distance-2 Jumps
                if (cfg != 0u) {
                    for (int d = 0; d < DIR_MAX; d++) {
                        OctiDir dir = static_cast<OctiDir>(d);
                        if (pod.prongAt(dir) != NO_PRONG) {
                            int dx = dirDeltaX(dir);
                            int dy = dirDeltaY(dir);
                            int nx = _useEdgelessBoard ? (((x + dx) % w + w) % w) : (x + dx);
                            int ny = _useEdgelessBoard ? (((y + dy) % h + h) % h) : (y + dy);

                            if (!_useEdgelessBoard) {
                                if (nx < 0 || nx >= w || ny < 0 || ny >= h) {
                                    continue;
                                }
                            }

                            const OctiSquare &destSq = _board->squareAt(nx, ny);
                            if (_mode == GAME_MODE_OFK) {
                                if (!destSq.isEmpty()) continue;
                            } else {
                                if (destSq.hasEnemyOf(_turn)) continue;
                            }

                            OctiHop hop;
                            hop.x = nx;
                            hop.y = ny;
                            hop.capture = false;

                            OctiMoveOp op;
                            op.x0 = x; op.y0 = y;
                            op.pod = pod;
                            op.hops.push_back(hop);
                            outMoves.push_back(OctiMove::fromValidatedSingleOp(std::move(op)));
                        }
                    }

                    const std::vector<OctiPod> &ghostPods = sq.pods();
                    jumpedSquares.clear();
                    currentHops.clear();
                    dfsJumpChains(_board, _useEdgelessBoard, _turn, x, y, pod, ghostPods,
                                  jumpedSquares, currentHops, x, y, outMoves);
                }
            }
        }
    }
}

struct PreludeCandidate {
    OctiMoveOp op;
    int endX = -1;
    int endY = -1;
    OctiDir superDir = DIR_UNDEF;
    std::uint8_t capX[16];
    std::uint8_t capY[16];
    int numCaptures = 0;
};

static void copyBoardSquares(const OctiBoard *src, OctiBoard &dst)
{
    int w = src->getBoardWidth();
    int h = src->getBoardHeight();
    for (int x = 0; x < w; x++) {
        for (int y = 0; y < h; y++) {
            OctiSquare &dSq = dst.squareAt(x, y);
            dSq.clear();
            const OctiSquare &sSq = src->squareAt(x, y);
            for (int i = 0; i < sSq.getNumPods(); i++) {
                dSq.addPod(sSq.getPodAt(i));
            }
        }
    }
}

struct FastSq {
    std::uint32_t rawPods[16];
    int count;
};

static inline std::uint32_t rawPodMakeOwner(OctiPodType t) noexcept
{
    return (static_cast<std::uint32_t>(t) & 0x7u) << 16u;
}

static inline OctiPodType rawPodGetType(std::uint32_t rc) noexcept
{
    return static_cast<OctiPodType>((rc >> 16u) & 0x7u);
}

static inline std::uint32_t rawPodAddProng(std::uint32_t rc, OctiDir dir, bool super) noexcept
{
    unsigned int shift = static_cast<unsigned int>(dir) * 2u;
    std::uint32_t mask = 0x3u << shift;
    std::uint32_t pval = super ? 2u : 1u;
    return (rc & ~mask) | (pval << shift);
}

static inline std::uint32_t rawPodRemoveProng(std::uint32_t rc, OctiDir dir) noexcept
{
    unsigned int shift = static_cast<unsigned int>(dir) * 2u;
    return rc & ~(0x3u << shift);
}

static inline int popcount16(std::uint32_t x) noexcept
{
    x = x - ((x >> 1u) & 0x5555u);
    x = (x & 0x3333u) + ((x >> 2u) & 0x3333u);
    return static_cast<int>((((x + (x >> 4u)) & 0x0F0Fu) * 0x0101u) >> 8u);
}

static inline int rawPodNumNormalProngs(std::uint32_t rc) noexcept
{
    std::uint32_t lo = rc & 0x5555u;
    std::uint32_t hi = (rc >> 1u) & 0x5555u;
    return popcount16(lo & ~hi);
}

static inline int rawPodNumSuperProngs(std::uint32_t rc) noexcept
{
    std::uint32_t lo = rc & 0x5555u;
    std::uint32_t hi = (rc >> 1u) & 0x5555u;
    return popcount16(hi & ~lo);
}

static inline std::uint64_t splitMixCanon64(std::uint64_t z) noexcept
{
    z += 0x9E3779B97F4A7C15ULL;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static inline void sortFastSqPods(FastSq &fSq) noexcept
{
    for (int i = 1; i < fSq.count; ++i) {
        std::uint32_t keyCfg = fSq.rawPods[i];
        int j = i - 1;
        while (j >= 0 && keyCfg < fSq.rawPods[j]) {
            fSq.rawPods[j + 1] = fSq.rawPods[j];
            --j;
        }
        fSq.rawPods[j + 1] = keyCfg;
    }
}

static inline void computeSqCanonHashes(int x, int y, const FastSq &sortedSq,
                                        std::uint64_t &outH1,
                                        std::uint64_t &outH2) noexcept
{
    if (sortedSq.count <= 0) {
        outH1 = 0ULL;
        outH2 = 0ULL;
        return;
    }
    std::uint64_t hdr = static_cast<std::uint64_t>(
        (x & 0xF) | ((y & 0xF) << 4) | ((sortedSq.count & 0xFF) << 8));
    std::uint64_t a = splitMixCanon64(hdr ^ 0x517CC1B727220A95ULL);
    std::uint64_t b = splitMixCanon64(hdr ^ 0xA24BAED4963EE407ULL);
    for (int i = 0; i < sortedSq.count; ++i) {
        std::uint64_t rc = static_cast<std::uint64_t>(sortedSq.rawPods[i]);
        a = splitMixCanon64(a ^ rc);
        b = splitMixCanon64(b + (rc * 0xD6E8FEB86659FD93ULL));
    }
    outH1 = (a == 0ULL) ? 1ULL : a;
    outH2 = (b == 0ULL) ? 1ULL : b;
}

struct FastGrid {
    FastSq sq[9][9];
    std::uint64_t sqH1[9][9];
    std::uint64_t sqH2[9][9];
    std::uint64_t totalH1;
    std::uint64_t totalH2;
    bool has4pEnemyOnHome;
};

struct CanonicalStateSig {
    std::uint64_t h1 = 0ULL;
    std::uint64_t h2 = 0ULL;
    std::uint64_t p0 = 0ULL;
    std::uint64_t p1 = 0ULL;
    std::uint32_t p2 = 0U;

    bool operator==(const CanonicalStateSig &o) const noexcept
    {
        return h1 == o.h1 && h2 == o.h2 &&
               p0 == o.p0 && p1 == o.p1 && p2 == o.p2;
    }
};

static inline void packPlayersIntoSig(const OctiPlayerState players[5],
                                      int numPlayers,
                                      CanonicalStateSig &sig) noexcept
{
    std::uint8_t buf[20] = {0};
    for (int p = 1; p <= numPlayers; ++p) {
        int base = (p - 1) * 5;
        buf[base + 0] = static_cast<std::uint8_t>(players[p].freePods);
        buf[base + 1] = static_cast<std::uint8_t>(players[p].capturedPods);
        buf[base + 2] = static_cast<std::uint8_t>(players[p].freeProngs);
        buf[base + 3] = static_cast<std::uint8_t>(players[p].freeSuperProngs);
        buf[base + 4] = static_cast<std::uint8_t>(players[p].eliminated ? 1 : 0);
    }
    std::memcpy(&sig.p0, buf + 0, 8);
    std::memcpy(&sig.p1, buf + 8, 8);
    std::memcpy(&sig.p2, buf + 16, 4);
}

struct SeenStateTable {
    struct Entry {
        CanonicalStateSig sig;
        std::uint32_t gen = 0U;
    };
    std::vector<Entry> slots;
    std::size_t count = 0;
    std::uint32_t curGen = 1U;

    SeenStateTable() : slots(2048) {}

    void clearForNewNode()
    {
        count = 0;
        ++curGen;
        if (curGen == 0U) {
            for (auto &e : slots) {
                e.gen = 0U;
            }
            curGen = 1U;
        }
    }

    static std::uint64_t probeHash(const CanonicalStateSig &s) noexcept
    {
        return splitMixCanon64(s.h1 ^ (s.h2 * 0x9E3779B97F4A7C15ULL) ^
                               s.p0 ^ s.p1 ^ static_cast<std::uint64_t>(s.p2));
    }

    void rehash(std::size_t newCap)
    {
        std::vector<Entry> oldSlots = std::move(slots);
        slots.assign(newCap, Entry{});
        const std::uint32_t g = curGen;
        std::size_t mask = newCap - 1;
        for (const auto &e : oldSlots) {
            if (e.gen != g) continue;
            std::size_t idx = static_cast<std::size_t>(probeHash(e.sig)) & mask;
            while (slots[idx].gen == g) {
                idx = (idx + 1) & mask;
            }
            slots[idx] = e;
        }
    }

    bool insert(const CanonicalStateSig &sig)
    {
        if ((count + 1) * 2 >= slots.size()) {
            rehash(slots.size() * 2);
        }
        const std::uint32_t g = curGen;
        std::size_t mask = slots.size() - 1;
        std::size_t idx = static_cast<std::size_t>(probeHash(sig)) & mask;
        while (slots[idx].gen == g) {
            if (slots[idx].sig == sig) {
                return false;
            }
            idx = (idx + 1) & mask;
        }
        slots[idx].sig = sig;
        slots[idx].gen = g;
        count++;
        return true;
    }
};

static void initFastGrid(const OctiBoard *board, OctiGameMode mode, FastGrid &grid)
{
    int w = board->getBoardWidth();
    int h = board->getBoardHeight();
    grid.totalH1 = 0ULL;
    grid.totalH2 = 0ULL;
    grid.has4pEnemyOnHome = false;
    for (int x = 0; x < w; x++) {
        for (int y = 0; y < h; y++) {
            const OctiSquare &sSq = board->squareAt(x, y);
            int n = sSq.getNumPods();
            FastSq &fSq = grid.sq[x][y];
            fSq.count = n;
            for (int i = 0; i < n; i++) {
                fSq.rawPods[i] = sSq.getPodAt(i).rawConfig();
            }
            if (n > 1) {
                sortFastSqPods(fSq);
            }
            computeSqCanonHashes(x, y, fSq, grid.sqH1[x][y], grid.sqH2[x][y]);
            grid.totalH1 ^= grid.sqH1[x][y];
            grid.totalH2 += grid.sqH2[x][y];
        }
    }
    if (mode == GAME_MODE_4) {
        static const int kHomeXY[5][2] = {
            {-1, -1}, {2, 2}, {2, 6}, {6, 6}, {6, 2}
        };
        for (int p = 1; p <= 4; ++p) {
            OctiPlayer player = static_cast<OctiPlayer>(p);
            const FastSq &hSq = grid.sq[kHomeXY[p][0]][kHomeXY[p][1]];
            for (int i = 0; i < hSq.count; ++i) {
                OctiPlayer podPlayer = podTypeToPlayer(rawPodGetType(hSq.rawPods[i]));
                if (podPlayer != PLAYER_UNDEF &&
                    !areAlliedPlayers(GAME_MODE_4, player, podPlayer)) {
                    grid.has4pEnemyOnHome = true;
                    break;
                }
            }
            if (grid.has4pEnemyOnHome) {
                break;
            }
        }
    }
}

static inline void copyFastSqActive(FastSq &dst, const FastSq &src) noexcept
{
    dst.count = src.count;
    for (int i = 0; i < src.count; ++i) {
        dst.rawPods[i] = src.rawPods[i];
    }
}

static CanonicalStateSig computePostMoveCanonicalSig(const OctiBoard *board,
                                                     const FastGrid &baseGrid,
                                                     const OctiPlayerState playersIn[5],
                                                     OctiGameMode mode,
                                                     OctiPlayer turn,
                                                     bool edgeless,
                                                     const OctiMoveOp &regOp)
{
    int w = board->getBoardWidth();
    int h = board->getBoardHeight();
    OctiPlayerState players[5];
    for (int p = 0; p < 5; p++) {
        players[p] = playersIn[p];
    }

    struct ModifiedSq {
        int x;
        int y;
        FastSq sq;
    };
    ModifiedSq mods[20];
    int numMods = 0;
    auto getModSq = [&](int mx, int my) -> FastSq & {
        for (int k = 0; k < numMods; ++k) {
            if (mods[k].x == mx && mods[k].y == my) {
                return mods[k].sq;
            }
        }
        mods[numMods].x = mx;
        mods[numMods].y = my;
        copyFastSqActive(mods[numMods].sq, baseGrid.sq[mx][my]);
        return mods[numMods++].sq;
    };

    const OctiPodType turnPodType = playerToPodType(turn);
    const std::uint32_t turnOwnerRaw = rawPodMakeOwner(turnPodType);
    const std::uint32_t wantRaw = turnOwnerRaw | (regOp.pod.rawConfig() & 0xFFFFu);
    bool destMaybe4pHome = false;

    if (regOp.addPod) {
        OctiSquareType st = board->squareAt(regOp.x0, regOp.y0).getType();
        FastSq &fSq = getModSq(regOp.x0, regOp.y0);
        if (mode == GAME_MODE_2) {
            if (regOp.liberate) {
                players[turn].capturedPods--;
            } else if (players[turn].freePods > 0) {
                players[turn].freePods--;
            } else {
                players[turn].capturedPods--;
            }
            fSq.rawPods[fSq.count++] = turnOwnerRaw;
        } else if (mode == GAME_MODE_4) {
            destMaybe4pHome = true;
            OctiSquareType homeType = (turn == PLAYER_A ? A_HOME_SQUARE :
                                       turn == PLAYER_B ? B_HOME_SQUARE :
                                       turn == PLAYER_C ? C_HOME_SQUARE : D_HOME_SQUARE);
            OctiPlayer partner = (turn == PLAYER_A ? PLAYER_C :
                                  turn == PLAYER_C ? PLAYER_A :
                                  turn == PLAYER_B ? PLAYER_D : PLAYER_B);
            OctiSquareType partnerHome = (partner == PLAYER_A ? A_HOME_SQUARE :
                                          partner == PLAYER_B ? B_HOME_SQUARE :
                                          partner == PLAYER_C ? C_HOME_SQUARE : D_HOME_SQUARE);
            if (st == homeType) {
                if (regOp.liberate) {
                    players[turn].capturedPods--;
                } else if (players[turn].freePods > 0) {
                    players[turn].freePods--;
                } else {
                    players[turn].capturedPods--;
                }
                fSq.rawPods[fSq.count++] = turnOwnerRaw;
            } else if (st == partnerHome) {
                if (players[partner].eliminated) {
                    players[partner].eliminated = false;
                    players[partner].capturedPods--;
                    fSq.rawPods[fSq.count++] = rawPodMakeOwner(playerToPodType(partner));
                } else {
                    players[turn].capturedPods--;
                    fSq.rawPods[fSq.count++] = turnOwnerRaw;
                }
            } else if (st == NEUTRAL_OCTI_SQUARE) {
                players[turn].capturedPods--;
                fSq.rawPods[fSq.count++] = turnOwnerRaw;
            } else {
                if (regOp.liberate) {
                    players[turn].capturedPods--;
                } else if (players[turn].freePods > 0) {
                    players[turn].freePods--;
                } else {
                    players[turn].capturedPods--;
                }
                fSq.rawPods[fSq.count++] = turnOwnerRaw;
            }
        }
    } else if (regOp.addProngDir != DIR_UNDEF && regOp.rmProngDir == DIR_UNDEF) {
        FastSq &fSq = getModSq(regOp.x0, regOp.y0);
        for (int i = 0; i < fSq.count; i++) {
            if (fSq.rawPods[i] == wantRaw) {
                fSq.rawPods[i] = rawPodAddProng(fSq.rawPods[i], regOp.addProngDir, regOp.superProng);
                break;
            }
        }
        if (regOp.superProng) {
            players[turn].freeSuperProngs--;
        } else {
            players[turn].freeProngs--;
        }
    } else if (regOp.addProngDir != DIR_UNDEF && regOp.rmProngDir != DIR_UNDEF) {
        FastSq &fSq = getModSq(regOp.x0, regOp.y0);
        for (int i = 0; i < fSq.count; i++) {
            if (fSq.rawPods[i] == wantRaw) {
                std::uint32_t rc = rawPodRemoveProng(fSq.rawPods[i], regOp.rmProngDir);
                fSq.rawPods[i] = rawPodAddProng(rc, regOp.addProngDir, false);
                break;
            }
        }
    } else if (!regOp.hops.empty()) {
        FastSq &startSq = getModSq(regOp.x0, regOp.y0);
        std::uint32_t movingRaw = wantRaw;
        for (int i = 0; i < startSq.count; i++) {
            if (startSq.rawPods[i] == wantRaw) {
                movingRaw = startSq.rawPods[i];
                for (int j = i + 1; j < startSq.count; j++) {
                    startSq.rawPods[j - 1] = startSq.rawPods[j];
                }
                startSq.count--;
                break;
            }
        }
        int cx = regOp.x0;
        int cy = regOp.y0;
        std::pair<int, int> capturedCoords[16];
        int numCaptured = 0;
        for (const auto &hop : regOp.hops) {
            if (hop.capture) {
                int jdx = 0, jdy = 0;
                findStepDelta(cx, cy, hop.x, hop.y, edgeless, w, h, jdx, jdy);
                int jx = edgeless ? (((cx + jdx / 2) % w + w) % w) : (cx + jdx / 2);
                int jy = edgeless ? (((cy + jdy / 2) % h + h) % h) : (cy + jdy / 2);
                std::pair<int, int> jc = {jx, jy};
                bool dup = false;
                for (int k = 0; k < numCaptured; k++) {
                    if (capturedCoords[k] == jc) {
                        dup = true;
                        break;
                    }
                }
                if (!dup) {
                    capturedCoords[numCaptured++] = jc;
                }
            }
            cx = hop.x;
            cy = hop.y;
        }
        FastSq &destSq = getModSq(cx, cy);
        destSq.rawPods[destSq.count++] = movingRaw;
        if (mode == GAME_MODE_4 &&
            ((cx == 2 || cx == 6) && (cy == 2 || cy == 6))) {
            destMaybe4pHome = true;
        }
        for (int k = 0; k < numCaptured; k++) {
            FastSq &cSq = getModSq(capturedCoords[k].first, capturedCoords[k].second);
            for (int i = 0; i < cSq.count; i++) {
                std::uint32_t sqRaw = cSq.rawPods[i];
                OctiPlayer owner = podTypeToPlayer(rawPodGetType(sqRaw));
                players[turn].freeProngs += rawPodNumNormalProngs(sqRaw);
                players[turn].freeSuperProngs += rawPodNumSuperProngs(sqRaw);
                players[owner].capturedPods++;
            }
            cSq.count = 0;
        }
    }

    if (mode == GAME_MODE_4 && (baseGrid.has4pEnemyOnHome || destMaybe4pHome)) {
        static const int kHomeCoords[5][2] = {
            {-1, -1}, {2, 2}, {2, 6}, {6, 6}, {6, 2}
        };
        auto readSq = [&](int sx, int sy) -> const FastSq & {
            for (int k = 0; k < numMods; ++k) {
                if (mods[k].x == sx && mods[k].y == sy) {
                    return mods[k].sq;
                }
            }
            return baseGrid.sq[sx][sy];
        };
        bool anyElimTriggered = false;
        for (int p = 1; p <= 4; p++) {
            if (!players[p].eliminated) {
                const FastSq &hSq = readSq(kHomeCoords[p][0], kHomeCoords[p][1]);
                OctiPlayer player = static_cast<OctiPlayer>(p);
                for (int i = 0; i < hSq.count; i++) {
                    OctiPlayer podPlayer = podTypeToPlayer(rawPodGetType(hSq.rawPods[i]));
                    if (podPlayer != PLAYER_UNDEF &&
                        !areAlliedPlayers(GAME_MODE_4, player, podPlayer)) {
                        anyElimTriggered = true;
                        break;
                    }
                }
                if (anyElimTriggered) {
                    break;
                }
            }
        }
        if (anyElimTriggered) {
            FastSq fullSq[9][9];
            for (int x = 0; x < w; x++) {
                for (int y = 0; y < h; y++) {
                    copyFastSqActive(fullSq[x][y], baseGrid.sq[x][y]);
                }
            }
            for (int k = 0; k < numMods; ++k) {
                copyFastSqActive(fullSq[mods[k].x][mods[k].y], mods[k].sq);
            }
            for (int p = 1; p <= 4; p++) {
                OctiPlayer player = static_cast<OctiPlayer>(p);
                if (!players[p].eliminated) {
                    const FastSq &hSq = fullSq[kHomeCoords[p][0]][kHomeCoords[p][1]];
                    bool captured = false;
                    for (int i = 0; i < hSq.count; i++) {
                        OctiPlayer podPlayer = podTypeToPlayer(rawPodGetType(hSq.rawPods[i]));
                        if (podPlayer != PLAYER_UNDEF &&
                            !areAlliedPlayers(GAME_MODE_4, player, podPlayer)) {
                            captured = true;
                            break;
                        }
                    }
                    if (captured) {
                        players[p].eliminated = true;
                        for (int x = 0; x < w; x++) {
                            for (int y = 0; y < h; y++) {
                                FastSq &fSq = fullSq[x][y];
                                int writeIdx = 0;
                                for (int i = 0; i < fSq.count; i++) {
                                    std::uint32_t rc = fSq.rawPods[i];
                                    if (podTypeToPlayer(rawPodGetType(rc)) == player) {
                                        players[turn].freeProngs += rawPodNumNormalProngs(rc);
                                        players[turn].freeSuperProngs += rawPodNumSuperProngs(rc);
                                        players[p].capturedPods++;
                                    } else {
                                        fSq.rawPods[writeIdx++] = rc;
                                    }
                                }
                                fSq.count = writeIdx;
                            }
                        }
                    }
                }
            }
            CanonicalStateSig sig;
            packPlayersIntoSig(players, 4, sig);
            for (int y = 0; y < h; y++) {
                for (int x = 0; x < w; x++) {
                    FastSq &fSq = fullSq[x][y];
                    if (fSq.count == 0) continue;
                    if (fSq.count > 1) sortFastSqPods(fSq);
                    std::uint64_t sh1 = 0ULL, sh2 = 0ULL;
                    computeSqCanonHashes(x, y, fSq, sh1, sh2);
                    sig.h1 ^= sh1;
                    sig.h2 += sh2;
                }
            }
            return sig;
        }
    }

    CanonicalStateSig sig;
    int numPlayers = (mode == GAME_MODE_4) ? 4 : 2;
    packPlayersIntoSig(players, numPlayers, sig);
    std::uint64_t h1 = baseGrid.totalH1;
    std::uint64_t h2 = baseGrid.totalH2;
    for (int k = 0; k < numMods; ++k) {
        int mx = mods[k].x;
        int my = mods[k].y;
        if (mods[k].sq.count > 1) {
            sortFastSqPods(mods[k].sq);
        }
        std::uint64_t nh1 = 0ULL, nh2 = 0ULL;
        computeSqCanonHashes(mx, my, mods[k].sq, nh1, nh2);
        h1 ^= baseGrid.sqH1[mx][my] ^ nh1;
        h2 = h2 - baseGrid.sqH2[mx][my] + nh2;
    }
    sig.h1 = h1;
    sig.h2 = h2;
    return sig;
}

static inline bool isEnemyHomeSquareForSp(OctiSquareType sqType,
                                          OctiGameMode mode,
                                          OctiPlayer p) noexcept
{
    if (mode == GAME_MODE_2 || mode == GAME_MODE_OFK) {
        if (p == PLAYER_A) {
            return sqType == B_HOME_SQUARE;
        }
        if (p == PLAYER_B) {
            return sqType == A_HOME_SQUARE;
        }
        return false;
    }
    if (mode == GAME_MODE_4) {
        if (p == PLAYER_A || p == PLAYER_C) {
            return sqType == B_HOME_SQUARE || sqType == D_HOME_SQUARE;
        }
        if (p == PLAYER_B || p == PLAYER_D) {
            return sqType == A_HOME_SQUARE || sqType == C_HOME_SQUARE;
        }
    }
    return false;
}

static inline bool testSqBit9x9(const std::uint64_t mask[2], int x, int y) noexcept
{
    int bitIdx = y * 9 + x;
    return ((mask[(bitIdx >> 6) & 1] >> (bitIdx & 63)) & 1ULL) != 0ULL;
}

static bool isOpsTacticalBitmask(const std::uint64_t enemySqMask[2],
                                 const std::uint64_t uncapturedEnemyHomeMask[2],
                                 bool edgeless,
                                 int w,
                                 int h,
                                 const OctiMoveOp *preludeOps,
                                 std::size_t numPreludes,
                                 const OctiMoveOp &regOp) noexcept
{
    const std::size_t totalOps = numPreludes + 1;
    for (std::size_t opIdx = 0; opIdx < totalOps; ++opIdx) {
        const OctiMoveOp &op = (opIdx < numPreludes) ? preludeOps[opIdx] : regOp;
        int cx = op.x0;
        int cy = op.y0;
        for (const auto &hop : op.hops) {
            int dx = hop.x - cx;
            int dy = hop.y - cy;
            if (edgeless) {
                for (int d = -2; d <= 2; d++) {
                    if (((cx + d) % w + w) % w == hop.x) {
                        dx = d;
                        break;
                    }
                }
                for (int d = -2; d <= 2; d++) {
                    if (((cy + d) % h + h) % h == hop.y) {
                        dy = d;
                        break;
                    }
                }
            }
            if (hop.capture && (std::abs(dx) == 2 || std::abs(dy) == 2)) {
                int mx = edgeless ? (((cx + dx / 2) % w + w) % w) : (cx + dx / 2);
                int my = edgeless ? (((cy + dy / 2) % h + h) % h) : (cy + dy / 2);
                if (mx >= 0 && mx < w && my >= 0 && my < h &&
                    testSqBit9x9(enemySqMask, mx, my)) {
                    return true;
                }
            }
            cx = hop.x;
            cy = hop.y;
        }
        if (!op.hops.empty()) {
            const OctiHop &lastHop = op.hops.back();
            bool continuedLater = false;
            for (std::size_t j = opIdx + 1; j < totalOps; ++j) {
                const OctiMoveOp &opJ = (j < numPreludes) ? preludeOps[j] : regOp;
                if (!opJ.hops.empty() &&
                    opJ.x0 == lastHop.x &&
                    opJ.y0 == lastHop.y) {
                    continuedLater = true;
                    break;
                }
            }
            if (!continuedLater &&
                testSqBit9x9(uncapturedEnemyHomeMask, lastHop.x, lastHop.y)) {
                return true;
            }
        }
    }
    return false;
}

static void dfsJumpChainsOpsForSp(const OctiBoard *board,
                                  bool edgeless,
                                  int cx, int cy,
                                  const OctiPod &pod,
                                  const std::vector<OctiPod> &ghostPods,
                                  std::uint64_t jumpedMask[2],
                                  std::vector<OctiHop> &currentHops,
                                  int sx, int sy,
                                  std::vector<OctiMoveOp> &outOps)
{
    int w = board->getBoardWidth();
    int h = board->getBoardHeight();
    std::uint32_t cfg = pod.rawConfig() & 0xFFFFu;

    for (int d = 0; d < DIR_MAX; d++) {
        if (((cfg >> (static_cast<unsigned int>(d) * 2u)) & 0x3u) == 0u) {
            continue;
        }
        OctiDir dir = static_cast<OctiDir>(d);

        int jdx = dirDeltaX(dir);
        int jdy = dirDeltaY(dir);
        int dx = jdx * 2;
        int dy = jdy * 2;

        int jx = edgeless ? (((cx + jdx) % w + w) % w) : (cx + jdx);
        int jy = edgeless ? (((cy + jdy) % h + h) % h) : (cy + jdy);
        int lx = edgeless ? (((cx + dx) % w + w) % w) : (cx + dx);
        int ly = edgeless ? (((cy + dy) % h + h) % h) : (cy + dy);

        if (!edgeless) {
            if (jx < 0 || jx >= w || jy < 0 || jy >= h) continue;
            if (lx < 0 || lx >= w || ly < 0 || ly >= h) continue;
        }

        int jBitIdx = jy * 9 + jx;
        int jWord = (jBitIdx >> 6) & 1;
        std::uint64_t jBit = 1ULL << (jBitIdx & 63);
        if ((jumpedMask[jWord] & jBit) != 0ULL) {
            continue;
        }

        const OctiSquare &jumpedSq = board->squareAt(jx, jy);
        if (isEmptyForJump(jumpedSq, jx, jy, sx, sy, ghostPods)) {
            continue;
        }

        const OctiSquare &landSq = board->squareAt(lx, ly);
        if (!isEmptyForJump(landSq, lx, ly, sx, sy, ghostPods)) {
            continue;
        }

        jumpedMask[jWord] |= jBit;

        // Branch 1: Without capture
        {
            OctiHop hop;
            hop.x = lx;
            hop.y = ly;
            hop.capture = false;

            currentHops.push_back(hop);
            OctiMoveOp op;
            op.x0 = sx;
            op.y0 = sy;
            op.pod = pod;
            op.hops = currentHops;
            outOps.push_back(std::move(op));

            dfsJumpChainsOpsForSp(board, edgeless, lx, ly, pod, ghostPods,
                                  jumpedMask, currentHops, sx, sy, outOps);
            currentHops.pop_back();
        }

        // Branch 2: With capture
        {
            OctiHop hop;
            hop.x = lx;
            hop.y = ly;
            hop.capture = true;

            currentHops.push_back(hop);
            OctiMoveOp op;
            op.x0 = sx;
            op.y0 = sy;
            op.pod = pod;
            op.hops = currentHops;
            outOps.push_back(std::move(op));

            dfsJumpChainsOpsForSp(board, edgeless, lx, ly, pod, ghostPods,
                                  jumpedMask, currentHops, sx, sy, outOps);
            currentHops.pop_back();
        }

        jumpedMask[jWord] &= ~jBit;
    }
}

static void generateZeroPreludeMoveOpsForSp(const OctiBoard *board,
                                            const OctiPlayerState players[5],
                                            OctiGameMode mode,
                                            OctiPlayer turn,
                                            bool useSuperProngs,
                                            bool useEdgelessBoard,
                                            std::vector<OctiHop> &currentHopsScratch,
                                            std::vector<OctiMoveOp> &outOps)
{
    outOps.clear();
    int w = board->getBoardWidth();
    int h = board->getBoardHeight();

    // 1. Pod placements & liberations
    if (mode != GAME_MODE_OFK) {
        int numPlayers = board->getNumPlayers();
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                const OctiSquare &sq = board->squareAt(x, y);
                OctiSquareType st = sq.getType();
                if (st == NEUTRAL_SQUARE) {
                    continue;
                }
                bool isFriendlyHome = false;
                if (numPlayers == 2) {
                    isFriendlyHome = (turn == PLAYER_A && st == A_HOME_SQUARE) ||
                                     (turn == PLAYER_B && st == B_HOME_SQUARE);
                } else if (numPlayers == 4) {
                    isFriendlyHome = (st == static_cast<OctiSquareType>(turn));
                }

                if (isFriendlyHome) {
                    if (sq.hasEnemyOf(turn)) {
                        continue;
                    }
                } else {
                    if (!sq.isOccupiedBy(turn)) {
                        continue;
                    }
                }

                if (mode == GAME_MODE_2) {
                    bool isFriendlyBase = (st == (turn == PLAYER_A ? A_HOME_SQUARE : B_HOME_SQUARE));
                    if (isFriendlyBase) {
                        if (players[turn].freePods > 0) {
                            OctiMoveOp op;
                            op.x0 = x; op.y0 = y; op.addPod = true; op.liberate = false;
                            outOps.push_back(std::move(op));
                        }
                        if (board->canLiberate(turn) && players[turn].capturedPods > 0 && sq.isOccupiedBy(turn)) {
                            OctiMoveOp op;
                            op.x0 = x; op.y0 = y; op.addPod = true; op.liberate = true;
                            outOps.push_back(std::move(op));
                        }
                    } else {
                        if (players[turn].freePods > 0) {
                            OctiMoveOp op;
                            op.x0 = x; op.y0 = y; op.addPod = true; op.liberate = false;
                            outOps.push_back(std::move(op));
                        }
                        if (players[turn].capturedPods > 0) {
                            OctiMoveOp op;
                            op.x0 = x; op.y0 = y; op.addPod = true; op.liberate = true;
                            outOps.push_back(std::move(op));
                        }
                    }
                } else if (mode == GAME_MODE_4) {
                    OctiSquareType homeType = (turn == PLAYER_A ? A_HOME_SQUARE :
                                               turn == PLAYER_B ? B_HOME_SQUARE :
                                               turn == PLAYER_C ? C_HOME_SQUARE : D_HOME_SQUARE);
                    OctiPlayer partner = (turn == PLAYER_A ? PLAYER_C :
                                          turn == PLAYER_C ? PLAYER_A :
                                          turn == PLAYER_B ? PLAYER_D : PLAYER_B);
                    OctiSquareType partnerHome = (partner == PLAYER_A ? A_HOME_SQUARE :
                                                  partner == PLAYER_B ? B_HOME_SQUARE :
                                                  partner == PLAYER_C ? C_HOME_SQUARE : D_HOME_SQUARE);

                    if (st == homeType) {
                        if (players[turn].freePods > 0) {
                            OctiMoveOp op;
                            op.x0 = x; op.y0 = y; op.addPod = true; op.liberate = false;
                            outOps.push_back(std::move(op));
                        }
                        if (sq.isOccupiedBy(turn) && board->canLiberate(turn) && players[turn].capturedPods > 0) {
                            OctiMoveOp op;
                            op.x0 = x; op.y0 = y; op.addPod = true; op.liberate = true;
                            outOps.push_back(std::move(op));
                        }
                    } else if (st == partnerHome) {
                        if (players[partner].eliminated) {
                            if (players[partner].capturedPods > 0) {
                                OctiMoveOp op;
                                op.x0 = x; op.y0 = y; op.addPod = true; op.liberate = true;
                                outOps.push_back(std::move(op));
                            }
                        } else {
                            if (board->canLiberate(turn) && players[turn].capturedPods > 0) {
                                OctiMoveOp op;
                                op.x0 = x; op.y0 = y; op.addPod = true; op.liberate = true;
                                outOps.push_back(std::move(op));
                            }
                        }
                    } else if (st == NEUTRAL_OCTI_SQUARE) {
                        if (players[turn].capturedPods > 0) {
                            OctiMoveOp op;
                            op.x0 = x; op.y0 = y; op.addPod = true; op.liberate = true;
                            outOps.push_back(std::move(op));
                        }
                    } else {
                        if (players[turn].freePods > 0) {
                            OctiMoveOp op;
                            op.x0 = x; op.y0 = y; op.addPod = true; op.liberate = false;
                            outOps.push_back(std::move(op));
                        }
                        if (players[turn].capturedPods > 0) {
                            OctiMoveOp op;
                            op.x0 = x; op.y0 = y; op.addPod = true; op.liberate = true;
                            outOps.push_back(std::move(op));
                        }
                    }
                }
            }
        }
    }

    // 2. Prong Additions, 3. Prong Repositions, 4. Distance-1 Moves, 5. Jumps
    OctiPodType turnPodType = playerToPodType(turn);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            const OctiSquare &sq = board->squareAt(x, y);
            int numPods = sq.getNumPods();
            if (numPods == 0) {
                continue;
            }
            std::uint32_t seenConfigs[32];
            int numSeen = 0;

            for (int i = 0; i < numPods; i++) {
                const OctiPod &pod = sq.getPodAt(i);
                std::uint32_t rc = pod.rawConfig();
                if (rawPodGetType(rc) != turnPodType) {
                    continue;
                }
                std::uint32_t cfg = rc & 0xFFFFu;
                bool dupCfg = false;
                for (int s = 0; s < numSeen; ++s) {
                    if (seenConfigs[s] == cfg) {
                        dupCfg = true;
                        break;
                    }
                }
                if (dupCfg) {
                    continue;
                }
                if (numSeen < 32) {
                    seenConfigs[numSeen++] = cfg;
                }

                // 2. Prong Additions
                if (players[turn].freeProngs > 0 ||
                    (useSuperProngs && players[turn].freeSuperProngs > 0)) {
                    for (int d = 0; d < DIR_MAX; d++) {
                        if (((cfg >> (static_cast<unsigned int>(d) * 2u)) & 0x3u) == 0u) {
                            OctiDir dir = static_cast<OctiDir>(d);
                            if (players[turn].freeProngs > 0) {
                                OctiMoveOp op;
                                op.x0 = x; op.y0 = y;
                                op.pod = pod;
                                op.addProngDir = dir;
                                op.superProng = false;
                                outOps.push_back(std::move(op));
                            }
                            if (useSuperProngs && players[turn].freeSuperProngs > 0) {
                                OctiMoveOp op;
                                op.x0 = x; op.y0 = y;
                                op.pod = pod;
                                op.addProngDir = dir;
                                op.superProng = true;
                                outOps.push_back(std::move(op));
                            }
                        }
                    }
                }

                // 3. Prong Repositions
                if (mode != GAME_MODE_OFK && cfg != 0u) {
                    for (int rm = 0; rm < DIR_MAX; rm++) {
                        if (((cfg >> (static_cast<unsigned int>(rm) * 2u)) & 0x3u) ==
                            static_cast<std::uint32_t>(NORMAL_PRONG)) {
                            OctiDir rmDir = static_cast<OctiDir>(rm);
                            for (int add = 0; add < DIR_MAX; add++) {
                                if (add != rm &&
                                    ((cfg >> (static_cast<unsigned int>(add) * 2u)) & 0x3u) == 0u) {
                                    OctiMoveOp op;
                                    op.x0 = x; op.y0 = y;
                                    op.pod = pod;
                                    op.rmProngDir = rmDir;
                                    op.addProngDir = static_cast<OctiDir>(add);
                                    op.superProng = false;
                                    outOps.push_back(std::move(op));
                                }
                            }
                        }
                    }
                }

                // 4. Distance-1 Moves & 5. Distance-2 Jumps
                if (cfg != 0u) {
                    for (int d = 0; d < DIR_MAX; d++) {
                        if (((cfg >> (static_cast<unsigned int>(d) * 2u)) & 0x3u) != 0u) {
                            OctiDir dir = static_cast<OctiDir>(d);
                            int dx = dirDeltaX(dir);
                            int dy = dirDeltaY(dir);
                            int nx = useEdgelessBoard ? (((x + dx) % w + w) % w) : (x + dx);
                            int ny = useEdgelessBoard ? (((y + dy) % h + h) % h) : (y + dy);

                            if (!useEdgelessBoard && (nx < 0 || nx >= w || ny < 0 || ny >= h)) {
                                continue;
                            }

                            const OctiSquare &destSq = board->squareAt(nx, ny);
                            if (mode == GAME_MODE_OFK) {
                                if (!destSq.isEmpty()) continue;
                            } else {
                                if (destSq.hasEnemyOf(turn)) continue;
                            }

                            OctiHop hop;
                            hop.x = nx;
                            hop.y = ny;
                            hop.capture = false;

                            OctiMoveOp op;
                            op.x0 = x;
                            op.y0 = y;
                            op.pod = pod;
                            op.hops.push_back(hop);
                            outOps.push_back(std::move(op));
                        }
                    }

                    const std::vector<OctiPod> &ghostPods = sq.pods();
                    std::uint64_t jumpedMask[2] = {0ULL, 0ULL};
                    currentHopsScratch.clear();
                    dfsJumpChainsOpsForSp(board, useEdgelessBoard, x, y, pod, ghostPods,
                                          jumpedMask, currentHopsScratch, x, y, outOps);
                }
            }
        }
    }
}

static void dfsCaptureOrBaseJumpChains(const OctiBoard *board,
                                       const std::uint64_t uncapturedEnemyHomeMask[2],
                                       bool edgeless,
                                       int cx, int cy,
                                       const OctiPod &pod,
                                       const std::vector<OctiPod> &ghostPods,
                                       std::uint64_t jumpedMask[2],
                                       std::vector<OctiHop> &currentHops,
                                       bool hasAnyCaptureSoFar,
                                       int sx, int sy,
                                       std::vector<OctiMoveOp> &outOps)
{
    int w = board->getBoardWidth();
    int h = board->getBoardHeight();
    std::uint32_t cfg = pod.rawConfig() & 0xFFFFu;

    for (int d = 0; d < DIR_MAX; d++) {
        if (((cfg >> (static_cast<unsigned int>(d) * 2u)) & 0x3u) == 0u) {
            continue;
        }
        OctiDir dir = static_cast<OctiDir>(d);

        int jdx = dirDeltaX(dir);
        int jdy = dirDeltaY(dir);
        int dx = jdx * 2;
        int dy = jdy * 2;

        int jx = edgeless ? (((cx + jdx) % w + w) % w) : (cx + jdx);
        int jy = edgeless ? (((cy + jdy) % h + h) % h) : (cy + jdy);
        int lx = edgeless ? (((cx + dx) % w + w) % w) : (cx + dx);
        int ly = edgeless ? (((cy + dy) % h + h) % h) : (cy + dy);

        if (!edgeless) {
            if (jx < 0 || jx >= w || jy < 0 || jy >= h) continue;
            if (lx < 0 || lx >= w || ly < 0 || ly >= h) continue;
        }

        int jBitIdx = jy * 9 + jx;
        int jWord = (jBitIdx >> 6) & 1;
        std::uint64_t jBit = 1ULL << (jBitIdx & 63);
        if ((jumpedMask[jWord] & jBit) != 0ULL) {
            continue;
        }

        const OctiSquare &jumpedSq = board->squareAt(jx, jy);
        if (isEmptyForJump(jumpedSq, jx, jy, sx, sy, ghostPods)) {
            continue;
        }

        const OctiSquare &landSq = board->squareAt(lx, ly);
        if (!isEmptyForJump(landSq, lx, ly, sx, sy, ghostPods)) {
            continue;
        }

        jumpedMask[jWord] |= jBit;
        const bool landUncapturedBase = testSqBit9x9(uncapturedEnemyHomeMask, lx, ly);

        // Branch 1: Without capture
        {
            OctiHop hop;
            hop.x = lx;
            hop.y = ly;
            hop.capture = false;

            currentHops.push_back(hop);
            if (hasAnyCaptureSoFar || landUncapturedBase) {
                OctiMoveOp op;
                op.x0 = sx;
                op.y0 = sy;
                op.pod = pod;
                op.hops = currentHops;
                outOps.push_back(std::move(op));
            }

            dfsCaptureOrBaseJumpChains(board, uncapturedEnemyHomeMask, edgeless,
                                       lx, ly, pod, ghostPods, jumpedMask,
                                       currentHops, hasAnyCaptureSoFar, sx, sy, outOps);
            currentHops.pop_back();
        }

        // Branch 2: With capture (includes friendly self-captures to preserve exact SeenStateTable dedup)
        {
            OctiHop hop;
            hop.x = lx;
            hop.y = ly;
            hop.capture = true;

            currentHops.push_back(hop);
            {
                OctiMoveOp op;
                op.x0 = sx;
                op.y0 = sy;
                op.pod = pod;
                op.hops = currentHops;
                outOps.push_back(std::move(op));
            }

            dfsCaptureOrBaseJumpChains(board, uncapturedEnemyHomeMask, edgeless,
                                       lx, ly, pod, ghostPods, jumpedMask,
                                       currentHops, true, sx, sy, outOps);
            currentHops.pop_back();
        }

        jumpedMask[jWord] &= ~jBit;
    }
}

static void generateZeroPreludeCaptureOrBaseMoves(
    const OctiBoard *board,
    const std::uint64_t uncapturedEnemyHomeMask[2],
    OctiGameMode mode,
    OctiPlayer turn,
    bool useEdgelessBoard,
    std::vector<OctiHop> &currentHopsScratch,
    std::vector<OctiMoveOp> &outOps)
{
    outOps.clear();
    int w = board->getBoardWidth();
    int h = board->getBoardHeight();
    OctiPodType turnPodType = playerToPodType(turn);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            const OctiSquare &sq = board->squareAt(x, y);
            int numPods = sq.getNumPods();
            if (numPods == 0) {
                continue;
            }
            std::uint32_t seenConfigs[32];
            int numSeen = 0;

            for (int i = 0; i < numPods; i++) {
                const OctiPod &pod = sq.getPodAt(i);
                std::uint32_t rc = pod.rawConfig();
                if (rawPodGetType(rc) != turnPodType) {
                    continue;
                }
                std::uint32_t cfg = rc & 0xFFFFu;
                if (cfg == 0u) {
                    continue;
                }
                bool dupCfg = false;
                for (int s = 0; s < numSeen; ++s) {
                    if (seenConfigs[s] == cfg) {
                        dupCfg = true;
                        break;
                    }
                }
                if (dupCfg) {
                    continue;
                }
                if (numSeen < 32) {
                    seenConfigs[numSeen++] = cfg;
                }

                // Distance-1 step onto an uncaptured enemy home square
                for (int d = 0; d < DIR_MAX; d++) {
                    OctiDir dir = static_cast<OctiDir>(d);
                    if (((cfg >> (static_cast<unsigned int>(d) * 2u)) & 0x3u) == 0u) {
                        continue;
                    }
                    int dx = dirDeltaX(dir);
                    int dy = dirDeltaY(dir);
                    int nx = useEdgelessBoard ? (((x + dx) % w + w) % w) : (x + dx);
                    int ny = useEdgelessBoard ? (((y + dy) % h + h) % h) : (y + dy);
                    if (!useEdgelessBoard && (nx < 0 || nx >= w || ny < 0 || ny >= h)) {
                        continue;
                    }
                    if (!testSqBit9x9(uncapturedEnemyHomeMask, nx, ny)) {
                        continue;
                    }
                    const OctiSquare &destSq = board->squareAt(nx, ny);
                    if (mode == GAME_MODE_OFK) {
                        if (!destSq.isEmpty()) continue;
                    } else {
                        if (destSq.hasEnemyOf(turn)) continue;
                    }

                    OctiHop hop;
                    hop.x = nx;
                    hop.y = ny;
                    hop.capture = false;

                    OctiMoveOp op;
                    op.x0 = x;
                    op.y0 = y;
                    op.pod = pod;
                    op.hops.push_back(hop);
                    outOps.push_back(std::move(op));
                }

                // Distance-2 jump chains with any capture or landing on uncaptured enemy home
                const std::vector<OctiPod> &ghostPods = sq.pods();
                std::uint64_t jumpedMask[2] = {0ULL, 0ULL};
                currentHopsScratch.clear();
                dfsCaptureOrBaseJumpChains(board, uncapturedEnemyHomeMask,
                                           useEdgelessBoard, x, y, pod, ghostPods,
                                           jumpedMask, currentHopsScratch,
                                           false, x, y, outOps);
            }
        }
    }
}

static void appendDeduplicatedRegularMoves(const OctiBoard *board,
                                           const OctiPlayerState players[5],
                                           OctiGameMode mode,
                                           OctiPlayer turn,
                                           bool useSuperProngs,
                                           bool useEdgelessBoard,
                                           const OctiMoveOp *preludeOps,
                                           std::size_t numPreludes,
                                           bool tacticalOnly,
                                           bool restrictToCaptureOrBaseRegOps,
                                           const std::uint64_t enemySqMask[2],
                                           const std::uint64_t uncapturedEnemyHomeMask[2],
                                           SeenStateTable &seenCanonicalStates,
                                           std::vector<OctiHop> &hopsScratch,
                                           std::vector<OctiMoveOp> &regOpScratch,
                                           std::vector<OctiMove> &outMoves)
{
    if (tacticalOnly && restrictToCaptureOrBaseRegOps) {
        generateZeroPreludeCaptureOrBaseMoves(
            board, uncapturedEnemyHomeMask, mode, turn,
            useEdgelessBoard, hopsScratch, regOpScratch);
    } else {
        generateZeroPreludeMoveOpsForSp(
            board, players, mode, turn,
            useSuperProngs, useEdgelessBoard, hopsScratch, regOpScratch);
    }
    if (regOpScratch.empty()) {
        return;
    }
    FastGrid baseGrid;
    initFastGrid(board, mode, baseGrid);
    int w = board->getBoardWidth();
    int h = board->getBoardHeight();
    for (auto &regOp : regOpScratch) {
        CanonicalStateSig sig = computePostMoveCanonicalSig(
            board, baseGrid, players, mode, turn, useEdgelessBoard, regOp);
        if (!seenCanonicalStates.insert(sig)) {
            continue;
        }
        if (tacticalOnly &&
            !isOpsTacticalBitmask(enemySqMask, uncapturedEnemyHomeMask,
                                  useEdgelessBoard, w, h,
                                  preludeOps, numPreludes, regOp)) {
            continue;
        }
        if (numPreludes == 0) {
            outMoves.push_back(OctiMove::fromValidatedSingleOp(std::move(regOp)));
        } else {
            std::vector<OctiMoveOp> ops;
            ops.reserve(numPreludes + 1);
            for (std::size_t pi = 0; pi < numPreludes; ++pi) {
                ops.push_back(preludeOps[pi]);
            }
            ops.push_back(std::move(regOp));
            outMoves.push_back(OctiMove::fromValidatedOps(std::move(ops)));
        }
    }
}

static void dfsStraightSuperProngJumps(const OctiBoard *board,
                                       bool edgeless,
                                       int sx, int sy,
                                       int origStartPodCount,
                                       int cx, int cy,
                                       const OctiPod &pod,
                                       OctiDir dir,
                                       std::uint64_t jumpedMask[2],
                                       std::vector<OctiHop> &currentHops,
                                       std::uint8_t curCapX[16],
                                       std::uint8_t curCapY[16],
                                       int numCurCaps,
                                       std::vector<PreludeCandidate> &outPreludes)
{
    int w = board->getBoardWidth();
    int h = board->getBoardHeight();

    int jdx = dirDeltaX(dir);
    int jdy = dirDeltaY(dir);
    int dx = jdx * 2;
    int dy = jdy * 2;

    int jx = edgeless ? (((cx + jdx) % w + w) % w) : (cx + jdx);
    int jy = edgeless ? (((cy + jdy) % h + h) % h) : (cy + jdy);
    int lx = edgeless ? (((cx + dx) % w + w) % w) : (cx + dx);
    int ly = edgeless ? (((cy + dy) % h + h) % h) : (cy + dy);

    if (!edgeless) {
        if (jx < 0 || jx >= w || jy < 0 || jy >= h) return;
        if (lx < 0 || lx >= w || ly < 0 || ly >= h) return;
    }

    int jBitIdx = jy * 9 + jx;
    int jWord = (jBitIdx >> 6) & 1;
    std::uint64_t jBit = 1ULL << (jBitIdx & 63);
    if ((jumpedMask[jWord] & jBit) != 0ULL) {
        return;
    }

    bool jumpedEmpty = (jx == sx && jy == sy)
                           ? (origStartPodCount <= 1)
                           : board->squareAt(jx, jy).isEmpty();
    if (jumpedEmpty) {
        return;
    }

    bool landingEmpty = (lx == sx && ly == sy)
                            ? (origStartPodCount <= 1)
                            : board->squareAt(lx, ly).isEmpty();
    if (!landingEmpty) {
        return;
    }

    jumpedMask[jWord] |= jBit;

    // Branch 1: Without capture
    {
        OctiHop hop;
        hop.x = lx;
        hop.y = ly;
        hop.capture = false;

        currentHops.push_back(hop);

        PreludeCandidate cand;
        cand.op.x0 = sx;
        cand.op.y0 = sy;
        cand.op.pod = pod;
        cand.op.isSuperProngBonusMove = true;
        cand.op.hops = currentHops;
        cand.endX = lx;
        cand.endY = ly;
        cand.superDir = dir;
        cand.numCaptures = numCurCaps;
        for (int k = 0; k < numCurCaps; ++k) {
            cand.capX[k] = curCapX[k];
            cand.capY[k] = curCapY[k];
        }
        outPreludes.push_back(std::move(cand));

        dfsStraightSuperProngJumps(board, edgeless, sx, sy, origStartPodCount,
                                   lx, ly, pod, dir, jumpedMask,
                                   currentHops, curCapX, curCapY, numCurCaps,
                                   outPreludes);

        currentHops.pop_back();
    }

    // Branch 2: With capture
    {
        OctiHop hop;
        hop.x = lx;
        hop.y = ly;
        hop.capture = true;

        currentHops.push_back(hop);
        int nextCaps = numCurCaps;
        if (nextCaps < 16) {
            curCapX[nextCaps] = static_cast<std::uint8_t>(jx);
            curCapY[nextCaps] = static_cast<std::uint8_t>(jy);
            ++nextCaps;
        }

        PreludeCandidate cand;
        cand.op.x0 = sx;
        cand.op.y0 = sy;
        cand.op.pod = pod;
        cand.op.isSuperProngBonusMove = true;
        cand.op.hops = currentHops;
        cand.endX = lx;
        cand.endY = ly;
        cand.superDir = dir;
        cand.numCaptures = nextCaps;
        for (int k = 0; k < nextCaps; ++k) {
            cand.capX[k] = curCapX[k];
            cand.capY[k] = curCapY[k];
        }
        outPreludes.push_back(std::move(cand));

        dfsStraightSuperProngJumps(board, edgeless, sx, sy, origStartPodCount,
                                   lx, ly, pod, dir, jumpedMask,
                                   currentHops, curCapX, curCapY, nextCaps,
                                   outPreludes);

        currentHops.pop_back();
    }

    jumpedMask[jWord] &= ~jBit;
}

static void enumerateSuperProngPreludes(const OctiBoard *board,
                                        bool edgeless,
                                        OctiPlayer turn,
                                        std::vector<OctiHop> &currentHopsScratch,
                                        std::vector<PreludeCandidate> &outPreludes)
{
    outPreludes.clear();
    int w = board->getBoardWidth();
    int h = board->getBoardHeight();
    const OctiPodType turnPodType = playerToPodType(turn);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            const OctiSquare &sq = board->squareAt(x, y);
            int numPods = sq.getNumPods();
            if (numPods == 0) {
                continue;
            }
            std::uint32_t seenConfigs[16];
            int numSeen = 0;

            for (int i = 0; i < numPods; i++) {
                const OctiPod &pod = sq.getPodAt(i);
                std::uint32_t rc = pod.rawConfig();
                if (rawPodGetType(rc) != turnPodType || rawPodNumSuperProngs(rc) == 0) {
                    continue;
                }
                std::uint32_t cfg = rc & 0xFFFFu;
                bool dupCfg = false;
                for (int s = 0; s < numSeen; s++) {
                    if (seenConfigs[s] == cfg) {
                        dupCfg = true;
                        break;
                    }
                }
                if (dupCfg) {
                    continue;
                }
                seenConfigs[numSeen++] = cfg;

                for (int d = 0; d < DIR_MAX; d++) {
                    OctiDir dir = static_cast<OctiDir>(d);
                    if (((cfg >> (static_cast<unsigned int>(d) * 2u)) & 0x3u) !=
                        static_cast<std::uint32_t>(SUPER_PRONG)) {
                        continue;
                    }

                    // 1. Single step along superprong direction
                    {
                        int dx = dirDeltaX(dir);
                        int dy = dirDeltaY(dir);
                        int nx = edgeless ? (((x + dx) % w + w) % w) : (x + dx);
                        int ny = edgeless ? (((y + dy) % h + h) % h) : (y + dy);

                        bool inBounds = edgeless || (nx >= 0 && nx < w && ny >= 0 && ny < h);
                        if (inBounds && !board->squareAt(nx, ny).hasEnemyOf(turn)) {
                            PreludeCandidate cand;
                            cand.op.x0 = x;
                            cand.op.y0 = y;
                            cand.op.pod = pod;
                            cand.op.isSuperProngBonusMove = true;
                            OctiHop hop;
                            hop.x = nx;
                            hop.y = ny;
                            hop.capture = false;
                            cand.op.hops.push_back(hop);
                            cand.endX = nx;
                            cand.endY = ny;
                            cand.superDir = dir;
                            outPreludes.push_back(std::move(cand));
                        }
                    }

                    // 2. Straight jump chain prefixes along superprong direction
                    {
                        std::uint64_t jumpedMask[2] = {0ULL, 0ULL};
                        std::uint8_t curCapX[16];
                        std::uint8_t curCapY[16];
                        currentHopsScratch.clear();
                        dfsStraightSuperProngJumps(board, edgeless, x, y, numPods,
                                                   x, y, pod, dir, jumpedMask,
                                                   currentHopsScratch,
                                                   curCapX, curCapY, 0,
                                                   outPreludes);
                    }
                }
            }
        }
    }
}

struct BoardSquareUndoLog {
    struct SavedSq {
        int x;
        int y;
        int count;
        std::uint32_t rawPods[16];
    };
    SavedSq entries[32];
    int size = 0;

    void saveSquareOnceSinceMark(const OctiBoard &board, int x, int y, int mark)
    {
        for (int i = mark; i < size; ++i) {
            if (entries[i].x == x && entries[i].y == y) {
                return;
            }
        }
        const OctiSquare &sq = board.squareAt(x, y);
        int n = sq.getNumPods();
        SavedSq &dst = entries[size++];
        dst.x = x;
        dst.y = y;
        dst.count = n;
        for (int i = 0; i < n; ++i) {
            dst.rawPods[i] = sq.getPodAt(i).rawConfig();
        }
    }

    void rollbackToMark(OctiBoard &board, int mark)
    {
        while (size > mark) {
            --size;
            const SavedSq &src = entries[size];
            OctiSquare &sq = board.squareAt(src.x, src.y);
            sq.clear();
            for (int i = 0; i < src.count; ++i) {
                sq.addPod(OctiPod(src.rawPods[i]));
            }
        }
    }
};

static inline void applyPreludeCaptureCoord(OctiBoard &board,
                                            OctiPlayerState players[5],
                                            OctiPlayer turn,
                                            int cx, int cy,
                                            std::uint64_t capMask[2],
                                            BoardSquareUndoLog &undoLog,
                                            int undoMark)
{
    int bitIdx = cy * 9 + cx;
    int word = (bitIdx >> 6) & 1;
    std::uint64_t bit = 1ULL << (bitIdx & 63);
    if ((capMask[word] & bit) != 0ULL) {
        return;
    }
    capMask[word] |= bit;

    OctiSquare &sq = board.squareAt(cx, cy);
    int n = sq.getNumPods();
    if (n <= 0) {
        return;
    }
    undoLog.saveSquareOnceSinceMark(board, cx, cy, undoMark);
    for (int i = 0; i < n; ++i) {
        std::uint32_t rc = sq.getPodAt(i).rawConfig();
        OctiPlayer owner = podTypeToPlayer(rawPodGetType(rc));
        players[turn].freeProngs += rawPodNumNormalProngs(rc);
        players[turn].freeSuperProngs += rawPodNumSuperProngs(rc);
        players[owner].capturedPods++;
    }
    sq.clear();
}

bool OctiGame::hasAnyLegalMove() const
{
    if (!_gameStarted || _gameOver || _turn == PLAYER_UNDEF || !_board) {
        return false;
    }

    int w = _board->getBoardWidth();
    int h = _board->getBoardHeight();

    // 1. Reserve pod placements & liberations (checked before pod/prong/move scan)
    if (_mode != GAME_MODE_OFK) {
        int numPlayers = _board->getNumPlayers();
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                const OctiSquare &sq = _board->squareAt(x, y);
                OctiSquareType st = sq.getType();
                if (st == NEUTRAL_SQUARE) {
                    continue;
                }
                bool isFriendlyHome = false;
                if (numPlayers == 2) {
                    isFriendlyHome = (_turn == PLAYER_A && st == A_HOME_SQUARE) ||
                                     (_turn == PLAYER_B && st == B_HOME_SQUARE);
                } else if (numPlayers == 4) {
                    isFriendlyHome = (st == static_cast<OctiSquareType>(_turn));
                }

                if (isFriendlyHome) {
                    if (sq.hasEnemyOf(_turn)) {
                        continue;
                    }
                } else {
                    if (!sq.isOccupiedBy(_turn)) {
                        continue;
                    }
                }

                if (_mode == GAME_MODE_2) {
                    bool isFriendlyBase =
                        (st == (_turn == PLAYER_A ? A_HOME_SQUARE : B_HOME_SQUARE));
                    if (isFriendlyBase) {
                        if (_players[_turn].freePods > 0) {
                            return true;
                        }
                        if (_board->canLiberate(_turn) &&
                            _players[_turn].capturedPods > 0 &&
                            sq.isOccupiedBy(_turn)) {
                            return true;
                        }
                    } else {
                        if (_players[_turn].freePods > 0 ||
                            _players[_turn].capturedPods > 0) {
                            return true;
                        }
                    }
                } else if (_mode == GAME_MODE_4) {
                    OctiSquareType homeType =
                        (_turn == PLAYER_A ? A_HOME_SQUARE :
                         _turn == PLAYER_B ? B_HOME_SQUARE :
                         _turn == PLAYER_C ? C_HOME_SQUARE : D_HOME_SQUARE);
                    OctiPlayer partner =
                        (_turn == PLAYER_A ? PLAYER_C :
                         _turn == PLAYER_C ? PLAYER_A :
                         _turn == PLAYER_B ? PLAYER_D : PLAYER_B);
                    OctiSquareType partnerHome =
                        (partner == PLAYER_A ? A_HOME_SQUARE :
                         partner == PLAYER_B ? B_HOME_SQUARE :
                         partner == PLAYER_C ? C_HOME_SQUARE : D_HOME_SQUARE);

                    if (st == homeType) {
                        if (_players[_turn].freePods > 0) {
                            return true;
                        }
                        if (sq.isOccupiedBy(_turn) &&
                            _board->canLiberate(_turn) &&
                            _players[_turn].capturedPods > 0) {
                            return true;
                        }
                    } else if (st == partnerHome) {
                        if (_players[partner].eliminated) {
                            if (_players[partner].capturedPods > 0) {
                                return true;
                            }
                        } else {
                            if (_board->canLiberate(_turn) &&
                                _players[_turn].capturedPods > 0) {
                                return true;
                            }
                        }
                    } else if (st == NEUTRAL_OCTI_SQUARE) {
                        if (_players[_turn].capturedPods > 0) {
                            return true;
                        }
                    } else {
                        if (_players[_turn].freePods > 0 ||
                            _players[_turn].capturedPods > 0) {
                            return true;
                        }
                    }
                }
            }
        }
    }

    // 2. Prong additions, 3. Prong repositions, 4. Steps, 5. Jumps
    const OctiPodType turnPodType = playerToPodType(_turn);
    const bool hasReserveProng =
        (_players[_turn].freeProngs > 0) ||
        (_useSuperProngs && _players[_turn].freeSuperProngs > 0);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            const OctiSquare &sq = _board->squareAt(x, y);
            int numPods = sq.getNumPods();
            if (numPods == 0) {
                continue;
            }
            for (int i = 0; i < numPods; i++) {
                const OctiPod &pod = sq.getPodAt(i);
                if (pod.getType() != turnPodType) {
                    continue;
                }
                int totalProngs = pod.getNumProngs();
                if (hasReserveProng && totalProngs < DIR_MAX) {
                    return true;
                }
                if (_mode != GAME_MODE_OFK &&
                    totalProngs < DIR_MAX &&
                    pod.getNumNormalProngs() > 0) {
                    return true;
                }
                if ((pod.rawConfig() & 0xFFFFu) == 0u) {
                    continue;
                }
                for (int d = 0; d < DIR_MAX; d++) {
                    OctiDir dir = static_cast<OctiDir>(d);
                    if (pod.prongAt(dir) == NO_PRONG) {
                        continue;
                    }
                    int dx = dirDeltaX(dir);
                    int dy = dirDeltaY(dir);
                    int nx = _useEdgelessBoard ? (((x + dx) % w + w) % w) : (x + dx);
                    int ny = _useEdgelessBoard ? (((y + dy) % h + h) % h) : (y + dy);
                    if (!_useEdgelessBoard &&
                        (nx < 0 || nx >= w || ny < 0 || ny >= h)) {
                        continue;
                    }

                    const OctiSquare &adjSq = _board->squareAt(nx, ny);
                    if (_mode == GAME_MODE_OFK) {
                        if (adjSq.isEmpty()) {
                            return true;
                        }
                    } else {
                        if (!adjSq.hasEnemyOf(_turn)) {
                            return true;
                        }
                    }

                    // Adjacent square in prong direction is non-empty if it blocked
                    // OFK step or had an enemy pod; check if a jump over (nx, ny) lands
                    // on an empty square. (Also covers jumping over friendly pods.)
                    if (!adjSq.isEmpty()) {
                        int lx = _useEdgelessBoard
                            ? (((x + dx * 2) % w + w) % w)
                            : (x + dx * 2);
                        int ly = _useEdgelessBoard
                            ? (((y + dy * 2) % h + h) % h)
                            : (y + dy * 2);
                        if (_useEdgelessBoard ||
                            (lx >= 0 && lx < w && ly >= 0 && ly < h)) {
                            if (_board->squareAt(lx, ly).isEmpty()) {
                                return true;
                            }
                        }
                    }
                }
            }
        }
    }

    return false;
}

static void generateSuperProngMovesImpl(const OctiBoard *origBoard,
                                        const OctiPlayerState origPlayers[5],
                                        OctiGameMode mode,
                                        OctiGameBases bases,
                                        OctiPlayer turn,
                                        bool useSuperProngs,
                                        bool useEdgelessBoard,
                                        bool tacticalOnly,
                                        std::vector<OctiMove> &outMoves)
{
    struct SpMoveGenScratch {
        SeenStateTable seenStates;
        std::vector<OctiMoveOp> regScratch;
        std::vector<PreludeCandidate> preludes1;
        std::vector<PreludeCandidate> preludes2;
        std::vector<OctiHop> jumpHops;
        OctiBoard workBoard;
        OctiGameMode workMode = GAME_MODE_UNDEF;
        bool workSp = false;
        bool workEdge = false;
        OctiGameBases workBases = GAME_BASES_3;

        SpMoveGenScratch()
            : workBoard(GAME_MODE_2, true, false, GAME_BASES_3)
        {
            workBoard.suppressBoardUpdateNotification(true);
            regScratch.reserve(160);
            preludes1.reserve(32);
            preludes2.reserve(32);
            jumpHops.reserve(16);
        }

        void syncWorkBoard(const OctiBoard *src, OctiGameMode m,
                           bool sp, bool edge, OctiGameBases b)
        {
            if (workMode != m || workSp != sp || workEdge != edge || workBases != b) {
                workBoard.setGameMode(m, sp, edge, b);
                workBoard.suppressBoardUpdateNotification(true);
                workMode = m;
                workSp = sp;
                workEdge = edge;
                workBases = b;
            }
            copyBoardSquares(src, workBoard);
        }
    };

    thread_local SpMoveGenScratch scratch;
    scratch.seenStates.clearForNewNode();

    std::uint64_t enemySqMask[2] = {0ULL, 0ULL};
    std::uint64_t uncapturedEnemyHomeMask[2] = {0ULL, 0ULL};
    bool origHas4pEnemyOnHome = false;
    if (tacticalOnly) {
        int w = origBoard->getBoardWidth();
        int h = origBoard->getBoardHeight();
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const OctiSquare &sq = origBoard->squareAt(x, y);
                int bitIdx = y * 9 + x;
                int word = (bitIdx >> 6) & 1;
                std::uint64_t bit = 1ULL << (bitIdx & 63);
                if (sq.hasEnemyOf(turn)) {
                    enemySqMask[word] |= bit;
                }
                if (isEnemyHomeSquareForSp(sq.getType(), mode, turn) &&
                    !sq.isCapturedByTeamOf(turn)) {
                    uncapturedEnemyHomeMask[word] |= bit;
                }
            }
        }
        if (mode == GAME_MODE_4) {
            static const int kHomeXY[5][2] = {
                {-1, -1}, {2, 2}, {2, 6}, {6, 6}, {6, 2}
            };
            for (int p = 1; p <= 4; ++p) {
                OctiPlayer player = static_cast<OctiPlayer>(p);
                const OctiSquare &hSq = origBoard->squareAt(kHomeXY[p][0], kHomeXY[p][1]);
                if (hSq.hasEnemyOf(player)) {
                    origHas4pEnemyOnHome = true;
                    break;
                }
            }
        }
    }

    const bool canRestrictWhenPreludeClean = tacticalOnly && !origHas4pEnemyOnHome;

    // 0-prelude regular moves
    appendDeduplicatedRegularMoves(origBoard, origPlayers, mode, turn,
                                   useSuperProngs, useEdgelessBoard,
                                   nullptr, 0, tacticalOnly,
                                   canRestrictWhenPreludeClean,
                                   enemySqMask, uncapturedEnemyHomeMask,
                                   scratch.seenStates, scratch.jumpHops,
                                   scratch.regScratch, outMoves);

    // Enumerate 1st superprong bonus prelude candidates
    enumerateSuperProngPreludes(origBoard, useEdgelessBoard, turn,
                                scratch.jumpHops, scratch.preludes1);
    if (scratch.preludes1.empty()) {
        return;
    }

    scratch.syncWorkBoard(origBoard, mode, useSuperProngs, useEdgelessBoard, bases);
    BoardSquareUndoLog undoLog;

    // 1-prelude moves
    for (const auto &p1 : scratch.preludes1) {
        int mark1 = undoLog.size;
        undoLog.saveSquareOnceSinceMark(scratch.workBoard, p1.op.x0, p1.op.y0, mark1);
        undoLog.saveSquareOnceSinceMark(scratch.workBoard, p1.endX, p1.endY, mark1);
        scratch.workBoard.squareAt(p1.op.x0, p1.op.y0).removePod(p1.op.pod);
        scratch.workBoard.squareAt(p1.endX, p1.endY).addPod(p1.op.pod);

        OctiPlayerState players1[5];
        for (int p = 0; p < 5; p++) {
            players1[p] = origPlayers[p];
        }
        if (p1.numCaptures > 0) {
            std::uint64_t capMask1[2] = {0ULL, 0ULL};
            for (int c = 0; c < p1.numCaptures; ++c) {
                applyPreludeCaptureCoord(scratch.workBoard, players1, turn,
                                         static_cast<int>(p1.capX[c]),
                                         static_cast<int>(p1.capY[c]),
                                         capMask1, undoLog, mark1);
            }
        }

        const bool p1HasCapOrBase =
            (p1.numCaptures > 0) ||
            (tacticalOnly && testSqBit9x9(uncapturedEnemyHomeMask, p1.endX, p1.endY));
        const bool restrict1 = canRestrictWhenPreludeClean && !p1HasCapOrBase;

        appendDeduplicatedRegularMoves(&scratch.workBoard, players1, mode, turn,
                                       useSuperProngs, useEdgelessBoard,
                                       &p1.op, 1, tacticalOnly, restrict1,
                                       enemySqMask, uncapturedEnemyHomeMask,
                                       scratch.seenStates, scratch.jumpHops,
                                       scratch.regScratch, outMoves);
        undoLog.rollbackToMark(scratch.workBoard, mark1);
    }

    // 2-prelude moves
    const OctiPodType turnPodType = playerToPodType(turn);
    OctiMoveOp preludePair[2];
    for (const auto &p1 : scratch.preludes1) {
        int markP1 = undoLog.size;
        undoLog.saveSquareOnceSinceMark(scratch.workBoard, p1.op.x0, p1.op.y0, markP1);
        undoLog.saveSquareOnceSinceMark(scratch.workBoard, p1.endX, p1.endY, markP1);
        scratch.workBoard.squareAt(p1.op.x0, p1.op.y0).removePod(p1.op.pod);
        scratch.workBoard.squareAt(p1.endX, p1.endY).addPod(p1.op.pod);

        const std::uint32_t p1Cfg = p1.op.pod.rawConfig() & 0xFFFFu;
        int sameCfgCountAfterP1 = 0;
        {
            const OctiSquare &p1LandSq = scratch.workBoard.squareAt(p1.endX, p1.endY);
            const int numLandPods = p1LandSq.getNumPods();
            for (int i = 0; i < numLandPods; i++) {
                const OctiPod &lp = p1LandSq.getPodAt(i);
                if (lp.getType() == turnPodType &&
                    (lp.rawConfig() & 0xFFFFu) == p1Cfg) {
                    sameCfgCountAfterP1++;
                }
            }
        }

        enumerateSuperProngPreludes(&scratch.workBoard, useEdgelessBoard, turn,
                                    scratch.jumpHops, scratch.preludes2);
        preludePair[0] = p1.op;
        const bool p1HasCapOrBase =
            (p1.numCaptures > 0) ||
            (tacticalOnly && testSqBit9x9(uncapturedEnemyHomeMask, p1.endX, p1.endY));
        for (const auto &p2 : scratch.preludes2) {
            if (sameCfgCountAfterP1 < 2 &&
                p2.op.x0 == p1.endX &&
                p2.op.y0 == p1.endY &&
                (p2.op.pod.rawConfig() & 0xFFFFu) == p1Cfg &&
                p2.superDir == p1.superDir) {
                continue;
            }
            int markP2 = undoLog.size;
            undoLog.saveSquareOnceSinceMark(scratch.workBoard, p2.op.x0, p2.op.y0, markP2);
            undoLog.saveSquareOnceSinceMark(scratch.workBoard, p2.endX, p2.endY, markP2);
            scratch.workBoard.squareAt(p2.op.x0, p2.op.y0).removePod(p2.op.pod);
            scratch.workBoard.squareAt(p2.endX, p2.endY).addPod(p2.op.pod);

            OctiPlayerState players2[5];
            for (int p = 0; p < 5; p++) {
                players2[p] = origPlayers[p];
            }
            if (p1.numCaptures > 0 || p2.numCaptures > 0) {
                std::uint64_t capMask2[2] = {0ULL, 0ULL};
                for (int c = 0; c < p1.numCaptures; ++c) {
                    applyPreludeCaptureCoord(scratch.workBoard, players2, turn,
                                             static_cast<int>(p1.capX[c]),
                                             static_cast<int>(p1.capY[c]),
                                             capMask2, undoLog, markP2);
                }
                for (int c = 0; c < p2.numCaptures; ++c) {
                    applyPreludeCaptureCoord(scratch.workBoard, players2, turn,
                                             static_cast<int>(p2.capX[c]),
                                             static_cast<int>(p2.capY[c]),
                                             capMask2, undoLog, markP2);
                }
            }

            const bool p2HasCapOrBase =
                (p2.numCaptures > 0) ||
                (tacticalOnly && testSqBit9x9(uncapturedEnemyHomeMask, p2.endX, p2.endY));
            const bool restrict2 =
                canRestrictWhenPreludeClean && !p1HasCapOrBase && !p2HasCapOrBase;

            preludePair[1] = p2.op;
            appendDeduplicatedRegularMoves(&scratch.workBoard, players2, mode, turn,
                                           useSuperProngs, useEdgelessBoard,
                                           preludePair, 2, tacticalOnly, restrict2,
                                           enemySqMask, uncapturedEnemyHomeMask,
                                           scratch.seenStates, scratch.jumpHops,
                                           scratch.regScratch, outMoves);
            undoLog.rollbackToMark(scratch.workBoard, markP2);
        }
        undoLog.rollbackToMark(scratch.workBoard, markP1);
    }
}

void OctiGame::generateLegalMoves(std::vector<OctiMove> &outMoves) const
{
    outMoves.clear();
    if (!_gameStarted || _gameOver || _turn == PLAYER_UNDEF || !_board) {
        return;
    }

    if (!_useSuperProngs) {
        generateZeroPreludeMoves(_board, _players, _mode, _turn,
                                 _useSuperProngs, _useEdgelessBoard, outMoves);
        return;
    }

    generateSuperProngMovesImpl(_board, _players, _mode, _bases, _turn,
                                _useSuperProngs, _useEdgelessBoard,
                                false, outMoves);
}

std::vector<OctiMove> OctiGame::generateLegalMoves() const
{
    std::vector<OctiMove> moves;
    generateLegalMoves(moves);
    return moves;
}

static inline bool isEnemyHomeSquareForTactical(OctiSquareType sqType,
                                                OctiGameMode mode,
                                                OctiPlayer p)
{
    if (mode == GAME_MODE_2 || mode == GAME_MODE_OFK) {
        if (p == PLAYER_A) {
            return sqType == B_HOME_SQUARE;
        }
        if (p == PLAYER_B) {
            return sqType == A_HOME_SQUARE;
        }
        return false;
    }
    if (mode == GAME_MODE_4) {
        if (p == PLAYER_A || p == PLAYER_C) {
            return sqType == B_HOME_SQUARE || sqType == D_HOME_SQUARE;
        }
        if (p == PLAYER_B || p == PLAYER_D) {
            return sqType == A_HOME_SQUARE || sqType == C_HOME_SQUARE;
        }
    }
    return false;
}

static void dfsTacticalJumpChains(const OctiBoard *board,
                                  OctiGameMode mode,
                                  bool edgeless,
                                  OctiPlayer turn,
                                  int cx, int cy,
                                  const OctiPod &pod,
                                  const std::vector<OctiPod> &ghostPods,
                                  std::vector<std::pair<int, int>> &jumpedSquares,
                                  std::vector<OctiHop> &currentHops,
                                  bool hasEnemyCaptureSoFar,
                                  int sx, int sy,
                                  std::vector<OctiMove> &outMoves)
{
    int w = board->getBoardWidth();
    int h = board->getBoardHeight();

    for (int d = 0; d < DIR_MAX; d++) {
        OctiDir dir = static_cast<OctiDir>(d);
        if (pod.prongAt(dir) == NO_PRONG) {
            continue;
        }

        int jdx = dirDeltaX(dir);
        int jdy = dirDeltaY(dir);
        int dx = jdx * 2;
        int dy = jdy * 2;

        int jx = edgeless ? (((cx + jdx) % w + w) % w) : (cx + jdx);
        int jy = edgeless ? (((cy + jdy) % h + h) % h) : (cy + jdy);
        int lx = edgeless ? (((cx + dx) % w + w) % w) : (cx + dx);
        int ly = edgeless ? (((cy + dy) % h + h) % h) : (cy + dy);

        if (!edgeless) {
            if (jx < 0 || jx >= w || jy < 0 || jy >= h) continue;
            if (lx < 0 || lx >= w || ly < 0 || ly >= h) continue;
        }

        std::pair<int, int> jCoord = {jx, jy};
        if (std::find(jumpedSquares.begin(), jumpedSquares.end(), jCoord) !=
            jumpedSquares.end()) {
            continue;
        }

        const OctiSquare &jumpedSq = board->squareAt(jx, jy);
        if (isEmptyForJump(jumpedSq, jx, jy, sx, sy, ghostPods)) {
            continue;
        }

        const OctiSquare &landSq = board->squareAt(lx, ly);
        if (!isEmptyForJump(landSq, lx, ly, sx, sy, ghostPods)) {
            continue;
        }

        const bool landIsUncapturedEnemyBase =
            isEnemyHomeSquareForTactical(landSq.getType(), mode, turn) &&
            !landSq.isCapturedByTeamOf(turn);

        // Branch 1: Without capture
        {
            OctiHop hop;
            hop.x = lx;
            hop.y = ly;
            hop.capture = false;

            currentHops.push_back(hop);
            jumpedSquares.push_back(jCoord);

            if (hasEnemyCaptureSoFar || landIsUncapturedEnemyBase) {
                OctiMoveOp op;
                op.x0 = sx;
                op.y0 = sy;
                op.pod = pod;
                op.hops = currentHops;
                outMoves.push_back(OctiMove::fromValidatedSingleOp(std::move(op)));
            }

            dfsTacticalJumpChains(board, mode, edgeless, turn, lx, ly, pod,
                                  ghostPods, jumpedSquares, currentHops,
                                  hasEnemyCaptureSoFar, sx, sy, outMoves);

            jumpedSquares.pop_back();
            currentHops.pop_back();
        }

        // Branch 2: With capture
        {
            const bool hasEnemyCapture2 =
                hasEnemyCaptureSoFar || jumpedSq.hasEnemyOf(turn);

            OctiHop hop;
            hop.x = lx;
            hop.y = ly;
            hop.capture = true;

            currentHops.push_back(hop);
            jumpedSquares.push_back(jCoord);

            if (hasEnemyCapture2 || landIsUncapturedEnemyBase) {
                OctiMoveOp op;
                op.x0 = sx;
                op.y0 = sy;
                op.pod = pod;
                op.hops = currentHops;
                outMoves.push_back(OctiMove::fromValidatedSingleOp(std::move(op)));
            }

            dfsTacticalJumpChains(board, mode, edgeless, turn, lx, ly, pod,
                                  ghostPods, jumpedSquares, currentHops,
                                  hasEnemyCapture2, sx, sy, outMoves);

            jumpedSquares.pop_back();
            currentHops.pop_back();
        }
    }
}

void OctiGame::generateTacticalMoves(std::vector<OctiMove> &outMoves) const
{
    outMoves.clear();
    if (!_gameStarted || _gameOver || _turn == PLAYER_UNDEF || !_board) {
        return;
    }

    if (_useSuperProngs) {
        generateSuperProngMovesImpl(_board, _players, _mode, _bases, _turn,
                                    _useSuperProngs, _useEdgelessBoard,
                                    true, outMoves);
        return;
    }

    int w = _board->getBoardWidth();
    int h = _board->getBoardHeight();
    OctiPodType turnPodType = playerToPodType(_turn);

    std::vector<std::pair<int, int>> jumpedSquares;
    std::vector<OctiHop> currentHops;
    jumpedSquares.reserve(32);
    currentHops.reserve(32);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            const OctiSquare &sq = _board->squareAt(x, y);
            int numPods = sq.getNumPods();
            if (numPods == 0) {
                continue;
            }
            std::uint32_t seenConfigs[32];
            int numSeen = 0;

            for (int i = 0; i < numPods; i++) {
                const OctiPod &pod = sq.getPodAt(i);
                if (pod.getType() != turnPodType) {
                    continue;
                }
                std::uint32_t cfg = pod.rawConfig() & 0xFFFFu;
                bool dupCfg = false;
                for (int s = 0; s < numSeen; ++s) {
                    if (seenConfigs[s] == cfg) {
                        dupCfg = true;
                        break;
                    }
                }
                if (dupCfg) {
                    continue;
                }
                if (numSeen < 32) {
                    seenConfigs[numSeen++] = cfg;
                }
                if (cfg == 0u) {
                    continue;
                }

                // 1. Distance-1 step onto an uncaptured enemy home square
                for (int d = 0; d < DIR_MAX; d++) {
                    OctiDir dir = static_cast<OctiDir>(d);
                    if (pod.prongAt(dir) == NO_PRONG) {
                        continue;
                    }
                    int dx = dirDeltaX(dir);
                    int dy = dirDeltaY(dir);
                    int nx = _useEdgelessBoard ? (((x + dx) % w + w) % w) : (x + dx);
                    int ny = _useEdgelessBoard ? (((y + dy) % h + h) % h) : (y + dy);
                    if (!_useEdgelessBoard &&
                        (nx < 0 || nx >= w || ny < 0 || ny >= h)) {
                        continue;
                    }

                    const OctiSquare &destSq = _board->squareAt(nx, ny);
                    if (_mode == GAME_MODE_OFK) {
                        if (!destSq.isEmpty()) continue;
                    } else {
                        if (destSq.hasEnemyOf(_turn)) continue;
                    }

                    if (!isEnemyHomeSquareForTactical(destSq.getType(), _mode, _turn) ||
                        destSq.isCapturedByTeamOf(_turn)) {
                        continue;
                    }

                    OctiHop hop;
                    hop.x = nx;
                    hop.y = ny;
                    hop.capture = false;

                    OctiMoveOp op;
                    op.x0 = x;
                    op.y0 = y;
                    op.pod = pod;
                    op.hops.push_back(hop);
                    outMoves.push_back(OctiMove::fromValidatedSingleOp(std::move(op)));
                }

                // 2. Distance-2 jump chains with enemy capture or enemy base landing
                const std::vector<OctiPod> &ghostPods = sq.pods();
                jumpedSquares.clear();
                currentHops.clear();
                dfsTacticalJumpChains(_board, _mode, _useEdgelessBoard, _turn,
                                      x, y, pod, ghostPods, jumpedSquares,
                                      currentHops, false, x, y, outMoves);
            }
        }
    }
}

std::vector<OctiMove> OctiGame::generateTacticalMoves() const
{
    std::vector<OctiMove> moves;
    generateTacticalMoves(moves);
    return moves;
}

void OctiGame::setLastPlyMetadata(const OctiPlyMetadata &meta)
{
    while (_plyMetadata.size() < _history.size()) {
        _plyMetadata.push_back(OctiPlyMetadata{});
    }
    if (!_plyMetadata.empty()) {
        _plyMetadata.back() = meta;
    }
}

void OctiGame::setPlyMetadata(std::size_t idx, const OctiPlyMetadata &meta)
{
    while (_plyMetadata.size() <= idx) {
        _plyMetadata.push_back(OctiPlyMetadata{});
    }
    _plyMetadata[idx] = meta;
}

void OctiGame::setAllPlyMetadata(const std::vector<OctiPlyMetadata> &metas)
{
    _plyMetadata = metas;
}

OctiPlyMetadata OctiGame::getPlyMetadata(std::size_t idx) const
{
    if (idx < _plyMetadata.size()) {
        return _plyMetadata[idx];
    }
    return OctiPlyMetadata{};
}

const std::vector<OctiPlyMetadata> &OctiGame::getAllPlyMetadata() const
{
    return _plyMetadata;
}

void OctiGame::setGameMetadata(const OctiGameMetadata &meta)
{
    _gameMetadata = meta;
}

const OctiGameMetadata &OctiGame::getGameMetadata() const
{
    return _gameMetadata;
}

void OctiGame::setResignedWinner(OctiPlayer winner)
{
    _gameResigned = true;
    _gameOver = true;
    _winner = winner;
    notifyGameUpdated();
    notifyUndoRedoStatusChanged();
}

std::string OctiGame::exportCanonicalMovesText() const
{
    std::ostringstream ss;
    for (const auto &entry : _history) {
        ss << entry.second.asString() << "\n";
    }
    return ss.str();
}

std::string OctiGame::computeCanonicalGameId() const
{
    std::ostringstream ss;
    ss << "mode=";
    if (_mode == GAME_MODE_2) ss << "2P";
    else if (_mode == GAME_MODE_4) ss << "4P";
    else ss << "OFK";
    ss << ";bases=" << static_cast<int>(_bases);
    ss << ";superProngs=" << (_useSuperProngs ? "1" : "0");
    ss << ";edgeless=" << (_useEdgelessBoard ? "1" : "0");
    ss << ";startPlayer=";
    switch (_startPlayer) {
    case PLAYER_A: ss << "A"; break;
    case PLAYER_B: ss << "B"; break;
    case PLAYER_C: ss << "C"; break;
    case PLAYER_D: ss << "D"; break;
    default: ss << "A"; break;
    }
    ss << ";moves=";
    for (std::size_t i = 0; i < _history.size(); ++i) {
        if (i > 0) ss << ",";
        ss << _history[i].second.asString();
    }
    return OctiCrypto::sha256Hex(ss.str());
}

static inline std::string playerToString(OctiPlayer p)
{
    switch (p) {
    case PLAYER_A: return "A";
    case PLAYER_B: return "B";
    case PLAYER_C: return "C";
    case PLAYER_D: return "D";
    default:       return "NONE";
    }
}

std::string OctiGame::serializeJson(const OctiGameMetadata &meta) const
{
    nlohmann::json root;
    root["format"] = "octi.transcript.v3";
    root["gameId"] = meta.gameId.empty() ? computeCanonicalGameId() : meta.gameId;
    root["source"] = meta.source.empty() ? "LOCAL" : meta.source;

    if (!meta.license.empty()) {
        root["license"] = meta.license;
    }
    if (!meta.attribution.empty()) {
        root["attribution"] = meta.attribution;
    }

    if (!meta.appVersion.empty() || !meta.platform.empty() || meta.extended.present) {
        nlohmann::json clientObj;
        if (!meta.appVersion.empty()) {
            clientObj["appVersion"] = meta.appVersion;
        }
        clientObj["buildType"] = meta.buildType.empty() ? "release" : meta.buildType;
        if (!meta.platform.empty()) {
            clientObj["platform"] = meta.platform;
        }
        if (meta.extended.present) {
            nlohmann::json extObj;
            extObj["startedAt"] = meta.extended.startedAt;
            extObj["endedAt"] = meta.extended.endedAt;
            extObj["utcOffsetMin"] = meta.extended.utcOffsetMin;
            extObj["locale"] = meta.extended.locale;
            extObj["osVersion"] = meta.extended.osVersion;
            extObj["installId"] = meta.extended.installId;
            clientObj["extended"] = extObj;
        }
        root["client"] = clientObj;
    }

    if (_mode == GAME_MODE_2) {
        root["mode"] = "2P";
    } else if (_mode == GAME_MODE_4) {
        root["mode"] = "4P";
    } else {
        root["mode"] = "OFK";
    }

    root["bases"] = static_cast<int>(_bases);
    root["superProngs"] = _useSuperProngs;
    root["edgeless"] = _useEdgelessBoard;
    root["startPlayer"] = playerToString(_startPlayer);

    if (!meta.status.empty()) {
        root["status"] = meta.status;
    } else if (!_gameStarted) {
        root["status"] = "NOT_STARTED";
    } else if (_gameResigned) {
        root["status"] = "RESIGNED";
    } else if (_gameOver) {
        root["status"] = "GAME_OVER";
    } else {
        root["status"] = "IN_PROGRESS";
    }

    root["winner"] = playerToString(_winner);

    std::string term = meta.termination;
    if (term.empty()) {
        if (_gameResigned) {
            term = "RESIGNATION";
        } else if (_gameOver && _winner != PLAYER_UNDEF) {
            bool baseOccupation = false;
            if (_board) {
                if (_mode == GAME_MODE_2) {
                    int neededBases = (_bases == GAME_BASES_1) ? 1 : 3;
                    if (_winner == PLAYER_A) {
                        int count = 0;
                        if (_board->squareAt(2, 6).isCapturedBy(PLAYER_A)) count++;
                        if (_board->squareAt(4, 6).isCapturedBy(PLAYER_A)) count++;
                        if (_board->squareAt(6, 6).isCapturedBy(PLAYER_A)) count++;
                        if (count >= neededBases) baseOccupation = true;
                    } else if (_winner == PLAYER_B) {
                        int count = 0;
                        if (_board->squareAt(2, 2).isCapturedBy(PLAYER_B)) count++;
                        if (_board->squareAt(4, 2).isCapturedBy(PLAYER_B)) count++;
                        if (_board->squareAt(6, 2).isCapturedBy(PLAYER_B)) count++;
                        if (count >= neededBases) baseOccupation = true;
                    }
                } else if (_mode == GAME_MODE_OFK) {
                    if (_winner == PLAYER_A) {
                        for (int x = 1; x <= 4; x++) {
                            if (_board->squareAt(x, 5).isCapturedBy(PLAYER_A)) {
                                baseOccupation = true;
                                break;
                            }
                        }
                    } else if (_winner == PLAYER_B) {
                        for (int x = 1; x <= 4; x++) {
                            if (_board->squareAt(x, 1).isCapturedBy(PLAYER_B)) {
                                baseOccupation = true;
                                break;
                            }
                        }
                    }
                } else if (_mode == GAME_MODE_4) {
                    if (_winner == PLAYER_A || _winner == PLAYER_C) {
                        if (_board->squareAt(2, 6).isCapturedByTeamOf(PLAYER_A) &&
                            _board->squareAt(6, 2).isCapturedByTeamOf(PLAYER_A)) {
                            baseOccupation = true;
                        }
                    } else if (_winner == PLAYER_B || _winner == PLAYER_D) {
                        if (_board->squareAt(2, 2).isCapturedByTeamOf(PLAYER_B) &&
                            _board->squareAt(6, 6).isCapturedByTeamOf(PLAYER_B)) {
                            baseOccupation = true;
                        }
                    }
                }
            }

            if (baseOccupation) {
                term = "BASE_OCCUPATION";
            } else {
                bool totalCapture = false;
                if (_board) {
                    if (_mode == GAME_MODE_2 || _mode == GAME_MODE_OFK) {
                        OctiPlayer loser = (_winner == PLAYER_A) ? PLAYER_B : PLAYER_A;
                        int loserPods = 0;
                        for (int x = 0; x < _board->getBoardWidth(); x++) {
                            for (int y = 0; y < _board->getBoardHeight(); y++) {
                                const OctiSquare &sq = _board->squareAt(x, y);
                                for (int i = 0; i < sq.getNumPods(); i++) {
                                    if (podTypeToPlayer(sq.getPodAt(i).getType()) == loser) {
                                        loserPods++;
                                    }
                                }
                            }
                        }
                        int loserFree = (_mode == GAME_MODE_OFK) ? 0 : _players[loser].freePods;
                        if (loserPods == 0 && loserFree == 0) {
                            totalCapture = true;
                        }
                    } else if (_mode == GAME_MODE_4) {
                        int bdPods = 0, acPods = 0;
                        for (int x = 0; x < _board->getBoardWidth(); x++) {
                            for (int y = 0; y < _board->getBoardHeight(); y++) {
                                const OctiSquare &sq = _board->squareAt(x, y);
                                for (int i = 0; i < sq.getNumPods(); i++) {
                                    OctiPlayer p = podTypeToPlayer(sq.getPodAt(i).getType());
                                    if (p == PLAYER_B || p == PLAYER_D) bdPods++;
                                    if (p == PLAYER_A || p == PLAYER_C) acPods++;
                                }
                            }
                        }
                        int bdFree = (_players[PLAYER_B].eliminated ? 0 : _players[PLAYER_B].freePods) +
                                     (_players[PLAYER_D].eliminated ? 0 : _players[PLAYER_D].freePods);
                        int acFree = (_players[PLAYER_A].eliminated ? 0 : _players[PLAYER_A].freePods) +
                                     (_players[PLAYER_C].eliminated ? 0 : _players[PLAYER_C].freePods);
                        if ((_winner == PLAYER_A || _winner == PLAYER_C) && bdPods == 0 && bdFree == 0) {
                            totalCapture = true;
                        } else if ((_winner == PLAYER_B || _winner == PLAYER_D) && acPods == 0 && acFree == 0) {
                            totalCapture = true;
                        }
                    }
                }

                if (totalCapture) {
                    term = "TOTAL_CAPTURE";
                } else {
                    term = "STALEMATE";
                }
            }
        }
    }
    root["termination"] = term;
    root["timeControl"] = meta.timeControl.empty() ? "untimed" : meta.timeControl;
    root["finalFen"] = _board ? _board->toPositionString() : "";

    // Players
    nlohmann::json playersArr = nlohmann::json::array();
    std::vector<OctiPlayerMetadata> players = meta.players;
    if (players.empty()) {
        int count = (_mode == GAME_MODE_4) ? 4 : 2;
        for (int i = 0; i < count; ++i) {
            OctiPlayerMetadata p;
            p.seat = i + 1;
            p.player = static_cast<OctiPlayer>(PLAYER_A + i);
            p.type = "HUMAN";
            p.handle = "Player";
            players.push_back(p);
        }
    }
    for (const auto &p : players) {
        nlohmann::json pObj;
        pObj["seat"] = p.seat;
        pObj["player"] = playerToString(p.player);
        pObj["type"] = p.type;
        pObj["handle"] = p.handle;
        if (p.type == "AI" || p.aiDifficulty > 0) {
            pObj["aiDifficulty"] = p.aiDifficulty;
            pObj["aiBudgetMs"] = p.aiBudgetMs;
        }
        playersArr.push_back(pObj);
    }
    root["players"] = playersArr;

    std::string baseJson = root.dump(2);
    std::ostringstream ss;
    if (baseJson.size() >= 2 && baseJson.back() == '}') {
        baseJson.pop_back();
        while (!baseJson.empty() && std::isspace(static_cast<unsigned char>(baseJson.back()))) {
            baseJson.pop_back();
        }
        ss << baseJson << ",\n  \"plies\": [";
        for (std::size_t i = 0; i < _history.size(); ++i) {
            if (i > 0) {
                ss << ",";
            }
            ss << "\n    ";
            nlohmann::json plyObj;
            plyObj["ply"] = static_cast<int>(i + 1);
            plyObj["player"] = playerToString(_history[i].first);
            plyObj["move"] = _history[i].second.asString();
            OctiPlyMetadata plyMeta = (i < _plyMetadata.size()) ? _plyMetadata[i] : OctiPlyMetadata{};
            plyObj["elapsedMs"] = plyMeta.elapsedMs;
            if (plyMeta.clockAfterMs >= 0) {
                plyObj["clockAfterMs"] = plyMeta.clockAfterMs;
            }
            if (plyMeta.isAiMove) {
                nlohmann::json aiObj;
                aiObj["difficulty"] = plyMeta.aiDifficulty;
                aiObj["budgetMs"] = plyMeta.aiBudgetMs;
                aiObj["depthReached"] = plyMeta.aiDepthReached;
                aiObj["nodesVisited"] = plyMeta.aiNodesVisited;
                aiObj["bestScore"] = plyMeta.aiBestScore;
                plyObj["ai"] = aiObj;
            }
            ss << plyObj.dump();
        }
        if (!_history.empty()) {
            ss << "\n  ";
        }
        ss << "]\n}\n";
    } else {
        root["plies"] = nlohmann::json::array();
        return root.dump(2) + "\n";
    }

    return ss.str();
}

std::string OctiGame::serializeJson() const
{
    return serializeJson(_gameMetadata);
}

void OctiGame::serialize(std::ostream &out) const
{
    out << serializeJson(_gameMetadata);
}

std::string OctiGame::serialize() const
{
    return serializeJson(_gameMetadata);
}

void OctiGame::deserialize(std::istream &in)
{
    if (_board) {
        _board->deserializeGame(in);
    }
}

void OctiGame::deserialize(const std::string &transcript)
{
    std::istringstream ss(transcript);
    deserialize(ss);
}

OctiGameSnapshot OctiGame::captureSnapshot() const
{
    OctiGameSnapshot snap;
    if (_board) {
        int w = _board->getBoardWidth();
        int h = _board->getBoardHeight();
        for (int x = 0; x < w; x++) {
            for (int y = 0; y < h; y++) {
                snap.squares[x][y] = _board->squareAt(x, y).pods();
            }
        }
    }
    for (int p = 0; p < 5; p++) {
        snap.players[p] = _players[p];
    }
    snap.turn = _turn;
    snap.winner = _winner;
    snap.gameStarted = _gameStarted;
    snap.gameOver = _gameOver;
    snap.gameResigned = _gameResigned;
    snap.searchPlyOffset = _searchPlyOffset;
    snap.nullMoveDepth = _nullMoveDepth;
    if (!_searchMode) {
        snap.plyMetadata = _plyMetadata;
    }
    return snap;
}

void OctiGame::restoreSnapshot(const OctiGameSnapshot &snap)
{
    if (_board) {
        int w = _board->getBoardWidth();
        int h = _board->getBoardHeight();
        for (int x = 0; x < w; x++) {
            for (int y = 0; y < h; y++) {
                OctiSquare &sq = _board->squareAt(x, y);
                sq.clear();
                for (const auto &pod : snap.squares[x][y]) {
                    sq.addPod(pod);
                }
            }
        }
    }
    for (int p = 0; p < 5; p++) {
        _players[p] = snap.players[p];
    }
    _turn = snap.turn;
    _winner = snap.winner;
    _gameStarted = snap.gameStarted;
    _gameOver = snap.gameOver;
    _gameResigned = snap.gameResigned;
    _searchPlyOffset = snap.searchPlyOffset;
    _nullMoveDepth = snap.nullMoveDepth;
    if (!_searchMode) {
        _plyMetadata = snap.plyMetadata;
    }
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
