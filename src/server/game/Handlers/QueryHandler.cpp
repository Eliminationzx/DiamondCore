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

#include "Common.h"
#include "Language.h"
#include "DatabaseEnv.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Opcodes.h"
#include "Log.h"
#include "World.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "UpdateMask.h"
#include "NPCHandler.h"
#include "Pet.h"
#include "MapManager.h"

void WorldSession::SendNameQueryOpcode(uint64 guid)
{
	GlobalPlayerData const* playerData = sWorld->GetGlobalPlayerData(GUID_LOPART(guid));

    WorldPacket data(SMSG_NAME_QUERY_RESPONSE, (8+1+1+1+1+1+10));
    data.appendPackGUID(guid);
    if (!playerData)
    {
        data << uint8(1);                           // name unknown
        SendPacket(&data);
        return;
    }

    data << uint8(0);                               // name known
    data << playerData->name;                       // played name
    data << uint8(0);                               // realm name - only set for cross realm interaction (such as Battlegrounds)
    data << uint8(playerData->race);
    data << uint8(playerData->gender);
    data << uint8(playerData->playerClass);

    // pussywizard: optimization
    /*Player* player = ObjectAccessor::FindPlayerInOrOutOfWorld(guid);
    if (DeclinedName const* names = (player ? player->GetDeclinedNames() : NULL))
    {
        data << uint8(1);                           // Name is declined
        for (uint8 i = 0; i < MAX_DECLINED_NAME_CASES; ++i)
            data << names->name[i];
    }
    else*/
        data << uint8(0);                           // Name is not declined

    SendPacket(&data);
}

void WorldSession::HandleNameQueryOpcode(WorldPacket& recvData)
{
    uint64 guid;
    recvData >> guid;

    // This is disable by default to prevent lots of console spam
    // sLog->outString("HandleNameQueryOpcode %u", guid);

    SendNameQueryOpcode(guid);
}

void WorldSession::HandleQueryTimeOpcode(WorldPacket & /*recvData*/)
{
    SendQueryTimeResponse();
}

void WorldSession::SendQueryTimeResponse()
{
    WorldPacket data(SMSG_QUERY_TIME_RESPONSE, 4+4);
    data << uint32(time(NULL));
    data << uint32(sWorld->GetNextDailyQuestsResetTime() - time(NULL));
    SendPacket(&data);
}

/// Only _static_ data is sent in this packet !!!
void WorldSession::HandleCreatureQueryOpcode(WorldPacket & recvData)
{
    uint32 entry;
    recvData >> entry;
    uint64 guid;
    recvData >> guid;

    CreatureTemplate const* ci = sObjectMgr->GetCreatureTemplate(entry);
    if (ci)
        SendPacket(&ci->queryData);
    else
    {
        ;//sLog->outDebug(LOG_FILTER_NETWORKIO, "WORLD: CMSG_CREATURE_QUERY - NO CREATURE INFO! (GUID: %u, ENTRY: %u)",
        //    GUID_LOPART(guid), entry);
        WorldPacket data(SMSG_CREATURE_QUERY_RESPONSE, 4);
        data << uint32(entry | 0x80000000);
        SendPacket(&data);
        ;//sLog->outDebug(LOG_FILTER_NETWORKIO, "WORLD: Sent SMSG_CREATURE_QUERY_RESPONSE");
    }
}

