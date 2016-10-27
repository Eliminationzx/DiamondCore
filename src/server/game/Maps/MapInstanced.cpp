/*
 * Copyright (C) 
 * Copyright (C) 
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "MapInstanced.h"
#include "ObjectMgr.h"
#include "MapManager.h"
#include "Battleground.h"
#include "VMapFactory.h"
#include "MMapFactory.h"
#include "InstanceSaveMgr.h"
#include "World.h"
#include "Group.h"
#include "Player.h"

MapInstanced::MapInstanced(uint32 id) : Map(id, 0, DUNGEON_DIFFICULTY_NORMAL)
{
    // initialize instanced maps list
    m_InstancedMaps.clear();
}

void MapInstanced::InitVisibilityDistance()
{ 
    if (m_InstancedMaps.empty())
        return;
    //initialize visibility distances for all instance copies
    for (InstancedMaps::iterator i = m_InstancedMaps.begin(); i != m_InstancedMaps.end(); ++i)
    {
        (*i).second->InitVisibilityDistance();
    }
}

void MapInstanced::Update(const uint32 t, const uint32 s_diff, bool /*thread*/)
{
    // take care of loaded GridMaps (when unused, unload it!)
    Map::Update(t, s_diff, false);

    // update the instanced maps
    InstancedMaps::iterator i = m_InstancedMaps.begin();

    while (i != m_InstancedMaps.end())
    {
        if (i->second->CanUnload(t))
        {
            if (!DestroyInstance(i))                             // iterator incremented
            {
                //m_unloadTimer
            }
        }
        else
        {
            // update only here, because it may schedule some bad things before delete
            if (sMapMgr->GetMapUpdater()->activated())
                sMapMgr->GetMapUpdater()->schedule_update(*i->second, t, s_diff);
            else
                i->second->Update(t, s_diff);
            ++i;
        }
    }
}

void MapInstanced::DelayedUpdate(const uint32 diff)
{ 
    for (InstancedMaps::iterator i = m_InstancedMaps.begin(); i != m_InstancedMaps.end(); ++i)
        i->second->DelayedUpdate(diff);

    Map::DelayedUpdate(diff); // this may be removed
}

/*
void MapInstanced::RelocationNotify()
{ 
    for (InstancedMaps::iterator i = m_InstancedMaps.begin(); i != m_InstancedMaps.end(); ++i)
        i->second->RelocationNotify();
}
*/

void MapInstanced::UnloadAll()
{ 
    // Unload instanced maps
    for (InstancedMaps::iterator i = m_InstancedMaps.begin(); i != m_InstancedMaps.end(); ++i)
        i->second->UnloadAll();

    // Delete the maps only after everything is unloaded to prevent crashes
    for (InstancedMaps::iterator i = m_InstancedMaps.begin(); i != m_InstancedMaps.end(); ++i)
        delete i->second;

    m_InstancedMaps.clear();

    // Unload own grids (just dummy(placeholder) grids, neccesary to unload GridMaps!)
    Map::UnloadAll();
}

/*
- return the right instance for the object, based on its InstanceId
- create the instance if it's not created already
- the player is not actually added to the instance (only in InstanceMap::Add)
*/
Map* MapInstanced::CreateInstanceForPlayer(const uint32 mapId, Player* player)
{ 
    if (GetId() != mapId || !player)
        return NULL;

    Map* map = NULL;

    if (IsBattlegroundOrArena())
    {
        // instantiate or find existing bg map for player
        // the instance id is set in battlegroundid
        uint32 newInstanceId = player->GetBattlegroundId();
        if (!newInstanceId)
            return NULL;

        map = sMapMgr->FindMap(mapId, newInstanceId);
        if (!map)
        {
            Battleground* bg = player->GetBattleground(true);
            if (bg && bg->GetStatus() < STATUS_WAIT_LEAVE)
                map = CreateBattleground(newInstanceId, bg);
            else
            {
                player->TeleportToEntryPoint();
                return NULL;
            }
        }
    }
    else
    {
        Difficulty realdiff = player->GetDifficulty(IsRaid());
        uint32 destInstId = sInstanceSaveMgr->PlayerGetDestinationInstanceId(player, GetId(), realdiff);

        if (destInstId)
        {
			InstanceSave* pSave = sInstanceSaveMgr->GetInstanceSave(destInstId);
			ASSERT(pSave); // pussywizard: must exist

            map = FindInstanceMap(destInstId);
            if (!map)
                map = CreateInstance(destInstId, pSave, realdiff);
			else if ((mapId == 631 || mapId == 724) && !map->HavePlayers() && map->GetDifficulty() != realdiff)
			{
				if (player->isBeingLoaded()) // pussywizard: crashfix (assert(passengers.empty) fail in ~transport), could be added to a transport during loading from db
					return NULL;

				if (!map->AllTransportsEmpty())
					map->AllTransportsRemovePassengers(); // pussywizard: gameobjects / summons (assert(passengers.empty) fail in ~transport)

				for (InstancedMaps::iterator i = m_InstancedMaps.begin(); i != m_InstancedMaps.end(); ++i)
					if (i->first == destInstId)
					{
						DestroyInstance(i);
						map = CreateInstance(destInstId, pSave, realdiff);
						break;
					}
			}
        }
        else
        {
            uint32 newInstanceId = sMapMgr->GenerateInstanceId();
			ASSERT(!FindInstanceMap(newInstanceId)); // pussywizard: instance with new id can't exist
            Difficulty diff = player->GetGroup() ? player->GetGroup()->GetDifficulty(IsRaid()) : player->GetDifficulty(IsRaid());
            map = CreateInstance(newInstanceId, NULL, diff);
        }
    }

    return map;
}

