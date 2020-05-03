/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* ScriptData
SDName: Boss_Heigan
SD%Complete: 65
SDComment: Missing traps dance
SDCategory: Naxxramas
EndScriptData */

#include "AI/ScriptDevAI/include/sc_common.h"
#include "naxxramas.h"

enum
{
    PHASE_GROUND            = 1,
    PHASE_PLATFORM          = 2,

    SAY_AGGRO1              = -1533109,
    SAY_AGGRO2              = -1533110,
    SAY_AGGRO3              = -1533111,
    SAY_SLAY                = -1533112,
    SAY_TAUNT1              = -1533113,
    SAY_TAUNT2              = -1533114,
    SAY_TAUNT3              = -1533115,
    SAY_TAUNT4              = -1533117,
    SAY_CHANNELING          = -1533116,
    SAY_DEATH               = -1533118,
    EMOTE_TELEPORT          = -1533136,
    EMOTE_RETURN            = -1533137,

    // Spells by boss
    SPELL_DECREPIT_FEVER    = 29998,
    SPELL_DECREPIT_FEVER_H  = 55011,
    SPELL_DISRUPTION        = 29310,
    SPELL_TELEPORT          = 30211,
    SPELL_PLAGUE_CLOUD      = 29350
};

struct boss_heiganAI : public ScriptedAI
{
    boss_heiganAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_pInstance = (instance_naxxramas*)pCreature->GetInstanceData();
        m_bIsRegularMode = pCreature->GetMap()->IsRegularDifficulty();
        Reset();
    }

    instance_naxxramas* m_pInstance;
    bool m_bIsRegularMode;

    uint8 m_uiPhase;
    uint8 m_uiPhaseEruption;

    uint32 m_uiFeverTimer;
    uint32 m_uiDisruptionTimer;
    uint32 m_uiEruptionTimer;
    uint32 m_uiPhaseTimer;
    uint32 m_uiTauntTimer;
    uint32 m_uiStartChannelingTimer;

    void ResetPhase()
    {
        m_uiPhaseEruption = 0;
        m_uiFeverTimer = 4000;
        m_uiEruptionTimer = m_uiPhase == PHASE_GROUND ? 15000 : 7500;
        m_uiDisruptionTimer = 5000;
        m_uiStartChannelingTimer = 1000;
        m_uiPhaseTimer = m_uiPhase == PHASE_GROUND ? 90000 : 45000;
    }

    void Reset() override
    {
        m_uiPhase = PHASE_GROUND;
        m_uiTauntTimer = urand(20000, 60000);               // TODO, find information
        ResetPhase();
    }

    void Aggro(Unit* /*pWho*/) override
    {
        switch (urand(0, 2))
        {
            case 0: DoScriptText(SAY_AGGRO1, m_creature); break;
            case 1: DoScriptText(SAY_AGGRO2, m_creature); break;
            case 2: DoScriptText(SAY_AGGRO3, m_creature); break;
        }

        if (m_pInstance)
            m_pInstance->SetData(TYPE_HEIGAN, IN_PROGRESS);
    }

    void KilledUnit(Unit* /*pVictim*/) override
    {
        DoScriptText(SAY_SLAY, m_creature);
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        DoScriptText(SAY_DEATH, m_creature);

        if (m_pInstance)
            m_pInstance->SetData(TYPE_HEIGAN, DONE);
    }

    void JustReachedHome() override
    {
        if (m_pInstance)
            m_pInstance->SetData(TYPE_HEIGAN, FAIL);
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->GetVictim())
            return;

        if (m_uiPhase == PHASE_GROUND)
        {
            // Teleport to platform
            if (m_uiPhaseTimer < uiDiff)
            {
                if (DoCastSpellIfCan(m_creature, SPELL_TELEPORT) == CAST_OK)
                {
                    DoScriptText(EMOTE_TELEPORT, m_creature);
                    m_creature->GetMotionMaster()->MoveIdle();

                    m_uiPhase = PHASE_PLATFORM;
                    ResetPhase();
                    return;
                }
            }
            else
                m_uiPhaseTimer -= uiDiff;

            // Fever
            if (m_uiFeverTimer < uiDiff)
            {
                DoCastSpellIfCan(m_creature, m_bIsRegularMode ? SPELL_DECREPIT_FEVER : SPELL_DECREPIT_FEVER_H);
                m_uiFeverTimer = 21000;
            }
            else
                m_uiFeverTimer -= uiDiff;

            // Disruption
            if (m_uiDisruptionTimer < uiDiff)
            {
                DoCastSpellIfCan(m_creature, SPELL_DISRUPTION);
                m_uiDisruptionTimer = 10000;
            }
            else
                m_uiDisruptionTimer -= uiDiff;
        }
        else                                                // Platform Phase
        {
            if (m_uiPhaseTimer < uiDiff)                    // Return to fight
            {
                m_creature->InterruptNonMeleeSpells(true);
                DoScriptText(EMOTE_RETURN, m_creature);
                m_creature->GetMotionMaster()->MoveChase(m_creature->GetVictim());

                m_uiPhase = PHASE_GROUND;
                ResetPhase();
                return;
            }
            m_uiPhaseTimer -= uiDiff;

            if (m_uiStartChannelingTimer)
            {
                if (m_uiStartChannelingTimer <= uiDiff)
                {
                    DoScriptText(SAY_CHANNELING, m_creature);
                    DoCastSpellIfCan(m_creature, SPELL_PLAGUE_CLOUD);
                    m_uiStartChannelingTimer = 0;           // no more
                }
                else
                    m_uiStartChannelingTimer -= uiDiff;
            }
        }

        // Taunt
        if (m_uiTauntTimer < uiDiff)
        {
            switch (urand(0, 3))
            {
                case 0: DoScriptText(SAY_TAUNT1, m_creature); break;
                case 1: DoScriptText(SAY_TAUNT2, m_creature); break;
                case 2: DoScriptText(SAY_TAUNT3, m_creature); break;
                case 3: DoScriptText(SAY_TAUNT4, m_creature); break;
            }
            m_uiTauntTimer = urand(20000, 70000);
        }
        else
            m_uiTauntTimer -= uiDiff;

        DoMeleeAttackIfReady();

        // Handling of the erruptions, this is not related to melee attack or spell-casting
        if (!m_pInstance)
            return;

        // Eruption
        if (m_uiEruptionTimer < uiDiff)
        {
            for (uint8 uiArea = 0; uiArea < MAX_HEIGAN_TRAP_AREAS; ++uiArea)
            {
                // Actually this is correct :P
                if (uiArea == (m_uiPhaseEruption % 6) || uiArea == 6 - (m_uiPhaseEruption % 6))
                    continue;

                m_pInstance->DoTriggerHeiganTraps(m_creature, uiArea);
            }

            m_uiEruptionTimer = m_uiPhase == PHASE_GROUND ? 10000 : 3000;
            ++m_uiPhaseEruption;
        }
        else
            m_uiEruptionTimer -= uiDiff;
    }
};

UnitAI* GetAI_boss_heigan(Creature* pCreature)
{
    return new boss_heiganAI(pCreature);
}

void AddSC_boss_heigan()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_heigan";
    pNewScript->GetAI = &GetAI_boss_heigan;
    pNewScript->RegisterSelf();
}
