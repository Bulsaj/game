#include "cbase.h"
#include "Timer.h"

#include "tier0/memdbgon.h"

void CTimer::Start(int start)
{
    if (m_bUsingCPMenu) return;
    m_iStartTick = start;
    SetRunning(true);

    //MOM_TODO: IDEA START:
    //Move this to mom_triggers.cpp, and make it take a CBasePlayer
    //as an argument, to pass into the RecipientFilter, so that
    //anyone who spectates a replay can start their hud_timer, but not
    //the server timer.
    DispatchStateMessage();
    //--- IDEA END


    IGameEvent *timeStartEvent = gameeventmanager->CreateEvent("timer_state");

    if (timeStartEvent)
    {
        timeStartEvent->SetBool("is_running", true);
        gameeventmanager->FireEvent(timeStartEvent);
    }
    CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
    if (pPlayer) {
        m_flTickOffsetFix[1] = GetTickIntervalOffset(pPlayer->GetAbsVelocity(), pPlayer->GetAbsOrigin(), 1);
    }
}

void CTimer::PostTime()
{
    if (steamapicontext->SteamHTTP() && steamapicontext->SteamUser() && !m_bWereCheatsActivated)
    {
        //Get required info 
        //MOM_TODO include the extra security measures for beta+
        uint64 steamID = steamapicontext->SteamUser()->GetSteamID().ConvertToUint64();
        const char* map = gpGlobals->mapname.ToCStr();
        int ticks = gpGlobals->tickcount - m_iStartTick;

        TickSet::Tickrate tickRate = TickSet::GetCurrentTickrate();
        
        //Build URL
        char webURL[512];
        Q_snprintf(webURL, 512, "http://momentum-mod.org/postscore/%llu/%s/%i/%s", steamID, map,
            ticks, tickRate.sType);

        DevLog("Ticks sent to server: %i\n", ticks);
        //Build request
        mom_UTIL->PostTime(webURL);
    }
    else
    {
        Warning("Failed to post scores online: Cannot access STEAM HTTP or Steam User!\n");
    }
}

////MOM_TODO: REMOVEME
//CON_COMMAND(mom_test_hash, "Tests SHA1 Hashing\n")
//{
//    char pathToZone[MAX_PATH];
//    char mapName[MAX_PATH];
//    V_ComposeFileName("maps", gpGlobals->mapname.ToCStr(), mapName, MAX_PATH);
//    Q_strncat(mapName, ".zon", MAX_PATH);
//    filesystem->RelativePathToFullPath(mapName, "MOD", pathToZone, MAX_PATH);
//    Log("File path is: %s\n", pathToZone);
//
//    CSHA1 sha1;
//    sha1.HashFile(pathToZone);
//    sha1.Final();
//    unsigned char hash[20];
//    sha1.GetHash(hash);
//    Log("The hash for %s is: ", mapName);
//    for (int i = 0; i < 20; i++)
//    {
//        Log("%02x", hash[i]);
//    }
//    Log("\n");
//}

