#include "cbase.h"

#include <vgui_controls/BuildGroup.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/ProgressBar.h>

#include <vgui_controls/TextEntry.h>

#include "PFrameButton.h"
#include "hud_mapfinished.h"
#include "mom_player_shared.h"
#include "mom_replayui.h"
#include "mom_shareddefs.h"
#include "momentum/util/mom_util.h"

C_ReplayUI::C_ReplayUI(const char *pElementName) : Frame(nullptr, pElementName)
{
    SetMoveable(true);
    SetSizeable(false);
    SetVisible(false);
    SetMaximizeButtonVisible(false);
    SetMinimizeButtonVisible(false);
    SetMenuButtonResponsive(false);
    SetSize(310, 210);

    m_iTotalDuration = 0;

    SetScheme("ClientScheme");
    LoadControlSettings("resource/ui/ReplayUI.res");

    m_pPlayPauseResume = FindControl<ToggleButton>("ReplayPlayPauseResume");

    m_pGoStart = FindControl<Button>("ReplayGoStart");
    m_pGoEnd = FindControl<Button>("ReplayGoEnd");
    m_pPrevFrame = FindControl<Button>("ReplayPrevFrame");
    m_pNextFrame = FindControl<Button>("ReplayNextFrame");
    m_pFastForward = FindControl<PFrameButton>("ReplayFastForward");
    m_pFastBackward = FindControl<PFrameButton>("ReplayFastBackward");
    m_pGo = FindControl<Button>("ReplayGo");

    m_pGotoTick = FindControl<TextEntry>("ReplayGoToTick");

    m_pTimescaleSlider = FindControl<CCvarSlider>("TimescaleSlider");
    m_pTimescaleLabel = FindControl<Label>("TimescaleLabel");
    m_pTimescaleEntry = FindControl<TextEntry>("TimescaleEntry");
    SetLabelText();

    m_pProgress = FindControl<ScrubbableProgressBar>("ReplayProgress");

    m_pProgressLabelFrame = FindControl<Label>("ReplayProgressLabelFrame");
    m_pProgressLabelTime = FindControl<Label>("ReplayProgressLabelTime");

}

void C_ReplayUI::OnThink()
{
    BaseClass::OnThink();

    if (m_pFastBackward->IsSelected())
    {
        shared->RGUI_HasSelected = RUI_MOVEBW;
        shared->RGUI_bIsPlaying = false;
        m_pPlayPauseResume->ForceDepressed(false);
    }
    else if (m_pFastForward->IsSelected())
    {
        shared->RGUI_HasSelected = RUI_MOVEFW;
        shared->RGUI_bIsPlaying = false;
        m_pPlayPauseResume->ForceDepressed(false);
    }
    else
    {
        if (shared->RGUI_bIsPlaying)
        {
            m_pPlayPauseResume->SetText("Playing");
            m_pPlayPauseResume->SetSelected(true);
        }
        else
        {
            shared->RGUI_HasSelected = RUI_NOTHING;
            m_pPlayPauseResume->SetText("Paused");
            m_pPlayPauseResume->SetSelected(false);
        }
    }

    C_MomentumPlayer *pPlayer = ToCMOMPlayer(CBasePlayer::GetLocalPlayer());
    if (pPlayer)
    {
        C_MomentumReplayGhostEntity *pGhost = pPlayer->GetReplayEnt();
        if (pGhost)
        {
            m_iTotalDuration = pGhost->m_iTotalTimeTicks;

            float fProgress = static_cast<float>(pGhost->m_iCurrentTick) / static_cast<float>(m_iTotalDuration);
            fProgress = clamp(fProgress, 0.0f, 1.0f);

            m_pProgress->SetProgress(fProgress);
            char labelFrame[512];
            // MOM_TODO: Localize this
            Q_snprintf(labelFrame, 512, "Tick: %i / %i", pGhost->m_iCurrentTick, m_iTotalDuration);
            m_pProgressLabelFrame->SetText(labelFrame);
            char curtime[BUFSIZETIME];
            char totaltime[BUFSIZETIME];
            mom_UTIL->FormatTime(TICK_INTERVAL * pGhost->m_iCurrentTick, curtime, 2);
            mom_UTIL->FormatTime(TICK_INTERVAL * m_iTotalDuration, totaltime, 2);

            char labelTime[512];
            // MOM_TODO: LOCALIZE
            Q_snprintf(labelTime, 512, "Time: %s -> %s", curtime, totaltime);
            m_pProgressLabelTime->SetText(labelTime);
            // Let's add a check if we entered into end zone without the trigger spot it (since we teleport directly),
            // then we will disable the replayui

            if (pGhost)
            {
                // always disable if map is finished
                if (pGhost->m_RunData.m_bMapFinished)
                {
                    SetVisible(false);
                }
            }
        }
    }
}



