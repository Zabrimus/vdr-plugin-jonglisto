/*
 * jonglisto.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id$
 */
#include <string>
#include <sstream>
#include <vector>
#include <map>

#include <vdr/plugin.h>
#include <vdr/channels.h>
#include <vdr/epg.h>

#include "services/scraper2vdr.h"

static const char *VERSION = "0.0.1";
static const char *DESCRIPTION = "Tool plugin for jonglisto-ng";
static const char *MAINMENUENTRY = NULL;

class cPluginJonglisto: public cPlugin {
private:

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
    virtual const char * MainMenuEntry(void) {
        return MAINMENUENTRY;
    }
    virtual cOsdObject * MainMenuAction(void);
    virtual cMenuSetupPage * SetupMenu(void);
    virtual bool SetupParse(const char *Name, const char *Value);
    virtual bool Service(const char *Id, void *Data = NULL);
    virtual const char **SVDRPHelpPages(void);
    virtual cString SVDRPCommand(const char *Command, const char *Option, int &ReplyCode);
};

cPluginJonglisto::cPluginJonglisto(void) {
    // Initialize any member variables here.
    // DON'T DO ANYTHING ELSE THAT MAY HAVE SIDE EFFECTS, REQUIRE GLOBAL
    // VDR OBJECTS TO EXIST OR PRODUCE ANY OUTPUT!
}

cPluginJonglisto::~cPluginJonglisto() {
    // Clean up after yourself!
}

const char *cPluginJonglisto::CommandLineHelp(void) {
    // Return a string that describes all known command line options.
    return NULL;
}

bool cPluginJonglisto::ProcessArgs(int argc, char *argv[]) {
    // Implement command line argument processing here if applicable.
    return true;
}

bool cPluginJonglisto::Initialize(void) {
    // Initialize any background activities the plugin shall perform.
    return true;
}

bool cPluginJonglisto::Start(void) {
    // Start any background activities the plugin shall perform.
    return true;
}

void cPluginJonglisto::Stop(void) {
    // Stop any background activities the plugin is performing.
}

void cPluginJonglisto::Housekeeping(void) {
    // Perform any cleanup or other regular tasks.
}

void cPluginJonglisto::MainThreadHook(void) {
    // Perform actions in the context of the main program thread.
    // WARNING: Use with great care - see PLUGINS.html!
}

cString cPluginJonglisto::Active(void) {
    // Return a message string if shutdown should be postponed
    return NULL;
}

time_t cPluginJonglisto::WakeupTime(void) {
    // Return custom wakeup time for shutdown script
    return 0;
}

cOsdObject *cPluginJonglisto::MainMenuAction(void) {
    // Perform the action when selected from the main VDR menu.
    return NULL;
}

cMenuSetupPage *cPluginJonglisto::SetupMenu(void) {
    // Return a setup menu in case the plugin supports one.
    return NULL;
}

bool cPluginJonglisto::SetupParse(const char *Name, const char *Value) {
    // Parse your own setup parameters and store their values.
    return false;
}

bool cPluginJonglisto::Service(const char *Id, void *Data) {
    // Handle custom service requests from other plugins
    return false;
}

const char **cPluginJonglisto::SVDRPHelpPages(void) {
    static const char *HelpPages[] = { "EINF <channel id> <event id>\n"
            "    Return the scraper information with id.",
            0 };

    return HelpPages;
}