//Called upon map load, loads any and all times stored in the <mapname>.tim file
void CTimer::LoadLocalTimes(const char *szMapname)
{
    char timesFilePath[MAX_PATH];

    V_ComposeFileName(MAP_FOLDER, UTIL_VarArgs("%s%s", szMapname, EXT_TIME_FILE), timesFilePath, MAX_PATH);

    KeyValues *timesKV = new KeyValues(szMapname);

    if (timesKV->LoadFromFile(filesystem, timesFilePath, "MOD"))
    {
        for (KeyValues *kv = timesKV->GetFirstSubKey(); kv; kv = kv->GetNextKey())
        {
            Time t = Time();
            t.time_sec = Q_atof(kv->GetName());
            t.tickrate = kv->GetFloat("rate");
            t.date = static_cast<time_t>(kv->GetInt("date"));
            t.flags = kv->GetInt("flags");

            for (KeyValues *subKv = kv->GetFirstSubKey(); subKv; subKv = subKv->GetNextKey()) 
            {
                if (!Q_strnicmp(subKv->GetName(), "stage", strlen("stage")))
                {
                    int i = Q_atoi(subKv->GetName() + 6); //atoi will need to ignore "stage " and only return the stage number
                    t.stagejumps[i] = subKv->GetInt("num_jumps");
                    t.stagestrafes[i] = subKv->GetInt("num_strafes");
                    t.stagetime[i] = subKv->GetFloat("time");
                    t.stageentertime[i] = subKv->GetFloat("enter_time");
                    t.stageavgsync[i] = subKv->GetFloat("avg_sync");
                    t.stageavgsync2[i] = subKv->GetFloat("avg_sync2");

                    //3D Velocity Stats
                    t.stageavgvel[i][0] = subKv->GetFloat("avg_vel");
                    t.stagemaxvel[i][0] = subKv->GetFloat("max_vel");
                    t.stagestartvel[i][0] = subKv->GetFloat("stage_enter_vel");
                    t.stageexitvel[i][0] = subKv->GetFloat("stage_exit_vel");

                    //2D Velocity Stats
                    t.stageavgvel[i][1] = subKv->GetFloat("avg_vel_2D");
                    t.stagemaxvel[i][1] = subKv->GetFloat("max_vel_2D");
                    t.stagestartvel[i][1] = subKv->GetFloat("stage_enter_vel_2D");
                    t.stageexitvel[i][1] = subKv->GetFloat("stage_exit_vel_2D");
                }
                if (!Q_strcmp(subKv->GetName(), "total"))
                {
                    t.stagejumps[0] = subKv->GetInt("jumps");
                    t.stagestrafes[0] = subKv->GetInt("strafes");
                    t.stageavgsync[0] = subKv->GetFloat("avgsync");
                    t.stageavgsync2[0] = subKv->GetFloat("avgsync2");
                    //3D
                    t.stageavgvel[0][0] = subKv->GetFloat("avg_vel");
                    t.stagemaxvel[0][0] = subKv->GetFloat("max_vel");
                    t.stagestartvel[0][0] = subKv->GetFloat("start_vel");
                    t.stageexitvel[0][0] = subKv->GetFloat("end_vel");
                    //2D
                    t.stageavgvel[0][1] = subKv->GetFloat("avg_vel_2D");
                    t.stagemaxvel[0][1] = subKv->GetFloat("max_vel_2D");
                    t.stagestartvel[0][1] = subKv->GetFloat("start_vel_2D");
                    t.stageexitvel[0][1] = subKv->GetFloat("end_vel_2D");
                }     
            }
            localTimes.AddToTail(t);
        }
    }
    else
    {
        DevLog("Failed to load local times; no local file was able to be loaded!\n");
    }
    timesKV->deleteThis();
}

