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

#include <functional>

#include "windowsingleplayer.h"
#include "windowgamesettings/gamesettings.h"
#include "windowgamesettings/windowgamesettings.h"
#include "windowmapselection/windowmapselection.h"
#include "windowclanselection/windowclanselection.h"
#include "windowlandingunitselection/windowlandingunitselection.h"
#include "../widgets/pushbutton.h"
#include "../../application.h"

#include "../../../main.h"
#include "../../../network.h"
#include "../../../player.h"
#include "../../../settings.h"
#include "../../../client.h"
#include "../../../server.h"

//------------------------------------------------------------------------------
cWindowSinglePlayer::cWindowSinglePlayer () :
	cWindowMain (lngPack.i18n ("Text~Button~Single_Player"))
{
	using namespace std::placeholders;

	const auto& menuPosition = getArea ().getMinCorner ();

	auto newGameButton = addChild (std::make_unique<cPushButton> (menuPosition + cPosition (390, 190), ePushButtonType::StandardBig, lngPack.i18n ("Text~Button~Game_New")));
	signalConnectionManager.connect (newGameButton->clicked, std::bind (&cWindowSinglePlayer::newGameClicked, this));

	auto loadGameButton = addChild (std::make_unique<cPushButton> (menuPosition + cPosition (390, 190 + buttonSpace), ePushButtonType::StandardBig, lngPack.i18n ("Text~Button~Game_Load")));
	signalConnectionManager.connect (loadGameButton->clicked, std::bind (&cWindowSinglePlayer::loadGameClicked, this));

	auto backButton = addChild (std::make_unique<cPushButton> (menuPosition + cPosition (415, 190 + buttonSpace * 6), ePushButtonType::StandardSmall, lngPack.i18n ("Text~Button~Back")));
	signalConnectionManager.connect (backButton->clicked, std::bind (&cWindowSinglePlayer::backClicked, this));
}

//------------------------------------------------------------------------------
cWindowSinglePlayer::~cWindowSinglePlayer ()
{}

//------------------------------------------------------------------------------
void cWindowSinglePlayer::newGameClicked ()
{
	if (!getActiveApplication ()) return;

	auto application = getActiveApplication ();

	auto windowGameSettings = getActiveApplication ()->show (std::make_shared<cWindowGameSettings> ());
	windowGameSettings->applySettings (cGameSettings ());

	windowGameSettings->done.connect ([=]()
	{
		auto windowMapSelection = application->show (std::make_shared<cWindowMapSelection> ());

		windowMapSelection->done.connect ([=]()
		{
			auto windowClanSelection = application->show (std::make_shared<cWindowClanSelection> ());

			windowClanSelection->done.connect ([=]()
			{
				auto windowLandingUnitSelectionOwn = application->show (std::make_shared<cWindowLandingUnitSelection> ());

				windowLandingUnitSelectionOwn->done.connect ([=]()
				{
					auto gameSettings = windowGameSettings->getGameSettings ();

					// Get data from all windows and start the game

					// ...

					windowLandingUnitSelectionOwn->close ();
					windowClanSelection->close ();
					windowMapSelection->close ();
					windowGameSettings->close ();
				});
			});
		});
	});
}

//------------------------------------------------------------------------------
void cWindowSinglePlayer::loadGameClicked ()
{
}

//------------------------------------------------------------------------------
void cWindowSinglePlayer::backClicked ()
{
	close ();
}
