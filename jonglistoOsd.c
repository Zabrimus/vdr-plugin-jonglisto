/*
 * jonglistoOsd.c
 */

#include <algorithm>

#include <vdr/channels.h>
#include <vdr/menu.h>
#include <vdr/timers.h>
#include <vdr/menu.h>
#include <vdr/tools.h>
#include <vdr/svdrp.h>

#include "jonglistoOsd.h"


static unsigned int svdrpPort;
static char* servername = NULL;

static cStringList favourites;
static cStringList channels;
static cStringList alarmIds;
static cStringList vdrNames;

static cStringList jonglistoResponse;

//***************************************************************************
// main menu
//***************************************************************************
cJonglistoPluginMenu::cJonglistoPluginMenu(const char* title, unsigned int sPort, const char* name) : cOsdMenu(title) {
    svdrpPort = sPort;
    servername= strdup(name);

    Clear();

    cOsdMenu::Add(new cOsdItem(tr("Show favourites")));
    cOsdMenu::Add(new cOsdItem(tr("Show alarms")));
    cOsdMenu::Add(new cOsdItem(tr("Show VDRs")));

    SetHelp(0, 0, 0,0);
    Display();
}

cJonglistoPluginMenu::~cJonglistoPluginMenu() {
    if (servername != NULL) {
        delete servername;
        servername = NULL;
    }
}

eOSState cJonglistoPluginMenu::ProcessKey(eKeys key) {
    eOSState state = cOsdMenu::ProcessKey(key);

    if (state != osUnknown)
        return state;

    switch (key) {
        case kOk: {
            if (Current() == 0) {
                // show channel favourites
                return AddSubMenu(new cJonglistoFavouriteMenu(cOsdMenu::Get(Current())->cOsdItem::Text()));
            } else if (Current() == 1) {
                // show alarms
                return AddSubMenu(new cJonglistoAlarmMenu(tr("Alarms")));
            } else if (Current() == 2) {
                // show alarms
                return AddSubMenu(new cJonglistoVdrMenu(tr("VDR")));
            }


            return osEnd;
        }

        default:
            break;
    }

    return state;
}

//***************************************************************************
// channel favourite menu
//***************************************************************************

cJonglistoFavouriteMenu::cJonglistoFavouriteMenu(const char* title) : cOsdMenu(title) {
    Clear();

    isyslog("Servername %s\n", servername);

    if (ExecSVDRPCommand(servername, *cString::sprintf("FAVL %d", svdrpPort), &jonglistoResponse)) {
        if (favourites.Size() > 0) {
            favourites.Clear();
        }

        for (int i = 0; i < jonglistoResponse.Size(); ++i) {
            if (startswith(jonglistoResponse.At(i), "900")) {
                favourites.Append(strdup(jonglistoResponse.At(i)));
            }
        }
    }

    for (int i = 0; i < favourites.Size(); i++) {
        cOsdMenu::Add(new cOsdItem((const char*)favourites.At(i) + 4));
    }

    SetHelp(0, 0, 0,0);
    Display();
}

eOSState cJonglistoFavouriteMenu::ProcessKey(eKeys key) {
    eOSState state = cOsdMenu::ProcessKey(key);

    if (state != osUnknown)
        return state;

    switch (key) {
        case kOk: {
            if (Current() >= 0) {
                return AddSubMenu(new cJonglistoEpgListMenu(cOsdMenu::Get(Current())->cOsdItem::Text()));
            }

            return osEnd;
        }

        default:
            break;
    }

    return state;
}

//***************************************************************************
// channel epg list menu
//***************************************************************************

