/*
 *   FILE: toplevel.c
 * AUTHOR: name (email)
 *   DATE: March 31 23:59:59 PST 2013
 *  DESCR:
 */

/* #define DEBUG */

#include "main.h"
#include "mazewar.h"
#include <string>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static bool updateView; /* true if update needed */
MazewarInstance::Ptr M;

/* Use this socket address to send packets to the multi-cast group. */
static Sockaddr groupAddr;
#define MAX_OTHER_RATS (MAX_RATS - 1)

int main(int argc, char *argv[]) {
  Loc x(1);
  Loc y(5);
  Direction dir(0);
  char *ratName;

  signal(SIGHUP, quit);
  signal(SIGINT, quit);
  signal(SIGTERM, quit);

  getName((char *)"Welcome to CS244B MazeWar!\n\nYour Name", &ratName);
  ratName[strlen(ratName) - 1] = 0;

  M = MazewarInstance::mazewarInstanceNew(string(ratName));
  MazewarInstance *a = M.ptr();
  strncpy(M->myName_, ratName, NAMESIZE);
  free(ratName);

  MazeInit(argc, argv);

  NewPosition(M);

  /* So you can see what a Rat is supposed to look like, we create
  one rat in the single player mode Mazewar.
  It doesn't move, you can't shoot it, you can just walk around it */

	JoinGame();
  play();

  return 0;
}

/* ----------------------------------------------------------------------- */
void JoinGame(){
  
}

/* ----------------------------------------------------------------------- */
void play(void) {
  MWEvent event;
  MW244BPacket incoming;

  event.eventDetail = &incoming;

  while (TRUE) {
    NextEvent(&event, M->theSocket());
    if (!M->peeking())
      switch (event.eventType) {
      case EVENT_A:
        aboutFace();
        break;

      case EVENT_S:
        leftTurn();
        break;

      case EVENT_D:
        forward();
        break;

      case EVENT_F:
        rightTurn();
        break;

      case EVENT_G:
        backward();
        break;

      case EVENT_C:
        cloak();
        break;

      case EVENT_BAR:
        shoot();
        break;

      case EVENT_LEFT_D:
        peekLeft();
        break;

      case EVENT_RIGHT_D:
        peekRight();
        break;

      case EVENT_NETWORK:
        processPacket(&event);
        break;

      case EVENT_INT:
        quit(0);
        break;
      }
    else
      switch (event.eventType) {
      case EVENT_RIGHT_U:
      case EVENT_LEFT_U:
        peekStop();
        break;

      case EVENT_NETWORK:
        processPacket(&event);
        break;
      }

    ratStates(); /* clean house */

    manageMissiles();

    DoViewUpdate();

    /* Any info to send over network? */
  }
}

/* ----------------------------------------------------------------------- */

static Direction _aboutFace[NDIRECTION] = {SOUTH, NORTH, WEST, EAST};
static Direction _leftTurn[NDIRECTION] = {WEST, EAST, NORTH, SOUTH};
static Direction _rightTurn[NDIRECTION] = {EAST, WEST, SOUTH, NORTH};

void aboutFace(void) {
  M->dirIs(_aboutFace[MY_DIR]);
  updateView = TRUE;
}

/* ----------------------------------------------------------------------- */

void leftTurn(void) {
  M->dirIs(_leftTurn[MY_DIR]);
  updateView = TRUE;
}

/* ----------------------------------------------------------------------- */

void rightTurn(void) {
  M->dirIs(_rightTurn[MY_DIR]);
  updateView = TRUE;
}

/* ----------------------------------------------------------------------- */

/* remember ... "North" is to the right ... positive X motion */

void forward(void) {
  register int tx = MY_X_LOC;
  register int ty = MY_Y_LOC;

  switch (MY_DIR) {
  case NORTH:
    if (!M->maze_[tx + 1][ty])
      tx++;
    break;
  case SOUTH:
    if (!M->maze_[tx - 1][ty])
      tx--;
    break;
  case EAST:
    if (!M->maze_[tx][ty + 1])
      ty++;
    break;
  case WEST:
    if (!M->maze_[tx][ty - 1])
      ty--;
    break;
  default:
    MWError((char *)"bad direction in Forward");
  }
  if ((MY_X_LOC != tx) || (MY_Y_LOC != ty)) {
    M->xlocIs(Loc(tx));
    M->ylocIs(Loc(ty));
    updateView = TRUE;
  }
}

