#include "cbase.h"
#include "hudelement.h"
#include "hud_numericdisplay.h"
#include "hud_macros.h"
#include "utlvector.h"
#include "KeyValues.h"
#include "iclientmode.h"
#include "view.h"
#include "menu.h"
#include "vgui_helpers.h"

#include <vgui_controls/Panel.h>
#include <vgui_controls/Frame.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/ILocalize.h>
#include <vgui_controls/AnimationController.h>

#include "mom_event_listener.h"
#include "momentum/util/mom_util.h"
#include "mom_player_shared.h"
#include "mom_shareddefs.h"

#include "tier0/memdbgon.h"

using namespace vgui;

static ConVar mom_timer("mom_timer", "1",
    FCVAR_CLIENTDLL | FCVAR_ARCHIVE,
    "Turn the timer display on/off\n");

static ConVar timer_mode("mom_timer_mode", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE | FCVAR_REPLICATED,
    "Set what type of timer you want.\n0 = Generic Timer (no splits)\n1 = Splits by Checkpoint\n");

class C_Timer : public CHudElement, public Panel
{
    DECLARE_CLASS_SIMPLE(C_Timer, Panel);
public:
    C_Timer();
    C_Timer(const char *pElementName);
    void OnThink() override;
    void Init() override;
    void Reset() override;
    void Paint() override;
    bool ShouldDraw() override
    {
        return mom_timer.GetBool() && CHudElement::ShouldDraw();
    }

    void ApplySchemeSettings(IScheme *pScheme) override
    {
        Panel::ApplySchemeSettings(pScheme);
        SetFgColor(GetSchemeColor("MOM.Panel.Fg", pScheme));
        m_TimeGain = GetSchemeColor("MOM.Timer.Gain", pScheme);
        m_TimeLoss = GetSchemeColor("MOM.Timer.Loss", pScheme);
    }
    void MsgFunc_Timer_State(bf_read &msg);
    void MsgFunc_Timer_Reset(bf_read &msg);
    void MsgFunc_Timer_Checkpoint(bf_read &msg);
    void MsgFunc_Timer_Stage(bf_read &msg);
    void MsgFunc_Timer_StageCount(bf_read&);
    float GetCurrentTime();
    bool m_bIsRunning;
    bool m_bTimerRan;
    int m_iStartTick;

protected:
    CPanelAnimationVar(float, m_flBlur, "Blur", "0");
    CPanelAnimationVar(Color, m_TextColor, "TextColor", "FgColor");
    CPanelAnimationVar(Color, m_Ammo2Color, "Ammo2Color", "FgColor");
    CPanelAnimationVar(Color, m_TimeGain, "TimeGainColor", "FgColor");
    CPanelAnimationVar(Color, m_TimeLoss, "TimeLossColor", "FgColor");

    CPanelAnimationVar(HFont, m_hTextFont, "TextFont", "HudHintTextLarge");
    CPanelAnimationVar(HFont, m_hTimerFont, "TimerFont", "HudNumbersSmallBold");
    CPanelAnimationVar(HFont, m_hSmallTextFont, "SmallTextFont", "HudNumbersSmall");

    CPanelAnimationVarAliasType(bool, center_time, "centerTime", "0",
        "BOOL");
    CPanelAnimationVarAliasType(float, time_xpos, "time_xpos", "0",
        "proportional_float");
    CPanelAnimationVarAliasType(float, time_ypos, "time_ypos", "2",
        "proportional_float");
    CPanelAnimationVarAliasType(bool, center_cps, "centerCps", "1",
        "BOOL");
    CPanelAnimationVarAliasType(float, cps_xpos, "cps_xpos", "50",
        "proportional_float");
    CPanelAnimationVarAliasType(float, cps_ypos, "cps_ypos", "25",
        "proportional_float");
    CPanelAnimationVarAliasType(bool, center_stage, "centerStage", "1",
        "BOOL");
    CPanelAnimationVarAliasType(float, stage_xpos, "stage_xpos", "50",
        "proportional_float");
    CPanelAnimationVarAliasType(float, stage_ypos, "stage_ypos", "40",
        "proportional_float");
private:
    int m_iStageCurrent, m_iStageCount;
    int initialTall;

