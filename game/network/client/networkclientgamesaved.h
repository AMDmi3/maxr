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

#ifndef game_network_client_networkclientgamesavedH
#define game_network_client_networkclientgamesavedH

#include <memory>
#include <vector>
#include <utility>

#include "networkclientgame.h"
#include "../../../maxrconfig.h"
#include "../../../utility/signal/signal.h"
#include "../../../utility/signal/signalconnectionmanager.h"
#include "../../../utility/position.h"

class cApplication;
class cStaticMap;
class cGameSettings;
class sPlayer;
class cPlayer;
class cPosition;
class cUnitUpgrade;

struct sLandingUnit;
struct sID;

class cNetworkClientGameSaved : public cNetworkClientGame
{
public:
	cNetworkClientGameSaved ();

	void start (cApplication& application);

	void setPlayers (std::vector<std::shared_ptr<sPlayer>> players, const sPlayer& localPlayer);

	void setGameSettings (std::shared_ptr<cGameSettings> gameSettings);

	void setStaticMap (std::shared_ptr<cStaticMap> staticMap);

	const std::shared_ptr<cGameSettings>& getGameSettings ();
	const std::shared_ptr<cStaticMap>& getStaticMap ();
	const std::vector<std::shared_ptr<sPlayer>>& getPlayers ();
	const std::shared_ptr<sPlayer>& getLocalPlayer ();

	int getLocalPlayerClan () const;

	cSignal<void ()> terminated;
private:
	cSignalConnectionManager signalConnectionManager;

	size_t localPlayerIndex;
	std::vector<std::shared_ptr<sPlayer>> players;

	std::shared_ptr<cStaticMap> staticMap;
	std::shared_ptr<cGameSettings> gameSettings;
};

#endif // game_network_client_networkclientgamesavedH
