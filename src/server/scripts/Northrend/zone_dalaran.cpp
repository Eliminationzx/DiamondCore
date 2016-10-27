/*
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

/* Script Data Start
SDName: Dalaran
SDAuthor: WarHead, MaXiMiUS
SD%Complete: 99%
SDComment: For what is 63990+63991? Same function but don't work correct...
SDCategory: Dalaran
Script Data End */

#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "Player.h"
#include "WorldSession.h"
#include "ScriptedEscortAI.h"

// Ours
class npc_steam_powered_auctioneer : public CreatureScript
{
	public:
		npc_steam_powered_auctioneer() : CreatureScript("npc_steam_powered_auctioneer") { }

		struct npc_steam_powered_auctioneerAI : public ScriptedAI
		{
			npc_steam_powered_auctioneerAI(Creature* creature) : ScriptedAI(creature) {}

			bool CanBeSeen(Player const* player)
			{
				if (player->GetTeamId() == TEAM_ALLIANCE)
					return me->GetEntry() == 35594;
				else
					return me->GetEntry() == 35607;
			}
		};

		CreatureAI* GetAI(Creature* creature) const
		{
			return new npc_steam_powered_auctioneerAI(creature);
		}
};

class npc_mei_francis_mount : public CreatureScript
{
	public:
		npc_mei_francis_mount() : CreatureScript("npc_mei_francis_mount") { }

		struct npc_mei_francis_mountAI : public ScriptedAI
		{
			npc_mei_francis_mountAI(Creature* creature) : ScriptedAI(creature) {}

			bool CanBeSeen(Player const* player)
			{
				if (player->GetTeamId() == TEAM_ALLIANCE)
					return me->GetEntry() == 32206 || me->GetEntry() == 32335 || me->GetEntry() == 31851;
				else
					return me->GetEntry() == 32207 || me->GetEntry() == 32336 || me->GetEntry() == 31852;
			}
		};

		CreatureAI* GetAI(Creature* creature) const
		{
			return new npc_mei_francis_mountAI(creature);
		}
};

/******************************************
***** Shady Gnome - A Suitable Disguise **
****************************************/

enum DisguiseEvent
{
    ACTION_SHANDY_INTRO			= 0,
    ACTION_WATER				= 1,
    ACTION_SHIRTS				= 2,
    ACTION_PANTS				= 3,
    ACTION_UNMENTIONABLES		= 4,
    
    EVENT_INTRO_DH1				= 1,
    EVENT_INTRO_DH2				= 2,
    EVENT_INTRO_DH3				= 3,
    EVENT_INTRO_DH4				= 4,
    EVENT_INTRO_DH5				= 5,
    EVENT_INTRO_DH6				= 6,
    EVENT_OUTRO_DH				= 7,

	SAY_SHANDY1					= 0,
	SAY_SHANDY2					= 1,
	SAY_SHANDY3					= 2,
	SAY_SHANDY_WATER			= 3, // shirts = 4, pants = 5, unmentionables = 6
	SAY_SHANDY4					= 7,
	SAY_SHANDY5					= 8,
	SAY_SHANDY6					= 9,
};

enum DisguiseMisc
{
	QUEST_SUITABLE_DISGUISE_A		= 20438,
	QUEST_SUITABLE_DISGUISE_H		= 24556,

	SPELL_EVOCATION_VISUAL			= 69659,

	NPC_AQUANOS_ENTRY				= 36851,
};

class npc_shandy_dalaran : public CreatureScript
{
public:
    npc_shandy_dalaran() : CreatureScript("npc_shandy_dalaran") { }

    struct npc_shandy_dalaranAI : public ScriptedAI
    {
        npc_shandy_dalaranAI(Creature* creature) : ScriptedAI(creature) { }

        void Reset()
        {
			_events.Reset();
			_aquanosGUID = 0;
        }  
        