    wchar_t m_pwCurrentTime[BUFSIZETIME];
    char m_pszString[BUFSIZETIME];
    wchar_t m_pwCurrentCheckpoints[BUFSIZELOCL];
    char m_pszStringCps[BUFSIZELOCL];
    wchar_t m_pwCurrentStages[BUFSIZELOCL];
    char m_pszStringStages[BUFSIZELOCL];
    wchar_t m_pwCurrentStatus[BUFSIZELOCL];
    char m_pszStringStatus[BUFSIZELOCL];
    wchar_t m_pwStageTime[BUFSIZETIME];
    char m_pszStageTimeString[BUFSIZETIME];
    wchar_t m_pwStageTimeLabel[BUFSIZELOCL];
    char m_pszStageTimeLabelString[BUFSIZELOCL];

    wchar_t m_pwStageTimeComparison[BUFSIZETIME];
    char m_pszStageTimeComparisonANSI[BUFSIZETIME], m_pszStageTimeComparisonLabel[BUFSIZELOCL];

    wchar_t m_pwStageStartString[BUFSIZELOCL], m_pwStageStartLabel[BUFSIZELOCL];

    

    int m_iTotalTicks;
    bool m_bPlayerInZone;
    bool m_bWereCheatsActivated = false;
    bool m_bShowCheckpoints;
    bool m_bMapFinished;
    int m_iCheckpointCount, m_iCheckpointCurrent;
    char stLocalized[BUFSIZELOCL], cpLocalized[BUFSIZELOCL], linearLocalized[BUFSIZELOCL],
        startZoneLocalized[BUFSIZELOCL], mapFinishedLocalized[BUFSIZELOCL], practiceModeLocalized[BUFSIZELOCL], 
        noTimerLocalized[BUFSIZELOCL];
};

DECLARE_HUDELEMENT(C_Timer);
// MOM_TODO add more for checkpoints and ending
DECLARE_HUD_MESSAGE(C_Timer, Timer_State);
DECLARE_HUD_MESSAGE(C_Timer, Timer_Reset);
DECLARE_HUD_MESSAGE(C_Timer, Timer_Checkpoint);
DECLARE_HUD_MESSAGE(C_Timer, Timer_Stage);
DECLARE_HUD_MESSAGE(C_Timer, Timer_StageCount);

C_Timer::C_Timer(const char *pElementName) :
CHudElement(pElementName), Panel(g_pClientMode->GetViewport(), "HudTimer")
{
    // This is already set for HUD elements, but still...
    SetProportional(true);
    SetKeyBoardInputEnabled(false);
    SetMouseInputEnabled(false);
    SetHiddenBits(HIDEHUD_WEAPONSELECTION);
}

void C_Timer::Init()
{
    HOOK_HUD_MESSAGE(C_Timer, Timer_State);
    HOOK_HUD_MESSAGE(C_Timer, Timer_Reset);
    HOOK_HUD_MESSAGE(C_Timer, Timer_Checkpoint);
    HOOK_HUD_MESSAGE(C_Timer, Timer_Stage);
    HOOK_HUD_MESSAGE(C_Timer, Timer_StageCount);
    initialTall = 48;
    m_iTotalTicks = 0;
    m_iStageCount = 0;
    //Reset();

    //cache localization strings
    FIND_LOCALIZATION(m_pwStageStartString, "#MOM_Stage_Start");
    LOCALIZE_TOKEN(Checkpoint, "#MOM_Checkpoint", cpLocalized);
    LOCALIZE_TOKEN(Stage, "#MOM_Stage", stLocalized);
    LOCALIZE_TOKEN(Linear, "#MOM_Linear", linearLocalized);
    LOCALIZE_TOKEN(InsideStart, "#MOM_InsideStartZone", startZoneLocalized);
    LOCALIZE_TOKEN(MapFinished, "#MOM_MapFinished", mapFinishedLocalized);
    LOCALIZE_TOKEN(PracticeMode, "#MOM_PracticeMode", practiceModeLocalized);
    LOCALIZE_TOKEN(NoTimer, "#MOM_NoTimer", noTimerLocalized);
}

void C_Timer::Reset()
{
    m_bIsRunning = false;
    m_bTimerRan = false;
    m_iTotalTicks = 0;
    m_iStageCurrent = 1;
    m_bShowCheckpoints = false;
    m_bPlayerInZone = false;
    m_bMapFinished = false;
    m_iCheckpointCount = 0;
    m_iCheckpointCurrent = 0;
}