cJonglistoEpgListMenu::cJonglistoEpgListMenu(const char* title) : cOsdMenu(title) {
    currentTime = time_t(0);
    time(&currentTime);
    preselectedTime = 0;

    for (int i = 0; i < 11; ++i) {
        preselectedTimeValues[i] = NULL;
    }

    preselectedTimeValues[0] = strdup(tr("Now"));
    preselectedTimeSize = 1;

    if (ExecSVDRPCommand(servername, "EPGT", &jonglistoResponse)) {
        int rsize = std::max(0, std::min(jonglistoResponse.Size(), 10));
        for (int i = 0; i < rsize; ++i) {
            preselectedTimeValues[i+1] = strdup(jonglistoResponse[i] + 4);
            preselectedTimeSize++;
        }
    }

    Setup();
}

cJonglistoEpgListMenu::~cJonglistoEpgListMenu() {
    for (int i = 0; i < 11; ++i) {
        if (!preselectedTimeValues[i]) {
            delete preselectedTimeValues[i];
        }
    }
};

eOSState cJonglistoEpgListMenu::ProcessKey(eKeys key) {
    eOSState state = cOsdMenu::ProcessKey(key);

    if (state != osUnknown)
        return state;

    switch (key) {
        case kOk: {
            if (Current() >= 2) {
                LOCK_CHANNELS_READ;
                const cChannel *ch = GetChannel(channels.At(Current()-2));
                return cDevice::PrimaryDevice()->SwitchChannel(ch, true) ? osEnd : osContinue;
            }

            break;
        }

        case kRed: {
            currentTime -= 30 * 60;
            Setup();
            break;
        }

        case kGreen: {
            currentTime += 30 * 60;
            Setup();
            break;
        }

        case kYellow: {
            if (preselectedTime > 0) {
                struct tm tm_selected;
                memset(&tm_selected, 0, sizeof(struct tm));
                strptime(preselectedTimeValues[preselectedTime], "%H:%M", &tm_selected);

                time_t now;
                struct tm * timeinfo;
                time (&now);
                timeinfo = localtime (&now);
                timeinfo->tm_hour = tm_selected.tm_hour;
                timeinfo->tm_min = tm_selected.tm_min;

                currentTime = mktime(timeinfo);

                if (currentTime < now) {
                    currentTime += 24 * 60 * 60;
                }
            } else {
                currentTime = time_t(0);
                time(&currentTime);
            }

            Setup();
            break;
        }

        case kBlue: {

            LOCK_TIMERS_READ;
            LOCK_CHANNELS_READ;
            if (Current() >= 2) {
                const cEvent *ev = GetEvent(GetChannel(channels.At(Current()-2)));

                if (ev != NULL) {
                    return AddSubMenu(new cJonglistoEpgDetailMenu(Timers, Channels, ev));
                }
            }
            break;
        }

        default:
            break;
    }

    return state;
}

void cJonglistoEpgListMenu::Setup() {
    char *buffer = NULL;

    Clear();

    LOCK_CHANNELS_READ;

    if (ExecSVDRPCommand(servername, *cString::sprintf("FAVC %s", Title()), &jonglistoResponse)) {
        if (channels.Size() > 0) {
            channels.Clear();
        }

        for (int i = 0; i < jonglistoResponse.Size(); ++i) {
            channels.Append(strdup(jonglistoResponse.At(i) + 4));
        }
    }

    SetCols(18, 9, 9);

    LOCK_SCHEDULES_READ;

    asprintf(&buffer, "%s: %s", tr("Selected time"), *DayDateTime(currentTime));
    currentTimeItem = new cOsdItem(buffer);
    currentTimeItem->SetSelectable(false);
    free(buffer);
    buffer = NULL;

    cOsdMenu::Add(currentTimeItem);
    cOsdMenu::Add(new cMenuEditStraItem(tr("Time"), &preselectedTime, preselectedTimeSize, preselectedTimeValues));

    for (int i = 0; i < channels.Size(); i++) {
        const cChannel *ch = GetChannel(channels.At(i));

        if (ch != NULL) {
            // get event for channel
            const cEvent *event = GetEvent(ch);

            if (event != NULL) {
                asprintf(&buffer, "%s\t%s\t%s\t%s", ch->Name(), *(event->GetTimeString()), *(event->GetEndTimeString()), event->Title());
                cOsdMenu::Add(new cOsdItem(buffer));
                free(buffer);
                buffer = NULL;
            } else {
                asprintf(&buffer, "%s\t\t\t%s", ch->Name(), "<event is not available>");
                cOsdMenu::Add(new cOsdItem(buffer));
                free(buffer);
                buffer = NULL;
            }
        } else {
            esyslog("jonglisto: channel %s not found.", channels.At(i));
        }
    }

    SetHelp(tr("-30 min"), tr("+30 min"), tr("Select Time"), tr("Details"));

    Display();
}

