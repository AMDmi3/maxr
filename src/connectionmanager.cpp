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

#include "connectionmanager.h"
#include "game/data/player/playerbasicdata.h"
#include "utility/log.h"
#include "netmessage2.h"
#include "maxrversion.h"
#include "utility/string/toString.h"

cConnectionManager::cConnectionManager() :
	network(nullptr),
	localClient(nullptr),
	localServer(nullptr),
	localPlayer(-1),
	serverSocket(nullptr),
	serverOpen(false),
	connecting(false),
	connectingPlayerReady(false)
{}

//------------------------------------------------------------------------------
cConnectionManager::~cConnectionManager()
{
	if (network)
		delete network;
}

//------------------------------------------------------------------------------
int cConnectionManager::openServer(int port)
{
	assert(localServer != nullptr);

	cLockGuard<cMutex> tl(mutex);

	if (serverOpen) return -1;

	if (!network)
		network = new cNetwork(*this, mutex);

	int result = network->openServer(port);
	if (result == 0)
		serverOpen = true;

	return result;
}

//------------------------------------------------------------------------------
void cConnectionManager::closeServer()
{
	cLockGuard<cMutex> tl(mutex);

	if (!network || !serverOpen) return;

	network->closeServer();
	serverOpen = false;
}

//------------------------------------------------------------------------------
bool cConnectionManager::isServerOpen() const
{
	return serverOpen;
}

//------------------------------------------------------------------------------
void cConnectionManager::acceptConnection(cSocket* socket, int playerNr)
{
	assert(localServer != nullptr);

	cLockGuard<cMutex> tl(mutex);
	
	auto x = std::find_if(clientSockets.begin(), clientSockets.end(), [&](const std::pair<cSocket*, int>& x) { return x.first == socket; });
	if (x == clientSockets.end())
	{
		//looks like the connection was disconnected during the handshake
		Log.write("ConnectionManager: accept called for unknown socket", cLog::eLOG_TYPE_WARNING);
		localServer->pushMessage(std::make_unique<cNetMessageTcpClose>(playerNr));
	}

	Log.write("ConnectionManager: Accepted connection and assigned playerNr: " + toString(playerNr), cLog::eLOG_TYPE_NET_DEBUG);

	//assign playerNr to the socket
	x->second = playerNr;

	cNetMessageTcpConnected message(playerNr, std::string(PACKAGE_VERSION) + PACKAGE_REV);

	cTextArchiveIn archive;
	archive << message;
	Log.write("ConnectionManager: <-- " + archive.data(), cLog::eLOG_TYPE_NET_DEBUG);

	sendMessage(socket, message);
}

//------------------------------------------------------------------------------
void cConnectionManager::declineConnection(cSocket* socket)
{
	assert(localServer != nullptr);

	cLockGuard<cMutex> tl(mutex);

	auto x = std::find_if(clientSockets.begin(), clientSockets.end(), [&](const std::pair<cSocket*, int>& x) { return x.first == socket; });
	if (x == clientSockets.end())
	{
		//looks like the connection was disconnected during the handshake
		Log.write("ConnectionManager: decline called for unknown socket", cLog::eLOG_TYPE_WARNING);
	}

	network->close(socket);
}

//------------------------------------------------------------------------------
void cConnectionManager::connectToServer(const std::string& host, int port, const cPlayerBasicData& player)
{
	assert(localClient != nullptr);

	cLockGuard<cMutex> tl(mutex);

	if (!network)
		network = new cNetwork(*this, mutex);

	Log.write("ConnectionManager: Connecting to " + host + ":" + toString(port), cLog::eLOG_TYPE_NET_DEBUG);

	network->connectToServer(host, port);

	//save credentials, until the conncetion is established
	connecting = true;
	connectingPlayerName = player.getName();
	connectingPlayerColor = player.getColor().getColor();
	connectingPlayerReady = player.isReady();
}