//Called every time a new time is achieved
void CTimer::SaveTime()
{
    const char *szMapName = gpGlobals->mapname.ToCStr();
    KeyValues *timesKV = new KeyValues(szMapName);
    int count = localTimes.Count();

    IGameEvent *runSaveEvent = gameeventmanager->CreateEvent("run_save");

    for (int i = 0; i < count; i++)
    {
        Time t = localTimes[i];
        char timeName[512];
        Q_snprintf(timeName, 512, "%.6f", t.time_sec);
        KeyValues *pSubkey = new KeyValues(timeName);
        pSubkey->SetFloat("rate", t.tickrate);
        pSubkey->SetInt("date", t.date);
        pSubkey->SetInt("flags", t.flags);
        
        KeyValues *pOverallKey = new KeyValues("total");
        pOverallKey->SetInt("jumps", t.stagejumps[0]);
        pOverallKey->SetInt("strafes", t.stagestrafes[0]);
        pOverallKey->SetFloat("avgsync", t.stageavgsync[0]);
        pOverallKey->SetFloat("avgsync2", t.stageavgsync2[0]);

        pOverallKey->SetFloat("start_vel", t.stagestartvel[0][0]);
        pOverallKey->SetFloat("end_vel", t.stageexitvel[0][0]);
        pOverallKey->SetFloat("avg_vel", t.stageavgvel[0][0]);
        pOverallKey->SetFloat("max_vel", t.stagemaxvel[0][0]);

        pOverallKey->SetFloat("start_vel_2D", t.stagestartvel[0][1]);
        pOverallKey->SetFloat("end_vel_2D", t.stageexitvel[0][1]);
        pOverallKey->SetFloat("avg_vel_2D", t.stageavgvel[0][1]);
        pOverallKey->SetFloat("max_vel_2D", t.stagemaxvel[0][1]);

        char stageName[9]; // "stage 64\0"
        if (GetStageCount() > 1)
        {
            for (int i2 = 1; i2 <= GetStageCount(); i2++) 
            {
                Q_snprintf(stageName, sizeof(stageName), "stage %d", i2);

                KeyValues *pStageKey = new KeyValues(stageName);
                pStageKey->SetFloat("time", t.stagetime[i2]);
                pStageKey->SetFloat("enter_time", t.stageentertime[i2]);
                pStageKey->SetInt("num_jumps", t.stagejumps[i2]);
                pStageKey->SetInt("num_strafes", t.stagestrafes[i2]);
                pStageKey->SetFloat("avg_sync", t.stageavgsync[i2]);
                pStageKey->SetFloat("avg_sync2", t.stageavgsync2[i2]);

                pStageKey->SetFloat("avg_vel", t.stageavgvel[i2][0]);
                pStageKey->SetFloat("max_vel", t.stagemaxvel[i2][0]);
                pStageKey->SetFloat("stage_enter_vel", t.stagestartvel[i2][0]);
                pStageKey->SetFloat("stage_exit_vel", t.stageexitvel[i2][0]);

                pStageKey->SetFloat("avg_vel_2D", t.stageavgvel[i2][1]);
                pStageKey->SetFloat("max_vel_2D", t.stagemaxvel[i2][1]);
                pStageKey->SetFloat("stage_enter_vel_2D", t.stagestartvel[i2][1]);
                pStageKey->SetFloat("stage_exit_vel_2D", t.stageexitvel[i2][1]);

                pSubkey->AddSubKey(pStageKey);
            }
        }

        pSubkey->AddSubKey(pOverallKey);
        timesKV->AddSubKey(pSubkey);
    }

    char file[MAX_PATH];

    V_ComposeFileName(MAP_FOLDER, UTIL_VarArgs("%s%s", szMapName, EXT_TIME_FILE), file, MAX_PATH);

    if (timesKV->SaveToFile(filesystem, file, "MOD", true) && runSaveEvent)
    {
        runSaveEvent->SetBool("run_saved", true);
        gameeventmanager->FireEvent(runSaveEvent);
        Log("Successfully saved new time!\n");
    }
    timesKV->deleteThis(); //We don't need to delete sub KV pointers e.g. pSubkey because this destructor deletes all child nodes
}