/// Only _static_ data is sent in this packet !!!
void WorldSession::HandleGameObjectQueryOpcode(WorldPacket & recvData)
{
    uint32 entry;
    recvData >> entry;
    uint64 guid;
    recvData >> guid;

    const GameObjectTemplate* info = sObjectMgr->GetGameObjectTemplate(entry);
    if (info)
    {
        ;//sLog->outDebug(LOG_FILTER_NETWORKIO, "WORLD: CMSG_GAMEOBJECT_QUERY '%s' - Entry: %u. ", info->name.c_str(), entry);
        WorldPacket data (SMSG_GAMEOBJECT_QUERY_RESPONSE, 150);
        data << uint32(entry);
        data << uint32(info->type);
        data << uint32(info->displayId);
        data << info->name;
        data << uint8(0) << uint8(0) << uint8(0);           // name2, name3, name4
        data << info->IconName;                                   // 2.0.3, string. Icon name to use instead of default icon for go's (ex: "Attack" makes sword)
        data << info->castBarCaption;                             // 2.0.3, string. Text will appear in Cast Bar when using GO (ex: "Collecting")
        data << info->unk1;                                 // 2.0.3, string
        data.append(info->raw.data, MAX_GAMEOBJECT_DATA);
        data << float(info->size);                          // go size
        for (uint32 i = 0; i < MAX_GAMEOBJECT_QUEST_ITEMS; ++i)
            data << uint32(info->questItems[i]);              // itemId[6], quest drop
        SendPacket(&data);
        ;//sLog->outDebug(LOG_FILTER_NETWORKIO, "WORLD: Sent SMSG_GAMEOBJECT_QUERY_RESPONSE");
    }
    else
    {
        ;//sLog->outDebug(LOG_FILTER_NETWORKIO, "WORLD: CMSG_GAMEOBJECT_QUERY - Missing gameobject info for (GUID: %u, ENTRY: %u)",
        //    GUID_LOPART(guid), entry);
        WorldPacket data (SMSG_GAMEOBJECT_QUERY_RESPONSE, 4);
        data << uint32(entry | 0x80000000);
        SendPacket(&data);
        ;//sLog->outDebug(LOG_FILTER_NETWORKIO, "WORLD: Sent SMSG_GAMEOBJECT_QUERY_RESPONSE");
    }
}

void WorldSession::HandleCorpseQueryOpcode(WorldPacket & /*recvData*/)
{
    ;//sLog->outDebug(LOG_FILTER_NETWORKIO, "WORLD: Received MSG_CORPSE_QUERY");

    Corpse* corpse = GetPlayer()->GetCorpse();

    if (!corpse)
    {
        WorldPacket data(MSG_CORPSE_QUERY, 1);
        data << uint8(0);                                   // corpse not found
        SendPacket(&data);
        return;
    }

    uint32 mapid = corpse->GetMapId();
    float x = corpse->GetPositionX();
    float y = corpse->GetPositionY();
    float z = corpse->GetPositionZ();
    uint32 corpsemapid = mapid;

    // if corpse at different map
    if (mapid != _player->GetMapId())
    {
        // search entrance map for proper show entrance
        if (MapEntry const* corpseMapEntry = sMapStore.LookupEntry(mapid))
        {
            if (corpseMapEntry->IsDungeon() && corpseMapEntry->entrance_map >= 0)
            {
                // if corpse map have entrance
                if (Map const* entranceMap = sMapMgr->CreateBaseMap(corpseMapEntry->entrance_map))
                {
                    mapid = corpseMapEntry->entrance_map;
                    x = corpseMapEntry->entrance_x;
                    y = corpseMapEntry->entrance_y;
                    z = entranceMap->GetHeight(GetPlayer()->GetPhaseMask(), x, y, MAX_HEIGHT);
                }
            }
        }
    }

    WorldPacket data(MSG_CORPSE_QUERY, 1+(6*4));
    data << uint8(1);                                       // corpse found
    data << int32(mapid);
    data << float(x);
    data << float(y);
    data << float(z);
    data << int32(corpsemapid);
    data << uint32(0);                                      // unknown
    SendPacket(&data);
}

void WorldSession::HandleNpcTextQueryOpcode(WorldPacket & recvData)
{
    uint32 textID;
    uint64 guid;

    recvData >> textID;
    ;//sLog->outDebug(LOG_FILTER_NETWORKIO, "WORLD: CMSG_NPC_TEXT_QUERY ID '%u'", textID);

    recvData >> guid;

    GossipText const* pGossip = sObjectMgr->GetGossipText(textID);

    WorldPacket data(SMSG_NPC_TEXT_UPDATE, 100);          // guess size
    data << textID;

    if (!pGossip)
    {
        for (uint32 i = 0; i < MAX_GOSSIP_TEXT_OPTIONS; ++i)
        {
            data << float(0);
            data << "Greetings $N";
            data << "Greetings $N";
            data << uint32(0);
            data << uint32(0);
            data << uint32(0);
            data << uint32(0);
            data << uint32(0);
            data << uint32(0);
            data << uint32(0);
        }
    }
    else
    {
        for (int i = 0; i < MAX_GOSSIP_TEXT_OPTIONS; ++i)
        {
            data << pGossip->Options[i].Probability;

            if (pGossip->Options[i].Text_0.empty())
                data << pGossip->Options[i].Text_1;
            else
                data << pGossip->Options[i].Text_0;

            if (pGossip->Options[i].Text_1.empty())
                data << pGossip->Options[i].Text_0;
            else
                data << pGossip->Options[i].Text_1;

            data << pGossip->Options[i].Language;

            for (int j = 0; j < MAX_GOSSIP_TEXT_EMOTES; ++j)
            {
                data << pGossip->Options[i].Emotes[j]._Delay;
                data << pGossip->Options[i].Emotes[j]._Emote;
            }
        }
    }

    SendPacket(&data);

    ;//sLog->outDebug(LOG_FILTER_NETWORKIO, "WORLD: Sent SMSG_NPC_TEXT_UPDATE");
}