/* ----------------------------------------------------------------------- */

void backward() {
  register int tx = MY_X_LOC;
  register int ty = MY_Y_LOC;

  switch (MY_DIR) {
  case NORTH:
    if (!M->maze_[tx - 1][ty])
      tx--;
    break;
  case SOUTH:
    if (!M->maze_[tx + 1][ty])
      tx++;
    break;
  case EAST:
    if (!M->maze_[tx][ty - 1])
      ty--;
    break;
  case WEST:
    if (!M->maze_[tx][ty + 1])
      ty++;
    break;
  default:
    MWError((char *)"bad direction in Backward");
  }
  if ((MY_X_LOC != tx) || (MY_Y_LOC != ty)) {
    M->xlocIs(Loc(tx));
    M->ylocIs(Loc(ty));
    updateView = TRUE;
  }
}

/* ----------------------------------------------------------------------- */

void peekLeft() {
  M->xPeekIs(MY_X_LOC);
  M->yPeekIs(MY_Y_LOC);
  M->dirPeekIs(MY_DIR);

  switch (MY_DIR) {
  case NORTH:
    if (!M->maze_[MY_X_LOC + 1][MY_Y_LOC]) {
      M->xPeekIs(MY_X_LOC + 1);
      M->dirPeekIs(WEST);
    }
    break;

  case SOUTH:
    if (!M->maze_[MY_X_LOC - 1][MY_Y_LOC]) {
      M->xPeekIs(MY_X_LOC - 1);
      M->dirPeekIs(EAST);
    }
    break;

  case EAST:
    if (!M->maze_[MY_X_LOC][MY_Y_LOC + 1]) {
      M->yPeekIs(MY_Y_LOC + 1);
      M->dirPeekIs(NORTH);
    }
    break;

  case WEST:
    if (!M->maze_[MY_X_LOC][MY_Y_LOC - 1]) {
      M->yPeekIs(MY_Y_LOC - 1);
      M->dirPeekIs(SOUTH);
    }
    break;

  default:
    MWError((char *)"bad direction in PeekLeft");
  }

  /* if any change, display the new view without moving! */

  if ((M->xPeek() != MY_X_LOC) || (M->yPeek() != MY_Y_LOC)) {
    M->peekingIs(TRUE);
    updateView = TRUE;
  }
}

/* ----------------------------------------------------------------------- */

void peekRight() {
  M->xPeekIs(MY_X_LOC);
  M->yPeekIs(MY_Y_LOC);
  M->dirPeekIs(MY_DIR);

  switch (MY_DIR) {
  case NORTH:
    if (!M->maze_[MY_X_LOC + 1][MY_Y_LOC]) {
      M->xPeekIs(MY_X_LOC + 1);
      M->dirPeekIs(EAST);
    }
    break;

  case SOUTH:
    if (!M->maze_[MY_X_LOC - 1][MY_Y_LOC]) {
      M->xPeekIs(MY_X_LOC - 1);
      M->dirPeekIs(WEST);
    }
    break;

  case EAST:
    if (!M->maze_[MY_X_LOC][MY_Y_LOC + 1]) {
      M->yPeekIs(MY_Y_LOC + 1);
      M->dirPeekIs(SOUTH);
    }
    break;

  case WEST:
    if (!M->maze_[MY_X_LOC][MY_Y_LOC - 1]) {
      M->yPeekIs(MY_Y_LOC - 1);
      M->dirPeekIs(NORTH);
    }
    break;

  default:
    MWError((char *)"bad direction in PeekRight");
  }

  /* if any change, display the new view without moving! */

  if ((M->xPeek() != MY_X_LOC) || (M->yPeek() != MY_Y_LOC)) {
    M->peekingIs(TRUE);
    updateView = TRUE;
  }
}

/* ----------------------------------------------------------------------- */

void peekStop() {
  M->peekingIs(FALSE);
  updateView = TRUE;
}

/* ----------------------------------------------------------------------- */

void shoot() { 


}

/* ----------------------------------------------------------------------- */

void cloak() { 
				printf("Implement cloak()\n");
}


/* ----------------------------------------------------------------------- */

