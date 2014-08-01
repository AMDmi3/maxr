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

#include "game/network/client/networkclientgamesaved.h"
#include "ui/graphical/menu/windows/windowgamesettings/gamesettings.h"
#include "ui/graphical/application.h"
#include "client.h"
#include "server.h"
#include "game/data/player/player.h"
#include "clientevents.h"

//------------------------------------------------------------------------------
cNetworkClientGameSaved::cNetworkClientGameSaved ()
{}

//------------------------------------------------------------------------------
void cNetworkClientGameSaved::start (cApplication& application)
{
	localClient = std::make_shared<cClient> (nullptr, network);

	localClient->setPlayers (players, localPlayerIndex);
	localClient->setMap (staticMap);
	localClient->setGameSettings (*gameSettings);

	sendRequestResync (*localClient, localClient->getActivePlayer ().getNr ());

	gameGuiController = std::make_unique<cGameGuiController> (application, staticMap);

	gameGuiController->setClient (localClient);

	gameGuiController->start ();

	using namespace std::placeholders;
	signalConnectionManager.connect (gameGuiController->triggeredSave, std::bind (&cNetworkClientGameSaved::save, this, _1, _2));

	application.addRunnable (shared_from_this ());

	signalConnectionManager.connect (gameGuiController->terminated, [&]()
    {
        // me pointer ensures that game object stays alive till this call has terminated
        auto me = application.removeRunnable (*this);
		terminated ();
	});
}

//------------------------------------------------------------------------------
void cNetworkClientGameSaved::setPlayers (std::vector<cPlayerBasicData> players_, const cPlayerBasicData& localPlayer)
{
	players = players_;
	auto localPlayerIter = std::find_if (players.begin (), players.end (), [&](const cPlayerBasicData& player){ return player.getNr () == localPlayer.getNr (); });
	assert (localPlayerIter != players.end());
	localPlayerIndex = localPlayerIter - players.begin ();
}

//------------------------------------------------------------------------------
void cNetworkClientGameSaved::setGameSettings (std::shared_ptr<cGameSettings> gameSettings_)
{
	gameSettings = gameSettings_;
}


//------------------------------------------------------------------------------
void cNetworkClientGameSaved::setStaticMap (std::shared_ptr<cStaticMap> staticMap_)
{
	staticMap = staticMap_;
}

//------------------------------------------------------------------------------
const std::shared_ptr<cGameSettings>& cNetworkClientGameSaved::getGameSettings ()
{
	return gameSettings;
}

//------------------------------------------------------------------------------
const std::shared_ptr<cStaticMap>& cNetworkClientGameSaved::getStaticMap ()
{
	return staticMap;
}

//------------------------------------------------------------------------------
const std::vector<cPlayerBasicData>& cNetworkClientGameSaved::getPlayers ()
{
	return players;
}

//------------------------------------------------------------------------------
const cPlayerBasicData& cNetworkClientGameSaved::getLocalPlayer ()
{
	return players[localPlayerIndex];
}
