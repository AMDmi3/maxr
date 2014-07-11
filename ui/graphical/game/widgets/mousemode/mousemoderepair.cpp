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

#include "ui/graphical/game/widgets/mousemode/mousemoderepair.h"
#include "ui/graphical/game/widgets/mouseaction/mouseactionrepair.h"
#include "ui/graphical/game/unitselection.h"
#include "map.h"
#include "vehicles.h"
#include "buildings.h"
#include "input/mouse/mouse.h"
#include "input/mouse/cursor/mousecursorsimple.h"

//------------------------------------------------------------------------------
eMouseModeType cMouseModeRepair::getType () const
{
	return eMouseModeType::Repair;
}

//------------------------------------------------------------------------------
void cMouseModeRepair::setCursor (cMouse& mouse, const cMap& map, const cPosition& mapPosition, const cUnitSelection& unitSelection, const cPlayer* player) const
{
	if (canExecuteAction (map, mapPosition, unitSelection))
	{
		mouse.setCursor (std::make_unique<cMouseCursorSimple> (eMouseCursorSimpleType::Repair));
	}
	else
	{
		mouse.setCursor (std::make_unique<cMouseCursorSimple> (eMouseCursorSimpleType::No));
	}
}

//------------------------------------------------------------------------------
std::unique_ptr<cMouseAction> cMouseModeRepair::getMouseAction (const cMap& map, const cPosition& mapPosition, const cUnitSelection& unitSelection, const cPlayer* player) const
{
	if (canExecuteAction (map, mapPosition, unitSelection))
	{
		return std::make_unique<cMouseActionRepair> ();
	}
	else return nullptr;
}

//------------------------------------------------------------------------------
bool cMouseModeRepair::canExecuteAction (const cMap& map, const cPosition& mapPosition, const cUnitSelection& unitSelection) const
{
	const auto selectedVehicle = unitSelection.getSelectedVehicle ();

	return selectedVehicle && selectedVehicle->canSupply (map, mapPosition, SUPPLY_TYPE_REPAIR);
}