/*
 * Exit from game, clean up window
 */

void quit(int sig) {

  StopWindow();
  exit(0);
}

/* ----------------------------------------------------------------------- */

void NewPosition(MazewarInstance::Ptr m) {
  Loc newX(0);
  Loc newY(0);
  Direction dir(0); /* start on occupied square */

  while (M->maze_[newX.value()][newY.value()]) {
    /* MAZE[XY]MAX is a power of 2 */
    newX = Loc(random() & (MAZEXMAX - 1));
    newY = Loc(random() & (MAZEYMAX - 1));

    /* In real game, also check that square is
       unoccupied by another rat */
  }

  /* prevent a blank wall at first glimpse */

  if (!m->maze_[(newX.value()) + 1][(newY.value())])
    dir = Direction(NORTH);
  if (!m->maze_[(newX.value()) - 1][(newY.value())])
    dir = Direction(SOUTH);
  if (!m->maze_[(newX.value())][(newY.value()) + 1])
    dir = Direction(EAST);
  if (!m->maze_[(newX.value())][(newY.value()) - 1])
    dir = Direction(WEST);

  m->xlocIs(newX);
  m->ylocIs(newY);
  m->dirIs(dir);
}

/* ----------------------------------------------------------------------- */

void MWError(char s[])

{
  StopWindow();
  fprintf(stderr, "CS244BMazeWar: %s\n", s);
  perror("CS244BMazeWar");
  exit(-1);
}

/* ----------------------------------------------------------------------- */

/* This is just for the sample version, rewrite your own */
Score GetRatScore(RatIndexType ratIndex) {
				if (ratIndex.value() == M->myIndex().value()) {
								return (M->score());
				} else {
								return M->rat(ratIndex).score;		
				}
}

/* ----------------------------------------------------------------------- */

/* This is just for the sample version, rewrite your own */
char *GetRatName(RatIndexType ratIndex) {
				if (ratIndex.value() == M->myIndex().value()) {
								return (M->myName_);
				} else {
								return M->rat(ratIndex).Name;		
				}				
				return ((char *)"Dummy");
}

/* ----------------------------------------------------------------------- */

/* This is just for the sample version, rewrite your own if necessary */
void ConvertIncoming(MW244BPacket *p) {}

/* ----------------------------------------------------------------------- */

/* This is just for the sample version, rewrite your own if necessary */
void ConvertOutgoing(MW244BPacket *p) {}

/* ----------------------------------------------------------------------- */

/* This is just for the sample version, rewrite your own */
void ratStates() {
  /* In our sample version, we don't know about the state of any rats over
     the net, so this is a no-op */
}

/* ----------------------------------------------------------------------- */

/* This is just for the sample version, rewrite your own */
void manageMissiles() {
  /* Leave this up to you. */
  /*
  //You may find the following lines useful
  //This subtracts one from the current rat's score and updates the display
  M->scoreIs( M->score().value()-1 );
  UpdateScoreCard(M->myRatId().value());
  */
}

/* ----------------------------------------------------------------------- */

void DoViewUpdate() {
  if (updateView) { /* paint the screen */
    ShowPosition(MY_X_LOC, MY_Y_LOC, MY_DIR);
    if (M->peeking())
      ShowView(M->xPeek(), M->yPeek(), M->dirPeek());
    else
      ShowView(MY_X_LOC, MY_Y_LOC, MY_DIR);
    updateView = FALSE;
  }
}

/* ----------------------------------------------------------------------- */

/*
 * Sample code to send a packet to a specific destination
 */

/*
 * Notice the call to ConvertOutgoing.  You might want to call ConvertOutgoing
 * before any call to sendto.
 */

void copybit(uint32_t *temp, uint32_t src, uint8_t offset, uint8_t length) {
				if(length > 32 || offset < 0 || length + offset > 32){
								MWError((char*)"Set packet bits error\n");
				}
				if(length != 32) {
								uint32_t mask = (1 << length) - 1;
								src &= mask;
								src = src << offset;
				}
				*temp |= src;
				return;
}	