        void SetData(uint32 type, uint32 data)
        {
            switch(type)
            {
                case ACTION_SHANDY_INTRO:
					if (Creature* aquanos = me->FindNearestCreature(NPC_AQUANOS_ENTRY, 30, true))
						_aquanosGUID = aquanos->GetGUID();

					_events.Reset();
					_lCount = 0;
					_lSource = 0;
					_canWash = false;
                    Talk(SAY_SHANDY1);
                    _events.ScheduleEvent(EVENT_INTRO_DH1, 5000);
					_events.ScheduleEvent(EVENT_OUTRO_DH, 10*MINUTE*IN_MILLISECONDS);
                    break;
				default:
                    if(_lSource == type && _canWash)
                    {
                        _canWash = false;
						_events.ScheduleEvent(EVENT_INTRO_DH2, type == ACTION_UNMENTIONABLES ? 4000 : 10000);
                        Talk(SAY_SHANDY2);
						if (Creature* aquanos = ObjectAccessor::GetCreature(*me, _aquanosGUID))
                            aquanos->CastSpell(aquanos, SPELL_EVOCATION_VISUAL, false);
                    }
                    break;
            }
        }

		void RollTask()
		{
			_lSource = urand(ACTION_SHIRTS, ACTION_UNMENTIONABLES);
			if (_lCount == 1 || _lCount == 4)
				_lSource = ACTION_WATER;

			Talk(SAY_SHANDY_WATER + _lSource - 1);
			_canWash = true;
		}
        
        void UpdateAI(uint32 diff)
        {
            _events.Update(diff);
            switch (_events.GetEvent())
            {
                case EVENT_INTRO_DH1:
                    Talk(SAY_SHANDY3);
                    _events.ScheduleEvent(EVENT_INTRO_DH2, 15000);
					_events.PopEvent();
                    break;
                case EVENT_INTRO_DH2:
					if (_lCount++ > 6)
						_events.ScheduleEvent(EVENT_INTRO_DH3, 6000);
					else
						RollTask();
                    
                    _events.PopEvent();
                    break;
                case EVENT_INTRO_DH3:
                    Talk(SAY_SHANDY4);
                    _events.ScheduleEvent(EVENT_INTRO_DH4, 20000);
					_events.PopEvent();
                    break;
                case EVENT_INTRO_DH4:
                    Talk(SAY_SHANDY5);             
                    _events.ScheduleEvent(EVENT_INTRO_DH5, 3000);
					_events.PopEvent();
                    break;
                case EVENT_INTRO_DH5:
                    me->SummonGameObject(201384, 5798.74f, 693.19f, 657.94f, 0.91f, 0, 0, 0, 0,90000000);
                    _events.ScheduleEvent(EVENT_INTRO_DH6, 1000);
					_events.PopEvent();
                    break;
                case EVENT_INTRO_DH6:
                    me->SetWalk(true);
                    me->GetMotionMaster()->MovePoint(0, 5797.55f, 691.97f, 657.94f);
                    _events.RescheduleEvent(EVENT_OUTRO_DH, 30000);
					_events.PopEvent();
                    break;
                case EVENT_OUTRO_DH:
                    me->GetMotionMaster()->MoveTargetedHome();
                    me->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);
					_events.Reset();
                    break;
            }
        }

		private:
			EventMap _events;
			uint64 _aquanosGUID;
			uint8 _lCount;
			uint32 _lSource;
			uint32 _resetTime;
	        