void CTimer::Stop(bool endTrigger /* = false */)
{
    CMomentumPlayer *pPlayer = ToCMOMPlayer(UTIL_GetLocalPlayer());

    IGameEvent *runSaveEvent = gameeventmanager->CreateEvent("run_save");
    IGameEvent *timeStopEvent = gameeventmanager->CreateEvent("timer_state");

    if (endTrigger && !m_bWereCheatsActivated && pPlayer)
    {
        // Post time to leaderboards if they're online
        // and if cheats haven't been turned on this session
        if (SteamAPI_IsSteamRunning())
            PostTime();

        //Save times locally too, regardless of SteamAPI condition
        Time t = Time();
        t.time_sec = static_cast<float>(gpGlobals->tickcount - m_iStartTick) * gpGlobals->interval_per_tick;
        t.tickrate = gpGlobals->interval_per_tick;
        t.flags = pPlayer->m_iRunFlags;
        time(&t.date);

        //OVERALL STATS - STAGE 0
        t.stagejumps[0] = pPlayer->m_nStageJumps[0];
        t.stagestrafes[0] = pPlayer->m_nStageStrafes[0];
        t.stageavgsync[0] = pPlayer->m_flStageStrafeSyncAvg[0];
        t.stageavgsync2[0] = pPlayer->m_flStageStrafeSync2Avg[0];
        for (int j = 0; j < 2; j++)
        {
            t.stageavgvel[0][j] = pPlayer->m_flStageVelocityAvg[0][j];
            t.stagemaxvel[0][j] = pPlayer->m_flStageVelocityMax[0][j];
            t.stagestartvel[0][j] = pPlayer->m_flStageEnterVelocity[0][j];
            t.stageexitvel[0][j] =  pPlayer->m_flStageExitVelocity[0][j];
        }
        // --------
        if (GetStageCount() > 1) //don't save stage specific stats if we are on a linear map
        {
            for (int i = 1; i <= GetStageCount(); i++) //stages start at 1 since stage 0 is overall stats
            {
                t.stageentertime[i] = m_iStageEnterTime[i];
                t.stagetime[i] = i == GetStageCount() ? (t.time_sec - m_iStageEnterTime[i]) :
                    m_iStageEnterTime[i+1] - m_iStageEnterTime[i]; //each stage's total time is the time from the previous stage to this one
                t.stagejumps[i] = pPlayer->m_nStageJumps[i];
                t.stagestrafes[i] = pPlayer->m_nStageStrafes[i];
                t.stageavgsync[i] = pPlayer->m_flStageStrafeSyncAvg[i];
                t.stageavgsync2[i] = pPlayer->m_flStageStrafeSync2Avg[i];
                for (int k = 0; k < 2; k++)
                {
                    t.stageavgvel[i][k] = pPlayer->m_flStageVelocityAvg[i][k];
                    t.stagemaxvel[i][k] = pPlayer->m_flStageVelocityMax[i][k];
                    t.stagestartvel[i][k] = pPlayer->m_flStageEnterVelocity[i][k];
                    t.stageexitvel[i][k] = pPlayer->m_flStageExitVelocity[i][k]; 
                } 
            }
        }   

        localTimes.AddToTail(t);

        SaveTime();

        m_flTickOffsetFix[0] = GetTickIntervalOffset(pPlayer->GetAbsVelocity(), pPlayer->GetAbsOrigin(), 0);
    }
    else if (runSaveEvent) //reset run saved status to false if we cant or didn't save
    {  
        runSaveEvent->SetBool("run_saved", false);
        gameeventmanager->FireEvent(runSaveEvent);
    }
    if (timeStopEvent)
    {
        timeStopEvent->SetBool("is_running", false);
        gameeventmanager->FireEvent(timeStopEvent);
    }
    
    if (pPlayer)
    {
        pPlayer->m_bIsInZone = endTrigger;
        pPlayer->m_bMapFinished = endTrigger;
    }
    SetRunning(false);
    DispatchStateMessage();
    m_iEndTick = gpGlobals->tickcount;
}
void CTimer::OnMapEnd(const char *pMapName)
{
    if (IsRunning())
        Stop(false);
    m_bWereCheatsActivated = false;
    SetCurrentCheckpointTrigger(nullptr);
    SetStartTrigger(nullptr);
    SetCurrentStage(nullptr);
    RemoveAllCheckpoints();
    localTimes.Purge();
    //MOM_TODO: onlineTimes.RemoveAll();
}

void CTimer::OnMapStart(const char *pMapName)
{
    SetGameModeConVars();
    m_bWereCheatsActivated = false;
    RequestStageCount();
    //DispatchMapStartMessage();
    LoadLocalTimes(pMapName);
    //MOM_TODO: g_Timer->LoadOnlineTimes();
}

