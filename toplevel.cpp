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

using namespace std;

static int shoot_timer = 0;
static int cloak_timer = 0;
static bool updateView; /* true if update needed */
MazewarInstance::Ptr M;

/* Use this socket address to send packets to the multi-cast group. */
static Sockaddr groupAddr;

/*
  event information
*/
static event_unprocessed events_all_player; //saved event to processed in the end of time slot
static std::map<uint32_t, respond_set> local_event_resend; //events sent from local this time slot
static uint32_t HeartbeatID = 1;
static uint32_t NextEventID = 1;
//static uint32_t CommittedEvent = 0;

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


  if (argc == 5) {
    M->xlocIs(atoi(argv[2]));
  M->ylocIs(atoi(argv[3]));
  // match input to direction representation
  if (!strcmp(argv[4], "n"))
    M->dirIs(NORTH);
  else if (!strcmp(argv[4], "s"))
    M->dirIs(SOUTH);
  else if (!strcmp(argv[4], "e"))
    M->dirIs(EAST);
  else if (!strcmp(argv[4], "w"))
    M->dirIs(WEST);
  else
    M->dirIs(NORTH);
  } 

  
  MazeInit(argc, argv);

  JoinGame();
  ShowAllPositions();
  play();

  return 0;
}

/* ----------------------------------------------------------------------- */
void JoinGame(){
  MWEvent event;
  MW244BPacket incoming;
  event.eventDetail = &incoming;
  //send SI request
  packetInfo packet;
  int temp_val = M->myRatId().value();
  packet.SIReq_.sourceId = temp_val;
  sendPacketToPlayer(STATEREQUEST, packet); 

  int join_phase_finsh = 0;
  //wait for 10 time slots i.e. 2 seconds, respond to hb, save SI response
  while(join_phase_finsh<10){
    NextEvent(&event, M->theSocket());
    switch(event.eventType){
      case EVENT_NETWORK:
        processPacket(&event, JOINPHASE);
        break;

      case EVENT_TIMEOUT:
        join_phase_finsh++;
        break;

      case EVENT_INT:
        quit(0);
        break;
    }
  }

  //Update own infomation, like x, y, dir
  Rat temp_rat = M->rat(0);
  //temp_rat.Name = M->myName_;
  temp_rat.dir = M->dir();
  temp_rat.score = M->score();
  temp_rat.x = M->xloc();
  temp_rat.y = M->yloc();
  temp_rat.cloaked = FALSE;
  for(int i=0; i<MAX_MISSILES; ++i)
    temp_rat.RatMissile[i].exist = FALSE;
  M->ratIs(temp_rat, 0);

  //send born event
  eventSpecificData eventData;
  eventData.bornData.position.x = temp_rat.y.value();
  eventData.bornData.position.y = temp_rat.y.value();
  eventData.bornData.direction = temp_rat.dir.value();
  packet = eventPacketGenerator(EVENTBORN, eventData); 
  sendPacketToPlayer(EVENT, packet); 

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
      case EVENT_TIMEOUT: {
        shoot_timer--;
        cloak_timer--;
        ratStates(); 
        manageMissiles();
        //After consistency resolved, state and view might be changed
        updateView = TRUE;
        DoViewUpdate(); 
      } break;

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
        processPacket(&event, PLAYPHASE);
        break;

      case EVENT_INT:
        quit(0);
        break;
      }
    else
      switch (event.eventType) {
      case EVENT_TIMEOUT: {
        ratStates(); /* clean house */
        manageMissiles();
        updateView = TRUE;
        DoViewUpdate();
      } break;
      case EVENT_RIGHT_U:
      case EVENT_LEFT_U:
        peekStop();
        break;

      case EVENT_NETWORK:
        processPacket(&event, PLAYPHASE);
        break;
      }
      DoViewUpdate();

    /* Any info to send over network? */
  }
}

/* ----------------------------------------------------------------------- */

static Direction _aboutFace[NDIRECTION] = {SOUTH, NORTH, WEST, EAST};
static Direction _leftTurn[NDIRECTION] = {WEST, EAST, NORTH, SOUTH};
static Direction _rightTurn[NDIRECTION] = {EAST, WEST, SOUTH, NORTH};

absoluteInfo absoluteInfoGenerator(){
  Rat temp_rat = M->rat(0);
  absoluteInfo info;
  info.score = temp_rat.score.value();
  info.position.x = temp_rat.x.value();
  info.position.y = temp_rat.y.value();
  info.direction = temp_rat.dir.value();
  info.cloak = temp_rat.cloaked;
  info.missileNumber = 0;
  Missile* temp_missile = temp_rat.RatMissile;
  for(int i=0; i<MAX_MISSILES; ++i){
    if(temp_missile->exist){
      info.missiles[i].exist = TRUE;
      info.missiles[i].position.x = temp_missile->x.value();
      info.missiles[i].position.y = temp_missile->y.value();
      info.missiles[i].direction = temp_missile->dir.value();
      info.missileNumber += 1 << i;
    }else {
      info.missiles[i].exist = FALSE;
    }
    temp_missile++;
  }
  return info;
}

packetInfo eventPacketGenerator(uint8_t eventtype, eventSpecificData eventData){
  packetInfo info;
  info.ev_.type = eventtype;
  int temp_val = M->myRatId().value();
  info.ev_.sourceId = temp_val;  info.ev_.eventId = NextEventID;
  NextEventID++;
  info.ev_.absoInfo = absoluteInfoGenerator();
  info.ev_.eventData = eventData;
  return info;
}