parsedInfo parsebit(uint32_t src, uint8_t offset, uint8_t length) {
        parsedInfo info;
        if(length > 32 || offset < 0 || length + offset > 32) {
                MWError((char*)"Set packet bits error\n");
        }
        if(length == 1) {
          info.bit_1 = src & 1;
        }else if(length <= 8) {
          uint32_t mask = 1<<8;
          info.bit_8 = src & (mask - 1);
        }else if(length <= 16) {
          uint32_t mask = 1<<16;
          info.bit_16 = src & (mask - 1);
        }else {
          info.bit_32 = src;
        }
        return info;
}

absoluteInfo parseAbsoluteInfo(uint8_t* address){
  absoluteInfo result;
  uint32_t temp;
  uint16_t temp16;
  memcpy(&result.score, address, 4);
  memset(&temp, 0, 4);
  memcpy(&temp, address+4, 4);
  result.position = parsebit(temp, 23, 9).bit_16;
  result.direction = parsebit(temp, 21, 2).bit_8;
  result.cloak = parsebit(temp, 20, 1).bit_1;
  result.missileNumber = parsebit(temp, 18, 2).bit_8;
  for(int count=0; count<MAX_MISSILES; ++count){
    memset(&temp16, 0, 2);
    memcpy(&temp, address+8+count*2, 2);
    result.missiles[count].missileId = parsebit(temp16, 14, 2).bit_8;
    result.missiles[count].position = parsebit(temp16, 5, 9).bit_16;
    result.missiles[count].direction = parsebit(temp16, 3, 2).bit_8;
  }
  return result;
}

eventSpecificData parseEventData(uint8_t* address, uint8_t type){
  uint32_t temp1, temp2;
  memset(&temp1, 0, 4);
  memset(&temp2, 0, 4);
  memcpy(&temp1, address, 4);
  memcpy(&temp2, address+4, 8);
  eventSpecificData result;
  switch(type){
    case 0:
            result.cloak = parsebit(temp1, 31, 1).bit_1;
    case 1:
            result.moveData.direction = parsebit(temp1, 30, 2).bit_8;
            result.moveData.speed = parsebit(temp1, 28, 2).bit_8;
    case 2:
            result.bornData.position = parsebit(temp1, 23, 9).bit_16;
            result.bornData.direction = parsebit(temp1, 21, 2).bit_8;
    case 3:
            result.missileProjData.missileId = parsebit(temp1, 30, 2).bit_8;
            result.missileProjData.position = parsebit(temp1, 21, 9).bit_16;
            result.missileProjData.direction = parsebit(temp1, 19, 2).bit_8;
    case 4:
            result.missileHitData.ownerId = parsebit(temp1, 0, 32).bit_32;
            result.missileHitData.missileId = parsebit(temp2, 30, 2).bit_8;
            result.missileHitData.position = parsebit(temp2, 21, 9).bit_16;
            result.missileHitData.direction = parsebit(temp2, 19, 2).bit_8;
  }
  return result;
}

uncommittedAction parseUncommit(uint8_t* address){
  uncommittedAction result;
  uint32_t temp;
  memset(&temp, 0, 4);
  memcpy(&temp, address, 4);
  result.type = parsebit(temp, 28, 4).bit_8;
  result.EventId = parsebit(temp, 0, 28).bit_32;
  result.eventData = parseEventData(address+4, result.type);
  return result;
}

void encodeEventData(uint32_t* temp1, uint32_t* temp2, uint8_t type, eventSpecificData eventData) {

				switch(type) {
								//event cloak
								case 0:
												copybit(temp1, eventData.cloak, 31, 1);

								//event movement
								case 1:
												copybit(temp1, eventData.moveData.direction, 30, 2);
												copybit(temp1, eventData.moveData.speed, 28, 2);

								//event born
								case 2:
												copybit(temp1, eventData.bornData.position, 23, 9);
												copybit(temp1, eventData.bornData.direction, 21, 2);

								//event missile projection
								case 3:
												copybit(temp1, eventData.missileProjData.missileId, 30, 2);
												copybit(temp1, eventData.missileProjData.position, 21, 9);
												copybit(temp1, eventData.missileProjData.direction, 19, 2);

								//event missile hit
								case 4:
												copybit(temp1, eventData.missileHitData.ownerId, 0, 32);
												copybit(temp2, eventData.missileHitData.missileId, 30, 2);
												copybit(temp2, eventData.missileHitData.position, 21, 9);
												copybit(temp2, eventData.missileHitData.direction, 19, 2);
				}
				return;
}

