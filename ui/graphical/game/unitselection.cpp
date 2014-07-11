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

#include <algorithm>

#include "ui/graphical/game/unitselection.h"

#include "map.h"
#include "vehicles.h"
#include "buildings.h"
#include "utility/box.h"

//------------------------------------------------------------------------------
bool cUnitSelection::selectUnitAt (const cMapField& field, bool base)
{
	cVehicle* plane = field.getPlane ();
	if (plane && !plane->moving)
	{
		return selectUnit (*plane);
	}
	cVehicle* vehicle = field.getVehicle ();
	if (vehicle && !vehicle->moving && !(plane /*&& (unitMenuActive || vehicle->owner != player)*/))
	{
		return selectUnit (*vehicle);
	}
	cBuilding* topBuilding = field.getTopBuilding ();
	const cVehicle* selectedVehicle = getSelectedVehicle ();
	if (topBuilding && (base || ((topBuilding->data.surfacePosition != sUnitData::SURFACE_POS_ABOVE || !selectedVehicle) && (!field.getTopBuilding ()->data.canBeLandedOn || (!selectedVehicle || selectedVehicle->data.factorAir == 0)))))
	{
		return selectUnit (*topBuilding);
	}
	cBuilding* baseBuilding = field.getBaseBuilding ();
	if ((base || !selectedVehicle) && baseBuilding && baseBuilding->owner != NULL)
	{
		return selectUnit (*baseBuilding);
	}
	return false;
}

//------------------------------------------------------------------------------
void cUnitSelection::addSelectedUnitBack (cUnit& unit)
{
	auto unitPtr = &unit;
	auto connection = selectedUnitsSignalConnectionManager.connect (unit.destroyed, [this, unitPtr]()
	{
		deselectUnit (*unitPtr);
	});
	selectedUnits.push_back (std::make_pair (&unit, connection));
}

//------------------------------------------------------------------------------
void cUnitSelection::addSelectedUnitFront (cUnit& unit)
{
	auto unitPtr = &unit;
	auto connection = selectedUnitsSignalConnectionManager.connect (unit.destroyed, [this, unitPtr]()
	{
		deselectUnit (*unitPtr);
	});
	selectedUnits.insert (selectedUnits.begin(), std::make_pair (&unit, connection));
}

//------------------------------------------------------------------------------
void cUnitSelection::removeSelectedUnit (const cUnit& unit)
{
	auto iter = std::find_if (selectedUnits.begin (), selectedUnits.end (), [&unit](const std::pair<cUnit*, cSignalConnection>& entry) { return entry.first == &unit; });
	if (iter == selectedUnits.end ()) return;

	selectedUnitsSignalConnectionManager.disconnect (iter->second);
	selectedUnits.erase (iter);
}

//------------------------------------------------------------------------------
void cUnitSelection::removeAllSelectedUnits ()
{
	selectedUnitsSignalConnectionManager.disconnectAll ();
	selectedUnits.clear ();
}

//------------------------------------------------------------------------------
bool cUnitSelection::selectVehiclesAt (const cBox<cPosition>& box, const cMap& map, const cPlayer& player)
{
	auto oldSelectedUnit = getSelectedUnit ();

	removeAllSelectedUnits ();

	for (int x = box.getMinCorner ().x (); x <= box.getMaxCorner ().x (); ++x)
	{
		for (int y = box.getMinCorner ().y (); y <= box.getMaxCorner ().y (); ++y)
		{
			const cPosition position (x, y);

			cVehicle* vehicle = map.getField (position).getVehicle ();
			if (!vehicle ||vehicle->owner != &player) vehicle = map.getField (position).getPlane ();

			if (vehicle && vehicle->owner == &player && !vehicle->isUnitBuildingABuilding () && !vehicle->isUnitClearing() && !vehicle->moving)
			{
				if (vehicle == oldSelectedUnit)
				{
					addSelectedUnitFront (*vehicle);
				}
				else
				{
					addSelectedUnitBack (*vehicle);
				}
			}
		}
	}

	if (oldSelectedUnit != getSelectedUnit ()) mainSelectionChanged ();
	groupSelectionChanged (); // FIXME: call only when the group has really changed!
	selectionChanged ();
	return false;
}

