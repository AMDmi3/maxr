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

#include <ctime>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <algorithm>

#include <SDL.h>
#include <SDL_thread.h>
#include <SDL_net.h>
#include <SDL_mixer.h>

#define __main__
#include "main.h"

#include "game/data/base/base.h"
#include "game/data/units/building.h"
#include "game/data/player/clans.h"
#include "dedicatedserver.h"
#include "utility/files.h"
#include "keys.h"
#include "loaddata.h"
#include "utility/log.h"
#include "game/data/map/map.h"
#include "mveplayer.h"
#include "network.h"
#include "pcx.h"
#include "game/data/player/player.h"
#include "game/logic/savegame.h"
#include "settings.h"
#include "sound.h"
#include "unifonts.h"
#include "game/data/units/vehicle.h"
#include "video.h"

#include "input/mouse/mouse.h"
#include "input/keyboard/keyboard.h"

#include "output/sound/sounddevice.h"

#include "ui/graphical/application.h"
#include "ui/graphical/menu/windows/windowstart.h"

using namespace std;

static int initNet();
static int initSDL();
static int initSound();
static void logMAXRVersion();
static void showIntro();

int main (int argc, char* argv[])
{
	if (!cSettings::getInstance().isInitialized())
	{
		Quit();
		return -1;
	}
	// stop on error during init of SDL basics. WARNINGS will be ignored!
	if (initSDL() == -1) return -1;

	// call it once to initialize
	is_main_thread();

	logMAXRVersion();

	if (!DEDICATED_SERVER)
	{
		Video.initSplash(); // show splashscreen
		initSound(); // now config is loaded and we can init sound and net
	}
	initNet();

	// load files
	volatile int loadingState = LOAD_GOING;
	SDL_Thread* dataThread = SDL_CreateThread (LoadData, "loadingData", const_cast<int*> (&loadingState));

	SDL_Event event;
	while (loadingState != LOAD_FINISHED)
	{
		if (loadingState == LOAD_ERROR)
		{
			Log.write ("Error while loading data!", cLog::eLOG_TYPE_ERROR);
			SDL_WaitThread (dataThread, NULL);
			Quit();
		}
		while (SDL_PollEvent (&event))
		{
			if (!DEDICATED_SERVER
				&& event.type == SDL_WINDOWEVENT
				&& event.window.event == SDL_WINDOWEVENT_EXPOSED)
			{
				Video.draw();
			}
		}
		SDL_Delay (100);
		if (!DEDICATED_SERVER) {
			// The draw may be conditionned when screen has changed.
			Video.draw();
		}
	}

	if (!DEDICATED_SERVER)
	{
		// play intro if we're supposed to and the file exists
		if (cSettings::getInstance().shouldShowIntro())
		{
			showIntro();
		}
		else
		{
			Log.write ("Skipped intro movie due settings", cLog::eLOG_TYPE_DEBUG);
		}
	}

	SDL_WaitThread (dataThread, NULL);

	if (DEDICATED_SERVER)
	{
		cDedicatedServer::instance().run();
	}
	else
	{
		Video.setResolution (Video.getResolutionX(), Video.getResolutionY(), true);
		Video.clearBuffer();

		cMouse mouse;
		cKeyboard keyboard;

		cApplication application;

		application.registerMouse (mouse);
		application.registerKeyboard (keyboard);

		auto startWindow = std::make_shared<cWindowStart>();
		application.show (startWindow);

		application.execute ();
	}

	Quit();
	return 0;
}

/**
 *Inits SDL
 *@author beko
 *@return -1 on error<br>0 on success<br>1 with warnings
 */
static int initSDL()
{
	int sdlInitResult = -1;
	if (DEDICATED_SERVER)
		sdlInitResult = SDL_Init (SDL_INIT_TIMER | SDL_INIT_NOPARACHUTE);  // start SDL basics without video
	else
		sdlInitResult = SDL_Init (SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_NOPARACHUTE);  // start SDL basics
	if (sdlInitResult == -1)
	{
		Log.write ("Could not init SDL", cLog::eLOG_TYPE_ERROR);
		Log.write (SDL_GetError(), cLog::eLOG_TYPE_ERROR);
		return -1;
	}
	else
	{
		Log.write ("Initalized SDL basics - looks good!", cLog::eLOG_TYPE_INFO);
		Log.mark();
		//made it - enough to start game
		return 0;
	}
}

