/* $Header: mazewar.h,v 1.7 88/08/25 09:59:51 kent Exp $ */

/*
 * mazewar.h - Definitions for MazeWar
 *
 * Author:	Christopher A. Kent
 * 		Western Research Laboratory
 * 		Digital Equipment Corporation
 * Date:	Wed Sep 24 1986
 */

/* Modified by Michael Greenwald for CS244B, Mar 1992,
   Greenwald@cs.stanford.edu */

/* Modified by Nicholas Dovidio for CS244B, Mar 2009,
 * ndovidio@stanford.edu
 * This version now uses the CS249a/b style of C++ coding.
 */

/***********************************************************
Copyright 1986 by Digital Equipment Corporation, Maynard, Massachusetts,

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the names of Digital not be
used in advertising or publicity pertaining to disstribution of the
software without specific, written prior permission.

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/

#ifndef MAZEWAR_H
#define MAZEWAR_H

#include "fwk/NamedInterface.h"
#include <map>
#include <set>
#include "Exception.h"
#include "Nominal.h"
#include <string>
/* fundamental constants */

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif /* TRUE */

/* You can modify this if you want to */
#define MAX_RATS 8

/* network stuff */
/* Feel free to modify.  This is the simplest version we came up with */

/* A unique MAZEPORT will be assigned to your team by the TA */
#define MAZEPORT 5018
/* The multicast group for Mazewar is 224.1.1.1 */
#define MAZEGROUP 0xe0010101
#define MAZESERVICE "mazewar244B"

/* The next two >must< be a power of two, because we subtract 1 from them
   to get a bitmask for random()
 */
#define JOINPHASE 0
#define PLAYPHASE 1
#define MAX_UNCOMMITED 5
#define MAZEXMAX 32
#define MAZEYMAX 16
#define VECTORSIZE 55
#define NAMESIZE 20
#define NDIRECTION 4
#define NORTH 0
#define SOUTH 1
#define EAST 2
#define WEST 3
#define NVIEW 4
#define LEFT 0
#define RIGHT 1
#define REAR 2
#define FRONT 3

#define HEARTBEAT 0
#define HEARTBEATACK 1
#define EVENT 2
#define EVENTACK 3
#define STATEREQUEST 4
#define STATERESPONSE 5
#define STATEACK 6

#define MAX_MISSILES 4

#define EVENTCLOAK 0
#define EVENTMOVE 1
#define EVENTBORN 2
#define EVENTSHOOT 3
#define EVENTHIT 4

using namespace std;

/* types */

typedef struct sockaddr_in Sockaddr;
typedef bool MazeRow[MAZEYMAX];
typedef MazeRow MazeType[MAZEXMAX];
typedef MazeRow *MazeTypePtr;
// typedef	short						Direction;
typedef struct { short x, y; } XYpoint;
typedef struct { XYpoint p1, p2; } XYpair;
typedef struct { short xcor, ycor; } XY;
typedef struct { unsigned short bits[16]; } BitCell;
typedef char RatName[NAMESIZE];


class Direction : public Ordinal<Direction, uint8_t> {
public:
  Direction(short num) : Ordinal<Direction, uint8_t>(num) {
    if (num < NORTH || num > NDIRECTION) {
      throw RangeException("Error: Unexpected value.\n");
    }
  }
};

class Loc : public Ordinal<Loc, uint8_t> {
public:
  Loc(short num) : Ordinal<Loc, uint8_t>(num) {
    if (num < 0) {
      throw RangeException("Error: Unexpected negative value.\n");
    }
  }
};

class Score : public Ordinal<Score, uint32_t> {
public:
  Score(int num) : Ordinal<Score, uint32_t>(num) {}
};

class RatIndexType : public Ordinal<RatIndexType, int> {
public:
  RatIndexType(int num) : Ordinal<RatIndexType, int>(num) {
    if (num < 0) {
      throw RangeException("Error: Unexpected negative value.\n");
    }
  }
};

class RatId : public Ordinal<RatId, unsigned int> {
public:
  RatId(unsigned int num) : Ordinal<RatId, unsigned int>(num) {}
};