//------------------------------------------------------------------------------
bool cConnectionManager::isConnectedToServer() const
{
	cLockGuard<cMutex> tl(mutex);

	return connecting || serverSocket != nullptr;
}

//------------------------------------------------------------------------------
void cConnectionManager::setLocalClient(INetMessageReceiver* client, int playerNr)
{
	cLockGuard<cMutex> tl(mutex);

	localPlayer = playerNr;
	localClient = client;
}

//------------------------------------------------------------------------------
void cConnectionManager::setLocalServer(INetMessageReceiver* server)
{
	cLockGuard<cMutex> tl(mutex);

	localServer = server;
}

//------------------------------------------------------------------------------
int cConnectionManager::sendToServer(const cNetMessage2& message)
{
	cLockGuard<cMutex> tl(mutex);

	if (localServer)
	{
		localServer->pushMessage(message.clone());
		return 0;
	}
	else if (serverSocket)
	{
		return sendMessage(serverSocket, message);
	}
	else
	{
		Log.write("Connection Manager: Can't send message. No local server and no connection to server", cLog::eLOG_TYPE_NET_ERROR);
		return -1;
	}
}

//------------------------------------------------------------------------------
int cConnectionManager::sendToPlayer(const cNetMessage2& message, int playerNr)
{
	//TODO: listen to MU_MSG_PLAYER_NR
	cLockGuard<cMutex> tl(mutex);

	if (playerNr == localPlayer)
	{
		localClient->pushMessage(message.clone());
		return 0;
	}
	else
	{
		auto x = std::find_if(clientSockets.begin(), clientSockets.end(), [&](const std::pair<cSocket*, int>& x) { return x.second == playerNr; });
		if (x == clientSockets.end())
		{
			Log.write("Connection Manager: Can't send message. No connection to player " + toString(playerNr), cLog::eLOG_TYPE_NET_ERROR);
			return -1;
		}

		return sendMessage(x->first, message);
	}
}

//------------------------------------------------------------------------------
int cConnectionManager::sendToPlayers(const cNetMessage2& message)
{
	cLockGuard<cMutex> tl(mutex);

	if (localPlayer != -1)
	{
		localClient->pushMessage(message.clone());
	}

	// serialize...
	std::vector<unsigned char> buffer;
	cBinaryArchiveIn archive(buffer);
	archive << message;

	int result = 0;
	for (const auto& client : clientSockets)
	{
		result += network->sendMessage(client.first, buffer.size(), buffer.data());
	}

	return result;
}

//------------------------------------------------------------------------------
void cConnectionManager::disconnectAll()
{
	cLockGuard<cMutex> tl(mutex);

	if (serverSocket)
	{
		network->close(serverSocket);
	}

	while (clientSockets.size() > 0)
	{
		//erease in loop
		network->close(clientSockets[0].first);
	}
}

//------------------------------------------------------------------------------
void cConnectionManager::connectionClosed(cSocket* socket)
{
	if (socket == serverSocket)
	{
		if (localClient)
		{
			localClient->pushMessage(std::make_unique<cNetMessageTcpClose>(-1));
		}
		serverSocket = nullptr;
	}
	else
	{
		auto x = std::find_if(clientSockets.begin(), clientSockets.end(), [&](const std::pair<cSocket*, int>& x) { return x.first == socket; });
		if (x == clientSockets.end())
		{
			Log.write("ConnectionManager: An unknown connection was closed", cLog::eLOG_TYPE_ERROR);
			return;
		}

		int playerNr = x->second;
		if (playerNr != -1 && localServer) //is a player associated with the socket? 
		{
			localServer->pushMessage(std::make_unique<cNetMessageTcpClose>(playerNr));
		}

		clientSockets.erase(x);
	}
}

//------------------------------------------------------------------------------
void cConnectionManager::incomingConnection(cSocket* socket)
{
	std::pair<cSocket*, int> connection;
	connection.first = socket;
	connection.second = -1;
	clientSockets.push_back(connection);

	cNetMessageTcpHello message(std::string(PACKAGE_VERSION) + PACKAGE_REV);

	cTextArchiveIn archive;
	archive << message;
	Log.write("ConnectionManager: --> " + archive.data(), cLog::eLOG_TYPE_NET_DEBUG);

	sendMessage(socket, message);
}

