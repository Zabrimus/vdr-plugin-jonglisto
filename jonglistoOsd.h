/*
 * jonglistoOsd.h
 *
 *  Created on: 13.02.2018
 *      Author: rh
 */

#ifndef JONGLISTOOSD_H_
#define JONGLISTOOSD_H_

#include <vdr/osdbase.h>
#include <vdr/menu.h>

//***************************************************************************
// Plugin menus
//***************************************************************************

class cJonglistoPluginMenu: public cOsdMenu {
public:
    cJonglistoPluginMenu(const char* title, const unsigned int sPort);
    virtual ~cJonglistoPluginMenu() {};
    virtual eOSState ProcessKey(eKeys key);

private:
};

//***************************************************************************
// Channel favourite menus
//***************************************************************************

class cJonglistoFavouriteMenu: public cOsdMenu {
public:
    cJonglistoFavouriteMenu(const char* title);
    virtual ~cJonglistoFavouriteMenu() {};
    virtual eOSState ProcessKey(eKeys key);

private:

};

class cJonglistoEpgListMenu: public cOsdMenu {
public:
    cJonglistoEpgListMenu(const char* title);
    ~cJonglistoEpgListMenu();
    virtual eOSState ProcessKey(eKeys key);

    void setTime(time_t time);
    void addMinutes(const unsigned int m);
    void subMinutes(const unsigned int m);

private:
    time_t currentTime;
    const char *preselectedTimeValues[11];
    unsigned int preselectedTimeSize;
    int preselectedTime;

    cOsdItem *currentTimeItem;

    void Setup();
    const cChannel* GetChannel(const char* channelID);
    const cEvent* GetEvent(const cChannel *ch);
    void showEpgDetails(const char* title);
};

class cJonglistoEpgDetailMenu: public cMenuEvent {
public:
    cJonglistoEpgDetailMenu(const cTimers *Timers, const cChannels *Channels, const cEvent *Event);
    virtual ~cJonglistoEpgDetailMenu() {};
    virtual eOSState ProcessKey(eKeys key);

private:
    const cEvent *event;
    eOSState Record();
};

//***************************************************************************
// Alarm menus
//***************************************************************************

class cJonglistoAlarmMenu: public cOsdMenu {
public:
    cJonglistoAlarmMenu(const char* title);
    ~cJonglistoAlarmMenu();
    virtual eOSState ProcessKey(eKeys key);

private:
    void Setup();
};

//***************************************************************************
// VDR menus
//***************************************************************************

class cJonglistoVdrMenu: public cOsdMenu {
public:
    cJonglistoVdrMenu(const char* title);
    ~cJonglistoVdrMenu();
    virtual eOSState ProcessKey(eKeys key);

private:
    void Setup();
};

class cJonglistoVdrDetailMenu: public cOsdMenu {
public:
    cJonglistoVdrDetailMenu(const char* title);
    ~cJonglistoVdrDetailMenu();
    virtual eOSState ProcessKey(eKeys key);

private:
    void Setup();
};

#endif /* JONGLISTOOSD_H_ */