void C_ReplayUI::OnControlModified(Panel *p)
{
    if (p == m_pTimescaleSlider && m_pTimescaleSlider->HasBeenModified())
    {
        SetLabelText();
    }
}

void C_ReplayUI::OnTextChanged(Panel* p)
{
    if (p == m_pTimescaleEntry)
    {
        char buf[64];
        m_pTimescaleEntry->GetText(buf, 64);

        float fValue = float(atof(buf));
        if (fValue >= 0.01 && fValue <= 10.0)
        {
            m_pTimescaleSlider->SetSliderValue(fValue);
            m_pTimescaleSlider->ApplyChanges();
        }
    }
}

void C_ReplayUI::OnNewProgress(float scale)
{
    int tickToGo = static_cast<int>(scale * m_iTotalDuration);
    if (tickToGo > -1 && tickToGo <= m_iTotalDuration)
    {
        engine->ServerCmd(VarArgs("mom_replay_goto %i", tickToGo));
    }
}

void C_ReplayUI::OnMouseWheeled(KeyValues *pKv)
{
    if (pKv->GetPtr("panel") == m_pProgress)
        OnCommand(pKv->GetInt("delta") > 0 ? "nextframe" : "prevframe");
}

void C_ReplayUI::SetLabelText() const
{
    if (m_pTimescaleSlider && m_pTimescaleEntry)
    {
        char buf[64];
        Q_snprintf(buf, sizeof(buf), "%.1f", m_pTimescaleSlider->GetSliderValue());
        m_pTimescaleEntry->SetText(buf);

        m_pTimescaleSlider->ApplyChanges();
    }   
}

// Command issued
void C_ReplayUI::OnCommand(const char *command)
{
    if (!shared)
        return BaseClass::OnCommand(command);
    C_MomentumReplayGhostEntity *pGhost = ToCMOMPlayer(CBasePlayer::GetLocalPlayer())->GetReplayEnt();
    if (!Q_strcasecmp(command, "play"))
    {
        shared->RGUI_bIsPlaying = !shared->RGUI_bIsPlaying;
    }
    else if (!Q_strcasecmp(command, "reload"))
    {
        engine->ServerCmd("mom_replay_restart");
    }
    else if (!Q_strcasecmp(command, "gotoend"))
    {
        engine->ServerCmd("mom_replay_goto_end");
    }
    else if (!Q_strcasecmp(command, "prevframe") && pGhost)
    {
        if (pGhost->m_iCurrentTick > 0)
        {
            engine->ServerCmd(VarArgs("mom_replay_goto %i", pGhost->m_iCurrentTick - 1));
        }
    }
    else if (!Q_strcasecmp(command, "nextframe") && pGhost)
    {
        if (pGhost->m_iCurrentTick < pGhost->m_iTotalTimeTicks)
        {
            engine->ServerCmd(VarArgs("mom_replay_goto %i", pGhost->m_iCurrentTick + 1));
        }
    }
    else if (!Q_strcasecmp(command, "gototick"))
    {
        // Teleport at the position we want with timer included
        char tick[32];
        m_pGotoTick->GetText(tick, sizeof(tick));
        engine->ServerCmd(VarArgs("mom_replay_goto %s", tick));
    }
    else
    {
        BaseClass::OnCommand(command);
    }
}

void replayui_f()
{
    C_ReplayUI *HudReplay = nullptr;
    if (HudReplay == nullptr)
        HudReplay = new C_ReplayUI("HudReplay");

    if (!HudReplay || !shared)
        return;

    if (HudReplay->IsVisible())
    {
        HudReplay->Close();
    }
    else
    {
        HudReplay->Activate();
    }
}

static ConCommand replayui("replayui", replayui_f, "Replay Ghost GUI.");