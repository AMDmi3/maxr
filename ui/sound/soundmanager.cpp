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

#include "ui/sound/soundmanager.h"
#include "ui/sound/effects/soundeffect.h"

#include "output/sound/sounddevice.h"
#include "output/sound/soundchannel.h"

//--------------------------------------------------------------------------
cSoundManager::cSoundManager () :
	muted (false),
	listenerPosition (0, 0),
	maxListeningDistance (20)
{}

//--------------------------------------------------------------------------
void cSoundManager::mute ()
{
	muted = true;
	stopAllSounds ();
}

//--------------------------------------------------------------------------
void cSoundManager::unmute ()
{
	muted = false;
}

//--------------------------------------------------------------------------
void cSoundManager::setListenerPosition (const cPosition& listenerPosition_)
{
	listenerPosition = listenerPosition_;
}

//--------------------------------------------------------------------------
void cSoundManager::setMaxListeningDistance (int distance)
{
	maxListeningDistance = distance;
}

//--------------------------------------------------------------------------
void cSoundManager::playSound (std::shared_ptr<cSoundEffect> sound, bool loop)
{
	if (!sound) return;

	if (muted) return;

	cMutex::Lock playingSoundsLock (playingSoundsMutex);

	const unsigned int currentGameTime = 0; // TODO: get game time

	const auto soundConflictHandlingType = sound->getSoundConflictHandlingType ();

	if (soundConflictHandlingType != eSoundConflictHandlingType::PlayAnyway)
	{
		// count conflicts and erase sounds that are no longer active
		unsigned int conflicts = 0;
		for (auto i = playingSounds.begin (); i != playingSounds.end (); /*erase in loop*/)
		{
			if (!i->active)
			{
				i = playingSounds.erase (i);
			}
			else
			{
				if (i->startGameTime == currentGameTime && sound->isInConflict (*i->sound))
				{
					++conflicts;
				}
				++i;
			}
		}

		if (conflicts > sound->getMaxConcurrentConflictedCount ())
		{
			switch (soundConflictHandlingType)
			{
			case eSoundConflictHandlingType::DiscardNew:
				return;
			case eSoundConflictHandlingType::StopOld:
				{
					// stop oldest sounds that are in conflict (list is sorted by start game time)
					while (conflicts > sound->getMaxConcurrentConflictedCount ())
					{
						for (auto i = playingSounds.begin (); i != playingSounds.end (); /*erase in loop*/)
						{
							const auto playingSound = i->sound; // copy so that we get an owning pointer in this scope
							const auto& soundGameTime = i->startGameTime;

							if (soundGameTime == currentGameTime && sound->isInConflict (*playingSound))
							{
								// first remove from list and than stop by method to avoid conflicts with "finished" callback.
								i = playingSounds.erase (i);
								playingSound->stop ();
								--conflicts;
							}
							else
							{
								++i;
							}
						}
					}
				}
				break;
			}
		}
	}

	// start new sound
	auto& channel = getChannelForSound (*sound);

	sound->play (channel, loop);

	playingSounds.emplace_back (std::move (sound), currentGameTime, true);
	auto& soundRef = *playingSounds.back ().sound;
	signalConnectionManager.connect (soundRef.stopped, std::bind (&cSoundManager::finishedSound, this, std::ref (soundRef)));

	// Sound list is always sorted by start game time.
	// Push order is kept by stable sort to handle sounds that are played at same game time
	std::stable_sort (playingSounds.begin (), playingSounds.end ());
}

//--------------------------------------------------------------------------
void cSoundManager::stopAllSounds ()
{
	cMutex::Lock playingSoundsLock(playingSoundsMutex);
	for (auto i = playingSounds.begin (); i != playingSounds.end (); ++i)
	{
		i->sound->stop ();
	}

	playingSounds.clear ();
}

////--------------------------------------------------------------------------
//void cSoundManager::playSound (const cSoundChunk& sound, const cPosition& soundPosition, bool disallowDuplicate)
//{
//	if (muted) return;
//
//	if (disallowDuplicate && isAlreadyPlayingSound (sound)) return;
//
//	auto& channel = cSoundDevice::getInstance ().getFreeSoundEffectChannel ();
//	channel.play (sound);
//
//	//const cPosition offset = soundPosition - listenerPosition;
//	//const auto soundDistance = std::max<char> (offset.l2Norm () / maxListeningDistance * 255, 255);
//	//const auto temp = (offset.x () == 0 && offset.x () == 0) ? 0 : std::atan2 (offset.y (), offset.x ());
//	//const auto soundAngle = (temp > 0 ? temp : (2*M_PI + temp)) * 360 / (2*M_PI) + 90;
//
//	//channel.setPosition (soundAngle, soundDistance);
//}

//--------------------------------------------------------------------------
cSoundChannel& cSoundManager::getChannelForSound (cSoundEffect& sound)
{
	switch (sound.getChannelType())
	{
	default:
	case eSoundChannelType::General:
		return cSoundDevice::getInstance ().getFreeSoundEffectChannel ();
	case eSoundChannelType::Voice:
		return cSoundDevice::getInstance ().getFreeVoiceChannel ();
	}
}

//--------------------------------------------------------------------------
void cSoundManager::finishedSound (cSoundEffect& sound)
{
	cMutex::Lock playingSoundsLock (playingSoundsMutex);

	auto iter = std::find_if (playingSounds.begin (), playingSounds.end (), [&sound](const sStoredSound& entry){ return entry.sound.get () == &sound; });

	if (iter != playingSounds.end ())
	{
		iter->active = false;
	}
}