void CTimer::RequestStageCount()
{
    CTriggerStage *stage = (CTriggerStage *) gEntList.FindEntityByClassname(NULL, "trigger_momentum_timer_stage");
    int iCount = 1;//CTriggerStart counts as one
    while (stage)
    {
        iCount++;
        stage = (CTriggerStage *) gEntList.FindEntityByClassname(stage, "trigger_momentum_timer_stage");
    }
    m_iStageCount = iCount;
}
//This function is called every time CTriggerStage::StartTouch is called
float CTimer::CalculateStageTime(int stage)
{
    if (stage > m_iLastStage)
    {
        //If the stage is a new one, we store the time we entered this stage in
        m_iStageEnterTime[stage] = stage == 1 ? 0.0f : //Always returns 0 for first stage.
            static_cast<float>(gpGlobals->tickcount - m_iStartTick) * gpGlobals->interval_per_tick;
        CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
        if (pPlayer) {
            m_flTickOffsetFix[stage] = GetTickIntervalOffset(pPlayer->GetAbsVelocity(), pPlayer->GetAbsOrigin(), 2);
        }
    }
    m_iLastStage = stage;
    return m_iStageEnterTime[stage];
}
void CTimer::DispatchResetMessage()
{
    CSingleUserRecipientFilter user(UTIL_GetLocalPlayer());
    user.MakeReliable();
    UserMessageBegin(user, "Timer_Reset");
    MessageEnd();
}

void CTimer::DispatchStageMessage()
{
    CBasePlayer* cPlayer = UTIL_GetLocalPlayer();
    if (cPlayer && GetCurrentStage())
    {
        CSingleUserRecipientFilter user(cPlayer);
        user.MakeReliable();
        UserMessageBegin(user, "Timer_Stage");
        WRITE_LONG(GetCurrentStage()->GetStageNumber());
        MessageEnd();
    }
}

void CTimer::DispatchStateMessage()
{
    //MOM_TODO: after replay merge: change cPlayer to be an argument 
    //of this method, and make that the Recipient, so that the hud_timer
    //can start if you're spectating a replay in first person
    CBasePlayer* cPlayer = UTIL_GetLocalPlayer();
    if (cPlayer)
    {
        CSingleUserRecipientFilter user(cPlayer);
        user.MakeReliable();
        UserMessageBegin(user, "Timer_State");
        WRITE_BOOL(m_bIsRunning);
        WRITE_LONG(m_iStartTick);
        MessageEnd();
    }
}

void CTimer::DispatchCheckpointMessage()
{
    CBasePlayer* cPlayer = UTIL_GetLocalPlayer();
    if (cPlayer)
    {
        CSingleUserRecipientFilter user(cPlayer);
        user.MakeReliable();
        UserMessageBegin(user, "Timer_Checkpoint");
        WRITE_BOOL(m_bUsingCPMenu);
        WRITE_LONG(m_iCurrentStepCP + 1);
        WRITE_LONG(checkpoints.Count());
        MessageEnd();
    }
}