InstanceMap* MapInstanced::CreateInstance(uint32 InstanceId, InstanceSave* save, Difficulty difficulty)
{ 
    // load/create a map
    TRINITY_GUARD(ACE_Thread_Mutex, Lock);

    // make sure we have a valid map id
    const MapEntry* entry = sMapStore.LookupEntry(GetId());
    if (!entry)
    {
        sLog->outError("CreateInstance: no entry for map %d", GetId());
        ASSERT(false);
    }
    const InstanceTemplate* iTemplate = sObjectMgr->GetInstanceTemplate(GetId());
    if (!iTemplate)
    {
        sLog->outError("CreateInstance: no instance template for map %d", GetId());
        ASSERT(false);
    }

    // some instances only have one difficulty
    GetDownscaledMapDifficultyData(GetId(), difficulty);

    ;//sLog->outDebug(LOG_FILTER_MAPS, "MapInstanced::CreateInstance: %s map instance %d for %d created with difficulty %s", save?"":"new ", InstanceId, GetId(), difficulty?"heroic":"normal");

    InstanceMap* map = new InstanceMap(GetId(), InstanceId, difficulty, this);
    ASSERT(map->IsDungeon());

    map->LoadRespawnTimes();

	if (save)
		map->CreateInstanceScript(true, save->GetInstanceData(), save->GetCompletedEncounterMask());
	else
		map->CreateInstanceScript(false, "", 0);

	if (!save) // this is for sure a dungeon (assert above), no need to check here
		sInstanceSaveMgr->AddInstanceSave(GetId(), InstanceId, difficulty);

    m_InstancedMaps[InstanceId] = map;
    return map;
}

BattlegroundMap* MapInstanced::CreateBattleground(uint32 InstanceId, Battleground* bg)
{ 
    // load/create a map
    TRINITY_GUARD(ACE_Thread_Mutex, Lock);

    ;//sLog->outDebug(LOG_FILTER_MAPS, "MapInstanced::CreateBattleground: map bg %d for %d created.", InstanceId, GetId());

    PvPDifficultyEntry const* bracketEntry = GetBattlegroundBracketByLevel(bg->GetMapId(), bg->GetMinLevel());

    uint8 spawnMode;

    if (bracketEntry)
        spawnMode = bracketEntry->difficulty;
    else
        spawnMode = REGULAR_DIFFICULTY;

    BattlegroundMap* map = new BattlegroundMap(GetId(), InstanceId, this, spawnMode);
    ASSERT(map->IsBattlegroundOrArena());
    map->SetBG(bg);
    bg->SetBgMap(map);

    m_InstancedMaps[InstanceId] = map;
    return map;
}

// increments the iterator after erase
bool MapInstanced::DestroyInstance(InstancedMaps::iterator &itr)
{ 
    itr->second->RemoveAllPlayers();
    if (itr->second->HavePlayers())
    {
        ++itr;
        return false;
    }

    itr->second->UnloadAll();

    // Free up the instance id and allow it to be reused for bgs and arenas (other instances are handled in the InstanceSaveMgr)
    //if (itr->second->IsBattlegroundOrArena())
    //    sMapMgr->FreeInstanceId(itr->second->GetInstanceId());

    // erase map
    delete itr->second;
    m_InstancedMaps.erase(itr++);

    return true;
}

bool MapInstanced::CanEnter(Player* /*player*/, bool /*loginCheck*/)
{ 
    //ASSERT(false);
    return true;
}