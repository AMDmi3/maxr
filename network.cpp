/***************************************************************************
 *      Mechanized Assault and Exploration Reloaded Projectfile            *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "network.h"
#include "events.h"
#include "server.h"
#include "netmessage.h"

sSocket::sSocket()
{
	iType = FREE_SOCKET;
	iState = STATE_UNUSED;
	messagelength = 0;
	buffer.clear();
}

sSocket::~sSocket()
{
	if ( iType != FREE_SOCKET ) SDLNet_TCP_Close ( socket );
}

void sDataBuffer::clear()
{
	iLenght = 0;
}

char* sDataBuffer::getWritePointer()
{
	return data + iLenght;
}

int sDataBuffer::getFreeSpace()
{
	return PACKAGE_LENGTH - iLenght;
}

void sDataBuffer::deleteFront(int n)
{
	memmove(data, data + n, iLenght - n);
	iLenght -= n;
}

cTCP::cTCP():
	TCPMutex()
{


	SocketSet = SDLNet_AllocSocketSet( MAX_CLIENTS );

	iLast_Socket = 0;
	sIP = "";
	iPort = 0;
	bHost = false;

	bExit = false;
	TCPHandleThread = SDL_CreateThread( CallbackHandleNetworkThread, this );
}

cTCP::~cTCP()
{
	bExit = true;
	SDL_WaitThread ( TCPHandleThread, NULL );
}


int cTCP::create()
{
	cMutex::Lock tl(TCPMutex);
	if( SDLNet_ResolveHost( &ipaddr, NULL, iPort ) == -1 ) { return -1; }

	int iNum;
	if ( ( iNum = getFreeSocket() ) == -1 ) { return -1; }

	Sockets[iNum].socket = SDLNet_TCP_Open ( &ipaddr );
	if ( !Sockets[iNum].socket )
	{
		deleteSocket ( iNum );
		return -1;
	}

	Sockets[iNum].iType = SERVER_SOCKET;
	Sockets[iNum].iState = STATE_NEW;

	bHost = true; // is the host

	return 0;
}

int cTCP::connect()
{
	cMutex::Lock tl( TCPMutex );
	if( SDLNet_ResolveHost( &ipaddr, sIP.c_str(), iPort ) == -1 ) { return -1; }

	int iNum;
	if ( ( iNum = getFreeSocket() ) == -1 ) { return -1; }

	Sockets[iNum].socket = SDLNet_TCP_Open ( &ipaddr );
	if ( !Sockets[iNum].socket )
	{
		deleteSocket ( iNum );
		return -1;
	}

	Sockets[iNum].iType = CLIENT_SOCKET;
	Sockets[iNum].iState = STATE_NEW;

	bHost = false;	// is not the host
	return 0;
}

int cTCP::sendTo( int iClientNumber, int iLenght, char *buffer )
{
	cMutex::Lock tl(TCPMutex);
	if ( iClientNumber >= 0 && iClientNumber < iLast_Socket && Sockets[iClientNumber].iType == CLIENT_SOCKET && ( Sockets[iClientNumber].iState == STATE_READY || Sockets[iClientNumber].iState == STATE_NEW ) )
	{
		// if the message is to long, cut it.
		// this will result in an error in nearly all cases
		if ( iLenght > PACKAGE_LENGTH )
		{
			Log.write( "Cut size of message!", LOG_TYPE_NET_ERROR );
			iLenght = PACKAGE_LENGTH;
		}

		if ( iLenght > 0 )
		{
			// send the message
			int iSendLenght = SDLNet_TCP_Send ( Sockets[iClientNumber].socket, buffer, iLenght );

			// delete socket when sending fails
			if ( iSendLenght != iLenght )
			{
				Sockets[iClientNumber].iState = STATE_DYING;
				cNetMessage *message = new cNetMessage( TCP_CLOSE );
				message->pushInt16(iClientNumber);
				pushEvent ( message );
				return -1;
			}
		}
	}
	return 0;
}

int cTCP::send( int iLenght, char *buffer )
{
	cMutex::Lock tl(TCPMutex);
	int iReturnVal = 0;
	for ( int i = 0; i < getSocketCount(); i++ )
	{
		if ( sendTo ( i, iLenght, buffer ) == -1 )
		{
			iReturnVal = -1;
		}
	}
	return iReturnVal;
}

int CallbackHandleNetworkThread( void *arg )
{
	cTCP *TCP = (cTCP *) arg;
	TCP->HandleNetworkThread();
	return 0;
}

void cTCP::HandleNetworkThread()
{
	while( !bExit )
	{
		SDLNet_CheckSockets ( SocketSet, 10 );


		// Check all Sockets
		for ( int i = 0; !bExit && i < iLast_Socket; i++ )
		{
			// there has to be added a new socket
			if ( Sockets[i].iState == STATE_NEW )
			{
				cMutex::Lock tl ( TCPMutex );
				if ( SDLNet_TCP_AddSocket ( SocketSet, Sockets[i].socket ) != -1 )
				{
					Sockets[i].iState = STATE_READY;
				}
				else
				{
					Sockets[i].iState = STATE_DELETE;
				}
			}
			// there has to be deleted a socket
			else if ( Sockets[i].iState == STATE_DELETE )
			{
				cMutex::Lock tl ( TCPMutex );
				SDLNet_TCP_DelSocket ( SocketSet, Sockets[i].socket );
				deleteSocket ( i );
				i--;
				continue;
			}
			// there is a new connection
			else if ( Sockets[i].iType == SERVER_SOCKET && SDLNet_SocketReady ( Sockets[i].socket ) )
			{
				cMutex::Lock tl ( TCPMutex );
				TCPsocket socket = SDLNet_TCP_Accept ( Sockets[i].socket );

				if ( socket != NULL )
				{
					Log.write("Incoming connection!", cLog::eLOG_TYPE_NET_DEBUG);
					int iNum;
					if ( ( iNum = getFreeSocket() ) != -1 )
					{
						Log.write("Connection accepted and assigned socket number " + iToStr(iNum), cLog::eLOG_TYPE_NET_DEBUG);
						Sockets[iNum].socket = socket;

						Sockets[iNum].iType = CLIENT_SOCKET;
						Sockets[iNum].iState = STATE_NEW;
						Sockets[iNum].buffer.clear();
						cNetMessage *message = new cNetMessage( TCP_ACCEPT );
						message->pushInt16(iNum);
						pushEvent( message );
					}
					else SDLNet_TCP_Close ( socket );
				}
				
			}
			// there has to be received new data
			else if ( Sockets[i].iType == CLIENT_SOCKET && Sockets[i].iState == STATE_READY && SDLNet_SocketReady ( Sockets[i].socket ) )
			{
				sSocket& s = Sockets[i];
				{
					cMutex::Lock tl(TCPMutex);

					//read available data from the socket to the buffer
					int recvlength;
					recvlength = SDLNet_TCP_Recv ( s.socket, s.buffer.getWritePointer(), s.buffer.getFreeSpace());
					if ( recvlength < 0 )	//TODO: gleich 0???
					{
						cNetMessage* message = new cNetMessage( TCP_CLOSE );
						message->pushInt16( i );
						pushEvent ( message );

						Sockets[i].iState = STATE_DYING;
						continue;
					}

					s.buffer.iLenght += recvlength;
				}

				//push all received messages
				bool messagePushed = false;
				int readPos = 0;
				do
				{
					//get length of next message
					if ( s.buffer.iLenght - readPos >= 3 && s.messagelength == 0 )
					{
						if ( s.buffer.data[readPos] != START_CHAR )
						{
							//something went terribly wrong. We are unable to continue the communication.
							Log.write ( "Wrong start character in received message. Socket closed!", LOG_TYPE_NET_ERROR );
							cNetMessage* message = new cNetMessage( TCP_CLOSE );
							message->pushInt16( i );
							pushEvent ( message );

							Sockets[i].iState = STATE_DYING;
							continue;
						}

						s.messagelength = SDL_SwapLE16( *(Sint16*)(s.buffer.data+readPos+1) );
						if ( s.messagelength > PACKAGE_LENGTH ) 
						{
							Log.write ( "Length of received message exceeds PACKAGE_LENGTH", LOG_TYPE_NET_ERROR );
							cNetMessage* message = new cNetMessage( TCP_CLOSE );
							message->pushInt16( i );
							pushEvent ( message );

							Sockets[i].iState = STATE_DYING;
							continue;
						}

					}

					//check if there is a complete message in buffer
					messagePushed = false;
					if ( s.messagelength != 0 && s.buffer.iLenght - readPos >= s.messagelength )
					{
						//push message
						cNetMessage* message = new cNetMessage( s.buffer.data + readPos );
						pushEvent( message );
						messagePushed = true;
						
						//save position of next message
						readPos += s.messagelength;

						s.messagelength = 0;
					}
				} while ( messagePushed );

				s.buffer.deleteFront(readPos);
			}
		}
	}
}

int cTCP::pushEvent( cNetMessage* message )
{
	if ( Server && Server->bStarted && (message->getClass() == NET_MSG_STATUS || message->getClass() == NET_MSG_SERVER) )
	{
		Server->pushEvent ( message );
	}
	else
	{
		EventHandler->pushEvent ( message );
	}
	return 0;
}

void cTCP::close( int iClientNumber )
{
	cMutex::Lock tl(TCPMutex);
	if ( iClientNumber >= 0 && iClientNumber < iLast_Socket && ( Sockets[iClientNumber].iType == CLIENT_SOCKET || Sockets[iClientNumber].iType == SERVER_SOCKET ) )
	{
		Sockets[iClientNumber].iState = STATE_DELETE;
	}
}

void cTCP::deleteSocket( int iNum )
{
	Sockets[iNum].~sSocket();
	for ( int i = iNum; i < iLast_Socket-1; i++ )
	{
		Sockets[i] = Sockets[i+1];
		memcpy ( Sockets[i].buffer.data, Sockets[i+1].buffer.data, Sockets[i].buffer.iLenght );
	}
	Sockets[iLast_Socket-1].iType = FREE_SOCKET;
	Sockets[iLast_Socket-1].iState = STATE_UNUSED;
	Sockets[iLast_Socket-1].messagelength = 0;
	Sockets[iLast_Socket-1].buffer.clear();
	iLast_Socket--;
}

void cTCP::setPort( int iPort )
{
	this->iPort = iPort;
}

void cTCP::setIP ( string sIP )
{
	this->sIP = sIP;
}

int cTCP::getSocketCount()
{
	return iLast_Socket;
}

int cTCP::getConnectionStatus()
{
	if ( iLast_Socket  > 0 ) return 1;
	return 0;
}
bool cTCP::isHost()
{
	return bHost;
}

int cTCP::getFreeSocket()
{
	if ( iLast_Socket == MAX_CLIENTS ) return -1;
	int iNum;
	for( iNum = 0; iNum < iLast_Socket; iNum++ )
	{
		if ( Sockets[iNum].iType == FREE_SOCKET ) break;
	}
	if ( iNum == iLast_Socket )
	{
		Sockets[iNum].iType = FREE_SOCKET;
		iLast_Socket++;
	}
	else if ( iNum > iLast_Socket ) return -1;

	return iNum;
}
