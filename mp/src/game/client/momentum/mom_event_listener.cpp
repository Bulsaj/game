#include "cbase.h"

#include "mom_event_listener.h"
#include "mom_player_shared.h"
#include "tier0/memdbgon.h"

void C_Momentum_EventListener::Init()
{
    //add listeners for all of our custom events
    ListenForGameEvent("timer_stopped");
    ListenForGameEvent("new_stage_enter");
    ListenForGameEvent("new_stage_exit");
    ListenForGameEvent("run_save");
    ListenForGameEvent("run_upload");
    ListenForGameEvent("timer_state");
    ListenForGameEvent("practice_mode");
    ListenForGameEvent("keypress");
}

void C_Momentum_EventListener::FireGameEvent(IGameEvent *pEvent)
{
    if (!Q_strcmp("timer_stopped", pEvent->GetName()))
    {
        m_flStageStrafeSyncAvg[0] = pEvent->GetFloat("avg_sync");
        m_flStageStrafeSync2Avg[0] = pEvent->GetFloat("avg_sync2");
        //3D
        m_flStageStartSpeed[0][0] = pEvent->GetFloat("start_vel");
        m_flStageExitSpeed[0][0] = pEvent->GetFloat("end_vel");
        m_flStageVelocityAvg[0][0] = pEvent->GetFloat("avg_vel");
        m_flStageVelocityMax[0][0] = pEvent->GetFloat("max_vel");
        //2D
        m_flStageStartSpeed[0][1] = pEvent->GetFloat("start_vel_2D");
        m_flStageExitSpeed[0][1] = pEvent->GetFloat("end_vel_2D");
        m_flStageVelocityAvg[0][1] = pEvent->GetFloat("avg_vel_2D");
        m_flStageVelocityMax[0][1] = pEvent->GetFloat("max_vel_2D");
    }
    else if (!Q_strcmp("new_stage_enter", pEvent->GetName()))
    {
        //MOM_TODO: we do not need to store m_iCurrentStage anymore?
        m_iCurrentStage = pEvent->GetInt("stage_num");
        //Note: stage_enter_time will NOT change upon multiple entries to the same stage trigger (only set once per run)
        m_flStageEnterTime[m_iCurrentStage] = pEvent->GetFloat("stage_enter_time");
        
        if (m_iCurrentStage > 1) //MOM_TODO: || m_iStageCount < 2 (linear maps use checkpoints?)
        {
            //The first stage doesn't have its time yet, we calculate it upon going into stage 2+
            m_flStageTime[m_iCurrentStage - 1] = m_flStageEnterTime[m_iCurrentStage] - m_flStageEnterTime[m_iCurrentStage - 1];
        } 

        m_flStageStrafeSyncAvg[m_iCurrentStage] = pEvent->GetFloat("avg_sync");
        m_flStageStrafeSync2Avg[m_iCurrentStage] = pEvent->GetFloat("avg_sync2");

        m_flStageStartSpeed[m_iCurrentStage][0] = pEvent->GetFloat("stage_enter_vel");
        m_flStageVelocityAvg[m_iCurrentStage][0] = pEvent->GetFloat("avg_vel");
        m_flStageVelocityMax[m_iCurrentStage][0] = pEvent->GetFloat("max_vel");

        m_flStageStartSpeed[m_iCurrentStage][1] = pEvent->GetFloat("stage_enter_vel_2D");
        m_flStageVelocityAvg[m_iCurrentStage][1] = pEvent->GetFloat("avg_vel_2D");
        m_flStageVelocityMax[m_iCurrentStage][1] = pEvent->GetFloat("max_vel_2D");
    }
    else if (!Q_strcmp("new_stage_exit", pEvent->GetName()))
    {
        m_flStageExitSpeed[m_iCurrentStage][0] = pEvent->GetFloat("stage_exit_vel");
        m_flStageExitSpeed[m_iCurrentStage][1] = pEvent->GetFloat("stage_exit_vel_2D");
    }
    else if (!Q_strcmp("run_save", pEvent->GetName()))
    {
        m_bTimeDidSave = pEvent->GetBool("run_saved");
    }
    else if (!Q_strcmp("run_upload", pEvent->GetName()))
    {
        m_bTimeDidUpload = pEvent->GetBool("run_posted");
        Q_strncpy(m_szRunUploadStatus, pEvent->GetString("web_msg"), sizeof(m_szRunUploadStatus));
        //MOM_TODO: potentially have stuff like new rank or something?
    }
    else if (!Q_strcmp("timer_state", pEvent->GetName()))
    {
        m_bTimerIsRunning = pEvent->GetBool("is_running");
    }
    else if (!Q_strcmp("practice_mode", pEvent->GetName()))
    {
        m_bPlayerHasPracticeMode = pEvent->GetBool("has_practicemode");
    }
    else if (!Q_strcmp("keypress", pEvent->GetName()))
    {
        m_iStageJumps[0] = pEvent->GetInt("num_jumps");
        m_iStageStrafes[0] = pEvent->GetInt("num_strafes");
    }
}

//Interface this event listener to the DLL
static C_Momentum_EventListener s_momListener;
C_Momentum_EventListener *g_MOMEventListener = &s_momListener;