void memcpy_helper(void* destination, void* source, std::size_t count){
          memcpy(destination, source, count);
          destination = static_cast<char*>(destination) + count;
          return;
}

void sendPacketToPlayer(RatId ratId, Sockaddr destSocket, unsigned char packType, packetInfo info) {
  /*
          MW244BPacket pack;
          DataStructureX *packX;

          pack.type = PACKET_TYPE_X;
          packX = (DataStructureX *) &pack.body;
          packX->foo = d1;
          packX->bar = d2;

          ....

          ConvertOutgoing(pack);

          if (sendto((int)M->theSocket(), &pack, sizeof(pack), 0,
                     (Sockaddr) destSocket, sizeof(Sockaddr)) < 0)
            { MWError("Sample error") };
  */

	MW244BPacket pack;
  pack.type = packType;
	uint32_t temp;
  uint32_t temp1, temp2;
  int count = 0;
  void* address = &pack.body;
  switch(packType){
					/* draft version, need to write a function like copybit(uint32_t temp, uint32_t src, uint8_t offset, uint8_t length) */
					/* need to change temp in copybit into an pointer */
					case HEARTBEAT:
								memcpy_helper(address, &info.hb_.heartbeatId, 4);
								memcpy_helper(address, &info.hb_.sourceId, 4);

					case HEARTBEATACK:
								memcpy_helper(address, &info.hbACK_.heartbeatId, 4);
								memcpy_helper(address, &info.hbACK_.sourceId, 4);
								memcpy_helper(address, &info.hbACK_.destinationId, 4);

					case EVENT:
								memset(&temp, 0, 4);
                copybit(&temp, info.ev_.type, 28, 4);	
								copybit(&temp, info.ev_.EventId, 0, 28);
								memcpy_helper(address, &temp, 4);
								memcpy_helper(address, &info.ev_.sourceId, 4);
								memcpy_helper(address, &info.ev_.absoInfo.score, 4);
								memset(&temp, 0, 4);
								copybit(&temp, info.ev_.absoInfo.position, 23, 9);
								copybit(&temp, info.ev_.absoInfo.direction, 21, 2);
								copybit(&temp, info.ev_.absoInfo.cloak, 20, 1);
								copybit(&temp, info.ev_.absoInfo.missileNumber, 18, 2);
								memcpy_helper(address, &temp, 4);
                for(count=0; count<MAX_MISSILES; ++count){
								  memset(&temp, 0, 4);
									copybit(&temp, info.ev_.absoInfo.missiles[count].missileId, 14, 2);
									copybit(&temp, info.ev_.absoInfo.missiles[count].position, 5, 9);
									copybit(&temp, info.ev_.absoInfo.missiles[count].direction, 3, 2);
                  uint16_t temp16 = temp;
									memcpy_helper(address, &temp16, 2);
								}	
								memset(&temp1, 0, 4);
								memset(&temp2, 0, 4);
							  encodeEventData(&temp1, &temp2, info.ev_.type, info.ev_.eventData);	
								memcpy_helper(address, &temp1, 4);
								memcpy_helper(address, &temp2, 4);

					case EVENTACK:
								memset(&temp, 0, 4);
								copybit(&temp, info.evACK_.eventId, 4, 28);
								memcpy_helper(address, &temp, 4);
								memcpy_helper(address, &info.evACK_.sourceId, 4);
								memcpy_helper(address, &info.evACK_.destinationId, 4);

					case STATEREQUEST:
								memcpy_helper(address, &info.SIReq_.sourceId, 4);

					case STATERESPONSE:
                memcpy_helper(address, &info.SIRes_.sourceId, 4); 
                memcpy_helper(address, &info.SIRes_.destinationId, 4); 
								memcpy_helper(address, &info.SIRes_.absoInfo.score, 4);
								memset(&temp, 0, 4);
								copybit(&temp, info.SIRes_.absoInfo.position, 23, 9);
								copybit(&temp, info.SIRes_.absoInfo.direction, 21, 2);
								copybit(&temp, info.SIRes_.absoInfo.cloak, 20, 1);
								copybit(&temp, info.SIRes_.absoInfo.missileNumber, 18, 2);
								memcpy_helper(address, &temp, 4);
                for(count=0; count<MAX_MISSILES; ++count){
                  memset(&temp, 0, 4);
                  copybit(&temp, info.ev_.absoInfo.missiles[count].missileId, 14, 2);
                  copybit(&temp, info.ev_.absoInfo.missiles[count].position, 5, 9);
                  copybit(&temp, info.ev_.absoInfo.missiles[count].direction, 3, 2);
                  uint16_t temp16 = temp;
                  memcpy_helper(address, &temp16, 2);
                } 
								//each uncommited event has 3 x 32bits, i.e. 12bytes
								for(int j=0; j<info.SIRes_.uncommitted_number; j++){
											//encoding data
											memset(&temp, 0, 4);
											copybit(&temp, info.SIRes_.uncommit[j].type, 28, 4);	
											copybit(&temp, info.SIRes_.uncommit[j].EventId, 0, 28);
											memset(&temp1, 0, 4);
											memset(&temp2, 0, 4);
							  			encodeEventData(&temp1, &temp2, info.SIRes_.uncommit[j].type, info.SIRes_.uncommit[j].eventData);	
											//copying data
											memcpy_helper(address, &temp, 4);
											memcpy_helper(address, &temp1, 4);
											memcpy_helper(address, &temp2, 4);
								}

					case STATEACK:
								memcpy_helper(address, &info.SIACK_.sourceId, 4);
								memcpy_helper(address, &info.SIACK_.destinationId, 4);

	}	

  if (sendto((int)M->theSocket(), &pack, sizeof(pack), 0,
                     (struct sockaddr *)&destSocket, sizeof(Sockaddr)) < 0)
            MWError((char *)"Sample error");
}

