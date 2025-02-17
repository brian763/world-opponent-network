#include "RoutingServer.h"
#include "connectedclientclass.h"
#include "SrvrNtwk.h"
#include "PManip.h"



   //hackie static conflict.  Statics are always zerod out, but jsut to make sure...
   static unsigned char StaticConflict[0xa0] = "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";

/*** Constructor/Destructor***/

RoutingServer::RoutingServer() {

   pthread_mutex_init(&Conflict_lock, NULL);
   BeginningOfConflictlist = NULL;
}
RoutingServer::~RoutingServer() {

   DeallocateConflicts();
   pthread_mutex_destroy(&Conflict_lock);
}

/* C/D functions */

/* DESCRIPTION: DeallocateServers
//
// Wipes every node.  Nothing fancy; intended to be used on termination, but
// could also server as a restart.
*/
void RoutingServer::DeallocateConflicts() {

   SilencerConflict_t * ptr;

   pthread_mutex_lock(&Conflict_lock);
   while(BeginningOfConflictlist != NULL) {
      ptr = BeginningOfConflictlist;
      BeginningOfConflictlist = BeginningOfConflictlist->next;
      free(ptr);
   }
   pthread_mutex_unlock(&Conflict_lock);
}



/* Basic malloc and free */

/* DESCRIPTION: NewNode
//
// Mallocs a node.  Since there's no node pool stuff, it's just an inline
// wrapper for malloc.
*/
inline SilencerConflict_t * RoutingServer::NewNode() {

   return((SilencerConflict_s*)malloc(sizeof(struct SilencerConflict_s)));
}
/* DESCRIPTION: FreeUnlinkedNode
//
// Much like NewNode, this becomes no more than an inline wrapper.
//
*/
void RoutingServer::FreeUnlinkedConflict(SilencerConflict_t * deadnode) {

   #if(PARANOID)

   if(deadnode == NULL) {
      printf(SIL_PASSEDNULL);
      return;
   }

   #endif


   free(deadnode);
}



/* Linked List Adds and Deletes */

/* DESCRIPTION: AddNode
//
// Takes an allocated server pointer and links it to our list.  Since it is
// unlikely that there will ever be more than a handful of servers, there is
// no reason to sort it in any way.  It is therefore a boring linked list.
//
// Only called in code segments already locked.
*/
void RoutingServer::AddConflict(SilencerConflict_t * ConflictToBeAdded) {


   #if(PARANOID)

   if(ConflictToBeAdded == NULL) {
      printf(SIL_PASSEDNULL);
      return;
   }

   #endif

   SilencerConflict_t * ptr = BeginningOfConflictlist;

   ptr = BeginningOfConflictlist;
   BeginningOfConflictlist = ConflictToBeAdded;
   BeginningOfConflictlist->next = ptr;
}
/* DESCRIPTION: DeleteNode
//
// Unlinks one node and frees it.
// Only called in code segments already locked.
*/
void RoutingServer::DeleteConflict(SilencerConflict_t * ConflictToBeDeleted) {


   #if(PARANOID)

   if(ConflictToBeDeleted == NULL) {
      printf(SIL_PASSEDNULL);
      return;
   }

   #endif


   //Special case of deleting the beginning node:
   if(BeginningOfConflictlist = ConflictToBeDeleted) {
      BeginningOfConflictlist = ConflictToBeDeleted->next;
   }
   else {

      SilencerConflict_t * ptr = BeginningOfConflictlist;

      //Someone somewhere points to the server that is to be deleted.
      //We need to find out who so we can bridge the hole we're about to make.

      while(ptr->next != ConflictToBeDeleted) { //ptr should never equal null if we have a server to delete, doy
         ptr = ptr->next;
      }

      ptr->next = ConflictToBeDeleted->next;
   }
   FreeUnlinkedConflict(ConflictToBeDeleted);
}


/* DESCRIPTION: ProcessNewClient
//
// First, a confession.  I have NO plans to actually create a fully featured
// routing server.  BUT, if one WERE to be created, I figure we'd need to
// keep track of every single connected client's ID.  That's where this comes
// in.  In an ideal program this will set up everything needed to chat and
// whatnot, returning the new user's ID to the calling thread (or a negative
// if there was a problem).  As is we just make a random ID and print stuff.
*/
int RoutingServer::ProcessNewClient(const unsigned char * packet, unsigned int length) {

   unsigned char Name[41];
   unsigned char Password[41];
   int ret;

   ret = ExtractString(&packet, length, Name, sizeof(Name)-1);
   if(ret < 1) { return(-1); }
   Name[ret] = '\0';

   ret = ExtractUnicodeString(&packet, length, Password, sizeof(Password)-1);
   if(ret < 1) { return(-1); }
   Password[ret] = '\0';

   printf("User %s -- %s connected.\n", Name, Password);

   return(0);
}

