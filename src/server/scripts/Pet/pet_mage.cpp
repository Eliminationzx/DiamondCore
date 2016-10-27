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

/*
 * Ordered alphabetically using scriptname.
 * Scriptnames of files in this file should be prefixed with "npc_pet_mag_".
 */

#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "CombatAI.h"
#include "Pet.h"

enum MageSpells
{
    SPELL_MAGE_CLONE_ME                 = 45204,
    SPELL_MAGE_MASTERS_THREAT_LIST      = 58838,
	SPELL_PET_HIT_SCALING				= 61013,
	SPELL_SUMMON_MIRROR_IMAGE1			= 58831,
	SPELL_SUMMON_MIRROR_IMAGE2			= 58833,
	SPELL_SUMMON_MIRROR_IMAGE3			= 58834,
	SPELL_SUMMON_MIRROR_IMAGE_GLYPH		= 65047
};

class DeathEvent : public BasicEvent
{
	public:
		DeathEvent(Creature& owner) : BasicEvent(), _owner(owner) { }

		bool Execute(uint64 /*eventTime*/, uint32 /*diff*/)
		{
			Unit::Kill(&_owner, &_owner);
			return true;
		}

	private:
		Creature& _owner;
};

class npc_pet_mage_mirror_image : public CreatureScript
{
    public:
        npc_pet_mage_mirror_image() : CreatureScript("npc_pet_mage_mirror_image") { }

        struct npc_pet_mage_mirror_imageAI : CasterAI
        {
            npc_pet_mage_mirror_imageAI(Creature* creature) : CasterAI(creature) { }

			uint32 selectionTimer;
			uint64 _ebonGarogyleGUID;

            void InitializeAI()
            {
                CasterAI::InitializeAI();
                Unit* owner = me->GetOwner();
                if (!owner)
                    return;

                // Clone Me!
                owner->CastSpell(me, SPELL_MAGE_CLONE_ME, true);

				// xinef: Glyph of Mirror Image (4th copy)
				float angle = 0.0f;
				switch (me->GetUInt32Value(UNIT_CREATED_BY_SPELL))
				{
					case SPELL_SUMMON_MIRROR_IMAGE1:
						angle = 0.5f * M_PI;
						break;
					case SPELL_SUMMON_MIRROR_IMAGE2:
						angle = M_PI;
						break;
					case SPELL_SUMMON_MIRROR_IMAGE3:
						angle = 1.5f * M_PI;
						break;
				}

				((Minion*)me)->SetFollowAngle(angle);
				me->GetMotionMaster()->MoveFollow(owner, PET_FOLLOW_DIST, me->GetFollowAngle(), MOTION_SLOT_ACTIVE);
				me->SetReactState(REACT_PASSIVE);

                // Xinef: Inherit Master's Threat List (not yet implemented)
                //owner->CastSpell((Unit*)NULL, SPELL_MAGE_MASTERS_THREAT_LIST, true);
				HostileReference* ref = owner->getHostileRefManager().getFirst();
				while (ref)
				{
					if (Unit* unit = ref->GetSource()->GetOwner())
						unit->AddThreat(me, ref->getThreat() - ref->getTempThreatModifier());
					ref = ref->next();
				}

				_ebonGarogyleGUID = 0;

				// Xinef: copy caster auras
				Unit::VisibleAuraMap const* visibleAuraMap = owner->GetVisibleAuras();
				for (Unit::VisibleAuraMap::const_iterator itr = visibleAuraMap->begin(); itr != visibleAuraMap->end(); ++itr)
					if (Aura* visAura = itr->second->GetBase())
					{
						// Ebon Gargoyle
						if (visAura->GetId() == 49206 && me->GetUInt32Value(UNIT_CREATED_BY_SPELL) == SPELL_SUMMON_MIRROR_IMAGE1)
						{
							if (Unit* garogyle = visAura->GetCaster())
								_ebonGarogyleGUID = garogyle->GetGUID();
							continue;
						}
						SpellScriptsBounds bounds = sObjectMgr->GetSpellScriptsBounds(visAura->GetId());
						if (bounds.first != bounds.second)
							continue;
						std::vector<int32> const* spellTriggered = sSpellMgr->GetSpellLinked(visAura->GetId() + SPELL_LINK_AURA);
						if (!spellTriggered || !spellTriggered->empty())
							continue;
						if (Aura* newAura = me->AddAura(visAura->GetId(), me))
							newAura->SetDuration(visAura->GetDuration());
					}

				me->m_Events.AddEvent(new DeathEvent(*me), me->m_Events.CalculateTime(29500));
            }

            // Do not reload Creature templates on evade mode enter - prevent visual lost
            void EnterEvadeMode()
            {
                if (me->IsInEvadeMode() || !me->IsAlive())
                    return;

                Unit* owner = me->GetCharmerOrOwner();

                me->CombatStop(true);
                if (owner && !me->HasUnitState(UNIT_STATE_FOLLOW))
                {
                    me->GetMotionMaster()->Clear(false);
					me->GetMotionMaster()->MoveFollow(owner, PET_FOLLOW_DIST, me->GetFollowAngle(), MOTION_SLOT_ACTIVE);
                }
            }

			bool MySelectNextTarget()
			{
				if (_ebonGarogyleGUID)
				{
					if (Unit* garogyle = ObjectAccessor::GetUnit(*me, _ebonGarogyleGUID))
						garogyle->GetAI()->AttackStart(me);
					_ebonGarogyleGUID = 0;
				}
				Unit* owner = me->GetOwner();
				if (owner && owner->GetTypeId() == TYPEID_PLAYER)
				{
					Unit* selection = owner->ToPlayer()->GetSelectedUnit();
					if (selection && selection != me->GetVictim())
					{
						// target has cc, search target without cc!
						if (selection->HasBreakableByDamageCrowdControlAura() || !me->IsValidAttackTarget(selection))
						{
							return false;
						}

						me->getThreatManager().resetAllAggro();
						me->AddThreat(selection, 1000000.0f);
						AttackStart(selection);
						return true;
					}
				}
				return false;
			}

			void Reset()
			{
				selectionTimer = 0;
			}

			void UpdateAI(uint32 diff)
			{
				events.Update(diff);
				if (events.GetTimer() < 1200)
					return;

				if (!me->IsInCombat() || !me->GetVictim())
				{
					MySelectNextTarget();
					return;
				}

				if (me->GetVictim()->HasBreakableByDamageCrowdControlAura() || !me->GetVictim()->IsAlive())
				{
					me->InterruptNonMeleeSpells(false);
					if (!MySelectNextTarget())
						EnterEvadeMode();
					return;
				}

				selectionTimer += diff;
				if (selectionTimer >= 1000)
				{
					MySelectNextTarget();
					selectionTimer = 0;
				}

				if (me->HasUnitState(UNIT_STATE_CASTING))
					return;

				if (uint32 spellId = events.GetEvent())
				{
					events.RescheduleEvent(spellId, spellId == 59637 ? 6500 : 2500);
					me->CastSpell(me->GetVictim(), spellId, false);
				}
			}
        };

        CreatureAI* GetAI(Creature* creature) const
        {
            return new npc_pet_mage_mirror_imageAI(creature);
        }
};

void AddSC_mage_pet_scripts()
{
    new npc_pet_mage_mirror_image();
}
