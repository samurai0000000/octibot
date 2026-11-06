/*
 * OctiGame.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include "OctiGame.hxx"
#include "OctiBoard.hxx"
#include "OctiCrypto.hxx"
#include <algorithm>
#include <cctype>
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
}

bool OctiGame::isSearchMode() const
{
    return _searchMode;
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
                if (generateLegalMoves().empty()) {
                    _gameOver = true;
                    _winner = (_turn == PLAYER_A) ? PLAYER_B : PLAYER_A;
                }
            } else if (_mode == GAME_MODE_4) {
                int seatsChecked = 0;
                while (seatsChecked < 4 && !_players[_turn].eliminated && generateLegalMoves().empty()) {
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
            if (generateLegalMoves().empty()) {
                _gameOver = true;
                _winner = (_turn == PLAYER_A) ? PLAYER_B : PLAYER_A;
            }
        } else if (_mode == GAME_MODE_4) {
            int seatsChecked = 0;
            while (seatsChecked < 4 && !_players[_turn].eliminated && generateLegalMoves().empty()) {
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

void OctiGame::applyMoveOps(const std::vector<OctiMoveOp> &ops)
{
    if (ops.empty()) {
        throw IllegalMoveException("no operations in move");
    }

    // Separate bonus ops and regular ops
    std::vector<OctiMoveOp> bonusOps;
    std::vector<OctiMoveOp> regularOps;
    for (const auto &op : ops) {
        if (op.isSuperProngBonusMove) {
            bonusOps.push_back(op);
        } else {
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
        for (const auto &bOp : bonusOps) {
            if (bOp.hops.empty()) {
                throw IllegalMoveException("superprong bonus move must contain hops");
            }
            if (!_board->isValidCoord(bOp.x0, bOp.y0)) {
                throw IllegalMoveException("superprong bonus starting square out of bounds");
            }
            OctiSquare &sq = _board->squareAt(bOp.x0, bOp.y0);
            int podIdx = -1;
            for (int i = 0; i < sq.getNumPods(); i++) {
                if (sq.getPodAt(i).getType() == playerToPodType(_turn) &&
                    (sq.getPodAt(i).rawConfig() & 0xFFFFu) == (bOp.pod.rawConfig() & 0xFFFFu)) {
                    podIdx = i;
                    break;
                }
            }
            if (podIdx == -1) {
                throw IllegalMoveException("pod for superprong bonus move not found");
            }
            OctiPod pod = sq.getPodAt(podIdx);
            sq.removePod(pod);

            int cx = bOp.x0;
            int cy = bOp.y0;
            std::set<std::pair<int, int>> jumpedSquares;

            for (const auto &hop : bOp.hops) {
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
        if (op.hops.empty()) {
            throw IllegalMoveException("movement operation must contain at least one hop");
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
            outMoves.emplace_back(std::vector<OctiMoveOp>{op});

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
            outMoves.emplace_back(std::vector<OctiMoveOp>{op});

            dfsJumpChains(board, edgeless, turn, lx, ly, pod, ghostPods,
                          jumpedSquares, currentHops, sx, sy, outMoves);

            jumpedSquares.pop_back();
            currentHops.pop_back();
        }
    }
}

void OctiGame::generateLegalMoves(std::vector<OctiMove> &outMoves) const
{
    outMoves.clear();
    if (!_gameStarted || _gameOver || _turn == PLAYER_UNDEF || !_board) {
        return;
    }

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
                            outMoves.emplace_back(std::vector<OctiMoveOp>{op});
                        }
                        if (_board->canLiberate(_turn) && _players[_turn].capturedPods > 0 && sq.isOccupiedBy(_turn)) {
                            OctiMoveOp op;
                            op.x0 = x; op.y0 = y; op.addPod = true; op.liberate = true;
                            outMoves.emplace_back(std::vector<OctiMoveOp>{op});
                        }
                    } else {
                        // Enemy base
                        if (_players[_turn].freePods > 0) {
                            OctiMoveOp op;
                            op.x0 = x; op.y0 = y; op.addPod = true; op.liberate = false;
                            outMoves.emplace_back(std::vector<OctiMoveOp>{op});
                        }
                        if (_players[_turn].capturedPods > 0) {
                            OctiMoveOp op;
                            op.x0 = x; op.y0 = y; op.addPod = true; op.liberate = true;
                            outMoves.emplace_back(std::vector<OctiMoveOp>{op});
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
                            outMoves.emplace_back(std::vector<OctiMoveOp>{op});
                        }
                        if (sq.isOccupiedBy(_turn) && _board->canLiberate(_turn) && _players[_turn].capturedPods > 0) {
                            OctiMoveOp op;
                            op.x0 = x; op.y0 = y; op.addPod = true; op.liberate = true;
                            outMoves.emplace_back(std::vector<OctiMoveOp>{op});
                        }
                    } else if (sq.getType() == partnerHome) {
                        // Never allow placing freePods on partnerHome
                        if (_players[partner].eliminated) {
                            if (_players[partner].capturedPods > 0) {
                                OctiMoveOp op;
                                op.x0 = x; op.y0 = y; op.addPod = true; op.liberate = true;
                                outMoves.emplace_back(std::vector<OctiMoveOp>{op});
                            }
                        } else {
                            if (_board->canLiberate(_turn) && _players[_turn].capturedPods > 0) {
                                OctiMoveOp op;
                                op.x0 = x; op.y0 = y; op.addPod = true; op.liberate = true;
                                outMoves.emplace_back(std::vector<OctiMoveOp>{op});
                            }
                        }
                    } else if (sq.getType() == NEUTRAL_OCTI_SQUARE) {
                        // Never allow placing freePods on NEUTRAL_OCTI_SQUARE
                        if (_players[_turn].capturedPods > 0) {
                            OctiMoveOp op;
                            op.x0 = x; op.y0 = y; op.addPod = true; op.liberate = true;
                            outMoves.emplace_back(std::vector<OctiMoveOp>{op});
                        }
                    } else {
                        // Enemy base
                        if (_players[_turn].freePods > 0) {
                            OctiMoveOp op;
                            op.x0 = x; op.y0 = y; op.addPod = true; op.liberate = false;
                            outMoves.emplace_back(std::vector<OctiMoveOp>{op});
                        }
                        if (_players[_turn].capturedPods > 0) {
                            OctiMoveOp op;
                            op.x0 = x; op.y0 = y; op.addPod = true; op.liberate = true;
                            outMoves.emplace_back(std::vector<OctiMoveOp>{op});
                        }
                    }
                }
            }
        }
    }

    // 2. Prong Additions, 3. Prong Repositions, 4. Distance-1 Moves, 5. Jumps
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            const OctiSquare &sq = _board->squareAt(x, y);
            std::set<std::uint32_t> seenConfigs;

            for (int i = 0; i < sq.getNumPods(); i++) {
                const OctiPod &pod = sq.getPodAt(i);
                if (pod.getType() != playerToPodType(_turn)) {
                    continue;
                }
                if (seenConfigs.count(pod.rawConfig() & 0xFFFFu) > 0) {
                    continue;
                }
                seenConfigs.insert(pod.rawConfig() & 0xFFFFu);

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
                            outMoves.emplace_back(std::vector<OctiMoveOp>{op});
                        }
                        if (_useSuperProngs && _players[_turn].freeSuperProngs > 0) {
                            OctiMoveOp op;
                            op.x0 = x; op.y0 = y;
                            op.pod = pod;
                            op.addProngDir = dir;
                            op.superProng = true;
                            outMoves.emplace_back(std::vector<OctiMoveOp>{op});
                        }
                    }
                }

                // 3. Prong Repositions
                if (_mode != GAME_MODE_OFK) {
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
                                    outMoves.emplace_back(std::vector<OctiMoveOp>{op});
                                }
                            }
                        }
                    }
                }

                // 4. Distance-1 Moves
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
                        outMoves.emplace_back(std::vector<OctiMoveOp>{op});
                    }
                }

                // 5. Distance-2 Jumps
                std::vector<OctiPod> ghostPods = sq.pods();
                std::vector<std::pair<int, int>> jumpedSquares;
                std::vector<OctiHop> currentHops;
                dfsJumpChains(_board, _useEdgelessBoard, _turn, x, y, pod, ghostPods,
                              jumpedSquares, currentHops, x, y, outMoves);
            }
        }
    }
}

std::vector<OctiMove> OctiGame::generateLegalMoves() const
{
    std::vector<OctiMove> moves;
    generateLegalMoves(moves);
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