void C_Timer::MsgFunc_Timer_State(bf_read &msg)
{
    C_MomentumPlayer *pPlayer = ToCMOMPlayer(C_BasePlayer::GetLocalPlayer());
    if (!pPlayer)
        return;

    bool started = msg.ReadOneBit();
    m_bIsRunning = started;
    m_iStartTick = (int) msg.ReadLong();//MOM_TODO: should this be gpGlobals->tickcount ?
    
    
    if (started)
    {
        //VGUI_ANIMATE("TimerStart");
        // Checking again, even if we just checked 8 lines before
        if (pPlayer != nullptr)
        {
            pPlayer->EmitSound("Momentum.StartTimer");
            m_bTimerRan = true;
        }
    }
    else // stopped
    {
        // Compare times.
        if (m_bWereCheatsActivated) //EY, CHEATER, STOP
        {
            DevWarning("sv_cheats was set to 1, thus making the run not valid \n");
        }
        else //He didn't cheat, we can carry on
        {
            //m_iTotalTicks = gpGlobals->tickcount - m_iStartTick;
            //DevMsg("Ticks upon exit: %i and total seconds: %f\n", m_iTotalTicks, gpGlobals->interval_per_tick);
            //Paint();
            //DevMsg("%s \n", m_pszString);
        }

        //VGUI_ANIMATE("TimerStop");
        if (pPlayer != nullptr)
        {
            pPlayer->EmitSound("Momentum.StopTimer");
            pPlayer->m_flLastRunTime = static_cast<float>(gpGlobals->tickcount - m_iStartTick) * gpGlobals->interval_per_tick;
        }

        //MOM_TODO: (Beta+) show scoreboard animation with new position on leaderboards?
    }
}

void C_Timer::MsgFunc_Timer_Reset(bf_read &msg)
{
    Reset();
}

void C_Timer::MsgFunc_Timer_Checkpoint(bf_read &msg)
{
    m_bShowCheckpoints = msg.ReadOneBit();
    m_iCheckpointCurrent = static_cast<int>(msg.ReadLong());
    m_iCheckpointCount = static_cast<int>(msg.ReadLong());
}

void C_Timer::MsgFunc_Timer_Stage(bf_read &msg)
{
    //MOM_TODO: remove this function, it was moved to the player
    m_iStageCurrent = static_cast<int>(msg.ReadLong());
    //g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("MenuPulse");
}

void C_Timer::MsgFunc_Timer_StageCount(bf_read &msg)
{
    m_iStageCount = static_cast<int>(msg.ReadLong());
}
float C_Timer::GetCurrentTime()
{
    //HACKHACK: The client timer stops 1 tick behind the server timer for unknown reasons,
    //so we add an extra tick here to make them line up again
    if (m_bIsRunning)
        m_iTotalTicks = gpGlobals->tickcount - m_iStartTick + 1;
    return static_cast<float>(m_iTotalTicks) * gpGlobals->interval_per_tick;
}

//Calculations should be done in here. Paint is for drawing what the calculations have done.
void C_Timer::OnThink()
{
    C_MomentumPlayer *pLocal = ToCMOMPlayer(C_BasePlayer::GetLocalPlayer());
    if (pLocal)
    {
        m_iStageCurrent = pLocal->m_iCurrentStage;
        m_bPlayerInZone = pLocal->m_bIsInZone;
        m_bMapFinished = pLocal->m_bMapFinished;

        //MOM_TODO: if m_iStageCount > 1
        // calculate comparisons and store them
    }
}