//------------------------------------------------------------------------------
bool cUnitSelection::selectUnit (cUnit& unit, bool add)
{
	if (selectedUnits.size () == 1 && selectedUnits[0].first == &unit) return false;

	if (!add) removeAllSelectedUnits();

	if (!isSelected (unit))
	{
		addSelectedUnitFront (unit);

		if (selectedUnits.size () == 1) mainSelectionChanged ();
		else groupSelectionChanged ();
		selectionChanged ();

		return true;
	}
	return false;
}

//------------------------------------------------------------------------------
void cUnitSelection::deselectUnit (const cUnit& unit)
{
	const auto oldSelectedUnitsCount = selectedUnits.size ();
	const auto isMainUnit = !selectedUnits.empty () && selectedUnits[0].first == &unit;

	removeSelectedUnit (unit);

	if (selectedUnits.size () != oldSelectedUnitsCount)
	{
		if (isMainUnit) mainSelectionChanged ();
		if (selectedUnits.size () > 0) groupSelectionChanged ();
		selectionChanged ();
	}
}

//------------------------------------------------------------------------------
void cUnitSelection::deselectUnits ()
{
	if (selectedUnits.empty ()) return;

	const auto oldSelectedUnitsCount = selectedUnits.size ();

	removeAllSelectedUnits ();

	if (oldSelectedUnitsCount > 0) mainSelectionChanged ();
	if (oldSelectedUnitsCount > 1) groupSelectionChanged ();
	selectionChanged ();
}

//------------------------------------------------------------------------------
cUnit* cUnitSelection::getSelectedUnit () const
{
	return selectedUnits.empty () ? nullptr : selectedUnits[0].first;
}

//------------------------------------------------------------------------------
cVehicle* cUnitSelection::getSelectedVehicle () const
{
	auto selectedUnit = getSelectedUnit ();
	return static_cast<cVehicle*>(selectedUnit && selectedUnit->isAVehicle () ? selectedUnit : nullptr);
}

//------------------------------------------------------------------------------
cBuilding* cUnitSelection::getSelectedBuilding () const
{
	auto selectedUnit = getSelectedUnit ();
	return static_cast<cBuilding*>(selectedUnit && selectedUnit->isABuilding () ? selectedUnit : nullptr);
}

//------------------------------------------------------------------------------
std::vector<cUnit*> cUnitSelection::getSelectedUnits () const
{
	std::vector<cUnit*> result;
	for (auto i = selectedUnits.begin (); i != selectedUnits.end (); ++i)
	{
		result.push_back (static_cast<cVehicle*>(i->first));
	}
	return result;
}

//------------------------------------------------------------------------------
std::vector<cVehicle*> cUnitSelection::getSelectedVehicles () const
{
	std::vector<cVehicle*> result;
	for (auto i = selectedUnits.begin (); i != selectedUnits.end (); ++i)
	{
		if (i->first->data.ID.isAVehicle ())
		{
			result.push_back (static_cast<cVehicle*>(i->first));
		}
	}
	return result;
}

//------------------------------------------------------------------------------
std::vector<cBuilding*> cUnitSelection::getSelectedBuildings () const
{
	std::vector<cBuilding*> result;
	for (auto i = selectedUnits.begin (); i != selectedUnits.end (); ++i)
	{
		if (i->first->data.ID.isABuilding ())
		{
			result.push_back (static_cast<cBuilding*>(i->first));
		}
	}
	return result;
}

//------------------------------------------------------------------------------
size_t cUnitSelection::getSelectedUnitsCount () const
{
	return selectedUnits.size ();
}

//------------------------------------------------------------------------------
size_t cUnitSelection::getSelectedVehiclesCount () const
{
	size_t result = 0;
	for (auto i = selectedUnits.begin (); i != selectedUnits.end (); ++i)
	{
		if (i->first->data.ID.isAVehicle ()) ++result;
	}
	return result;
}

//------------------------------------------------------------------------------
size_t cUnitSelection::getSelectedBuildingsCount () const
{
	size_t result = 0;
	for (auto i = selectedUnits.begin (); i != selectedUnits.end (); ++i)
	{
		if (i->first->data.ID.isABuilding ()) ++result;
	}
	return result;
}

//------------------------------------------------------------------------------
bool cUnitSelection::isSelected (const cUnit& unit) const
{
	auto iter = std::find_if (selectedUnits.begin (), selectedUnits.end (), [&unit](const std::pair<cUnit*, cSignalConnection>& entry) { return entry.first == &unit; });
	return iter != selectedUnits.end ();
}