void CTimer::DispatchStageCountMessage()
{
    CBasePlayer* cPlayer = UTIL_GetLocalPlayer();
    if (cPlayer)
    {
        CSingleUserRecipientFilter user(cPlayer);
        user.MakeReliable();
        UserMessageBegin(user, "Timer_StageCount");
        WRITE_LONG(m_iStageCount);
        MessageEnd();
    }
}
float CTimer::GetTickIntervalOffset(const Vector velocity, const Vector origin, const int zoneType)
{
    Ray_t ray;
    Vector prevOrigin = Vector(origin.x - (velocity.x * gpGlobals->interval_per_tick),
        origin.y - (velocity.y * gpGlobals->interval_per_tick),
        origin.z - (velocity.z * gpGlobals->interval_per_tick));

    DevLog("Origin X:%f Y:%f Z:%f\n", origin.x, origin.y, origin.z);
    DevLog("Prev Origin: X:%f Y:%f Z:%f\n", prevOrigin[0], prevOrigin[1], prevOrigin[2]);
    if (zoneType == 0){
        //endzone has to have the ray _start_ before we entered the end zone, hence why we start with prevOrigin
        //and trace "forwards" to our current origin, hitting the end trigger on the way.
        CTriggerTraceEnum endTriggerTraceEnum(&ray, velocity, origin);
        ray.Init(prevOrigin, origin);
        enginetrace->EnumerateEntities(ray, true, &endTriggerTraceEnum);
    }
    else if (zoneType == 1)
    {
        //on the other hand, start zones start the ray _after_ we exited the start zone, 
        //so we start at our current origin and trace backwards to our prev origin
        CTriggerTraceEnum startTriggerTraceEnum(&ray, velocity, origin);
        ray.Init(origin, prevOrigin);
        enginetrace->EnumerateEntities(ray, true, &startTriggerTraceEnum);
    }
    else if (zoneType == 2)
    {
        //same as endzone here
        CTriggerTraceEnum stageTriggerTraceEnum(&ray, velocity, origin);
        ray.Init(prevOrigin, origin);
        enginetrace->EnumerateEntities(ray, true, &stageTriggerTraceEnum);
    }
    else
    {
        Warning("CTimer::GetTickIntervalOffset: Incorrect Zone Type!");
        return -1;
    }
    return m_flTickIntervalOffsetOut; //HACKHACK Lol
}
// override of IEntityEnumerator's EnumEntity() in order for our trace to hit zone triggers
// member of CTimer to avoid linker errors
bool CTimer::CTriggerTraceEnum::EnumEntity(IHandleEntity *pHandleEntity)
{
    trace_t tr;
    // store entity that we found on the trace
    CBaseEntity *pEnt = gEntList.GetBaseEntity(pHandleEntity->GetRefEHandle());

    if (pEnt->IsSolid())
        return false;
    enginetrace->ClipRayToEntity(*m_pRay, MASK_ALL, pHandleEntity, &tr);

    if (tr.fraction < 1.0f) // tr.fraction = 1.0 means the trace completed
    {
        float dist = m_currOrigin.DistTo(tr.endpos);
        DevLog("DIST: %f\n", dist);
        DevLog("Time offset: %f\n", dist / m_currVelocity.Length()); //velocity = dist/time, so it follows that time = distance / velocity.
        g_Timer->m_flTickIntervalOffsetOut = dist / m_currVelocity.Length();
        return true;
    }
    else
    {
        DevLog("Didn't hit a zone trigger.\n");
        return false;
    }
}
//CON_COMMAND_F(hud_timer_request_stages, "", FCVAR_DONTRECORD | FCVAR_CLIENTCMD_CAN_EXECUTE | FCVAR_HIDDEN)
//{
//    g_Timer->DispatchStageCountMessage();
//}

//set ConVars according to Gamemode. Tickrate is by in tickset.h
void CTimer::SetGameModeConVars()
{
    ConVarRef gm("mom_gamemode");
    switch (gm.GetInt())
    {
    case MOMGM_SURF:
        sv_maxvelocity.SetValue(3500);
        sv_airaccelerate.SetValue(150);
        sv_maxspeed.SetValue(260);
        break;
    case MOMGM_BHOP:
        sv_maxvelocity.SetValue(100000);
        sv_airaccelerate.SetValue(1000);
        sv_maxspeed.SetValue(260);
        break;
    case MOMGM_SCROLL:
        sv_maxvelocity.SetValue(3500);
        sv_airaccelerate.SetValue(100);
        sv_maxspeed.SetValue(250);
        break;
    case MOMGM_UNKNOWN:
    case MOMGM_ALLOWED:
        sv_maxvelocity.SetValue(3500);
        sv_airaccelerate.SetValue(150);
        sv_maxspeed.SetValue(260);
        break;
    default:
        DevWarning("[%i] GameMode not defined.\n", gm.GetInt());
        break;
    }
    DevMsg("CTimer set values:\nsv_maxvelocity: %i\nsv_airaccelerate: %i \nsv_maxspeed: %i\n",
        sv_maxvelocity.GetInt(), sv_airaccelerate.GetInt(), sv_maxspeed.GetInt());
}
//Practice mode that stops the timer and allows the player to noclip.
void CTimer::EnablePractice(CBasePlayer *pPlayer)
{
    pPlayer->SetParent(NULL);
    pPlayer->SetMoveType(MOVETYPE_NOCLIP);
    ClientPrint(pPlayer, HUD_PRINTCONSOLE, "Practice mode ON!\n");
    pPlayer->AddEFlags(EFL_NOCLIP_ACTIVE);
    g_Timer->Stop(false);

    IGameEvent *pracModeEvent = gameeventmanager->CreateEvent("practice_mode");
    if (pracModeEvent)
    {
        pracModeEvent->SetBool("has_practicemode", true);
        gameeventmanager->FireEvent(pracModeEvent);
    }

}
void CTimer::DisablePractice(CBasePlayer *pPlayer)
{
    pPlayer->RemoveEFlags(EFL_NOCLIP_ACTIVE);
    ClientPrint(pPlayer, HUD_PRINTCONSOLE, "Practice mode OFF!\n");
    pPlayer->SetMoveType(MOVETYPE_WALK);

    IGameEvent *pracModeEvent = gameeventmanager->CreateEvent("practice_mode");
    if (pracModeEvent)
    {
        pracModeEvent->SetBool("has_practicemode", false);
        gameeventmanager->FireEvent(pracModeEvent);
    }

}
bool CTimer::IsPracticeMode(CBaseEntity *pOther)
{
    return pOther->GetMoveType() == MOVETYPE_NOCLIP && (pOther->GetEFlags() & EFL_NOCLIP_ACTIVE);
}
//--------- CPMenu stuff --------------------------------