packetInfo packetParser(MW244BPacket* pack) {
  packetInfo info;
  uint32_t temp;
  //uint16_t temp16;
  switch (pack->type) {
          case HEARTBEAT:
              memcpy(&info.hb_.heartbeatId, pack->body, 4);
              memcpy(&info.hb_.sourceId, pack->body+4, 4);

          case HEARTBEATACK:
              memcpy(&info.hbACK_.heartbeatId, pack->body, 4);
              memcpy(&info.hbACK_.sourceId, pack->body+4, 4);
              memcpy(&info.hbACK_.destinationId, pack->body+8, 4);

          case EVENT:
              memset(&temp, 0, 4);
              memcpy(&temp, pack->body, 4);
              info.ev_.type = parsebit(temp, 28, 4).bit_8;
              info.ev_.EventId = parsebit(temp, 0, 28).bit_32;
              memcpy(&info.ev_.sourceId, pack->body+4, 4);
              info.ev_.absoInfo = parseAbsoluteInfo((uint8_t*)pack->body +8);
              info.ev_.eventData = parseEventData((uint8_t*)pack->body+16+MAX_MISSILES*2, info.ev_.type);

          case EVENTACK:
              memset(&temp, 0, 4);
              memcpy(&temp, pack->body, 4);
              info.evACK_.eventId = parsebit(temp, 4, 28).bit_32;
              memcpy(&info.evACK_.sourceId, pack->body+4, 4);
              memcpy(&info.evACK_.destinationId, pack->body+8, 4);
     
          case STATEREQUEST:
              memcpy(&info.SIReq_.sourceId, pack->body, 4);         
           
          case STATERESPONSE:
              memcpy(&info.SIRes_.sourceId, pack->body, 4);
              memcpy(&info.SIRes_.destinationId, pack->body+4, 4);
              info.SIRes_.absoInfo = parseAbsoluteInfo((uint8_t*)pack->body+8);
              memcpy(&info.SIRes_.uncommitted_number, pack->body+16+MAX_MISSILES*2, 4);
              //info.SIRes_.uncommitted_number = parsebit((uint32_t*)(pack->body+16+MAX_MISSILES*2), 0, 32).bit_32;
              for(int j=0; j<info.SIRes_.uncommitted_number; ++j){
                info.SIRes_.uncommit[j] = parseUncommit((uint8_t*)pack->body+20+MAX_MISSILES*2);
              }
           
          case STATEACK:        
              memcpy(&info.SIACK_.sourceId, pack->body, 4);
              memcpy(&info.SIACK_.destinationId, pack->body+4, 4); 
            
  }
  return info;
}