/* DESCRIPTION: ProcessNewConflict
//
// Conflicts are sent by the player who is creating them.  The structure
// the client sends is always 136 bytes, and we send the same struct to other
// clients.  For all intents, it's a black box; there's a nice debug printf
// in the Silencer client somewhere detailing what all the various parts
// are, but while it was useful before, it's not that vital now.
//
// This ended up being more complicated than I would have hoped.  There are
// more packets understood by Silencer than I know of, and for a reason I do
// not understand, the thing just stopped working after I cleaned up the
// previous hackneyed code.  Eventually I figured out that the client wants
// to see its own conflict message sent back by the server, which was happening
// in the hacky varsion--by accident.  But I have yet to figure out how
// conflicts are IDd and removed when the client quits.  Due to this, the list
// of conflicts will now be replaced with a single conflict that will be
// overwritten as needed.
*/
void RoutingServer::ProcessNewConflict(const unsigned char * packet, unsigned int length) {

   if(length != 0x92) { return; }

   memcpy(StaticConflict, "\x97\x00\x05\x02\x09", 5);
   memcpy(StaticConflict+5, packet, 0x92);
}

int RoutingServer::GetConflictPacket(unsigned char **packetptr) {


   *packetptr = StaticConflict;
   return(0x97);
}

#if 0




/* Functions related to the linked list */

/* DESCRIPTION: FindNodeBySockaddr
//
// Calls IP version of function.
//
*/
SilencerGameServer_t * RoutingServer::FindNodeBySockaddr(struct sockaddr_in server) {

   return(FindNodeByIP(server.sin_addr.s_addr, server.sin_port));
}
/* DESCRIPTION: FindNodeByIP
//
// Compares IPs until it finds a match.  It's unlikely it will have to look far.
// Returns NULL if not found.
//
*/
SilencerGameServer_t * RoutingServer::FindNodeByIP(unsigned long int IP, unsigned short int port) {

   SilencerGameServer_t * ptr = BeginningOfServerlist;

   while(BeginningOfServerlist != NULL) {

      if(ptr->IntServerAddress == IP && ptr->IntServerPort == port) { return(ptr); }
      ptr = ptr->next;
   }
   return(NULL);
}



/* DESCRIPTION: ProcessHeartbeat
//
// Once the listening thread has determined it has a heartbeat (it's
// basically processed the header), it forwards the rest of the packet
// here for processing.  We extract the relevant info, make a server struct,
// and tack it on to the server linked list.  Unless it's a broken heartbeat.
// Then we just chuck it.
*/
void RoutingServer::ProcessHeartbeat(const unsigned char * packet, unsigned int len) {

   unsigned char strings[64];
   int LengthOfString;

   UINT32 IPaddress;
   UINT32 IPport;

   SilencerGameServer_t * ptr;

   //bypass the header.
   packet += (sizeof(HEARTBEAT_HEADER)-1);
   len -= (sizeof(HEARTBEAT_HEADER)-1);


   //After the header the thing should identify itself as a game server.

   if(ExtractUnicodeString(&packet, len, strings, sizeof(strings)) == sizeof("/Silencer/GameServers")-1 &&
      memcmp(strings, "/Silencer/GameServers", sizeof("/Silencer/GameServers")-1) == 0) {

      len -= (((sizeof("/Silencer/GameServers")-1) << 1) + 2);

      if(ExtractUnicodeString(&packet, len, strings, sizeof(strings)) == sizeof("GameServer")-1 &&
         memcmp(strings, "GameServer", sizeof("GameServer")-1) == 0) {

         len -= (((sizeof("GameServer")-1) << 1) + 2);

         //Time to extract the port and IP.
         if(len > 7 && *packet == 6) {
            packet++;
            ExtractWord (&packet, &IPport);
            ExtractDWord(&packet, &IPaddress);

            len -= 7;

            //Now we get into actual usable stuff, like the server name.
            LengthOfString = ExtractUnicodeString(&packet, len, strings, sizeof(strings)-2);

            if(LengthOfString > 0 && len >= (sizeof(HEARTBEAT_FOOTER)+1) &&
               memcmp(HEARTBEAT_FOOTER, packet, sizeof(HEARTBEAT_FOOTER)-1) == 0) {

               strings[LengthOfString] = '\0';
               len -= ((LengthOfString << 1) + (sizeof(HEARTBEAT_FOOTER)+1));
               packet += (sizeof(HEARTBEAT_FOOTER)-1);

               //The math is correct, trust it.
               printf("Heartbeat: Server %u.%u.%u.%u:%u named %s has %u of %u players.\n", (*(char *)(&IPaddress)), (*(((char *)(&IPaddress))+1)), (*(((char *)(&IPaddress))+2)), (*(((char *)(&IPaddress))+3)), IPport, strings, packet[0], packet[1]);

               pthread_mutex_lock(&Serverlist_lock);

               ptr = FindNodeByIP(IPaddress, IPport);
               if(ptr == NULL) { //not found, make our own

                  ptr = NewNode();
                  if(ptr == NULL) {
                     printf(SIL_NOMEMERROR);
                  }
                  else {
                     ptr->IntServerAddress = IPaddress;
                     ptr->IntServerPort = IPport;

                     ptr->Players = packet[0]; //The last two bytes are player and max, respectively
                     ptr->PlayerMax = packet[1];

                     ptr->LengthOfServerName = LengthOfString;
                     memcpy(ptr->ServerName, strings, LengthOfString);
                     ptr->last_heartbeat = (unsigned int)time(NULL);
                     AddNode(ptr);
                  }
               }
               else {
                  //I think this is the only bit that can change.
                  ptr->Players = packet[0];
                  ptr->last_heartbeat = (unsigned int)time(NULL);

                  // Just to mix things up (and keep things fresh if there are
                  // many servers), move it to the front of the linked list.
                  // unimplemented right now, possible tweak later.
               }
               pthread_mutex_unlock(&Serverlist_lock);
               return;
            }
         }
      }
   }

   printf(SIL_NOTSILENCER);
   return;
}