void C_Timer::Paint(void)
{
    //MOM_TODO: Potentially have splits, where we subtract the enter time
    //for the current stage from the total time, to show the player what 
    //their time is on the current stage?
    mom_UTIL->FormatTime(GetCurrentTime(), m_pszString, 2);
    ANSI_TO_UNICODE(m_pszString, m_pwCurrentTime);

    //find out status of checkpoints (linear vs checkpoints)
    if (m_bShowCheckpoints)
    {
        Q_snprintf(m_pszStringCps, sizeof(m_pszStringCps), "%s %i/%i",
            cpLocalized, // Checkpoint localization
            m_iCheckpointCurrent, //CurrentCP
            m_iCheckpointCount //CPCount
            );

        ANSI_TO_UNICODE(m_pszStringCps, m_pwCurrentCheckpoints);
    }

    char prevStageString[BUFSIZELOCL];

    if (m_iStageCount > 1)
    {
        //Current stage/total stages
        Q_snprintf(m_pszStringStages, sizeof(m_pszStringStages), "%s %i/%i",
            stLocalized,// "Stage"
            m_iStageCurrent,// Current stage
            m_iStageCount // Total number of stages
            );

        if (m_iStageCurrent > 1)
        {

            Q_snprintf(prevStageString, BUFSIZELOCL, "%s %i",
                stLocalized, // Stage localization
                m_iStageCurrent - 1); // Last stage number

            
            //MOM_TODO: calculate diff from WR (online)
            
            //Draw the timer split
            mom_UTIL->FormatTime(g_MOMEventListener->m_flStageEnterTime[m_iStageCurrent], m_pszStageTimeString);
            Q_snprintf(m_pszStageTimeLabelString, sizeof(m_pszStageTimeLabelString), "%s (%s)",
                prevStageString,
                m_pszStageTimeString
                );
            ANSI_TO_UNICODE(m_pszStageTimeLabelString, m_pwStageTimeLabel);
        }
    }
    else //it's a linear map
    {
        Q_snprintf(m_pszStringStages, sizeof(m_pszStringStages), linearLocalized);
    }

    ANSI_TO_UNICODE(m_pszStringStages, m_pwCurrentStages);

    //find out status of timer (start zone/end zone/practice mode)
    //No matter what, we always want the player's status printed out, if they're in a zone
    if (m_bPlayerInZone)
    {
        if (m_iStageCurrent == 1)
        {
            //Start zone
            Q_snprintf(m_pszStringStatus, sizeof(m_pszStringStatus), startZoneLocalized);
            ANSI_TO_UNICODE(m_pszStringStatus, m_pwStageStartLabel);
        }
        else if (m_bPlayerInZone && m_bMapFinished) //don't check for zone # in case the player skipped one somehow
        {
            //End zone
            Q_snprintf(m_pszStringStatus, sizeof(m_pszStringStatus), mapFinishedLocalized);
            ANSI_TO_UNICODE(m_pszStringStatus, m_pwStageStartLabel);
        }
        else
        {
            //Stage # Start
            wchar_t stageCurrent[128];//00'\0' and max stages is 64

            V_snwprintf(stageCurrent, ARRAYSIZE(stageCurrent), L"%d", m_iStageCurrent);
            //Fills the "Stage %s1 Start" string
            g_pVGuiLocalize->ConstructString(m_pwStageStartLabel, sizeof(m_pwStageStartLabel),
                m_pwStageStartString, 1, stageCurrent);
        }
    }

    if (!m_bIsRunning)
    {
        if (g_MOMEventListener->m_bPlayerHasPracticeMode)//In practice mode
        {
            Q_snprintf(m_pszStringStatus, sizeof(m_pszStringStatus), practiceModeLocalized);
        }
        else //no timer
        {
            Q_snprintf(m_pszStringStatus, sizeof(m_pszStringStatus), noTimerLocalized);
        }
        ANSI_TO_UNICODE(m_pszStringStatus, m_pwCurrentStatus);
    }

    
    // Draw the text label.
    surface()->DrawSetTextFont(m_bIsRunning ? m_hTimerFont : m_hTextFont);
    surface()->DrawSetTextColor(GetFgColor());

    int dummy, totalWide;
    // Draw current time.
    GetSize(totalWide, dummy);

    if (center_time)
    {
        int timeWide;
        surface()->GetTextSize(m_bIsRunning ? m_hTimerFont : m_hTextFont, m_bIsRunning ? m_pwCurrentTime : m_pwCurrentStatus, timeWide, dummy);
        int offsetToCenter = ((totalWide - timeWide) / 2);
        surface()->DrawSetTextPos(offsetToCenter, time_ypos);
    }
    else
    {
        surface()->DrawSetTextPos(time_xpos, time_ypos);
    }

    //draw either timer display or the timer status
    //If the timer isn't running, it'll print "No timer" or "Practice mode"
    surface()->DrawPrintText(m_bIsRunning ? m_pwCurrentTime : m_pwCurrentStatus, m_bIsRunning ? wcslen(m_pwCurrentTime) : wcslen(m_pwCurrentStatus));

    surface()->DrawSetTextFont(m_hSmallTextFont);
    if (m_bShowCheckpoints)
    {
        if (center_cps)
        {
            int cpsWide;
            surface()->GetTextSize(m_hSmallTextFont, m_pwCurrentCheckpoints, cpsWide, dummy);
            int offsetToCenter = ((totalWide - cpsWide) / 2);
            surface()->DrawSetTextPos(offsetToCenter, cps_ypos);
        }
        else
            surface()->DrawSetTextPos(cps_xpos, cps_ypos);

        surface()->DrawPrintText(m_pwCurrentCheckpoints, wcslen(m_pwCurrentCheckpoints));
    }
    else //don't draw stages when drawing checkpoints, and vise versa.
    {
        // MOM_TODO: Print this only if map gamemode is supported
        if (center_stage)
        {
            int stageWide;
            surface()->GetTextSize(m_hSmallTextFont, m_bPlayerInZone ? m_pwStageStartLabel : m_pwCurrentStages, stageWide, dummy);
            int offsetToCenter = ((totalWide - stageWide) / 2);
            surface()->DrawSetTextPos(offsetToCenter, stage_ypos);
        }
        else
            surface()->DrawSetTextPos(stage_xpos, stage_ypos);

        //If we're inside a stage trigger, print that stage's start label
        surface()->DrawPrintText(m_bPlayerInZone ? m_pwStageStartLabel : m_pwCurrentStages, 
            wcslen(m_bPlayerInZone ? m_pwStageStartLabel : m_pwCurrentStages));

        //MOM_TODO: Overhaul comparisons (stage PB, vs WR, velocities...)
        if (m_iStageCurrent > 1 && m_bIsRunning) //only draw stage timer if we are on stage 2 or above.
        {
#if 0//MOM_TODO: REMOVE ME!
            if (hasComparison)
            {
                
                char tempStageChar[BUFSIZELOCL];
                wchar_t tempStageWChar[BUFSIZELOCL];
                Q_snprintf(tempStageChar, BUFSIZELOCL, "%s %s",
                    prevStageString,//Previous stage
                    comp);//WR/PB
                ANSI_TO_UNICODE(tempStageChar, tempStageWChar);
                
                //find the length of the entire comparison string
                char tempComparisonLabel[260];
                wchar_t tempComparisonWChar[260];
                Q_snprintf(tempComparisonLabel, 260, "%s %s", 
                    tempStageChar,
                    m_pszStageTimeComparisonLabel);
                ANSI_TO_UNICODE(tempComparisonLabel, tempComparisonWChar);
                int text_xpos = GetWide() / 2 - UTIL_ComputeStringWidth(m_hSmallTextFont, tempComparisonWChar) / 2;

                //And now break the string up into chunks so we can color just the time loss/gain

                //Draw "Stage ##"
                surface()->DrawSetTextColor(m_TextColor);
                surface()->DrawSetTextPos(text_xpos, cps_ypos);
                surface()->DrawPrintText(tempStageWChar, wcslen(tempStageWChar));

                //Draw the comparison after "Stage ##"
                int lengthOfStage = UTIL_ComputeStringWidth(m_hSmallTextFont, tempStageWChar);
                surface()->DrawSetTextColor(losingTime ? m_TimeLoss : m_TimeGain);//MOM_TODO: possibly handle ties?
                surface()->DrawSetTextPos(text_xpos + lengthOfStage + 2, cps_ypos); //add 2 px margin so our times dont overlap each other so much 
                surface()->DrawPrintText(m_pwStageTimeComparison, wcslen(m_pwStageTimeComparison));

            }
           else
#endif            
           {
                int text_xpos = GetWide() / 2 - UTIL_ComputeStringWidth(m_hSmallTextFont, m_pwStageTimeLabel) / 2;
                surface()->DrawSetTextPos(text_xpos, cps_ypos);
                surface()->DrawPrintText(m_pwStageTimeLabel, wcslen(m_pwStageTimeLabel));
            }
        }
    }
}