const cChannel* cJonglistoEpgListMenu::GetChannel(const char* channelID) {
    LOCK_CHANNELS_READ;

    tChannelID chid = tChannelID::FromString(channelID);
    return Channels->GetByChannelID(chid);
}

const cEvent* cJonglistoEpgListMenu::GetEvent(const cChannel *ch) {
    LOCK_SCHEDULES_READ;

    const cSchedule *schedule = Schedules->GetSchedule(ch, false);
    if (schedule != NULL) {
        return schedule->GetEventAround(currentTime);
    } else {
        return NULL;
    }
}

void cJonglistoEpgListMenu::showEpgDetails(const char* title) {
}

//***************************************************************************
// epg detail menu
//***************************************************************************

cJonglistoEpgDetailMenu::cJonglistoEpgDetailMenu(const cTimers *Timers, const cChannels *Channels, const cEvent *Event) : cMenuEvent(Timers, Channels, Event) {
    event = Event;

    Clear();

    SetHelp(tr("Record"), tr("Alarm"), 0, tr("Switch"));

    Display();
}

eOSState cJonglistoEpgDetailMenu::ProcessKey(eKeys key) {
    eOSState state = cOsdMenu::ProcessKey(key);

    if (state != osUnknown)
        return state;

    switch (key) {
        case kRed: {
            return Record();
        }

        case kGreen: {
            cString command = cString::sprintf("ALRM %u %lu %s %s", svdrpPort, event->StartTime() - 2 * 60, *event->ChannelID().ToString(), event->Title());

            if (ExecSVDRPCommand(servername, *command, &jonglistoResponse)) {
                if (jonglistoResponse.Size() > 0) {
                    if (startswith(jonglistoResponse.At(0), "900")) {
                        Skins.Message(mtInfo, tr("Alarm timer created"));
                        return osContinue;
                    } else {
                        Skins.Message(mtError, tr("Alarm timer not created"));
                    }
                } else {
                    Skins.Message(mtError, tr("No response reveived"));
                }
            } else {
                Skins.Message(mtError, tr("Connection to jonglisto-ng failed"));
            }

            break;
        }

        case kBlue: {
            LOCK_CHANNELS_READ;
            return cDevice::PrimaryDevice()->SwitchChannel(Channels->GetByChannelID(event->ChannelID()), true) ? osEnd : osContinue;
        }

        default:
            break;
    }

    return state;
}

eOSState cJonglistoEpgDetailMenu::Record(void) {
    if (event != NULL) {
        LOCK_TIMERS_WRITE;
        LOCK_CHANNELS_READ;
        LOCK_SCHEDULES_READ;
        Timers->SetExplicitModify();

        if (Timers->GetMatch(event) == NULL) {
            cTimer *Timer = new cTimer(event);
            if (Setup.SVDRPPeering && *Setup.SVDRPDefaultHost) {
               Timer->SetRemote(Setup.SVDRPDefaultHost);
            }

            Timers->Add(Timer);
            Timers->SetModified();

            cString ErrorMessage;
            if (!HandleRemoteTimerModifications(Timer, NULL, &ErrorMessage)) {
                Timers->Del(Timer);
                Skins.Message(mtError, ErrorMessage);
            }
        }

        if (HasSubMenu()) {
           CloseSubMenu();
        }
    }

    return osEnd;
}