class TokenId : public Ordinal<TokenId, long> {
public:
  TokenId(long num) : Ordinal<TokenId, long>(num) {}
};

class RatAppearance {

public:
  RatAppearance() : x(1), y(1), tokenId(0){};
  bool visible;
  Loc x, y;
  short distance;
  TokenId tokenId;
};

struct Missile{
								Missile() : exist(false), x(0), y(0), dir(NORTH) {};
								bool exist;
								Loc x, y;
								Direction dir;
};

class Rat {

public:
  Rat() : playing(0), cloaked(0), x(1), y(1), score(0), dir(NORTH){
		for(int i=0; i < MAX_MISSILES; i++){
				//this->RatMissile[i] = new Missile();
		}	
	};
	RatName Name;
  bool playing;
  bool cloaked;
  Loc x, y;
  Direction dir;
	Score score;
	Missile RatMissile[MAX_MISSILES];
};

typedef RatAppearance RatApp_type[MAX_RATS];
typedef RatAppearance *RatLook;






/* events */

#define EVENT_A 1   /* user pressed "A" */
#define EVENT_S 2   /* user pressed "S" */
#define EVENT_F 3   /* user pressed "F" */
#define EVENT_D 4   /* user pressed "D" */
#define EVENT_G 5   /* user pressed "G" */
#define EVENT_C 6   /* user pressed "C" */
#define EVENT_BAR 7 /* user pressed space bar */

#define EVENT_LEFT_D 30   /* user pressed left mouse button */
#define EVENT_RIGHT_D 31  /* user pressed right button */
#define EVENT_MIDDLE_D 32 /* user pressed middle button */
#define EVENT_LEFT_U 33   /* user released l.M.b */
#define EVENT_RIGHT_U 34  /* user released r.M.b */

#define EVENT_NETWORK 50 /* incoming network packet */
#define EVENT_INT 51     /* user pressed interrupt key */
#define EVENT_TIMEOUT 52 /* nothing happened! */

extern unsigned short ratBits[];
/* replace this with appropriate definition of your own */
typedef struct {
  unsigned char type;
  u_long body[256];
} MW244BPacket;

typedef struct {
  short eventType;
  MW244BPacket *eventDetail; /* for incoming data */
  Sockaddr eventSource;
} MWEvent;

void *malloc();
Sockaddr *resolveHost();

/* defined in display.c */
extern RatApp_type Rats2Display;

/* variables "exported" by the mazewar "module" */
class MazewarInstance : public Fwk::NamedInterface {
public:
  typedef Fwk::Ptr<MazewarInstance const> PtrConst;
  typedef Fwk::Ptr<MazewarInstance> Ptr;

  static MazewarInstance::Ptr mazewarInstanceNew(string s) {
    MazewarInstance *m = new MazewarInstance(s);
    return m;
  }

  inline Direction dir() const { return dir_; }
  void dirIs(Direction dir) { this->dir_ = dir; }
  inline Direction dirPeek() const { return dirPeek_; }
  void dirPeekIs(Direction dirPeek) { this->dirPeek_ = dirPeek; }

  inline long mazePort() const { return mazePort_; }
  void mazePortIs(long mazePort) { this->mazePort_ = mazePort; }
  inline Sockaddr *myAddr() const { return myAddr_; }
  void myAddrIs(Sockaddr *myAddr) { this->myAddr_ = myAddr; }
  inline RatId myRatId() const { return myRatId_; }
  void myRatIdIs(RatId myRatId) { this->myRatId_ = myRatId; }