void CTimer::CreateCheckpoint(CBasePlayer *pPlayer)
{
    if (!pPlayer) return;
    Checkpoint c;
    c.ang = pPlayer->GetAbsAngles();
    c.pos = pPlayer->GetAbsOrigin();
    c.vel = pPlayer->GetAbsVelocity();
    checkpoints.AddToTail(c);
    m_iCurrentStepCP++;
}

void CTimer::RemoveLastCheckpoint()
{
    if (checkpoints.IsEmpty()) return;
    checkpoints.Remove(m_iCurrentStepCP);
    m_iCurrentStepCP--;//If there's one element left, we still need to decrease currentStep to -1
}

void CTimer::TeleportToCP(CBasePlayer* cPlayer, int cpNum)
{
    if (checkpoints.IsEmpty() || !cPlayer) return;
    Checkpoint c = checkpoints[cpNum];
    cPlayer->Teleport(&c.pos, &c.ang, &c.vel);
}

void CTimer::SetUsingCPMenu(bool pIsUsingCPMenu)
{
    m_bUsingCPMenu = pIsUsingCPMenu;
}

void CTimer::SetCurrentCPMenuStep(int pNewNum)
{
    m_iCurrentStepCP = pNewNum;
}

//--------- CTriggerOnehop stuff --------------------------------

int CTimer::AddOnehopToListTail(CTriggerOnehop *pTrigger)
{
    return onehops.AddToTail(pTrigger);
}

bool CTimer::RemoveOnehopFromList(CTriggerOnehop *pTrigger)
{
    return onehops.FindAndRemove(pTrigger);
}

int CTimer::FindOnehopOnList(CTriggerOnehop *pTrigger)
{
    return onehops.Find(pTrigger);
}

CTriggerOnehop *CTimer::FindOnehopOnList(int pIndexOnList)
{
    return onehops.Element(pIndexOnList);
}

//--------- Commands --------------------------------

class CTimerCommands
{
public:
    static void ResetToStart()
    {
        CBasePlayer* cPlayer = UTIL_GetCommandClient();
        CTriggerTimerStart *start;
        if ((start = g_Timer->GetStartTrigger()) != NULL && cPlayer)
        {
            // Don't set angles if still in start zone.
            if (g_Timer->IsRunning() && start->GetHasLookAngles())
            {
                QAngle ang = start->GetLookAngles();

                cPlayer->Teleport(&start->WorldSpaceCenter(), &ang, &vec3_origin);
            }
            else
            {
                cPlayer->Teleport(&start->WorldSpaceCenter(), NULL, &vec3_origin);
            }
        }
    }

    static void ResetToCheckpoint()
    {
        CTriggerStage *stage;
        CBaseEntity* pPlayer = UTIL_GetCommandClient();
        if ((stage = g_Timer->GetCurrentStage()) != NULL && pPlayer)
        {
            pPlayer->Teleport(&stage->WorldSpaceCenter(), NULL, &vec3_origin);
        }
    }