//***************************************************************************
// alarm list menu
//***************************************************************************

cJonglistoAlarmMenu::cJonglistoAlarmMenu(const char* title) : cOsdMenu(title) {
    Setup();
}

cJonglistoAlarmMenu::~cJonglistoAlarmMenu() {
    if (alarmIds.Size() > 0) {
        alarmIds.Clear();
    }
}

eOSState cJonglistoAlarmMenu::ProcessKey(eKeys key) {
    eOSState state = cOsdMenu::ProcessKey(key);

    if (state != osUnknown)
        return state;

    if (Current() >= 0) {
        switch (key) {
            case kOk: {
                return osEnd;
            }

            case kRed: {
                cStringList response;
                cString cmd = cString::sprintf("ALRC toggle %s", alarmIds.At(Current()));
                if (ExecSVDRPCommand(servername, *cmd, &jonglistoResponse)) {
                    if (jonglistoResponse.Size() > 0) {
                        if (startswith(jonglistoResponse.At(0), "900")) {
                            Setup();
                            return osContinue;
                        } else {
                            Skins.Message(mtError, tr("set on/off failed"));
                        }
                    } else {
                        Skins.Message(mtError, tr("got no response from jonglisto-ng"));
                    }
                } else {
                    Skins.Message(mtError, tr("command failed"));
                }
                break;
            }

            case kGreen: {
                break;
            }

            case kYellow: {
                cStringList response;
                cString cmd = cString::sprintf("ALRC delete %s", alarmIds.At(Current()));
                if (ExecSVDRPCommand(servername, *cmd, &jonglistoResponse)) {
                    if (jonglistoResponse.Size() > 0) {
                        if (startswith(jonglistoResponse.At(0), "900")) {
                            Setup();
                        } else {
                            Skins.Message(mtError, tr("deletion failed"));
                        }
                    } else {
                        Skins.Message(mtError, tr("got no response from jonglisto-ng"));
                    }
                } else {
                    Skins.Message(mtError, tr("command failed"));
                }
                break;
            }

            case kBlue: {
                break;
            }

            default:
                break;
        }
    }

    return state;
}

void cJonglistoAlarmMenu::Setup() {
    char *rest = NULL;
    char *id = NULL;
    char *active = NULL;
    char *nextSchedule = NULL;
    char *channelId = NULL;
    char *alarmTitle = NULL;

    Clear();

    SetCols(3, 15, 22);

    if (ExecSVDRPCommand(servername, *cString::sprintf("ALRL %u", svdrpPort), &jonglistoResponse)) {
        if (jonglistoResponse.Size() > 0) {
            LOCK_CHANNELS_READ;

            if (alarmIds.Size() > 0) {
                alarmIds.Clear();
            }

            for (int i = 0; i < jonglistoResponse.Size(); i++) {
                if (startswith(jonglistoResponse.At(i), "900") && strlen(jonglistoResponse.At(i)) > 5) {
                    rest = jonglistoResponse.At(i) + 4;

                    id = strtok_r(rest, " ", &rest);
                    active = strtok_r(rest, " ", &rest);
                    nextSchedule = strtok_r(rest, " ", &rest);
                    channelId = strtok_r(rest, " ", &rest);
                    alarmTitle = rest;

                    if (id == NULL || active == NULL || nextSchedule == NULL || channelId == NULL || alarmTitle == NULL) {
                        // wrong answer
                        // ignore
                    } else {
                        const tChannelID chid = tChannelID::FromString(channelId);
                        const cChannel *ch = Channels->GetByChannelID(chid);

                        alarmIds.Append(strdup(id));

                        cString itemStr = cString::sprintf("%s\t%s\t%s\t%s",
                                active[0] == '1' ? "O" : "",
                                nextSchedule[0] == '0' ? tr("never") : *DayDateTime(atol(nextSchedule)),
                                ch->Name(),
                                alarmTitle);

                        cOsdMenu::Add(new cOsdItem(itemStr));
                    }

                    SetHelp(tr("on/off"), 0, tr("Delete"), 0);
                } else {
                    Skins.Message(mtError, tr("No alarms defined"));
                    return;
                }
            }
        } else {
            Skins.Message(mtError, tr("No alarms defined"));
            return;
        }
    } else {
        Skins.Message(mtError, tr("Could not get alarms"));
        return;
    }

    Display();
}