/**
 *Inits SDL_sound
 *@author beko
 *@return -1 on error<br>0 on success<br>1 with warnings
 */
static int initSound()
{
    if (!cSettings::getInstance ().isSoundEnabled ())
	{
		Log.write ("Sound disabled due configuration", cLog::eLOG_TYPE_INFO);
		return 1;
	}

	if (SDL_Init (SDL_INIT_AUDIO) < 0)     //start sound
	{
		Log.write ("Could not init SDL_INIT_AUDIO", cLog::eLOG_TYPE_WARNING);
		Log.write ("Sound won't be available!", cLog::eLOG_TYPE_WARNING);
		Log.write (SDL_GetError(), cLog::eLOG_TYPE_WARNING);
		cSettings::getInstance().setSoundEnabled (false, false);
		return -1;
	}

    try
    {
        cSoundDevice::getInstance ().initialize (cSettings::getInstance ().getFrequency (), cSettings::getInstance ().getChunkSize ());
    }
    catch (std::runtime_error& e)
    {
        Log.write ("Could not init SDL_mixer:", cLog::eLOG_TYPE_WARNING);
		Log.write (e.what(), cLog::eLOG_TYPE_WARNING);
		Log.write ("Sound won't be available!", cLog::eLOG_TYPE_WARNING);
		cSettings::getInstance().setSoundEnabled (false, false);
		return -1;
	}
	Log.write ("Sound started", cLog::eLOG_TYPE_INFO);
	return 0;
}

/**
 *Inits SDL_net
 *@author beko
 *@return -1 on error<br>0 on success<br>1 with warnings
 */
static int initNet()
{
	if (SDLNet_Init() == -1)   // start SDL_net
	{
		Log.write ("Could not init SDLNet_Init\nNetwork games won' be available! ", cLog::eLOG_TYPE_WARNING);
		Log.write (SDL_GetError(), cLog::eLOG_TYPE_WARNING);
		return -1;
	}
	Log.write ("Net started", cLog::eLOG_TYPE_INFO);
	return 0;
}

static void logMAXRVersion()
{
	std::string sVersion = PACKAGE_NAME; sVersion += " ";
	sVersion += PACKAGE_VERSION; sVersion += " ";
	sVersion += PACKAGE_REV; sVersion += " ";
	Log.write (sVersion, cLog::eLOG_TYPE_INFO);
	std::string sBuild = "Build: "; sBuild += MAX_BUILD_DATE;
	Log.write (sBuild, cLog::eLOG_TYPE_INFO);
#if HAVE_AUTOVERSION_H
	std::string sBuildVerbose = "On: ";
	sBuildVerbose += BUILD_UNAME_S;
	sBuildVerbose += " ";
	sBuildVerbose += BUILD_UNAME_R;
	Log.write (sBuildVerbose, cLog::eLOG_TYPE_INFO);

	sBuildVerbose = "From: ";
	sBuildVerbose += BUILD_USER;
	sBuildVerbose += " at ";
	sBuildVerbose += BUILD_UNAME_N;
	Log.write (sBuildVerbose, cLog::eLOG_TYPE_INFO);
#endif
	Log.mark();
	Log.write (sVersion, cLog::eLOG_TYPE_NET_DEBUG);
	Log.write (sBuild, cLog::eLOG_TYPE_NET_DEBUG);
}



