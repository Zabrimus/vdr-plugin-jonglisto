/*
 * jonglisto.h
 */

#ifndef JONGLISTO_H_
#define JONGLISTO_H_

static const char *VERSION = "0.1.0";
static const char *DESCRIPTION = "Tool plugin for jonglisto-ng";
static const char *MAINMENUENTRY = "Jonglisto";

#include "services/scraper2vdr.h"

#include <vdr/osdbase.h>

class cPluginJonglisto: public cPlugin {
private:
    unsigned int jonglistoPort = 8080;
    unsigned int svdrpPort = 6419;
    cString jonglistoHost;

    std::stringstream printSeries(cSeries series);
    std::stringstream printMovie(cMovie movie);
    cString searchAndPrintEvent(int &ReplyCode, ScraperGetEventType *Data = NULL);

    cTimer* GetTimer(const cTimer *Timer);
    cTimer* GetTimer(long int id);

public:
    cPluginJonglisto(void);
    virtual ~cPluginJonglisto();
    virtual const char * Version(void) {
        return VERSION;
    }
    virtual const char * Description(void) {
        return DESCRIPTION;
    }
    virtual const char *CommandLineHelp(void);
    virtual bool ProcessArgs(int argc, char *argv[]);
    virtual bool Initialize(void);
    virtual bool Start(void);
    virtual void Stop(void);
    virtual void Housekeeping(void);
    virtual void MainThreadHook(void);
    virtual cString Active(void);
    virtual time_t WakeupTime(void);
    const char * MainMenuEntry(void);
    virtual cOsdObject * MainMenuAction(void);
    virtual cMenuSetupPage * SetupMenu(void);
    virtual bool SetupParse(const char *Name, const char *Value);
    virtual bool Service(const char *Id, void *Data = NULL);
    virtual const char **SVDRPHelpPages(void);
    virtual cString SVDRPCommand(const char *Command, const char *Option, int &ReplyCode);
};


#endif /* JONGLISTO_H_ */