//***************************************************************************
// vdr menu
//***************************************************************************

cJonglistoVdrMenu::cJonglistoVdrMenu(const char* title) : cOsdMenu(title) {
    Setup();
}

cJonglistoVdrMenu::~cJonglistoVdrMenu() {
    if (vdrNames.Size() > 0) {
        vdrNames.Clear();
    }
}

eOSState cJonglistoVdrMenu::ProcessKey(eKeys key) {
    eOSState state = cOsdMenu::ProcessKey(key);

    if (state != osUnknown)
        return state;

    if (Current() >= 0) {
        switch (key) {
            case kOk: {
                break;
            }

            case kRed: {
                if (Current() >= 0) {
                    // check VDR
                    cString req = cString::sprintf("VDRP %s", vdrNames.At(Current()));

                    if (ExecSVDRPCommand(servername, *req, &jonglistoResponse)) {
                        if (jonglistoResponse.Size() >= 1) {
                            if (startswith(jonglistoResponse.At(0), "900")) {
                                return AddSubMenu(new cJonglistoVdrDetailMenu(vdrNames.At(Current())));
                            } else if (startswith(jonglistoResponse.At(0), "901")) {
                                Skins.Message(mtError, tr("System is alive, VDR is down"));
                            } else if (startswith(jonglistoResponse.At(0), "902")) {
                                Skins.Message(mtError, tr("System is down"));
                            } else {
                                Skins.Message(mtError, tr("Unknown VDR"));
                            }
                        } else {
                            Skins.Message(mtError, tr("Connection to jonglisto-ng failed"));
                        }
                    } else {
                        Skins.Message(mtError, tr("Connection to jonglisto-ng failed"));
                    }
                }
                break;
            }

            case kGreen: {
                // wake on lan
                if (Current() >= 0) {
                    cString req = cString::sprintf("VDRW %s", vdrNames.At(Current()));

                    if (ExecSVDRPCommand(servername, *req, &jonglistoResponse)) {
                        if (jonglistoResponse.Size() >= 1) {
                            if (startswith(jonglistoResponse.At(0), "900")) {
                                Skins.Message(mtError, tr("Wake on lan packet sent"));
                            } else {
                                Skins.Message(mtError, tr("MAC is not configured"));
                            }
                        } else {
                            Skins.Message(mtError, tr("Connection to jonglisto-ng failed"));
                        }
                    } else {
                        Skins.Message(mtError, tr("Connection to jonglisto-ng failed"));
                    }
                }

                break;
            }

            case kYellow: {
                break;
            }

            case kBlue: {
                break;
            }

            default:
                break;
        }
    }

    return state;
}