			bool _canWash;
    };
    
    bool OnGossipHello(Player* player, Creature* creature)
    {
        if (creature->IsQuestGiver())
            player->PrepareQuestMenu(creature->GetGUID());

        if (player->GetQuestStatus(QUEST_SUITABLE_DISGUISE_A) == QUEST_STATUS_INCOMPLETE || 
			player->GetQuestStatus(QUEST_SUITABLE_DISGUISE_H) == QUEST_STATUS_INCOMPLETE)
        {
            if(player->GetTeamId() == TEAM_ALLIANCE)
                player->ADD_GOSSIP_ITEM(0, "Arcanist Tybalin said you might be able to lend me a certain tabard.", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF);
            else
                player->ADD_GOSSIP_ITEM(0, "Magister Hathorel said you might be able to lend me a certain tabard.", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF);
        }

        player->SEND_GOSSIP_MENU(player->GetGossipTextId(creature), creature->GetGUID());
        return true;
    }
    
    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action)
    {
        switch (action)
        {
            case GOSSIP_ACTION_INFO_DEF:
                player->CLOSE_GOSSIP_MENU(); 
                creature->SetUInt32Value(UNIT_NPC_FLAGS, 0);
                creature->AI()->SetData(ACTION_SHANDY_INTRO, 0);
                break;
        }
        return true;
    }

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_shandy_dalaranAI(creature);
    }
};
enum ArchmageLandalockQuests
{
    QUEST_SARTHARION_MUST_DIE               = 24579,
    QUEST_ANUBREKHAN_MUST_DIE               = 24580,
    QUEST_NOTH_THE_PLAGUEBINGER_MUST_DIE    = 24581,
    QUEST_INSTRUCTOR_RAZUVIOUS_MUST_DIE     = 24582,
    QUEST_PATCHWERK_MUST_DIE                = 24583,
    QUEST_MALYGOS_MUST_DIE                  = 24584,
    QUEST_FLAME_LEVIATHAN_MUST_DIE          = 24585,
    QUEST_RAZORSCALE_MUST_DIE               = 24586,
    QUEST_IGNIS_THE_FURNACE_MASTER_MUST_DIE = 24587,
    QUEST_XT_002_DECONSTRUCTOR_MUST_DIE     = 24588,
    QUEST_LORD_JARAXXUS_MUST_DIE            = 24589,
    QUEST_LORD_MARROWGAR_MUST_DIE           = 24590
};

enum ArchmageLandalockImages
{
    NPC_SARTHARION_IMAGE                = 37849,
    NPC_ANUBREKHAN_IMAGE                = 37850,
    NPC_NOTH_THE_PLAGUEBINGER_IMAGE     = 37851,
    NPC_INSTRUCTOR_RAZUVIOUS_IMAGE      = 37853,
    NPC_PATCHWERK_IMAGE                 = 37854,
    NPC_MALYGOS_IMAGE                   = 37855,
    NPC_FLAME_LEVIATHAN_IMAGE           = 37856,
    NPC_RAZORSCALE_IMAGE                = 37858,
    NPC_IGNIS_THE_FURNACE_MASTER_IMAGE  = 37859,
    NPC_XT_002_DECONSTRUCTOR_IMAGE      = 37861,
    NPC_LORD_JARAXXUS_IMAGE             = 37862,
    NPC_LORD_MARROWGAR_IMAGE            = 37864
};

class npc_archmage_landalock : public CreatureScript
{
	public:
		npc_archmage_landalock() : CreatureScript("npc_archmage_landalock")
		{
		}

		CreatureAI* GetAI(Creature* creature) const
		{
			return new npc_archmage_landalockAI(creature);
		}

		struct npc_archmage_landalockAI : public ScriptedAI
		{
			npc_archmage_landalockAI(Creature* creature) : ScriptedAI(creature)
			{
				_switchImageTimer = MINUTE*IN_MILLISECONDS;
				_summonGUID = 0;
			}