  inline bool peeking() const { return peeking_; }
  void peekingIs(bool peeking) { this->peeking_ = peeking; }
  inline int theSocket() const { return theSocket_; }
  void theSocketIs(int theSocket) { this->theSocket_ = theSocket; }
  inline Score score() const { return score_; }
  void scoreIs(Score score) { this->score_ = score; }
  inline Loc xloc() const { return xloc_; }
  void xlocIs(Loc xloc) { this->xloc_ = xloc; }
  inline Loc yloc() const { return yloc_; }
  void ylocIs(Loc yloc) { this->yloc_ = yloc; }
  inline Loc xPeek() const { return xPeek_; }
  void xPeekIs(Loc xPeek) { this->xPeek_ = xPeek; }
  inline Loc yPeek() const { return yPeek_; }
  void yPeekIs(Loc yPeek) { this->yPeek_ = yPeek; }
  inline int active() const { return active_; }
  void activeIs(int active) { this->active_ = active; }
	inline Rat rat(RatIndexType num) const { return mazeRats_[num.value()]; }
	void ratIs(Rat rat, RatIndexType num) { this->mazeRats_[num.value()] = rat; }
	inline RatIndexType myIndex() const { return this->myRatIndex_;}
	void myIndexIs(RatIndexType index){ this->myRatIndex_ = index;}

  MazeType maze_;
  MazeType occupy_;
  RatName myName_;
	//Used to map Index with their ID
	std::map<RatId, int> AllRats;
  bool mazeplay[MAX_RATS];

protected:
  MazewarInstance(string s)
      : Fwk::NamedInterface(s), dir_(0), dirPeek_(0), myRatId_(0), score_(0),
        xloc_(1), yloc_(3), xPeek_(0), yPeek_(0), myRatIndex_(0) {
    myAddr_ = (Sockaddr *)malloc(sizeof(Sockaddr));
    if (!myAddr_) {
      printf("Error allocating sockaddr variable");
    }
    for(int i=0; i<MAX_RATS; i++) mazeplay[i] = FALSE;
  }
  Direction dir_;
  Direction dirPeek_;

  long mazePort_;
  Sockaddr *myAddr_;
  Rat mazeRats_[MAX_RATS];
  RatId myRatId_;
  RatIndexType myRatIndex_;

  bool peeking_;
  int theSocket_;
  Score score_;
  Loc xloc_;
  Loc yloc_;
  Loc xPeek_;
  Loc yPeek_;
  int active_;
};
extern MazewarInstance::Ptr M;

#define MY_RAT_INDEX 0
#define MY_DIR M->dir().value()
#define MY_X_LOC M->xloc().value()
#define MY_Y_LOC M->yloc().value()

typedef std::map<uint32_t, int> respond_set;
typedef std::map<uint32_t, int>::iterator respond_set_iter;
typedef uint32_t EventId;
typedef uint32_t PlayerID;

struct Position_{
  uint8_t x;
  uint8_t y;
};

/* Protocol packet information*/
union parsedInfo{
  bool bit_1;
  uint8_t bit_8;
  uint16_t bit_16;
  uint32_t bit_32;
};

struct heartbeat{
  uint32_t heartbeatId;
  uint32_t sourceId;
};

struct heartbeatACK{
  uint32_t heartbeatId;
  uint32_t sourceId;
	uint32_t destinationId;
};

struct missileInfo{
	uint8_t exist;
	Position_ position;
	uint8_t direction;
};

struct movement{
	uint8_t direction;
	uint8_t speed;
};

struct born{
	Position_ position;
	uint8_t direction;
};

struct missileProjection{
	uint8_t missileId;
	Position_ position;
	uint8_t direction;
};

struct missileHit{
	uint32_t ownerId;			
	uint8_t missileId;
	Position_ position;
	uint8_t direction;
};

union eventSpecificData{
	bool cloak;
	movement moveData;
	born bornData;
	missileProjection missileProjData; 
	missileHit missileHitData;
};

struct absoluteInfo{
	uint32_t score;
  Position_ position;
	uint8_t direction;
	bool cloak;
	uint8_t missileNumber;
	missileInfo missiles[MAX_MISSILES];
};

struct event{
  uint8_t type;
	EventId eventId;
  uint32_t sourceId;
  absoluteInfo absoInfo;
	eventSpecificData eventData;
};

struct eventACK{
  EventId eventId;
	uint32_t sourceId;
	uint32_t destinationId;
};

struct SIReq{
	uint32_t sourceId;
};

struct uncommittedAction{
  uint8_t type;
	EventId eventId;
	eventSpecificData eventData;
};

struct SIRes{
	uint32_t sourceId;
	uint32_t destinationId;
  absoluteInfo absoInfo;
	uint32_t uncommitted_number;
	uncommittedAction uncommit[MAX_UNCOMMITED];
};