static void showIntro()
{
	const std::string filename = cSettings::getInstance().getMvePath() + PATH_DELIMITER + "MAXINT.MVE";

	if (!FileExists (filename.c_str()))
	{
		Log.write ("Couldn't find movie " + filename, cLog::eLOG_TYPE_WARNING);
	}
	// Close maxr sound for intro movie
    cSoundDevice::getInstance ().close ();

	Log.write ("Starting movie " + filename, cLog::eLOG_TYPE_DEBUG);
	const int mvereturn = MVEPlayer (filename.c_str(),
									 Video.getResolutionX(), Video.getResolutionY(),
									 !Video.getWindowMode(),
									 !cSettings::getInstance().isSoundMute());
	Log.write ("MVEPlayer returned " + iToStr (mvereturn), cLog::eLOG_TYPE_DEBUG);
	//FIXME: make this case sensitive - my mve is e.g. completly lower cases -- beko

	// reinit maxr sound
	if (cSettings::getInstance().isSoundEnabled())
	{
        try
        {
            cSoundDevice::getInstance ().initialize (cSettings::getInstance ().getFrequency (), cSettings::getInstance ().getChunkSize ());
        }
        catch (std::runtime_error& e)
        {
            Log.write ("Can't reinit sound after playing intro" + iToStr (mvereturn), cLog::eLOG_TYPE_DEBUG);
            Log.write (e.what(), cLog::eLOG_TYPE_DEBUG);
        }
	}
}

/**
 * Return if it is the main thread.
 * @note: should be called by main once by the main thread to initialize.
 */
bool is_main_thread()
{
	static const SDL_threadID main_thread_id = SDL_ThreadID();
	return main_thread_id == SDL_ThreadID();
}

/**
 *Terminates app
 *@author beko
 */
void Quit()
{
	delete font;

	UnitsData.svehicles.clear();
	UnitsData.sbuildings.clear();

	//unload files here
    cSoundDevice::getInstance ().close ();
	SDLNet_Quit();
	Video.clearMemory();
	SDL_Quit();
	Log.write ("EOF");
	exit (0);
}

string iToStr (int x)
{
	stringstream strStream;
	strStream << x;
	return strStream.str();
}

string iToHex (unsigned int x)
{
	stringstream strStream;
	strStream << std::hex << x;
	return strStream.str();
}

string fToStr (float x)
{
	stringstream strStream;
	strStream << x;
	return strStream.str();
}

std::string pToStr (const void* x)
{
	stringstream strStream;
	strStream << x;
	return "0x" + strStream.str();
}

std::string bToStr (bool x)
{
	return x ? "true" : "false";
}

// Round //////////////////////////////////////////////////////////////////////
// Rounds a Number to 'iDecimalPlace' digits after the comma:
float Round (float dValueToRound, unsigned int iDecimalPlace)
{
	dValueToRound *= powf (10.f, (int) iDecimalPlace);
	if (dValueToRound >= 0)
		dValueToRound = floorf (dValueToRound + 0.5f);
	else
		dValueToRound = ceilf (dValueToRound - 0.5f);
	dValueToRound /= powf (10.f, (int) iDecimalPlace);
	return dValueToRound;
}

int Round (float dValueToRound)
{
	return (int) Round (dValueToRound, 0);
}


//------------------------------------------------------------------------------
// ----------- sID Implementation ----------------------------------------------
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
string sID::getText() const
{
	char tmp[6];
	TIXML_SNPRINTF (tmp, sizeof (tmp), "%.2d %.2d", iFirstPart, iSecondPart);
	return tmp;
}

//------------------------------------------------------------------------------
void sID::generate (const string& text)
{
	const string::size_type spacePos = text.find (" ", 0);
	iFirstPart = atoi (text.substr (0, spacePos).c_str());
	iSecondPart = atoi (text.substr (spacePos, text.length()).c_str());
}

//------------------------------------------------------------------------------
const sUnitData* sID::getUnitDataOriginalVersion (const cPlayer* Owner) const
{
	if (isAVehicle())
	{
		int index = UnitsData.getVehicleIndexBy (*this);
		const int clan = Owner ? Owner->getClan() : -1;
		return &UnitsData.getVehicle (index, clan);
	}
	else
	{
		int index = UnitsData.getBuildingIndexBy (*this);
		const int clan = Owner ? Owner->getClan() : -1;
		return &UnitsData.getBuilding (index, clan);
	}
}