			uint32 GetImageEntry(uint32 QuestId)
			{
				switch (QuestId)
				{
					case QUEST_SARTHARION_MUST_DIE:
						return NPC_SARTHARION_IMAGE;
					case QUEST_ANUBREKHAN_MUST_DIE:
						return NPC_ANUBREKHAN_IMAGE;
					case QUEST_NOTH_THE_PLAGUEBINGER_MUST_DIE:
						return NPC_NOTH_THE_PLAGUEBINGER_IMAGE;
					case QUEST_INSTRUCTOR_RAZUVIOUS_MUST_DIE:
						return NPC_INSTRUCTOR_RAZUVIOUS_IMAGE;
					case QUEST_PATCHWERK_MUST_DIE:
						return NPC_PATCHWERK_IMAGE;
					case QUEST_MALYGOS_MUST_DIE:
						return NPC_MALYGOS_IMAGE;
					case QUEST_FLAME_LEVIATHAN_MUST_DIE:
						return NPC_FLAME_LEVIATHAN_IMAGE;
					case QUEST_RAZORSCALE_MUST_DIE:
						return NPC_RAZORSCALE_IMAGE;
					case QUEST_IGNIS_THE_FURNACE_MASTER_MUST_DIE:
						return NPC_IGNIS_THE_FURNACE_MASTER_IMAGE;
					case QUEST_XT_002_DECONSTRUCTOR_MUST_DIE:
						return NPC_XT_002_DECONSTRUCTOR_IMAGE;
					case QUEST_LORD_JARAXXUS_MUST_DIE:
						return NPC_LORD_JARAXXUS_IMAGE;
					default: //case QUEST_LORD_MARROWGAR_MUST_DIE:
						return NPC_LORD_MARROWGAR_IMAGE;
				}
			}

			void JustSummoned(Creature* image)
			{
				// xinef: screams like a baby
				if (image->GetEntry() != NPC_ANUBREKHAN_IMAGE)
					image->SetUnitMovementFlags(MOVEMENTFLAG_RIGHT);
				_summonGUID = image->GetGUID();
			}

			void UpdateAI(uint32 diff)
			{
				ScriptedAI::UpdateAI(diff);

				_switchImageTimer += diff;
				if (_switchImageTimer > MINUTE*IN_MILLISECONDS)
				{
					_switchImageTimer = 0;
					QuestRelationBounds objectQR = sObjectMgr->GetCreatureQuestRelationBounds(me->GetEntry());
					for (QuestRelations::const_iterator i = objectQR.first; i != objectQR.second; ++i)
					{
						uint32 questId = i->second;
						Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
						if (!quest || !quest->IsWeekly())
							continue;

						uint32 newEntry = GetImageEntry(questId);
						if (GUID_ENPART(_summonGUID) != newEntry)
						{
							if (Creature* image = ObjectAccessor::GetCreature(*me, _summonGUID))
								image->DespawnOrUnsummon();

							float z = 653.622f;
							if (newEntry == NPC_MALYGOS_IMAGE || newEntry == NPC_RAZORSCALE_IMAGE || newEntry == NPC_SARTHARION_IMAGE)
								z += 3.0f;
							me->SummonCreature(newEntry, 5703.077f, 583.9757f, z, 3.926991f);
						}
					}
				}
			}
		private:
			uint32 _switchImageTimer;
			uint64 _summonGUID;
		};
};

// Theirs
/*******************************************************
 * npc_mageguard_dalaran
 *******************************************************/

enum Spells
{
    SPELL_TRESPASSER_A                     = 54028,
    SPELL_TRESPASSER_H                     = 54029,

    SPELL_SUNREAVER_DISGUISE_FEMALE        = 70973,
    SPELL_SUNREAVER_DISGUISE_MALE          = 70974,
    SPELL_SILVER_COVENANT_DISGUISE_FEMALE  = 70971,
    SPELL_SILVER_COVENANT_DISGUISE_MALE    = 70972,
};

enum NPCs // All outdoor guards are within 35.0f of these NPCs
{
    NPC_APPLEBOUGH_A                       = 29547,
    NPC_SWEETBERRY_H                       = 29715,
    NPC_SILVER_COVENANT_GUARDIAN_MAGE      = 29254,
    NPC_SUNREAVER_GUARDIAN_MAGE            = 29255,
};

