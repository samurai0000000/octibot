/*
 * OctiBoard.hxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#ifndef OCTI_BOARD_HXX
#define OCTI_BOARD_HXX

#include <iosfwd>
#include <memory>
#include <string>
#include <vector>
#include "OctiDefs.hxx"
#include "OctiExceptions.hxx"
#include "OctiMove.hxx"
#include "OctiSquare.hxx"

namespace octi {

class OctiBoard;
class OctiGame;
struct OctiGameMetadata;

class OctiBoardListener {
public:
    virtual ~OctiBoardListener() = default;
    virtual void boardUpdated(OctiBoard *board, const OctiMove *move) = 0;
    virtual void gameChanged(OctiBoard *board, OctiGame *oldGame, OctiGame *newGame) = 0;
};

class OctiBoard {
public:
    OctiBoard();
    explicit OctiBoard(OctiGameMode mode,
                       bool useSuperProngs = false,
                       bool useEdgelessBoard = false,
                       OctiGameBases bases = GAME_BASES_3);
    ~OctiBoard();

    OctiBoard(const OctiBoard &) = delete;
    OctiBoard &operator=(const OctiBoard &) = delete;

    void setGameMode(OctiGameMode mode,
                     bool useSuperProngs = false,
                     bool useEdgelessBoard = false,
                     OctiGameBases bases = GAME_BASES_3);

    void clear();

    OctiGameMode getGameMode() const;
    int getNumPlayers() const;
    int getBoardWidth() const;
    int getBoardHeight() const;
    bool isValidCoord(int x, int y) const;

    OctiSquare &squareAt(int x, int y);
    const OctiSquare &squareAt(int x, int y) const;

    OctiGame *getGame();
    const OctiGame *getGame() const;

    bool canLiberate(OctiPlayer player) const;

    void addBoardListener(OctiBoardListener *listener);
    void removeBoardListener(OctiBoardListener *listener);
    void suppressBoardUpdateNotification(bool suppress);
    bool isBoardUpdateNotificationSuppressed() const;
    void notifyBoardTurnUpdate(const OctiMove *move);

    std::string toPositionString() const;
    void fromPositionString(const std::string &pos);

    void serializeGame(std::ostream &out) const;
    std::string serializeGame() const;
    void deserializeGame(std::istream &in);
    void deserializeGame(const std::string &transcript);

    std::string serializeJson(const OctiGameMetadata &meta) const;
    std::string serializeJson() const;
    std::string exportCanonicalMovesText() const;
    std::string computeCanonicalGameId() const;

    friend std::ostream &operator<<(std::ostream &os, const OctiBoard &board);

private:
    void notifyGameChanged(OctiGame *oldGame, OctiGame *newGame);

    OctiSquare _boardSquares[9][9];
    OctiGameMode _mode;
    int _width;
    int _height;
    std::unique_ptr<OctiGame> _game;
    std::vector<OctiBoardListener *> _listeners;
    bool _suppressNotification;
};

} // namespace octi

#endif // OCTI_BOARD_HXX

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