//------------------------------------------------------------------------------
bool sID::less_buildingFirst (const sID& ID) const
{
	return iFirstPart == ID.iFirstPart ? iSecondPart < ID.iSecondPart : iFirstPart > ID.iFirstPart;
}

//------------------------------------------------------------------------------
bool sID::less_vehicleFirst (const sID& ID) const
{
	return iFirstPart == ID.iFirstPart ? iSecondPart < ID.iSecondPart : iFirstPart < ID.iFirstPart;
}

//------------------------------------------------------------------------------
bool sID::operator == (const sID& ID) const
{
	if (iFirstPart == ID.iFirstPart && iSecondPart == ID.iSecondPart) return true;
	return false;
}

//------------------------------------------------------------------------------
cUnitsData::cUnitsData() :
	ptr_small_beton (0),
	ptr_small_beton_org (0),
	ptr_connector (0),
	ptr_connector_org (0),
	ptr_connector_shw (0),
	ptr_connector_shw_org (0),
	initializedClanUnitData (false)
{
}

//------------------------------------------------------------------------------
const sUnitData& cUnitsData::getVehicle (int nr, int clan)
{
	return getUnitData_Vehicles (clan) [nr];
}

//------------------------------------------------------------------------------
const sUnitData& cUnitsData::getBuilding (int nr, int clan)
{
	return getUnitData_Buildings (clan) [nr];
}

//------------------------------------------------------------------------------
const sUnitData& cUnitsData::getUnit (const sID& id, int clan)
{
	if (id.isAVehicle ())
	{
		return getVehicle (getVehicleIndexBy (id), clan);
	}
	else
	{
		assert (id.isABuilding ());
		return getBuilding (getBuildingIndexBy (id), clan);
	}
}

const std::vector<sUnitData>& cUnitsData::getUnitData_Vehicles (int clan)
{
	if (!initializedClanUnitData)
		initializeClanUnitData();

	if (clan < 0 || clan > (int) clanUnitDataVehicles.size())
	{
		return svehicles;
	}
	return clanUnitDataVehicles[clan];
}

const std::vector<sUnitData>& cUnitsData::getUnitData_Buildings (int clan)
{
	if (!initializedClanUnitData)
		initializeClanUnitData();

	if (clan < 0 || clan > (int) clanUnitDataBuildings.size())
	{
		return sbuildings;
	}
	return clanUnitDataBuildings[clan];
}

int cUnitsData::getBuildingIndexBy (sID id) const
{
	if (id.isABuilding() == false) return -1;
	for (unsigned int i = 0; i != UnitsData.getNrBuildings(); ++i)
	{
		if (sbuildings[i].ID == id) return i;
	}
	return -1;
}

int cUnitsData::getVehicleIndexBy (sID id) const
{
	if (id.isAVehicle() == false) return -1;
	for (unsigned int i = 0; i != UnitsData.getNrVehicles(); ++i)
	{
		if (svehicles[i].ID == id) return i;
	}
	return -1;
}

const sBuildingUIData* cUnitsData::getBuildingUI (sID id) const
{
	const int index = getBuildingIndexBy (id);
	if (index == -1) return 0;
	return &buildingUIs[index];
}

const sVehicleUIData* cUnitsData::getVehicleUI (sID id) const
{
	const int index = getVehicleIndexBy (id);
	if (index == -1) return 0;
	return &vehicleUIs[index];
}

unsigned int cUnitsData::getNrVehicles() const
{
	return (int) svehicles.size();
}

unsigned int cUnitsData::getNrBuildings() const
{
	return (int) sbuildings.size();
}

static int getConstructorIndex()
{
	for (unsigned int i = 0; i != UnitsData.getNrVehicles(); ++i)
	{
		const sUnitData& vehicle = UnitsData.svehicles[i];

		if (vehicle.canBuild.compare ("BigBuilding") == 0)
		{
			return i;
		}
	}
	return -1;
}

static int getEngineerIndex()
{
	for (unsigned int i = 0; i != UnitsData.getNrVehicles(); ++i)
	{
		const sUnitData& vehicle = UnitsData.svehicles[i];

		if (vehicle.canBuild.compare ("SmallBuilding") == 0)
		{
			return i;
		}
	}
	return -1;
}

