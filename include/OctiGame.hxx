/*
 * OctiGame.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef OCTI_GAME_HXX
#define OCTI_GAME_HXX

#include <cstdint>
#include <iosfwd>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "OctiDefs.hxx"
#include "OctiExceptions.hxx"
#include "OctiMove.hxx"
#include "OctiPod.hxx"

namespace octi {

class OctiBoard;
class OctiGame;

class OctiGameListener {
public:
    virtual ~OctiGameListener() = default;
    virtual void gameUpdated(OctiGame *game) = 0;
    virtual void UndoRedoStatusChanged(OctiGame *game, bool canUndo, bool canRedo) = 0;
};

struct OctiPlayerState {
    int freePods = 0;
    int capturedPods = 0;
    int freeProngs = 0;
    int freeSuperProngs = 0;
    long timeRemainingMs = 0;
    bool eliminated = false;
};

struct OctiPlyMetadata {
    std::int64_t elapsedMs = 0;      // Turn thinking / composition time in ms
    std::int64_t clockAfterMs = -1;  // Remaining clock after move (-1 if untimed)
    bool isAiMove = false;           // True if played by OctiBotEngine
    int aiDifficulty = 0;            // BotDifficulty level (1..5)
    int aiBudgetMs = 0;              // Configured AI time budget in ms
    int aiDepthReached = 0;          // Iterative deepening depth completed
    std::uint64_t aiNodesVisited = 0;// Minimax + quiescence nodes searched
    int aiBestScore = 0;             // Evaluation score in centipawns/units
};

struct OctiPlayerMetadata {
    int seat = 1;
    OctiPlayer player = PLAYER_A;
    std::string type = "HUMAN";      // "HUMAN" or "AI"
    std::string handle = "Player";
    int aiDifficulty = 0;
    int aiBudgetMs = 0;
};

struct OctiClientExtendedMetadata {
    bool present = false;
    std::int64_t startedAt = 0;
    std::int64_t endedAt = 0;
    int utcOffsetMin = 0;
    std::string locale;
    std::string osVersion;
    std::string installId;
};

struct OctiGameMetadata {
    std::string gameId;
    std::string source = "LOCAL";
    std::string status;
    std::string appVersion;
    std::string buildType = "release";
    std::string platform;
    OctiClientExtendedMetadata extended;
    std::string termination;
    std::string timeControl = "untimed";
    std::string license;
    std::string attribution;
    std::vector<OctiPlayerMetadata> players;
};

struct OctiGameSnapshot {
    std::vector<OctiPod> squares[9][9];
    OctiPlayerState players[5];
    OctiPlayer turn = PLAYER_UNDEF;
    OctiPlayer winner = PLAYER_UNDEF;
    bool gameStarted = false;
    bool gameOver = false;
    bool gameResigned = false;
    OctiMove lastMove;
    OctiPlayer lastPlayer = PLAYER_UNDEF;
    int searchPlyOffset = 0;
    std::vector<OctiPlyMetadata> plyMetadata;
    OctiPlyMetadata lastPlyMeta;
};

class OctiGame {
public:
    explicit OctiGame(OctiBoard *board,
                      OctiGameMode mode = GAME_MODE_2,
                      bool useSuperProngs = false,
                      bool useEdgelessBoard = false,
                      OctiGameBases bases = GAME_BASES_3);
    ~OctiGame();

    OctiGame(const OctiGame &) = delete;
    OctiGame &operator=(const OctiGame &) = delete;

    void addGameListener(OctiGameListener *listener);
    void removeGameListener(OctiGameListener *listener);

    OctiBoard *getBoard();
    const OctiBoard *getBoard() const;

    OctiGameMode getMode() const;
    OctiGameBases getBases() const;
    int getBasesToWin() const { return static_cast<int>(_bases); }
    bool useSuperProngs() const;
    bool useEdgelessBoard() const;

    bool isGameStarted() const;
    bool isGameOver() const;
    bool isGameResigned() const;
    OctiPlayer getTurn() const;
    OctiPlayer getStartPlayer() const;
    OctiPlayer getWinner() const;

    bool isPlayerEliminated(OctiPlayer p) const;
    int getFreePods(OctiPlayer p) const;
    int getCapturedPods(OctiPlayer p) const;
    int getFreeProngs(OctiPlayer p) const;
    int getFreeSuperProngs(OctiPlayer p) const;

    long getTimeRemaining(OctiPlayer p) const;
    void setTimeRemaining(OctiPlayer p, long ms);

    int getNumMoves() const;
    void getMoves(std::vector<std::pair<OctiPlayer, OctiMove>> &history) const;

    bool canUndo() const;
    bool canRedo() const;

    void reset();
    void start(OctiPlayer startPlayer = PLAYER_A);
    void randomize();
    void resign(OctiPlayer player);

    void move(const std::string &moveStr);
    void move(const char *moveStr);
    void move(const OctiMove &m);
    void move(const std::vector<OctiMoveOp> &ops);
    void move(OctiPlayer player, const OctiMove &m);
    void move(OctiPlayer player, const std::string &moveStr);

    void undo();
    void redo();

    // Preparatory Move Workflow (GUI staging)
    void prepareAddPod(int x, int y, bool liberate = false);
    void prepareAddProng(int x, int y, OctiPod pod, OctiDir dir, bool superProng = false);
    void prepareReposition(int x, int y, OctiPod pod, OctiDir srcDir, OctiDir tgtDir);
    void prepareHops(int x, int y, OctiPod pod, const std::vector<OctiHop> &hops);
    void prepareCapture(int x, int y);
    void commitPreparedMove();
    void rollbackPreparedMove();
    bool hasPreparedMove() const;
    std::string getPreparedMoveString() const;

    // Legal move generation
    void generateLegalMoves(std::vector<OctiMove> &outMoves) const;
    std::vector<OctiMove> generateLegalMoves() const;

    // Transcript serialization and deserialization
    void serialize(std::ostream &out) const;
    std::string serialize() const;
    void deserialize(std::istream &in);
    void deserialize(const std::string &transcript);

    // Metadata accessors and JSON v3.0 export
    void setLastPlyMetadata(const OctiPlyMetadata &meta);
    void setPlyMetadata(std::size_t idx, const OctiPlyMetadata &meta);
    void setAllPlyMetadata(const std::vector<OctiPlyMetadata> &metas);
    OctiPlyMetadata getPlyMetadata(std::size_t idx) const;
    const std::vector<OctiPlyMetadata> &getAllPlyMetadata() const;
    void setGameMetadata(const OctiGameMetadata &meta);
    const OctiGameMetadata &getGameMetadata() const;
    std::string serializeJson(const OctiGameMetadata &meta) const;
    std::string serializeJson() const;
    std::string exportCanonicalMovesText() const;
    std::string computeCanonicalGameId() const;
    void setResignedWinner(OctiPlayer winner);

    // Snapshot / state restore
    OctiGameSnapshot captureSnapshot() const;
    void restoreSnapshot(const OctiGameSnapshot &snap);
    void setSearchMode(bool searchMode);
    bool isSearchMode() const;

private:
    void notifyGameUpdated();
    void notifyUndoRedoStatusChanged();

    void executeMoveInternal(const OctiMove &m, bool isRedo = false);
    void applyMoveOps(const std::vector<OctiMoveOp> &ops);
    void check4PlayerElimination();
    void advanceTurn();
    void updateGameOverAndWinner();

    OctiBoard *_board;
    OctiGameMode _mode;
    OctiGameBases _bases;
    bool _useSuperProngs;
    bool _useEdgelessBoard;

    bool _gameStarted;
    bool _gameOver;
    bool _gameResigned;
    OctiPlayer _startPlayer;
    OctiPlayer _turn;
    OctiPlayer _winner;

    OctiPlayerState _players[5];

    std::vector<std::pair<OctiPlayer, OctiMove>> _history;
    std::vector<OctiPlyMetadata> _plyMetadata;
    OctiGameMetadata _gameMetadata;
    std::vector<OctiGameSnapshot> _undoStack;
    std::vector<OctiGameSnapshot> _redoStack;

    // Preparatory state
    bool _hasPreparedMove;
    OctiGameSnapshot _prepSnapshot;
    std::vector<OctiMoveOp> _prepOps;

    std::vector<OctiGameListener *> _listeners;
    bool _searchMode = false;
    int _searchPlyOffset = 0;
};

} // namespace octi

#endif // OCTI_GAME_HXX

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