cString cPluginJonglisto::SVDRPCommand(const char *Command, const char *Option, int &ReplyCode) {
    char *rest = const_cast<char*>(Option);
    char *channelId;
    char *eventId;

    cPlugin *p;

    if (strcasecmp(Command, "EINF") == 0) {
        if (rest && *rest) {
            if ((channelId = strtok_r(rest, " ", &rest)) != NULL) {
                if ((eventId = strtok_r(rest, " ", &rest)) != NULL) {
                    LOCK_CHANNELS_READ;
                    LOCK_SCHEDULES_READ;

                    // find the desired channel
                    tChannelID chid = tChannelID::FromString(channelId);
                    const cChannel *channel = Channels->GetByChannelID(chid);

                    // get desired schedule
                    const cSchedule *schedule = Schedules->GetSchedule(channel, false);
                    const cEvent *event = schedule->GetEvent(strtol(eventId, NULL, 10));

                    if (event == NULL) {
                        ReplyCode = 952;
                        return cString::sprintf("event not found: %s -> %lu", eventId, strtol(eventId, NULL, 10));
                    }

                    // get event type
                    ScraperGetEventType eventType;
                    eventType.event = event;

                    p = cPluginManager::CallFirstService("GetEventType", (void*)&eventType);
                    if (p) {
                        if (eventType.type == tvType::tSeries) {
                            // get series information
                            cSeries series;
                            series.seriesId = eventType.seriesId;
                            series.episodeId = eventType.episodeId;

                            p = cPluginManager::CallFirstService("GetSeries", (void*)&series);
                            if (p) {
                                std::stringstream seriesOut;

                                seriesOut << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\" ?>"
                                          << "<scraper><series>"
                                          << "<id>" << eventType.seriesId << "</id>"
                                          << "<name><![CDATA[" << series.name << "]]></name>"
                                          << "<overview><![CDATA[" << series.overview << "]]></overview>"
                                          << "<firstAired><![CDATA[" << series.firstAired << "]]></firstAired>"
                                          << "<network><![CDATA[" << series.network << "]]></network>"
                                          << "<status><![CDATA[" << series.status << "]]></status>"
                                          << "<rating><![CDATA[" << series.rating << "]]></rating>";

                                // create posters
                                for (auto it = series.posters.begin(); it != series.posters.end(); ++it) {
                                    seriesOut << "<poster height=\"" << it->height << "\" width=\"" << it->width << "\"><![CDATA[" << it->path << "]]></poster>";
                                }

                                // create banners
                                for (auto it = series.banners.begin(); it != series.banners.end(); ++it) {
                                    seriesOut << "<banner height=\"" << it->height << "\" width=\"" << it->width << "\"><![CDATA[" << it->path << "]]></banner>";
                                }

                                // create fanarts
                                for (auto it = series.fanarts.begin(); it != series.fanarts.end(); ++it) {
                                    seriesOut << "<fanart height=\"" << it->height << "\" width=\"" << it->width << "\"><![CDATA[" << it->path << "]]></fanart>";
                                }

                                // create season poster
                                seriesOut << "<seasonPoster height=\"" << series.seasonPoster.height << "\" width=\"" << series.seasonPoster.width << "\"><![CDATA[" << series.seasonPoster.path << "]]></seasonPoster>";

                                // create actors
                                for (auto it = series.actors.begin(); it != series.actors.end(); ++it) {
                                    seriesOut << "<actor height=\"" << it->actorThumb.height << "\" width=\"" << it->actorThumb.width << "\"><path><![CDATA[" << it->actorThumb.path << "]]></path><name><![CDATA[" << it->name << "]]></name><role><![CDATA[" << it->role << "]]></role></actor>";
                                }

                                // create episode
                                seriesOut << "<episode height=\"" << series.episode.episodeImage.height << "\" width=\"" << series.episode.episodeImage.width << "\"><![CDATA[" << series.episode.episodeImage.path << "]]></episode>";

                                seriesOut << "</series></scraper>";

                                return seriesOut.str().c_str();
                            } else {
                                ReplyCode = 951;
                                return "event not found";
                            }
                        } else if (eventType.type == tvType::tMovie) {
                            cMovie movie;
                            movie.movieId = eventType.movieId;

                            p = cPluginManager::CallFirstService("GetMovie", (void*)&movie);
                            if (p) {
                                std::stringstream movieOut;
                                movieOut << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\" ?>"
                                         << "<scraper><movie>"
                                         << "<id>" << movie.movieId << "</id>"
                                         << "<adult><![CDATA[" << movie.adult << "]]></adult>"
                                         << "<budget><![CDATA[" << movie.budget << "]]></budget>"
                                         << "<genre><![CDATA[" << movie.genres << "]]></genre>"
                                         << "<homepage><![CDATA[" << movie.homepage << "]]></homepage>"
                                         << "<originalTitle><![CDATA[" << movie.originalTitle << "]]></originalTitle>"
                                         << "<overview><![CDATA[" << movie.overview << "]]></overview>"
                                         << "<popularity><![CDATA[" << movie.popularity << "]]></popularity>"
                                         << "<releaseDate><![CDATA[" << movie.releaseDate << "]]></releaseDate>"
                                         << "<revenue><![CDATA[" << movie.revenue << "]]></revenue>"
                                         << "<runtime><![CDATA[" << movie.runtime << "]]></runtime>"
                                         << "<tagline><![CDATA[" << movie.tagline << "]]></tagline>"
                                         << "<title><![CDATA[" << movie.title << "]]></title>"
                                         << "<voteAverage><![CDATA[" << movie.voteAverage << "]]></voteAverage>";

                                // create actors
                                for (auto it = movie.actors.begin(); it != movie.actors.end(); ++it) {
                                    movieOut << "<actor height=\"" << it->actorThumb.height << "\" width=\"" << it->actorThumb.width << "\"><path><![CDATA[" << it->actorThumb.path << "]]></path><name><![CDATA[" << it->name << "]]></name><role><![CDATA[" << it->role << "]]></role></actor>";
                                }

                                // create fanArts
                                movieOut << "<fanart height=\"" << movie.collectionFanart.height << "\" width=\"" << movie.collectionFanart.width << "\"><![CDATA[" << movie.collectionFanart.path << "]]></fanart>";
                                movieOut << "<fanart height=\"" << movie.fanart.height << "\" width=\"" << movie.fanart.width << "\"><![CDATA[" << movie.fanart.path << "]]></fanart>";

                                // create poster
                                movieOut << "<poster height=\"" << movie.collectionPoster.height << "\" width=\"" << movie.collectionPoster.width << "\"><![CDATA[" << movie.collectionPoster.path << "]]></poster>";
                                movieOut << "<poster height=\"" << movie.poster.height << "\" width=\"" << movie.poster.width << "\"><![CDATA[" << movie.poster.path << "]]></poster>";

                                movieOut << "</movie></scraper>";

                                return movieOut.str().c_str();
                            } else {
                                ReplyCode = 951;
                                return "event not found";
                            }
                        } else {
                            ReplyCode = 951;
                            return "event not found";
                        }
                    } else {
                        ReplyCode = 951;
                        return "service not found";
                    }
                } else {
                    ReplyCode = 950;
                    return "parameter <event id> is missing";
                }
            } else {
                ReplyCode = 950;
                return "parameter <channel id> is missing";
            }
        } else {
            ReplyCode = 950;
            return "no parameter found";
        }
    }

    return NULL;
}

VDRPLUGINCREATOR(cPluginJonglisto); // Don't touch this!