static int getSurveyorIndex()
{
	for (unsigned int i = 0; i != UnitsData.getNrVehicles(); ++i)
	{
		const sUnitData& vehicle = UnitsData.svehicles[i];

		if (vehicle.canSurvey)
		{
			return i;
		}
	}
	return -1;
}

void cUnitsData::initializeIDData()
{
	int constructorIndex = getConstructorIndex();
	int engineerIndex = getEngineerIndex();
	int surveyorIndex = getSurveyorIndex();

	assert (constructorIndex != -1);
	assert (engineerIndex != -1);
	assert (surveyorIndex != -1);

	constructorID = svehicles[constructorIndex].ID;
	engineerID = svehicles[engineerIndex].ID;
	surveyorID = svehicles[surveyorIndex].ID;
}

//------------------------------------------------------------------------------
void cUnitsData::initializeClanUnitData()
{
	if (initializedClanUnitData == true) return;

	cClanData& clanData = cClanData::instance();
	clanUnitDataVehicles.resize (clanData.getNrClans());
	clanUnitDataBuildings.resize (clanData.getNrClans());
	for (int i = 0; i != clanData.getNrClans(); ++i)
	{
		const cClan* clan = clanData.getClan (i);
		if (clan == 0)
			continue;

		vector<sUnitData>& clanListVehicles = clanUnitDataVehicles[i];

		// make a copy of the vehicle's stats
		clanListVehicles = svehicles;
		for (size_t j = 0; j != svehicles.size(); ++j)
		{
			const sUnitData& curVehicle = svehicles[j];
			const cClanUnitStat* changedStat = clan->getUnitStat (curVehicle.ID);

			if (changedStat == NULL) continue;

			sUnitData& clanVehicle = clanListVehicles[j];
			if (changedStat->hasModification ("Damage"))
				clanVehicle.setDamage(changedStat->getModificationValue ("Damage"));
			if (changedStat->hasModification ("Range"))
				clanVehicle.setRange(changedStat->getModificationValue ("Range"));
			if (changedStat->hasModification ("Armor"))
				clanVehicle.setArmor(changedStat->getModificationValue ("Armor"));
			if (changedStat->hasModification ("Hitpoints"))
				clanVehicle.hitpointsMax = changedStat->getModificationValue ("Hitpoints");
			if (changedStat->hasModification ("Scan"))
				clanVehicle.setScan(changedStat->getModificationValue ("Scan"));
			if (changedStat->hasModification ("Speed"))
				clanVehicle.speedMax = changedStat->getModificationValue ("Speed") * 4;
			if (changedStat->hasModification ("Built_Costs"))
				clanVehicle.buildCosts = changedStat->getModificationValue ("Built_Costs");
		}

		vector<sUnitData>& clanListBuildings = clanUnitDataBuildings[i];
		// make a copy of the building's stats
		clanListBuildings = sbuildings;
		for (size_t j = 0; j != sbuildings.size(); ++j)
		{
			const sUnitData& curBuilding = sbuildings[j];
			const cClanUnitStat* changedStat = clan->getUnitStat (curBuilding.ID);
			if (changedStat == NULL) continue;
			sUnitData& clanBuilding = clanListBuildings[j];
			if (changedStat->hasModification ("Damage"))
				clanBuilding.setDamage(changedStat->getModificationValue ("Damage"));
			if (changedStat->hasModification ("Range"))
				clanBuilding.setRange(changedStat->getModificationValue ("Range"));
			if (changedStat->hasModification ("Armor"))
				clanBuilding.setArmor(changedStat->getModificationValue ("Armor"));
			if (changedStat->hasModification ("Hitpoints"))
				clanBuilding.hitpointsMax = changedStat->getModificationValue ("Hitpoints");
			if (changedStat->hasModification ("Scan"))
				clanBuilding.setScan(changedStat->getModificationValue ("Scan"));
			if (changedStat->hasModification ("Speed"))
				clanBuilding.speedMax = changedStat->getModificationValue ("Speed") * 4;
			if (changedStat->hasModification ("Built_Costs"))
				clanBuilding.buildCosts = changedStat->getModificationValue ("Built_Costs");
		}
	}

	initializedClanUnitData = true;
}