class npc_mageguard_dalaran : public CreatureScript
{
public:
    npc_mageguard_dalaran() : CreatureScript("npc_mageguard_dalaran") { }

    struct npc_mageguard_dalaranAI : public ScriptedAI
    {
        npc_mageguard_dalaranAI(Creature* creature) : ScriptedAI(creature)
        {
            creature->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
            creature->ApplySpellImmune(0, IMMUNITY_DAMAGE, SPELL_SCHOOL_NORMAL, true);
            creature->ApplySpellImmune(0, IMMUNITY_DAMAGE, SPELL_SCHOOL_MASK_MAGIC, true);
        }

        void Reset(){}

        void EnterCombat(Unit* /*who*/){}

        void AttackStart(Unit* /*who*/){}

        void MoveInLineOfSight(Unit* who)
        {
            if (!who || !who->IsInWorld() || who->GetZoneId() != 4395)
                return;

            if (!me->IsWithinDist(who, 40.0f, false))
                return;

            Player* player = who->GetCharmerOrOwnerPlayerOrPlayerItself();

			if (!player || player->IsGameMaster() || player->IsBeingTeleported() || (player->GetPositionZ() > 670 && player->GetVehicle()) ||
                // If player has Disguise aura for quest A Meeting With The Magister or An Audience With The Arcanist, do not teleport it away but let it pass
                player->HasAura(SPELL_SUNREAVER_DISGUISE_FEMALE) || player->HasAura(SPELL_SUNREAVER_DISGUISE_MALE) ||
                player->HasAura(SPELL_SILVER_COVENANT_DISGUISE_FEMALE) || player->HasAura(SPELL_SILVER_COVENANT_DISGUISE_MALE))
                return;

            switch (me->GetEntry())
            {
                case NPC_SILVER_COVENANT_GUARDIAN_MAGE:
                    if (player->GetTeamId() == TEAM_HORDE)              // Horde unit found in Alliance area
                    {
                        if (GetClosestCreatureWithEntry(me, NPC_APPLEBOUGH_A, 32.0f))
                        {
                            if (me->isInBackInMap(who, 12.0f))   // In my line of sight, "outdoors", and behind me
                                DoCast(who, SPELL_TRESPASSER_A); // Teleport the Horde unit out
                        }
                        else                                      // In my line of sight, and "indoors"
                            DoCast(who, SPELL_TRESPASSER_A);     // Teleport the Horde unit out
                    }
                    break;
                case NPC_SUNREAVER_GUARDIAN_MAGE:
                    if (player->GetTeamId() == TEAM_ALLIANCE)           // Alliance unit found in Horde area
                    {
                        if (GetClosestCreatureWithEntry(me, NPC_SWEETBERRY_H, 32.0f))
                        {
                            if (me->isInBackInMap(who, 12.0f))   // In my line of sight, "outdoors", and behind me
                                DoCast(who, SPELL_TRESPASSER_H); // Teleport the Alliance unit out
                        }
                        else                                      // In my line of sight, and "indoors"
                            DoCast(who, SPELL_TRESPASSER_H);     // Teleport the Alliance unit out
                    }
                    break;
            }
            me->SetOrientation(me->GetHomePosition().GetOrientation());
            return;
        }

        void UpdateAI(uint32 /*diff*/){}
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_mageguard_dalaranAI(creature);
    }
};

enum MinigobData
{
    ZONE_DALARAN            = 4395,

    SPELL_MANABONKED        = 61834,
    SPELL_TELEPORT_VISUAL   = 51347,
    SPELL_IMPROVED_BLINK    = 61995,

    EVENT_SELECT_TARGET     = 1,
    EVENT_BLINK             = 2,
    EVENT_DESPAWN_VISUAL    = 3,
    EVENT_DESPAWN           = 4,

    MAIL_MINIGOB_ENTRY      = 264,
    MAIL_DELIVER_DELAY_MIN  = 5*MINUTE,
    MAIL_DELIVER_DELAY_MAX  = 15*MINUTE
};