//------------------------------------------------------------------------------
int cConnectionManager::sendMessage(cSocket* socket, const cNetMessage2& message)
{
	// serialize...
	std::vector<unsigned char> buffer;
	cBinaryArchiveIn archive(buffer);
	archive << message;

	return network->sendMessage(socket, buffer.size(), buffer.data());
}

//------------------------------------------------------------------------------
void cConnectionManager::messageReceived(cSocket* socket, unsigned char* data, int length)
{
	std::unique_ptr<cNetMessage2> message;
	try
	{
		message = cNetMessage2::createFromBuffer(data, length);
	}
	catch (std::runtime_error& e)
	{
		Log.write((std::string)"ConnectionManager: Can't deserialize net message: " + e.what(), cLog::eLOG_TYPE_NET_ERROR);
		return;
	}

	//TODO: security filter

	// handle messages for the connection handshake
	switch (message->getType())
	{
	case eNetMessageType::TCP_HELLO:
	{
		cTextArchiveIn archive;
		archive << *message;
		Log.write("ConnectionManager: <-- " + archive.data(), cLog::eLOG_TYPE_NET_DEBUG);

		//TODO: game version check
		if (localServer)
		{
			// server shouldn't get this message
			return;
		}

		cNetMessageTcpWantConnect response;
		response.playerName = connectingPlayerName;
		response.playerColor = connectingPlayerColor;
		response.ready = connectingPlayerReady;
		response.gameVersion = std::string(PACKAGE_VERSION) + PACKAGE_REV;

		cTextArchiveIn archiveResponse;
		archiveResponse << response;
		Log.write("ConnectionManager: --> " + archiveResponse.data(), cLog::eLOG_TYPE_NET_DEBUG);

		sendMessage(socket, response);
		return;
	}
	case eNetMessageType::TCP_WANT_CONNECT:
	{
		cTextArchiveIn archive;
		archive << *message;
		Log.write("ConnectionManager: <-- " + archive.data(), cLog::eLOG_TYPE_NET_DEBUG);

		if (!localServer)
		{
			// clients shouldn't get this message
			return;
		}
		auto x = std::find_if(clientSockets.begin(), clientSockets.end(), [&](const std::pair<cSocket*, int>& x) { return x.first == socket; });
		assert(x != clientSockets.end());

		if (x->second != -1)
		{
			Log.write("ConnectionManager: Received TCP_WANT_CONNECT from alreay connected player", cLog::eLOG_TYPE_NET_ERROR);
			return;
		}
				
		//TODO: game version check

		auto msgTcpWantConnect = static_cast<cNetMessageTcpWantConnect*>(message.get());
		msgTcpWantConnect->socket = socket;

		break;
	}
	case eNetMessageType::TCP_CONNECTED:
	{
		if (localServer)
		{
			// server shouldn't get this message
			return;
		}
		cTextArchiveIn archive;
		archive << *message;
		Log.write("ConnectionManager: <-- " + archive.data(), cLog::eLOG_TYPE_NET_DEBUG);

		//TODO: set localPlayerNr?
		break;
	}
	default:
		break;
	}

	if (localServer)
	{
		localServer->pushMessage(std::move(message));
	}
	else if (localClient)
	{
		localClient->pushMessage(std::move(message));
	}
}

//------------------------------------------------------------------------------
void cConnectionManager::connectionResult(cSocket* socket)
{
	assert(localClient);

	connecting = false;
	serverSocket = socket;

	if (socket == nullptr)
	{
		Log.write("ConnectionManager: Connect to server failed", cLog::eLOG_TYPE_NET_WARNING);
		auto message = std::make_unique<cNetMessageTcpConnectFailed>();
		localClient->pushMessage(std::move(message));
	}
}