void aboutFace(void) {
  M->dirIs(_aboutFace[MY_DIR]);
  updateView = TRUE;
  eventSpecificData eventData;
  eventData.moveData.direction = _aboutFace[MY_DIR].value();
  eventData.moveData.speed = 0;
  packetInfo info = eventPacketGenerator(EVENTMOVE, eventData);
  sendPacketToPlayer(EVENT, info); 
}

/* ----------------------------------------------------------------------- */

void leftTurn(void) {
  M->dirIs(_leftTurn[MY_DIR]);
  updateView = TRUE;
  eventSpecificData eventData;
  eventData.moveData.direction = _leftTurn[MY_DIR].value();
  eventData.moveData.speed = 0;
  packetInfo info = eventPacketGenerator(EVENTMOVE, eventData);
  sendPacketToPlayer(EVENT, info); 
}

/* ----------------------------------------------------------------------- */

void rightTurn(void) {
  M->dirIs(_rightTurn[MY_DIR]);
  updateView = TRUE;
  eventSpecificData eventData;
  eventData.moveData.direction = _rightTurn[MY_DIR].value();
  eventData.moveData.speed = 0;
  packetInfo info = eventPacketGenerator(EVENTMOVE, eventData);
  sendPacketToPlayer(EVENT, info); 
}

/* ----------------------------------------------------------------------- */

/* remember ... "North" is to the right ... positive X motion */

void forward(void) {
  register int tx = MY_X_LOC;
  register int ty = MY_Y_LOC;

  switch (MY_DIR) {
  case NORTH:
    if (!M->maze_[tx + 1][ty] && !M->occupy_[tx + 1][ty])
      tx++;
    break;
  case SOUTH:
    if (!M->maze_[tx - 1][ty] && !M->occupy_[tx - 1][ty])
      tx--;
    break;
  case EAST:
    if (!M->maze_[tx][ty + 1] && !M->occupy_[tx][ty + 1])
      ty++;
    break;
  case WEST:
    if (!M->maze_[tx][ty - 1] && !M->occupy_[tx][ty - 1])
      ty--;
    break;
  default:
    MWError((char *)"bad direction in Forward");
  }
  if ((MY_X_LOC != tx) || (MY_Y_LOC != ty)) {
    M->xlocIs(Loc(tx));
    M->ylocIs(Loc(ty));
    updateView = TRUE;
    eventSpecificData eventData;
    eventData.moveData.direction = MY_DIR;
    eventData.moveData.speed = 1;
    packetInfo info = eventPacketGenerator(EVENTMOVE, eventData);
    sendPacketToPlayer(EVENT, info); 
  }
}

/* ----------------------------------------------------------------------- */