void cJonglistoVdrMenu::Setup() {
    char *rest = NULL;
    char *name = NULL;
    char *host = NULL;
    char *port = NULL;

    Clear();

    SetCols(15, 22);

    if (ExecSVDRPCommand(servername, "VDRL", &jonglistoResponse)) {
        if (jonglistoResponse.Size() > 0) {
            if (vdrNames.Size() > 0) {
                vdrNames.Clear();
            }

            for (int i = 0; i < jonglistoResponse.Size(); i++) {
                if (startswith(jonglistoResponse.At(i), "900") && strlen(jonglistoResponse.At(i)) > 5) {
                    rest = jonglistoResponse.At(i) + 4;

                    name  = strtok_r(rest, " ", &rest);
                    host = strtok_r(rest, " ", &rest);
                    port = rest;

                    if (name == NULL || host == NULL || port == NULL) {
                        // wrong answer
                        // ignore
                    } else {
                        strreplace(name, '|', ' ');
                        vdrNames.Append(strdup(name));
                        cString itemStr = cString::sprintf("%s\t%s:%s", name, host, port);
                        cOsdMenu::Add(new cOsdItem(itemStr));
                    }

                    SetHelp(tr("Details"), tr("wake on lan"), 0, 0);
                } else {
                    Skins.Message(mtError, tr("No VDR defined"));
                    return;
                }
            }
        } else {
            Skins.Message(mtError, tr("No VDR defined"));
            return;
        }
    } else {
        Skins.Message(mtError, tr("Could not get VDRs"));
        return;
    }

    Display();
}

cJonglistoVdrDetailMenu::cJonglistoVdrDetailMenu(const char* title) : cOsdMenu(title) {
    Setup();
}

cJonglistoVdrDetailMenu::~cJonglistoVdrDetailMenu() {
}

eOSState cJonglistoVdrDetailMenu::ProcessKey(eKeys key) {
    eOSState state = cOsdMenu::ProcessKey(key);

    if (state != osUnknown)
        return state;

    if (Current() >= 0) {
        switch (key) {
            case kOk: {
                return osEnd;
            }

            case kRed: {
                break;
            }

            case kGreen: {
                break;
            }

            case kYellow: {
                break;
            }

            case kBlue: {
                break;
            }

            default:
                break;
        }
    }

    return state;
}

void cJonglistoVdrDetailMenu::Setup() {
    char *rest = NULL;
    char *name = NULL;
    char *version = NULL;
    char *desc = NULL;
    char *dtotal = NULL;
    char *dfree = NULL;
    char *dperc = NULL;

    Clear();

    SetCols(15, 18);

    cString req = cString::sprintf("VDRD %s", Title());

    if (ExecSVDRPCommand(servername, *req, &jonglistoResponse)) {
        if (jonglistoResponse.Size() > 0) {
            for (int i = 0; i < jonglistoResponse.Size(); i++) {
                if (startswith(jonglistoResponse.At(i), "900") && strlen(jonglistoResponse.At(i)) > 5) {
                    rest = jonglistoResponse.At(i) + 4;
                    if (i == 0) {
                        // first line
                        version = strtok_r(rest, " ", &rest);
                        dfree = strtok_r(rest, " ", &rest);
                        dtotal = strtok_r(rest, " ", &rest);
                        dperc = rest;

                        if (version == NULL || dfree == NULL || dtotal == NULL || dperc == NULL) {
                            // wrong answer
                            // ignore
                        } else {
                            strreplace(dfree, '|', ' ');
                            strreplace(dtotal, '|', ' ');
                            cString itemStr = cString::sprintf("%s\t%s\t%s %s/%s  %u%%", tr("VDR version"), version, tr("HD: "), dfree, dtotal, atoi(dperc));
                            cOsdMenu::Add(new cOsdItem(itemStr));
                        }
                    } else {
                        name = strtok_r(rest, " ", &rest);
                        version = strtok_r(rest, " ", &rest);
                        desc  = rest;

                        if (name == NULL || version == NULL) {
                            // wrong answer
                            // ignore
                        } else {
                            cString itemStr = cString::sprintf("%s\t%s\t%s", name, version, desc != NULL ? desc : "");
                            cOsdMenu::Add(new cOsdItem(itemStr));
                        }
                    }
                } else {
                    Skins.Message(mtError, tr("No VDR defined"));
                    return;
                }
            }
        } else {
            Skins.Message(mtError, tr("No VDR defined"));
            return;
        }
    } else {
        Skins.Message(mtError, tr("Could not get VDRs"));
        return;
    }

    Display();
}