    static void CPMenu(const CCommand &args)
    {
        if (!g_Timer->IsUsingCPMenu())
            g_Timer->SetUsingCPMenu(true);

        if (g_Timer->IsRunning())
        {
            // MOM_TODO: consider
            // 1. having a local timer running, as people may want to time their routes they're using CP menu for
            // 2. gamemodes (KZ) where this is allowed

            ConVarRef gm("mom_gamemode");
            switch (gm.GetInt())
            {
            case MOMGM_SURF:
            case MOMGM_BHOP:
            case MOMGM_SCROLL:
                g_Timer->Stop(false);

                //case MOMGM_KZ:
            default:
                break;
            }
        }
        if (args.ArgC() > 1)
        {
            int sel = Q_atoi(args[1]);
            CBasePlayer* cPlayer = UTIL_GetCommandClient();
            switch (sel)
            {
            case 1://create a checkpoint
                g_Timer->CreateCheckpoint(cPlayer);
                break;

            case 2://load previous checkpoint
                g_Timer->TeleportToCP(cPlayer, g_Timer->GetCurrentCPMenuStep());
                break;

            case 3://cycle through checkpoints forwards (+1 % length)
                if (g_Timer->GetCPCount() > 0)
                {
                    g_Timer->SetCurrentCPMenuStep((g_Timer->GetCurrentCPMenuStep() + 1) % g_Timer->GetCPCount());
                    g_Timer->TeleportToCP(cPlayer, g_Timer->GetCurrentCPMenuStep());
                }
                break;

            case 4://cycle backwards through checkpoints
                if (g_Timer->GetCPCount() > 0)
                {
                    g_Timer->SetCurrentCPMenuStep(g_Timer->GetCurrentCPMenuStep() == 0 ? g_Timer->GetCPCount() - 1 : g_Timer->GetCurrentCPMenuStep() - 1);
                    g_Timer->TeleportToCP(cPlayer, g_Timer->GetCurrentCPMenuStep());
                }
                break;

            case 5://remove current checkpoint
                g_Timer->RemoveLastCheckpoint();
                break;
            case 6://remove every checkpoint
                g_Timer->RemoveAllCheckpoints();
                break;
            case 0://They closed the menu
                g_Timer->SetUsingCPMenu(false);
                break;
            default:
                if (cPlayer != NULL)
                {
                    cPlayer->EmitSound("Momentum.UIMissingMenuSelection");
                }
                break;
            }
        }
        g_Timer->DispatchCheckpointMessage();
    }

    static void PracticeMove()
    {
        CBasePlayer *pPlayer = UTIL_GetCommandClient();
        if (!pPlayer)
            return;
        Vector velocity = pPlayer->GetAbsVelocity();

        if (!g_Timer->IsPracticeMode(pPlayer))
        {
            if (velocity.Length2DSqr() != 0)
                DevLog("You cannot enable practice mode while moving!\n");
            else
                g_Timer->EnablePractice(pPlayer);
        }
        else //player is either already in practice mode
            g_Timer->DisablePractice(pPlayer);
    }
};


static ConCommand mom_practice("mom_practice", CTimerCommands::PracticeMove, "Toggle. Stops timer and allows player to fly around in noclip.\n" 
    "Only activates when player is standing still (xy vel = 0)\n",
    FCVAR_CLIENTCMD_CAN_EXECUTE);
static ConCommand mom_reset_to_start("mom_restart", CTimerCommands::ResetToStart, "Restarts the player to the start trigger.\n",
    FCVAR_CLIENTCMD_CAN_EXECUTE | FCVAR_SERVER_CAN_EXECUTE);
static ConCommand mom_reset_to_checkpoint("mom_reset", CTimerCommands::ResetToCheckpoint, "Teleports the player back to the start of the current stage.\n",
    FCVAR_CLIENTCMD_CAN_EXECUTE | FCVAR_SERVER_CAN_EXECUTE);
static ConCommand mom_cpmenu("cpmenu", CTimerCommands::CPMenu, "", FCVAR_HIDDEN | FCVAR_SERVER_CAN_EXECUTE | FCVAR_CLIENTCMD_CAN_EXECUTE);

static CTimer s_Timer;
CTimer *g_Timer = &s_Timer;