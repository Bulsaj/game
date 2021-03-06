#pragma once

#include "cbase.h"

#include "SettingsPage.h"
#include "hud_comparisons.h"
#include <vgui_controls/Button.h>
#include <vgui_controls/CvarToggleCheckButton.h>
#include <vgui_controls/Frame.h>
#include <vgui_controls/pch_vgui_controls.h>
#include <vgui_controls/AnimationController.h>

using namespace vgui;

class ComparisonsSettingsPage : public SettingsPage
{
    DECLARE_CLASS_SIMPLE(ComparisonsSettingsPage, SettingsPage);

    ComparisonsSettingsPage(Panel *pParent);

    ~ComparisonsSettingsPage();

    void DestroyBogusComparePanel();

    void InitBogusComparePanel();

    void OnMainDialogClosed() const;

    void OnMainDialogShow() const;

    void OnApplyChanges() override;

    void LoadSettings() override;

    //This uses OnCheckbox and not OnModified because we want to be able to enable
    // the other checkboxes regardless of whether the player clicks Apply/OK
    void OnCheckboxChecked(Panel *p) override;

    void OnTextChanged(Panel *p) override;

    MESSAGE_FUNC_PTR(CursorEnteredCallback, "OnCursorEntered", panel)
    {
        int bogusPulse = DetermineBogusPulse(panel);

        m_pBogusComparisonsPanel->SetBogusPulse(bogusPulse);
        if (bogusPulse > 0)
        {
            g_pClientMode->GetViewportAnimationController()->StartAnimationSequence(m_pComparisonsFrame, "PulseComparePanel");
        }
    }

    MESSAGE_FUNC_PTR(CursorExitedCallback, "OnCursorExited", panel)
    {
        if (DetermineBogusPulse(panel) > 0)
        {
            m_pBogusComparisonsPanel->ClearBogusPulse();
            g_pClientMode->GetViewportAnimationController()->StartAnimationSequence(m_pComparisonsFrame, "StopPulseComparePanel");
        }
    }

    MESSAGE_FUNC_INT_INT(OnComparisonResize, "OnSizeChange", wide, tall)
    {
        int scaledPad = scheme()->GetProportionalScaledValue(15);
        m_pComparisonsFrame->SetSize(wide + scaledPad, tall + float(scaledPad) * 1.5f);
        m_pBogusComparisonsPanel->SetPos(m_pComparisonsFrame->GetXPos() + scaledPad/2, m_pComparisonsFrame->GetYPos() + scaledPad);
    }

private:

    CvarToggleCheckButton<ConVarRef> *m_pCompareShow, *m_pCompareFormat, *m_pTimeShowOverall,
        *m_pTimeShowZone, *m_pVelocityShow, *m_pVelocityShowAvg, *m_pVelocityShowMax, *m_pVelocityShowEnter, 
        *m_pVelocityShowExit, *m_pSyncShow, *m_pSyncShowS1, *m_pSyncShowS2, *m_pJumpShow, *m_pStrafeShow;
    TextEntry *m_pMaxZones;
    ComboBox *m_pTimeType;
    Label *m_pTimeTypeLabel, *m_pMaxZonesLabel;
    Frame *m_pComparisonsFrame;
    C_RunComparisons *m_pBogusComparisonsPanel;

    int DetermineBogusPulse(Panel *panel) const;
};