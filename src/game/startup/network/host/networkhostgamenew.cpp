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

#include "game/startup/network/host/networkhostgamenew.h"
#include "game/data/gamesettings.h"
#include "ui/graphical/application.h"
#include "game/logic/client.h"
#include "game/logic/server2.h"
#include "game/data/player/player.h"
#include "game/data/units/landingunit.h"
#include "game/logic/clientevents.h"
#include "game/logic/action/actioninitnewgame.h"

//------------------------------------------------------------------------------
cNetworkHostGameNew::cNetworkHostGameNew() :
	localPlayerClan (-1)
{}

//------------------------------------------------------------------------------
void cNetworkHostGameNew::start (cApplication& application)
{
	assert (gameSettings != nullptr);

	server = std::make_unique<cServer2> (connectionManager);
	localClient = std::make_shared<cClient> (connectionManager);

	server->setMap (staticMap);
	localClient->setMap(staticMap);

	server->setUnitsData(unitsData);
	localClient->setUnitsData(unitsData);

	//TODO: set clandata

	localClient->setPlayers(players, localPlayerNr);
	server->setPlayers(players);

	server->setGameSettings (*gameSettings);
	localClient->setGameSettings(*gameSettings);

	connectionManager->setLocalServer(server.get());
	connectionManager->setLocalClient(localClient.get(), localPlayerNr);

	server->start();

	cActionInitNewGame action;
	action.clan = localPlayerClan;
	action.landingUnits = localPlayerLandingUnits;
	action.landingPosition = localPlayerLandingPosition;
	action.unitUpgrades = localPlayerUnitUpgrades;
	localClient->sendNetMessage(action);

	gameGuiController = std::make_unique<cGameGuiController> (application, staticMap);
	gameGuiController->setSingleClient(localClient);
	gameGuiController->setServer(server.get());

	cGameGuiState playerGameGuiState;
	playerGameGuiState.setMapPosition (localPlayerLandingPosition);
	gameGuiController->addPlayerGameGuiState (localPlayerNr, std::move (playerGameGuiState));

	gameGuiController->start();

	resetTerminating();

	application.addRunnable (shared_from_this());

	signalConnectionManager.connect (gameGuiController->terminated, [&]() { exit(); });
}

//------------------------------------------------------------------------------
void cNetworkHostGameNew::setPlayers (std::vector<cPlayerBasicData> players_, const cPlayerBasicData& localPlayer)
{
	players = players_;
	localPlayerNr = localPlayer.getNr();
}

//------------------------------------------------------------------------------
void cNetworkHostGameNew::setGameSettings (std::shared_ptr<cGameSettings> gameSettings_)
{
	gameSettings = gameSettings_;
}

//------------------------------------------------------------------------------
void cNetworkHostGameNew::setStaticMap (std::shared_ptr<cStaticMap> staticMap_)
{
	staticMap = staticMap_;
}

//------------------------------------------------------------------------------
void cNetworkHostGameNew::setLocalPlayerClan (int clan)
{
	localPlayerClan = clan;
}

//------------------------------------------------------------------------------
void cNetworkHostGameNew::setLocalPlayerLandingUnits (std::vector<sLandingUnit> landingUnits_)
{
	localPlayerLandingUnits = std::move (landingUnits_);
}

//------------------------------------------------------------------------------
void cNetworkHostGameNew::setLocalPlayerUnitUpgrades (std::vector<std::pair<sID, cUnitUpgrade>> unitUpgrades_)
{
	localPlayerUnitUpgrades = std::move (unitUpgrades_);
}

//------------------------------------------------------------------------------
void cNetworkHostGameNew::setLocalPlayerLandingPosition (const cPosition& landingPosition_)
{
	localPlayerLandingPosition = landingPosition_;
}

//------------------------------------------------------------------------------
const std::shared_ptr<cGameSettings>& cNetworkHostGameNew::getGameSettings()
{
	return gameSettings;
}

//------------------------------------------------------------------------------
const std::shared_ptr<cStaticMap>& cNetworkHostGameNew::getStaticMap()
{
	return staticMap;
}

//------------------------------------------------------------------------------
const std::vector<cPlayerBasicData>& cNetworkHostGameNew::getPlayers()
{
	return players;
}

//------------------------------------------------------------------------------
const cPlayerBasicData& cNetworkHostGameNew::getLocalPlayer()
{
	return *std::find_if(players.begin(), players.end(), [&](const cPlayerBasicData& player) { return player.getNr() == localPlayerNr; });
}

//------------------------------------------------------------------------------
const std::vector<sLandingUnit>& cNetworkHostGameNew::getLandingUnits()
{
	return localPlayerLandingUnits;
}

//------------------------------------------------------------------------------
int cNetworkHostGameNew::getLocalPlayerClan() const
{
	return localPlayerClan;
}