//------------------------------------------------------------------------------
void cUnitsData::scaleSurfaces (float zoom)
{
	// Vehicles:
	for (unsigned int i = 0; i < getNrVehicles(); ++i)
	{
		vehicleUIs[i].scaleSurfaces (zoom);
	}
	// Buildings:
	for (unsigned int i = 0; i < getNrBuildings(); ++i)
	{
		buildingUIs[i].scaleSurfaces (zoom);
	}

	if (dirt_small_org != nullptr && dirt_small != nullptr) scaleSurface (dirt_small_org.get (), dirt_small.get (), (int)(dirt_small_org->w * zoom), (int)(dirt_small_org->h * zoom));
	if (dirt_small_shw_org != nullptr && dirt_small_shw != nullptr) scaleSurface (dirt_small_shw_org.get (), dirt_small_shw.get (), (int)(dirt_small_shw_org->w * zoom), (int)(dirt_small_shw_org->h * zoom));
	if (dirt_big_org != nullptr && dirt_big != nullptr) scaleSurface (dirt_big_org.get (), dirt_big.get (), (int)(dirt_big_org->w * zoom), (int)(dirt_big_org->h * zoom));
	if (dirt_big_shw_org != nullptr && dirt_big_shw != nullptr) scaleSurface (dirt_big_shw_org.get (), dirt_big_shw.get (), (int)(dirt_big_shw_org->w * zoom), (int)(dirt_big_shw_org->h * zoom));
}

//------------------------------------------------------------------------------
sFreezeModes::sFreezeModes() :
	waitForOthers (false),
	waitForServer (false),
	waitForReconnect (false),
	waitForTurnEnd (false),
	pause (false),
	waitForPlayer (false),
	playerNumber (-1)
{}

void sFreezeModes::enable (eFreezeMode mode, int playerNumber_)
{
	switch (mode)
	{
		case FREEZE_WAIT_FOR_SERVER: waitForServer = true; break;
		case FREEZE_WAIT_FOR_OTHERS: waitForOthers = true; break;
		case FREEZE_PAUSE: pause = true; break;
		case FREEZE_WAIT_FOR_RECONNECT: waitForReconnect = true; break;
		case FREEZE_WAIT_FOR_TURNEND: waitForTurnEnd = true; break;
		case FREEZE_WAIT_FOR_PLAYER: waitForPlayer = true; break;
	}

	if (playerNumber_ != -1)
		playerNumber = playerNumber_;
}

void sFreezeModes::disable (eFreezeMode mode)
{
	switch (mode)
	{
		case FREEZE_WAIT_FOR_SERVER: waitForServer = false; break;
		case FREEZE_WAIT_FOR_OTHERS: waitForOthers = false; break;
		case FREEZE_PAUSE: pause = false; break;
		case FREEZE_WAIT_FOR_RECONNECT: waitForReconnect = false; break;
		case FREEZE_WAIT_FOR_TURNEND: waitForTurnEnd = false; break;
		case FREEZE_WAIT_FOR_PLAYER: waitForPlayer = false; break;
	}
}

bool sFreezeModes::isFreezed() const
{
	return waitForServer | waitForOthers | pause |
		   waitForReconnect | waitForTurnEnd | waitForPlayer;
}

bool sFreezeModes::isEnable (eFreezeMode mode) const
{
	switch (mode)
	{
		case FREEZE_WAIT_FOR_SERVER: return waitForServer;
		case FREEZE_WAIT_FOR_OTHERS: return waitForOthers;
		case FREEZE_PAUSE: return pause;
		case FREEZE_WAIT_FOR_RECONNECT: return waitForReconnect;
		case FREEZE_WAIT_FOR_TURNEND: return waitForTurnEnd;
		case FREEZE_WAIT_FOR_PLAYER: return waitForPlayer;
	}
	assert (0); // Incorrect parameter
	return false;
}