/* DESCRIPTION: FillPacketBufferWithIPs
//
// It's assumed that the buffer is an outgoing packet.  This cycles through the
// serverlist and adds the whole IP PORT combo.  But there's an interesting
// question left:  We're using TCP/IP.  There are no hard limits, only how
// much we're willing to send and how much Silencer is willing to take.
// Silencer however treats it like UDP, with a hard limit of 7D00h.  Well
// TCP can't be relied on to not split our answer into multiple parts, so I
// propose keeping it below 500 or so bytes  This is about 10 servers.
//
*/
#define GAMESERVER_PACKET_ENTITY1_HEADER "\x53\x0a\x00G a m e S e r v e r \x06"
#define GAMESERVER_PACKET_ENTITY2_HEADER "\x01\x00\x00\x00\x00\x00"
unsigned int RoutingServer::FillPacketBufferWithGameServers(unsigned char * packet, unsigned int SizeOfBuffer) {

   unsigned char * ptr;
   unsigned int packetlen;
   SilencerGameServer_t * serverptr;
   unsigned int servers;

   unsigned int currentTime;

   //We'll be messing with the serverlist continuously.  We might as well hold
   //the lock and keep it now and forevermore.

   pthread_mutex_lock(&Serverlist_lock);

   servers = 0;
   packetlen = 2;
   ptr = packet+2;
   serverptr = BeginningOfServerlist;
   currentTime = time(NULL);

   while(serverptr != NULL) {

      //Firrt things first, if a server's getting old, don't mess with it.
      if(serverptr->last_heartbeat + ServerTimeoutValueInSeconds < currentTime) {

         DeleteNode(serverptr);
         continue;
      }


      //The overhead for the server is 39 + 2*the length of its name
      if(packetlen + (39 + (serverptr->LengthOfServerName << 1)) >= SizeOfBuffer) { break; }

      //There's enough room if we're here, so start writing.
      WriteAndPassStringA(&ptr, GAMESERVER_PACKET_ENTITY1_HEADER, sizeof(GAMESERVER_PACKET_ENTITY1_HEADER)-1);

      //Port and IP
      WriteAndPassWord(&ptr, &(serverptr->IntServerPort));
      WriteAndPassDWord(&ptr, &(serverptr->IntServerAddress));

      //Write the server's name and size.
      WriteAndPassWord(&ptr, &(serverptr->LengthOfServerName));
      WriteAndPassStringW(&ptr, serverptr->ServerName, serverptr->LengthOfServerName);

      //There's some more weird junk here
      WriteAndPassStringA(&ptr, GAMESERVER_PACKET_ENTITY2_HEADER, sizeof(GAMESERVER_PACKET_ENTITY2_HEADER)-1);

      //Player totals
      WriteAndPassByte(&ptr, &(serverptr->PlayerMax));
      WriteAndPassByte(&ptr, &(serverptr->Players));

      servers++;
      packetlen += (39 + (serverptr->LengthOfServerName << 1));
      serverptr = serverptr->next;
   }

   // We can easily check and see if we're aborting early by checking serverptr
   // for a null, but there's not really anything we can do about it.
   pthread_mutex_unlock(&Serverlist_lock);

   //We skipped the first two bytes because we didn't know this yet.
   WriteWord(packet, servers);

   return(packetlen);
}

#endif