/// Only _static_ data is sent in this packet !!!
void WorldSession::HandlePageTextQueryOpcode(WorldPacket & recvData)
{
    ;//sLog->outDebug(LOG_FILTER_NETWORKIO, "WORLD: Received CMSG_PAGE_TEXT_QUERY");

    uint32 pageID;
    recvData >> pageID;
    recvData.read_skip<uint64>();                          // guid

    while (pageID)
    {
        PageText const* pageText = sObjectMgr->GetPageText(pageID);
                                                            // guess size
        WorldPacket data(SMSG_PAGE_TEXT_QUERY_RESPONSE, 50);
        data << pageID;

        if (!pageText)
        {
            data << "Item page missing.";
            data << uint32(0);
            pageID = 0;
        }
        else
        {
            data << pageText->Text;
            data << uint32(pageText->NextPage);
            pageID = pageText->NextPage;
        }
        SendPacket(&data);

        ;//sLog->outDebug(LOG_FILTER_NETWORKIO, "WORLD: Sent SMSG_PAGE_TEXT_QUERY_RESPONSE");
    }
}

void WorldSession::HandleCorpseMapPositionQuery(WorldPacket & recvData)
{
    ;//sLog->outDebug(LOG_FILTER_NETWORKIO, "WORLD: Recv CMSG_CORPSE_MAP_POSITION_QUERY");

    uint32 unk;
    recvData >> unk;

    WorldPacket data(SMSG_CORPSE_MAP_POSITION_QUERY_RESPONSE, 4+4+4+4);
    data << float(0);
    data << float(0);
    data << float(0);
    data << float(0);
    SendPacket(&data);
}

void WorldSession::HandleQuestPOIQuery(WorldPacket& recvData)
{
    uint32 count;
    recvData >> count; // quest count, max=25

    if (count > MAX_QUEST_LOG_SIZE)
    {
        recvData.rfinish();
        return;
    }

    // Read quest ids and add the in a unordered_set so we don't send POIs for the same quest multiple times
	UNORDERED_SET<uint32> questIds;
    for (uint32 i = 0; i < count; ++i)
        questIds.insert(recvData.read<uint32>()); // quest id

    WorldPacket data(SMSG_QUEST_POI_QUERY_RESPONSE, 4 + (4 + 4)*questIds.size());
    data << uint32(questIds.size()); // count

	for (UNORDERED_SET<uint32>::const_iterator itr = questIds.begin(); itr != questIds.end(); ++itr)
    {
        uint32 questId = *itr;
        bool questOk = false;

        uint16 questSlot = _player->FindQuestSlot(questId);

        if (questSlot != MAX_QUEST_LOG_SIZE)
            questOk =_player->GetQuestSlotQuestId(questSlot) == questId;

        if (questOk)
        {
            QuestPOIVector const* POI = sObjectMgr->GetQuestPOIVector(questId);

            if (POI)
            {
                data << uint32(questId); // quest ID
                data << uint32(POI->size()); // POI count

                for (QuestPOIVector::const_iterator itr = POI->begin(); itr != POI->end(); ++itr)
                {
                    data << uint32(itr->Id);                // POI index
                    data << int32(itr->ObjectiveIndex);     // objective index
                    data << uint32(itr->MapId);             // mapid
                    data << uint32(itr->AreaId);            // areaid
                    data << uint32(itr->FloorId);           // floorid
                    data << uint32(itr->Unk3);              // unknown
                    data << uint32(itr->Unk4);              // unknown
                    data << uint32(itr->points.size());     // POI points count

                    for (std::vector<QuestPOIPoint>::const_iterator itr2 = itr->points.begin(); itr2 != itr->points.end(); ++itr2)
                    {
                        data << int32(itr2->x); // POI point x
                        data << int32(itr2->y); // POI point y
                    }
                }
            }
            else
            {
                data << uint32(questId); // quest ID
                data << uint32(0); // POI count
            }
        }
        else
        {
            data << uint32(questId); // quest ID
            data << uint32(0); // POI count
        }
    }

    SendPacket(&data);
}