void backward() {
  register int tx = MY_X_LOC;
  register int ty = MY_Y_LOC;

  switch (MY_DIR) {
  case NORTH:
    if (!M->maze_[tx - 1][ty] && !M->occupy_[tx - 1][ty])
      tx--;
    break;
  case SOUTH:
    if (!M->maze_[tx + 1][ty] && !M->occupy_[tx + 1][ty])
      tx++;
    break;
  case EAST:
    if (!M->maze_[tx][ty - 1] && !M->occupy_[tx][ty - 1])
      ty--;
    break;
  case WEST:
    if (!M->maze_[tx][ty + 1] && !M->occupy_[tx][ty + 1])
      ty++;
    break;
  default:
    MWError((char *)"bad direction in Backward");
  }
  if ((MY_X_LOC != tx) || (MY_Y_LOC != ty)) {
    M->xlocIs(Loc(tx));
    M->ylocIs(Loc(ty));
    updateView = TRUE;
    eventSpecificData eventData;
    eventData.moveData.direction = MY_DIR;
    eventData.moveData.speed = 2;
    packetInfo info = eventPacketGenerator(EVENTMOVE, eventData);
    sendPacketToPlayer(EVENT, info); 
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
  //Add cooldown timer
  if(shoot_timer > 0){
    printf("Missile Projection still in Cooldown!\n");
    return;
  }
  //set shoot timer = 3 seconds
  shoot_timer = 15;
  Rat temp_rat = M->rat(0);
  int i;
  for(i=0; i<MAX_MISSILES; ++i){
    if(!temp_rat.RatMissile[i].exist) break;
  }
  if(i == MAX_MISSILES){
    printf("Max missile limit %d", MAX_MISSILES);
    return;
  }

  Missile temp_missile;

  temp_missile.exist = TRUE;
  temp_missile.x = temp_rat.x;
  temp_missile.y = temp_rat.y;
  temp_missile.dir = temp_rat.dir;
  eventSpecificData eventData;
  eventData.missileProjData.missileId = i;
  eventData.missileProjData.position.x = temp_rat.x.value();
  eventData.missileProjData.position.y = temp_rat.y.value();
  eventData.missileProjData.direction = temp_rat.dir.value();
  sendPacketToPlayer(EVENT, eventPacketGenerator(EVENTSHOOT, eventData));

  temp_rat.RatMissile[i] = temp_missile;
  M->ratIs(temp_rat, 0);
}

/* ----------------------------------------------------------------------- */

void cloak() { 
  if(cloak_timer > 0){
    printf("Cloak still in Cooldown!\n");
    return;
  }
  cloak_timer = 15;
  //implement timer here
  eventSpecificData eventData;
  eventData.cloak = TRUE;
  packetInfo info = eventPacketGenerator(EVENTSHOOT, eventData);
  sendPacketToPlayer(EVENT, info);

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
  Loc newX(MY_X_LOC);
  Loc newY(MY_Y_LOC);
  Direction dir(MY_DIR); /* start on occupied square */

  while (!M->maze_[newX.value()][newY.value()] && M->occupy_[newX.value()][newY.value()]) {
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
                char buf[1];
                sprintf(buf, "%d", ratIndex.value());
								return ((char *)"Player %s", buf);
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

Rat setAbsoInfo(Rat temp_rat, absoluteInfo absoInfo){
  temp_rat.cloaked = absoInfo.cloak;
  temp_rat.dir = absoInfo.direction;
  temp_rat.x = absoInfo.position.x;
  temp_rat.y = absoInfo.position.y;
  temp_rat.score = absoInfo.score;
  for(int i=0; i<MAX_MISSILES; ++i){
    if(absoInfo.missiles[i].exist){
      temp_rat.RatMissile[i].exist  = TRUE;
      temp_rat.RatMissile[i].x      = absoInfo.missiles[i].position.x;
      temp_rat.RatMissile[i].y      = absoInfo.missiles[i].position.y;
      temp_rat.RatMissile[i].dir    = absoInfo.missiles[i].direction;
      
    }
  }
  return temp_rat;
}

Rat processEvent(Rat temp_rat, eventSpecificData eventData, uint8_t type){
  switch(type){
    case EVENTCLOAK: {
      temp_rat.cloaked = eventData.cloak;
    }break;

    case EVENTMOVE: {
      if(eventData.moveData.speed == 0){//change direction
        temp_rat.dir = eventData.moveData.direction;
      }else {//move
        //offset = 1 if forward, otherwise -1
        int offset = (eventData.moveData.speed == 1)? 1: -1;
        int x = temp_rat.x.value();
        int y = temp_rat.y.value();
        switch(temp_rat.dir.value()){
          case NORTH:
            if (!M->maze_[x + offset][y])
              temp_rat.x = x + offset;
            break;
          case SOUTH:
            if (!M->maze_[x - offset][y])
              temp_rat.x = x - offset;
            break;
          case EAST:
            if (!M->maze_[x][y + offset])
              temp_rat.y = y + offset;
            break;
          case WEST:
            if (!M->maze_[x][y - offset])
              temp_rat.y = y - offset;
            break;
        }
      }
    }break;      

    case EVENTBORN: {
      temp_rat.dir  = eventData.bornData.direction;
      temp_rat.x    = eventData.bornData.position.x;
      temp_rat.y    = eventData.bornData.position.y;    
    }break;  

    case EVENTSHOOT: {
      int missileId = eventData.missileProjData.missileId;
      if(missileId < 0 || missileId >= MAX_MISSILES){
        MWError((char*)"Wrong missileId\n");
        break;
      } 
      temp_rat.RatMissile[missileId].exist = TRUE;
      temp_rat.RatMissile[missileId].x = eventData.missileProjData.position.x;
      temp_rat.RatMissile[missileId].y = eventData.missileProjData.position.y;
      temp_rat.RatMissile[missileId].dir = eventData.missileProjData.direction;
    }break;

    case EVENTHIT:  {
      //Here only change the score, disappear the missile   
      temp_rat.score = temp_rat.score.value() - (5 + (temp_rat.cloaked)? 2:0);
      //If the owner exist, add score, disapper the missile
      if(M->AllRats.find(eventData.missileHitData.ownerId) != M->AllRats.end()){
        int ratindex = (M->AllRats.find(eventData.missileHitData.ownerId))->second;
        Rat owner = M->rat(ratindex);
        owner.score = owner.score.value() + 11 + (temp_rat.cloaked)? 2:0;
        owner.RatMissile[eventData.missileHitData.missileId].exist = FALSE;
      }
    }break;
  }
  return temp_rat;
}


void clearOccupy(void) {
  int i, j;

  for (i = 0; i < MAZEXMAX; i++) {
    for (j = 0; j < MAZEYMAX; j++) {
      M->occupy_[i][j] = FALSE;
    }
  }
}


/* This is just for the sample version, rewrite your own */
//Here process all events to update status in mazeRats_[], triggered by a event_timeout
void ratStates() {
  int ratindex;
  packetInfo info;
  Rat temp_rat;
  //Process all events saved in events_all_player by ID
  event_unprocessed_iter iter = events_all_player.begin();
  while(iter != events_all_player.end()){
    //Process this player's state
    if(M->AllRats.find(iter->first) != M->AllRats.end()){
      int tempxxx = (M->AllRats.find(iter->first))->first;
      ratindex = (M->AllRats.find(iter->first))->second;
      player_unprocessed event_per_player = iter->second;
      //Already been sorted by EventID
      player_unprocessed_iter iter_event;
      for(iter_event=event_per_player.begin(); iter_event!=event_per_player.end(); iter_event++){
        info = iter_event->second;
        absoluteInfo absoInfo = info.ev_.absoInfo;
        eventSpecificData eventData = info.ev_.eventData;
        
        if(iter_event == event_per_player.begin())  temp_rat = setAbsoInfo(temp_rat, absoInfo);
        temp_rat = processEvent(temp_rat, eventData, info.ev_.type);
      }
      M->ratIs(temp_rat, ratindex);
    }
    iter++;
  }
  
  if(0){
  //Process hbACK_set, if value > 50(no response for 10 secs), think it's dropped; else, add 1
  int index = 0;
  for(respond_set_iter iter = hbACK_set.begin(); iter!=hbACK_set.end(); iter++){
    if(iter->second >= 50){
      M->mazeplay[index] = FALSE;
      temp_rat = M->rat(index);
      temp_rat.playing = FALSE;
      M->ratIs(temp_rat, index);
    }
    else iter->second++;
    index++;
  }
}

  //Send heartbeat
  info.hb_.heartbeatId = HeartbeatID;
  HeartbeatID++;
  int temp_val = M->myRatId().value();
  info.hb_.sourceId = temp_val;
  sendPacketToPlayer(HEARTBEAT, info);

  //Clear uncommitted events
  for(iter = events_all_player.begin(); iter != events_all_player.end(); iter++){
    player_unprocessed empty_events = player_unprocessed();
    iter->second = empty_events;
  }

  //Update own state in M
  temp_rat = M->rat(0);
  int temp;
  //temp = M->dir.value();
  M->dirIs(temp_rat.dir);
  temp = M->xloc().value();
  M->xlocIs(temp_rat.x);
  temp = M->yloc().value();
  M->ylocIs(temp_rat.y);
  temp = M->score().value();
  M->scoreIs(temp_rat.score);

  //update M->occupy_
  clearOccupy();
  for(int i=0; i<MAX_RATS; ++i){
    if(M->rat(i).playing){
      int x = M->rat(i).y.value();
      int y = M->rat(i).x.value();
      M->occupy_[x][y] = TRUE;
    }
  }
}

/* ----------------------------------------------------------------------- */

/* This is just for the sample version, rewrite your own */
void manageMissiles() {
  /*Here updates all missiles in all rats, if the missile hit yourself, send a missilehit packet and born event, if the missile hit a wall,
    change it's exist to FALSE
  */
  Rat temp_rat;
  //move all missiles forward, if it hits wall, disappear it; if it hits self, send hit event
  for(int i=0; i<MAX_RATS; ++i){
    //Check all rats
    temp_rat = M->rat(i);
    if(temp_rat.playing){

      for(int j=0; j<MAX_MISSILES; ++j){
        if(temp_rat.RatMissile[j].exist){
          //Move it forward
          int tx = temp_rat.RatMissile[j].x.value();
          int ty = temp_rat.RatMissile[j].y.value();
        
          switch (temp_rat.RatMissile[j].dir.value()) {
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
          }
          if(tx != temp_rat.RatMissile[j].x.value() || ty != temp_rat.RatMissile[j].y.value()){

            //if this missile belongs to self, show it, set update view
            if(i == 0)
              showMissile(Loc(tx), Loc(ty), temp_rat.RatMissile[j].dir, temp_rat.RatMissile[j].x, temp_rat.RatMissile[j].y, TRUE);
            temp_rat.RatMissile[j].x = tx;
            temp_rat.RatMissile[j].y = ty;

            //if this missile belongs to others, check if self tagged
            if(tx == M->xloc().value() && ty == M->yloc().value()){
              eventSpecificData eventData;
              for(std::map<uint32_t, int>::iterator iter = M->AllRats.begin(); iter != M->AllRats.end(); iter++){
                if(iter->second == i)    eventData.missileHitData.ownerId = iter->first;
              }
              eventData.missileHitData.missileId  = j;
              eventData.missileHitData.position.x = tx;
              eventData.missileHitData.position.y = ty;
              eventData.missileHitData.direction  = temp_rat.RatMissile[j].dir.value();
              packetInfo info = eventPacketGenerator(EVENTHIT, eventData);
              sendPacketToPlayer(EVENT, info);
            }
          } else{
          //Hit the wall, disappear
            temp_rat.RatMissile[j].exist = FALSE;
            if(i == 0)
              clearSquare(temp_rat.RatMissile[j].x, temp_rat.RatMissile[j].y);

            
          }
          M->ratIs(temp_rat, i);
        } 
      }
    }
  }
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
        src = src >> offset;
        if(length == 1) {
          info.bit_1 = src & 1;
        }else if(length <= 8) {
          uint32_t mask = 1<<length;
          info.bit_8 = src & (mask - 1);
        }else if(length <= 16) {
          uint32_t mask = 1<<length;
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
  memcpy(&temp, ((uint8_t*)address)+4, 4);
  result.position.x = parsebit(temp, 27, 5).bit_16;
  result.position.y = parsebit(temp, 23, 4).bit_16;
  result.direction = parsebit(temp, 21, 2).bit_8;
  result.cloak = parsebit(temp, 20, 1).bit_1;
  result.missileNumber = parsebit(temp, 16, 4).bit_8;
  for(int count=0; count<MAX_MISSILES; ++count){
    memset(&temp16, 0, 2);
    memcpy(&temp16, ((uint8_t*)address)+8+count*2, 2);
    result.missiles[count].exist = parsebit(temp16, 14, 2).bit_8;
    result.missiles[count].position.x = parsebit(temp16, 9, 5).bit_16;
    result.missiles[count].position.y = parsebit(temp16, 5, 4).bit_16;
    result.missiles[count].direction = parsebit(temp16, 3, 2).bit_8;
  }
  return result;
}

eventSpecificData parseEventData(uint8_t* address, uint8_t type){
  uint32_t temp1, temp2;
  memset(&temp1, 0, 4);
  memset(&temp2, 0, 4);
  memcpy(&temp1, address, 4);
  memcpy(&temp2, ((uint8_t*)address)+4, 8);
  eventSpecificData result;
  switch(type){
    case EVENTCLOAK:
            result.cloak = parsebit(temp1, 31, 1).bit_1;
            break;

    case EVENTMOVE:
            result.moveData.direction = parsebit(temp1, 30, 2).bit_8;
            result.moveData.speed = parsebit(temp1, 28, 2).bit_8;
            break;

    case EVENTBORN:
            result.bornData.position.x = parsebit(temp1, 27, 5).bit_16;
            result.bornData.position.y = parsebit(temp1, 23, 4).bit_16;
            result.bornData.direction = parsebit(temp1, 21, 2).bit_8;
            break;

    case EVENTSHOOT:
            result.missileProjData.missileId = parsebit(temp1, 30, 2).bit_8;
            result.missileProjData.position.x = parsebit(temp1, 25, 5).bit_16;
            result.missileProjData.position.y = parsebit(temp1, 21, 4).bit_16;
            result.missileProjData.direction = parsebit(temp1, 19, 2).bit_8;
            break;

    case EVENTHIT:
            result.missileHitData.ownerId = parsebit(temp1, 0, 32).bit_32;
            result.missileHitData.missileId = parsebit(temp2, 30, 2).bit_8;
            result.missileHitData.position.x = parsebit(temp2, 25, 5).bit_16;
            result.missileHitData.position.y = parsebit(temp2, 21, 4).bit_16;
            result.missileHitData.direction = parsebit(temp2, 19, 2).bit_8;
            break;
  }
  return result;
}

uncommittedAction parseUncommit(uint8_t* address){
  uncommittedAction result;
  uint32_t temp;
  memset(&temp, 0, 4);
  memcpy(&temp, address, 4);
  result.type = parsebit(temp, 28, 4).bit_8;
  result.eventId = parsebit(temp, 0, 28).bit_32;
  result.eventData = parseEventData(address+4, result.type);
  return result;
}

void encodeEventData(uint32_t* temp1, uint32_t* temp2, uint8_t type, eventSpecificData eventData) {

				switch(type) {
								//event cloak
								case EVENTCLOAK:
												copybit(temp1, eventData.cloak, 31, 1);
                        break;

								//event movement
								case EVENTMOVE:
												copybit(temp1, eventData.moveData.direction, 30, 2);
												copybit(temp1, eventData.moveData.speed, 28, 2);
                        break;

								//event born
								case EVENTBORN:
												copybit(temp1, eventData.bornData.position.x, 27, 5);
                        copybit(temp1, eventData.bornData.position.y, 23, 4);
												copybit(temp1, eventData.bornData.direction, 21, 2);
                        break;

								//event missile projection
								case EVENTSHOOT:
												copybit(temp1, eventData.missileProjData.missileId, 30, 2);
												copybit(temp1, eventData.missileProjData.position.x, 25, 5);
                        copybit(temp1, eventData.missileProjData.position.y, 21, 4);
												copybit(temp1, eventData.missileProjData.direction, 19, 2);
                        break;

								//event missile hit
								case EVENTHIT:
												copybit(temp1, eventData.missileHitData.ownerId, 0, 32);
												copybit(temp2, eventData.missileHitData.missileId, 30, 2);
												copybit(temp2, eventData.missileHitData.position.x, 25, 5);
                        copybit(temp2, eventData.missileHitData.position.y, 21, 4);
												copybit(temp2, eventData.missileHitData.direction, 19, 2);
                        break;
				}
				return;
}

void memcpy_helper(void* destination, void* source, std::size_t count, std::size_t* offset){
          memset((uint8_t*)destination+*offset, 0, count);
          memcpy((uint8_t*)destination+*offset, source, count);
          //destination = (void*)(static_cast<char*>(destination) + count);
          *offset += count;
          return;
}

void sendPacketToPlayer(unsigned char packType, packetInfo info) {
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
  std::size_t offset = 0;
  switch(packType){
					/* draft version, need to write a function like copybit(uint32_t temp, uint32_t src, uint8_t offset, uint8_t length) */
					/* need to change temp in copybit into an pointer */
					case HEARTBEAT:
								memcpy_helper(address, &info.hb_.heartbeatId, 4, &offset);
								memcpy_helper(address, &info.hb_.sourceId, 4, &offset);
                break;

					case HEARTBEATACK:
								memcpy_helper(address, &info.hbACK_.heartbeatId, 4, &offset);
								memcpy_helper(address, &info.hbACK_.sourceId, 4, &offset);
								memcpy_helper(address, &info.hbACK_.destinationId, 4, &offset);
                break;

					case EVENT:
								memset(&temp, 0, 4);
                copybit(&temp, info.ev_.type, 28, 4);	
								copybit(&temp, info.ev_.eventId, 0, 28);
								memcpy_helper(address, &temp, 4, &offset);
								memcpy_helper(address, &info.ev_.sourceId, 4, &offset);
								memcpy_helper(address, &info.ev_.absoInfo.score, 4, &offset);
								memset(&temp, 0, 4);
								copybit(&temp, info.ev_.absoInfo.position.x, 27, 5);
                copybit(&temp, info.ev_.absoInfo.position.y, 23, 4);
								copybit(&temp, info.ev_.absoInfo.direction, 21, 2);
								copybit(&temp, info.ev_.absoInfo.cloak, 20, 1);
								copybit(&temp, info.ev_.absoInfo.missileNumber, 16, 4);
								memcpy_helper(address, &temp, 4, &offset);
                for(count=0; count<MAX_MISSILES; ++count){
								  memset(&temp, 0, 4);
									copybit(&temp, info.ev_.absoInfo.missiles[count].exist, 14, 2);
									copybit(&temp, info.ev_.absoInfo.missiles[count].position.x, 9, 5);
                  copybit(&temp, info.ev_.absoInfo.missiles[count].position.y, 5, 4);
									copybit(&temp, info.ev_.absoInfo.missiles[count].direction, 3, 2);
                  uint16_t temp16 = temp;
									memcpy_helper(address, &temp16, 2, &offset);
								}	
								memset(&temp1, 0, 4);
								memset(&temp2, 0, 4);
							  encodeEventData(&temp1, &temp2, info.ev_.type, info.ev_.eventData);	
								memcpy_helper(address, &temp1, 4, &offset);
								memcpy_helper(address, &temp2, 4, &offset);
                break;

					case EVENTACK:
								memset(&temp, 0, 4);
								copybit(&temp, info.evACK_.eventId, 4, 28);
								memcpy_helper(address, &temp, 4, &offset);
								memcpy_helper(address, &info.evACK_.sourceId, 4, &offset);
								memcpy_helper(address, &info.evACK_.destinationId, 4, &offset);
                break;

					case STATEREQUEST:
								memcpy_helper(address, &info.SIReq_.sourceId, 4, &offset);
                break;

					case STATERESPONSE:
                memcpy_helper(address, &info.SIRes_.sourceId, 4, &offset); 
                memcpy_helper(address, &info.SIRes_.destinationId, 4, &offset); 
								memcpy_helper(address, &info.SIRes_.absoInfo.score, 4, &offset);
								memset(&temp, 0, 4);
								copybit(&temp, info.SIRes_.absoInfo.position.x, 27, 5);
                copybit(&temp, info.SIRes_.absoInfo.position.y, 23, 4);
								copybit(&temp, info.SIRes_.absoInfo.direction, 21, 2);
								copybit(&temp, info.SIRes_.absoInfo.cloak, 20, 1);
								copybit(&temp, info.SIRes_.absoInfo.missileNumber, 16, 4);
								memcpy_helper(address, &temp, 4, &offset);
                for(count=0; count<MAX_MISSILES; ++count){
                  memset(&temp, 0, 4);
                  copybit(&temp, info.SIRes_.absoInfo.missiles[count].exist, 14, 2);
                  copybit(&temp, info.SIRes_.absoInfo.missiles[count].position.x, 9, 5);
                  copybit(&temp, info.SIRes_.absoInfo.missiles[count].position.y, 5, 4);
                  copybit(&temp, info.SIRes_.absoInfo.missiles[count].direction, 3, 2);
                  uint16_t temp16 = temp;
                  memcpy_helper(address, &temp16, 2, &offset);
                } 
                memcpy_helper(address, &info.SIRes_.uncommitted_number, 4, &offset);
								//each uncommited event has 3 x 32bits, i.e. 12bytes
								for(int j=0; j<info.SIRes_.uncommitted_number; j++){
											//encoding data
											memset(&temp, 0, 4);
											copybit(&temp, info.SIRes_.uncommit[j].type, 28, 4);	
											copybit(&temp, info.SIRes_.uncommit[j].eventId, 0, 28);
											memset(&temp1, 0, 4);
											memset(&temp2, 0, 4);
							  			encodeEventData(&temp1, &temp2, info.SIRes_.uncommit[j].type, info.SIRes_.uncommit[j].eventData);	
											//copying data
											memcpy_helper(address, &temp, 4, &offset);
											memcpy_helper(address, &temp1, 4, &offset);
											memcpy_helper(address, &temp2, 4, &offset);
								}
                break;

					case STATEACK:
								memcpy_helper(address, &info.SIACK_.sourceId, 4, &offset);
								memcpy_helper(address, &info.SIACK_.destinationId, 4, &offset);
                break;

	}	
  
  if (sendto((int)M->theSocket(), &pack, sizeof(pack), 0, (struct sockaddr *)&groupAddr, sizeof(Sockaddr)) < 0)
            MWError((char *)"Sample error");
}

packetInfo packetParser(MW244BPacket* pack) {
  packetInfo info;
  uint32_t temp;
  //uint16_t temp16;
  switch (pack->type) {
          case HEARTBEAT:
              memcpy(&info.hb_.heartbeatId, pack->body, 4);
              memcpy(&info.hb_.sourceId, ((uint8_t*)pack->body)+4, 4);
              break;

          case HEARTBEATACK:
              memcpy(&info.hbACK_.heartbeatId, pack->body, 4);
              memcpy(&info.hbACK_.sourceId, ((uint8_t*) pack->body)+4, 4);
              memcpy(&info.hbACK_.destinationId, ((uint8_t*) pack->body)+8, 4);
              break;

          case EVENT:
              memset(&temp, 0, 4);
              memcpy(&temp, pack->body, 4);
              info.ev_.type = parsebit(temp, 28, 4).bit_8;
              info.ev_.eventId = parsebit(temp, 0, 28).bit_32;
              memcpy(&info.ev_.sourceId, ((uint8_t*) pack->body)+4, 4);
              info.ev_.absoInfo = parseAbsoluteInfo(((uint8_t*) pack->body) +8);
              info.ev_.eventData = parseEventData(((uint8_t*) pack->body)+16+MAX_MISSILES*2, info.ev_.type);
              break;

          case EVENTACK:
              memset(&temp, 0, 4);
              memcpy(&temp, pack->body, 4);
              info.evACK_.eventId = parsebit(temp, 4, 28).bit_32;
              memcpy(&info.evACK_.sourceId, ((uint8_t*) pack->body)+4, 4);
              memcpy(&info.evACK_.destinationId, ((uint8_t*) pack->body)+8, 4);
              break;
     
          case STATEREQUEST:
              memcpy(&info.SIReq_.sourceId, pack->body, 4);
              break;         
           
          case STATERESPONSE:
              memcpy(&info.SIRes_.sourceId, pack->body, 4);
              memcpy(&info.SIRes_.destinationId, ((uint8_t*) pack->body)+4, 4);
              info.SIRes_.absoInfo = parseAbsoluteInfo(((uint8_t*) pack->body)+8);
              memcpy(&info.SIRes_.uncommitted_number, ((uint8_t*) pack->body)+16+MAX_MISSILES*2, 4);
              for(int j=0; j<info.SIRes_.uncommitted_number; ++j){
                info.SIRes_.uncommit[j] = parseUncommit(((uint8_t*) pack->body)+20+MAX_MISSILES*2);
              }
              break;
           
          case STATEACK:        
              memcpy(&info.SIACK_.sourceId, pack->body, 4);
              memcpy(&info.SIACK_.destinationId, ((uint8_t*) pack->body)+4, 4); 
              break;
            
  }
  return info;
}



/*
  This function handles received packets as bellow:
    Heartbeat     - Directly send back HeartbeatACK
    HeartbeatACK  - Update the static set hbACK_set
    Event         - Save it into unprocessed queue; if it doesn't exist already, send back EventACK
    EventACK      - Update the local_event_resend
    STATERequest  - Send STATEPesponse, and save it into resend queue
    STATEResponse - This phase shouldn't receive it, drop
    STATEACK      - Delete STATEResponse in resend queue
*/
void join_handler(unsigned char type, packetInfo info){
  packetInfo respond;

  switch(type) {
    case HEARTBEAT: {
      if(info.hb_.sourceId == M->myRatId().value()) break;
      respond.hbACK_.heartbeatId   = info.hb_.heartbeatId;
      int temp_val = M->myRatId().value();
      respond.hbACK_.sourceId      = temp_val;
      respond.hbACK_.destinationId = info.hb_.sourceId;
      sendPacketToPlayer(HEARTBEATACK, respond);
    }break;

    case STATERESPONSE:{
      /*Two things here:
          1. Save all uncommitted events
          2. Update all received absolute information in mazeRats_
      */

      if(info.SIRes_.sourceId == M->myRatId().value()) break;
      //If the SI response from this source has been received, break; else, save it
      if(hbACK_set.find(info.SIRes_.sourceId)!=hbACK_set.end()) break;
      else hbACK_set.insert({info.SIRes_.sourceId, 0});

      //Generate the uncommitted event for next timeslot
      respond.ev_.absoInfo = info.SIRes_.absoInfo;
      respond.ev_.sourceId = info.SIRes_.sourceId;

      player_unprocessed temp_events;
      for(int i=0; i<info.SIRes_.uncommitted_number; ++i){
        respond.ev_.type = info.SIRes_.uncommit[i].type;  
        respond.ev_.eventId = info.SIRes_.uncommit[i].eventId;
        respond.ev_.eventData = info.SIRes_.uncommit[i].eventData;
        temp_events.insert({respond.ev_.eventId, respond});
      }
      if(info.SIRes_.uncommitted_number !=0){
        std::map<uint32_t, int>::iterator temp_index = M->AllRats.find(respond.ev_.sourceId);
        if(temp_index != M->AllRats.end())
        events_all_player[temp_index->second] = std::map<uint32_t, packetInfo>();
      } 

      int index = 1;
      //Update information in mazeRats_, 0 is reserved for own
      for(; index < MAX_RATS; ++index){
          if (M->AllRats.find(info.SIRes_.sourceId)==M->AllRats.end() && M->mazeplay[index]==false){
            M->AllRats.insert({info.SIRes_.sourceId, index});
            M->mazeplay[index] = TRUE;
            break;
          }
      }
      if(index >= MAX_RATS){
        printf("Reach max rats limit, can not let new rat join!\n");
        break;
      } 
      Rat temp_rat;
      //TODO: add name
      //temp_rat.Name = "Tom";
      temp_rat.playing = TRUE;
      temp_rat.cloaked = info.SIRes_.absoInfo.cloak;
      temp_rat.x = info.SIRes_.absoInfo.position.x;
      temp_rat.y = info.SIRes_.absoInfo.position.y;
      temp_rat.dir = info.SIRes_.absoInfo.direction;
      temp_rat.score = info.SIRes_.absoInfo.score;
      for(int i=0; i<MAX_MISSILES; ++i){
        if(info.SIRes_.absoInfo.missiles[i].exist != 0){
          temp_rat.RatMissile[i].exist = TRUE;
          temp_rat.RatMissile[i].x = info.SIRes_.absoInfo.missiles[i].position.x;
          temp_rat.RatMissile[i].y = info.SIRes_.absoInfo.missiles[i].position.y;
          temp_rat.RatMissile[i].dir = info.SIRes_.absoInfo.missiles[i].direction;
        }
      }
      M->ratIs(temp_rat, index);
      M->occupy_[info.SIRes_.absoInfo.position.x][info.SIRes_.absoInfo.position.y] = TRUE;
    }break;
  }
}

void packet_handler(unsigned char type, packetInfo info) {
  packetInfo respond;
  event_unprocessed_iter iter;
  int temp_val;
  switch(type) {
    case HEARTBEAT: { 
      if(info.hb_.sourceId == M->myRatId().value()) break;
      respond.hbACK_.heartbeatId   = info.hb_.heartbeatId;
      temp_val = M->myRatId().value();
      respond.hbACK_.sourceId      = temp_val;
      respond.hbACK_.destinationId = info.hb_.sourceId;
      sendPacketToPlayer(HEARTBEATACK, respond);
    }break;

    case HEARTBEATACK: {
      //update the static set for heartbeat respond
      if(info.hbACK_.sourceId == M->myRatId().value()) break;
      //update the heartbeat response
      respond_set_iter iter = hbACK_set.find(info.hbACK_.sourceId);
      if(iter != hbACK_set.end()) iter->second = 0;
    }break;

    case EVENT: { 
      iter = events_all_player.find(info.ev_.sourceId);
      if(iter == events_all_player.end())  break; //exit if the player doesn't exist
      player_unprocessed second = iter->second;
      player_unprocessed_iter player_iter = iter->second.find(info.ev_.eventId);
      if(player_iter != iter->second.end()) break; //exit if the event already exist
      second.insert({info.ev_.eventId, info});
      events_all_player[info.ev_.sourceId][info.ev_.eventId] = info;

      if(info.ev_.sourceId == M->myRatId().value()) break;
      respond.evACK_.eventId       = info.ev_.eventId;
      temp_val = M->myRatId().value();
      respond.hbACK_.sourceId      = temp_val;
      respond.evACK_.destinationId = info.ev_.sourceId;
      sendPacketToPlayer(EVENTACK, respond);
    }break;

    case EVENTACK: { 
      if(info.evACK_.sourceId == M->myRatId().value()) break;
      if(info.evACK_.destinationId != M->myRatId().value())  break;
      std::map<uint32_t, respond_set>::iterator iter_ = local_event_resend.find(info.evACK_.eventId);
      if(iter_ == local_event_resend.end())  break; //event has already been committed
      iter_->second.erase(info.evACK_.sourceId);
    }break;

    case STATEREQUEST: { 
      //Check whether any slot available for new player
      int index = 1;
      for(; index < MAX_RATS; ++index){
          if (M->AllRats.find(info.SIReq_.sourceId)==M->AllRats.end() && M->mazeplay[index]==false){
            M->AllRats.insert({info.SIReq_.sourceId, index});
            M->mazeplay[index] = TRUE;
            break;
          }
      }
      if(index >= MAX_RATS){
        printf("Reach max rats limit, can not let new rat join!\n");
      }
      else{
        Rat temp_rat;
        //TODO: add name
        //temp_rat.Name = "Tom";
        temp_rat.playing = TRUE;
        M->ratIs(temp_rat, index);
        printf("Welcome new player!\n");
        NotifyPlayer();
        events_all_player[info.SIReq_.sourceId] = std::map<uint32_t, packetInfo>();
      }

      //Generate packet to responed
      if(info.SIReq_.sourceId == M->myRatId().value()) break;
      temp_val = M->myRatId().value();
      respond.hbACK_.sourceId      = temp_val;
      respond.SIRes_.destinationId = info.SIReq_.sourceId;
      respond.SIRes_.absoInfo.score = M->score().value();
      respond.SIRes_.absoInfo.position.x = M->xloc().value();
      respond.SIRes_.absoInfo.position.y = M->yloc().value();
      respond.SIRes_.absoInfo.direction = M->dir().value();
      respond.SIRes_.absoInfo.missileNumber = 0;
      Rat temp_rat = M->rat(0);
      Missile* temp_missile = temp_rat.RatMissile;
      //Missile temp_missile[MAX_MISSILES] = temp_rat.RatMissile;
      for(int i=0; i<MAX_MISSILES; ++i){
        respond.SIRes_.absoInfo.missiles[i].exist = temp_missile->exist;
        respond.SIRes_.absoInfo.missiles[i].position.x = temp_missile->x.value();
        respond.SIRes_.absoInfo.missiles[i].position.y = temp_missile->y.value();
        respond.SIRes_.absoInfo.missiles[i].direction = temp_missile->dir.value();
        temp_missile++;
      }
      iter = events_all_player.find(M->myRatId().value());
      respond.SIRes_.uncommitted_number = 0;
      //respond.SIRes_.uncommitted_number = iter->second.size();
      //if(iter->second.size() != 0){
      //int i = 0;
      //for(player_unprocessed_iter iter_events = iter->second.begin(); iter_events != iter->second.end(); ++iter_events){ //size of all local events
      //  respond.SIRes_.uncommit[i].type       =  iter_events->second.ev_.type;
      //  respond.SIRes_.uncommit[i].eventId    =  iter_events->second.ev_.eventId;
      //  respond.SIRes_.uncommit[i].eventData  =  iter_events->second.ev_.eventData;
      //  ++i;
      //}
      
      //else{
     //   respond.SIRes_.uncommitted_number = 0;
      //}
      sendPacketToPlayer(STATERESPONSE, respond);
    }break;

    case STATERESPONSE:
    break;

  }


}
/* ----------------------------------------------------------------------- */

/* Sample of processPacket. */

void processPacket(MWEvent *eventPacket, int phase) {

  packetInfo info = packetParser(eventPacket->eventDetail);
  switch(phase){
    case JOINPHASE:
      join_handler(eventPacket->eventDetail->type, info);
      break;
    case PLAYPHASE:
      packet_handler(eventPacket->eventDetail->type, info);
      break;
  }
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

  printf("netinit finished!\n");

  /* set up some stuff strictly for this local sample */
  uint32_t temp = random();
  M->myRatIdIs(temp);
  M->scoreIs(0);
  SetMyRatIndexType(0);
	M->AllRats.insert({temp, M->myIndex().value()});

  /* Get the multi-cast address ready to use in SendData()
     calls. */
  memcpy(&groupAddr, &nullAddr, sizeof(Sockaddr));
  groupAddr.sin_addr.s_addr = htonl(MAZEGROUP);
}

/* ----------------------------------------------------------------------- */