void packet_handler(packetInfo info) {


}
/* ----------------------------------------------------------------------- */

/* Sample of processPacket. */

void processPacket(MWEvent *eventPacket) {
  /*
          MW244BPacket		*pack = eventPacket->eventDetail;
          DataStructureX		*packX;

          switch(pack->type) {
          case PACKET_TYPE_X:
            packX = (DataStructureX *) &(pack->body);
            break;
          case ...
          }
  */

  /*
	 * case Heartbeat: 									Send Heartbeat ACK
	 * case Heartbeat ACK: 							Clear relevant Heartbeat resend
	 * case Event: 											Save the action into next time slot to process, send Event ACK
	 * case Event ACK: 									Clear relevant Event resend
	 * case State Inquiry Request: 			Send State Inquiry Response
	 * case State Inquiry Response:			Clear relevant State Inquiry Request resend, send State Inquiry ACK 
	 * case State Inquiry ACK:					Clear relevant State Inquiry Response resend
	 */
  packetInfo info = packetParser(eventPacket->eventDetail);
  packet_handler(info);
	return;
}

/* ----------------------------------------------------------------------- */

/* This will presumably be modified by you.
   It is here to provide an example of how to open a UDP port.
   You might choose to use a different strategy
 */
void netInit() {
  Sockaddr nullAddr;
  Sockaddr *thisHost;
  char buf[128];
  int reuse;
  u_char ttl;
  struct ip_mreq mreq;

  /* MAZEPORT will be assigned by the TA to each team */
  M->mazePortIs(htons(MAZEPORT));

  gethostname(buf, sizeof(buf));
  if ((thisHost = resolveHost(buf)) == (Sockaddr *)NULL)
    MWError((char *)"who am I?");
  bcopy((caddr_t)thisHost, (caddr_t)(M->myAddr()), sizeof(Sockaddr));

  M->theSocketIs(socket(AF_INET, SOCK_DGRAM, 0));
  if (M->theSocket() < 0)
    MWError((char *)"can't get socket");

  /* SO_REUSEADDR allows more than one binding to the same
     socket - you cannot have more than one player on one
     machine without this */
  reuse = 1;
  if (setsockopt(M->theSocket(), SOL_SOCKET, SO_REUSEADDR, &reuse,
                 sizeof(reuse)) < 0) {
    MWError((char *)"setsockopt failed (SO_REUSEADDR)");
  }

  nullAddr.sin_family = AF_INET;
  nullAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  nullAddr.sin_port = M->mazePort();
  if (bind(M->theSocket(), (struct sockaddr *)&nullAddr, sizeof(nullAddr)) < 0)
    MWError((char *)"netInit binding");

  /* Multicast TTL:
     0 restricted to the same host
     1 restricted to the same subnet
     32 restricted to the same site
     64 restricted to the same region
     128 restricted to the same continent
     255 unrestricted

     DO NOT use a value > 32. If possible, use a value of 1 when
     testing.
  */

  ttl = 1;
  if (setsockopt(M->theSocket(), IPPROTO_IP, IP_MULTICAST_TTL, &ttl,
                 sizeof(ttl)) < 0) {
    MWError((char *)"setsockopt failed (IP_MULTICAST_TTL)");
  }

  /* join the multicast group */
  mreq.imr_multiaddr.s_addr = htonl(MAZEGROUP);
  mreq.imr_interface.s_addr = htonl(INADDR_ANY);
  if (setsockopt(M->theSocket(), IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mreq,
                 sizeof(mreq)) < 0) {
    MWError((char *)"setsockopt failed (IP_ADD_MEMBERSHIP)");
  }

  /*
   * Now we can try to find a game to join; if none, start one.
   */

  printf("\n");

  /* set up some stuff strictly for this local sample */
  M->myRatIdIs(htonl(thisHost->sin_addr.s_addr));
  M->scoreIs(0);
  SetMyRatIndexType(0);
	M->AllRats.insert({M->myIndex(), M->myRatId()});

  /* Get the multi-cast address ready to use in SendData()
     calls. */
  memcpy(&groupAddr, &nullAddr, sizeof(Sockaddr));
  groupAddr.sin_addr.s_addr = htonl(MAZEGROUP);
}

/* ----------------------------------------------------------------------- */