struct SIACK{
	uint32_t sourceId;
	uint32_t destinationId;
};

typedef union{
				heartbeat hb_;
				heartbeatACK hbACK_;
				event ev_;
				eventACK evACK_;
				SIReq SIReq_;
				SIRes SIRes_;
				SIACK SIACK_;
} packetInfo; 

static respond_set hbACK_set;
typedef std::map<EventId, packetInfo> player_unprocessed;
typedef std::map<EventId, packetInfo>::iterator player_unprocessed_iter;
typedef std::map<PlayerID, player_unprocessed> event_unprocessed;
typedef std::map<PlayerID, player_unprocessed>::iterator event_unprocessed_iter;

/* display.c */
void InitDisplay(int, char **);
void StartDisplay(void);
void ShowView(Loc, Loc, Direction);
void SetMyRatIndexType(RatIndexType);
void SetRatPosition(RatIndexType, Loc, Loc, Direction);
void ClearRatPosition(RatIndexType);
void ShowPosition(Loc, Loc, Direction);
void ShowAllPositions(void);
void showMe(Loc, Loc, Direction);
void clearPosition(RatIndexType, Loc, Loc);
void clearSquare(Loc xClear, Loc yClear);
void NewScoreCard(void);
void UpdateScoreCard(RatIndexType);
void FlipBitmaps(void);
void bitFlip(BitCell *, int size);
void SwapBitmaps(void);
void byteSwap(BitCell *, int size);
void showMissile(Loc, Loc, Direction, Loc, Loc, bool);
/* init.c */
void MazeInit(int, char **);
void ratStates(void);
void getMaze(void);
void setRandom(void);
void getName(char *, char **);
void getString(char *, char **);
void getHostName(char *, char **, Sockaddr *);
Sockaddr *resolveHost(char *);
bool emptyAhead();
bool emptyRight();
bool emptyLeft();
bool emptyBehind();

/* toplevel.c */
void JoinGame(void);
void play(void);
void aboutFace(void);
void leftTurn(void);
void rightTurn(void);
void forward(void);
void backward(void);
void peekLeft(void);
void peekRight(void);
void peekStop(void);
void shoot(void);
void cloak(void);
void quit(int);
void NewPosition(MazewarInstance::Ptr M);
void MWError(char *);
Score GetRatScore(RatIndexType);
char *GetRatName(RatIndexType);
void ConvertIncoming(MW244BPacket *);
void ConvertOutgoing(MW244BPacket *);
void ratState(void);
void manageMissiles(void);
void DoViewUpdate(void);
void sendPacketToPlayer(unsigned char, packetInfo);
void processPacket(MWEvent *, int);
void netInit(void);
void copybit(uint32_t, uint32_t, uint8_t, uint8_t);
parsedInfo parsebit(uint32_t, uint8_t, uint8_t);
absoluteInfo parseAbsoluteInfo(uint8_t*);
eventSpecificData parseEventData(uint8_t*, uint8_t);
uncommittedAction parseUncommit(uint8_t*);
void encodeEventData(uint32_t*, uint32_t*, uint8_t, eventSpecificData);
void memcpy_helper(void*, void*, std::size_t);
packetInfo packetParser(MW244BPacket*);
packetInfo eventPacketGenerator(uint8_t, eventSpecificData);

/* winsys.c */
void InitWindow(int, char **);
void StartWindow(int, int);
void ClearView(void);
void DrawViewLine(int, int, int, int);
void NextEvent(MWEvent *, int);
bool KBEventPending(void);
void HourGlassCursor(void);
void RatCursor(void);
void DeadRatCursor(void);
void HackMazeBitmap(Loc, Loc, BitCell *);
void DisplayRatBitmap(int, int, int, int, int, int);
void WriteScoreString(RatIndexType);
void ClearScoreLine(RatIndexType);
void InvertScoreLine(RatIndexType);
void NotifyPlayer(void);
void DrawString(const char *, uint32_t, uint32_t, uint32_t);
void StopWindow(void);

#endif