class npc_minigob_manabonk : public CreatureScript
{
    public:
        npc_minigob_manabonk() : CreatureScript("npc_minigob_manabonk") {}

        struct npc_minigob_manabonkAI : public ScriptedAI
        {
            npc_minigob_manabonkAI(Creature* creature) : ScriptedAI(creature)
            {
                me->setActive(true);
            }

            void Reset()
            {
                me->SetVisible(false);
                events.ScheduleEvent(EVENT_SELECT_TARGET, IN_MILLISECONDS);
            }

            Player* SelectTargetInDalaran()
            {
                std::list<Player*> PlayerInDalaranList;
                PlayerInDalaranList.clear();

                Map::PlayerList const &players = me->GetMap()->GetPlayers();
                for (Map::PlayerList::const_iterator itr = players.begin(); itr != players.end(); ++itr)
                    if (Player* player = itr->GetSource()->ToPlayer())
                        if (player->GetZoneId() == ZONE_DALARAN && !player->IsFlying() && !player->IsMounted() && !player->IsGameMaster())
                            PlayerInDalaranList.push_back(player);

                if (PlayerInDalaranList.empty())
                    return NULL;
                return Trinity::Containers::SelectRandomContainerElement(PlayerInDalaranList);
            }

            void SendMailToPlayer(Player* player)
            {
                SQLTransaction trans = CharacterDatabase.BeginTransaction();
                int16 deliverDelay = irand(MAIL_DELIVER_DELAY_MIN, MAIL_DELIVER_DELAY_MAX);
                MailDraft(MAIL_MINIGOB_ENTRY, true).SendMailTo(trans, MailReceiver(player), MailSender(MAIL_CREATURE, me->GetEntry()), MAIL_CHECK_MASK_NONE, deliverDelay);
                CharacterDatabase.CommitTransaction(trans);
            }

            void UpdateAI(uint32 diff)
            {
                events.Update(diff);

                while (uint32 eventId = events.ExecuteEvent())
                {
                    switch (eventId)
                    {
                        case EVENT_SELECT_TARGET:
                            me->SetVisible(true);
                            DoCast(me, SPELL_TELEPORT_VISUAL);
                            if (Player* player = SelectTargetInDalaran())
                            {
                                me->NearTeleportTo(player->GetPositionX(), player->GetPositionY(), player->GetPositionZ(), 0.0f);
                                DoCast(player, SPELL_MANABONKED);
                                SendMailToPlayer(player);
                            }
                            events.ScheduleEvent(EVENT_BLINK, 3*IN_MILLISECONDS);
                            break;
                        case EVENT_BLINK:
                            DoCast(me, SPELL_IMPROVED_BLINK);
                            Position pos;
                            me->GetRandomNearPosition(pos, (urand(15, 40)));
                            me->GetMotionMaster()->MovePoint(0, pos.m_positionX, pos.m_positionY, pos.m_positionZ);
                            events.ScheduleEvent(EVENT_DESPAWN, 3*IN_MILLISECONDS);
                            events.ScheduleEvent(EVENT_DESPAWN_VISUAL, 2.5*IN_MILLISECONDS);
                            break;
                        case EVENT_DESPAWN_VISUAL:
                            DoCast(me, SPELL_TELEPORT_VISUAL);
                            break;
                        case EVENT_DESPAWN:
                            me->DespawnOrUnsummon();
                            break;
                        default:
                            break;
                    }
                }
            }

        private:
            EventMap events;
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_minigob_manabonkAI(creature);
    }
};

void AddSC_dalaran()
{
	// our
	new npc_steam_powered_auctioneer();
	new npc_mei_francis_mount();
    new npc_shandy_dalaran();
	new npc_archmage_landalock();

	// theirs
    new npc_mageguard_dalaran();
    new npc_minigob_manabonk();
}
