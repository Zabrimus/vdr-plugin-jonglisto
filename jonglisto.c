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

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <netinet/in.h>
#include <getopt.h>
#include <vdr/plugin.h>
#include <vdr/channels.h>
#include <vdr/epg.h>
#include <vdr/tools.h>
#include <vdr/config.h>
#include <vdr/timers.h>

#include "services/scraper2vdr.h"

static const char *VERSION = "0.0.5";
static const char *DESCRIPTION = "Tool plugin for jonglisto-ng";
static const char *MAINMENUENTRY = "Jonglisto";

//***************************************************************************
// Plugin Main Menu
//***************************************************************************

class cJonglistoPluginMenu : public cOsdMenu {
    public:
        cJonglistoPluginMenu(const char* title, cString host, const long int port, const cString loc, const long int osd);
        virtual ~cJonglistoPluginMenu() { };
        virtual eOSState ProcessKey(eKeys key);

    private:
        long int jonglistoPort;
        cString jonglistoHost;
        cString locale;
        long int osdserverPort;
};

cJonglistoPluginMenu::cJonglistoPluginMenu(const char* title, cString host, long int port, cString loc, long int osd) : cOsdMenu(title) {
    jonglistoPort = port;
    jonglistoHost = host;
    locale = loc;
    osdserverPort = osd;

    Clear();
    cOsdMenu::Add(new cOsdItem(tr("Show favourites")));
    SetHelp(0, 0, 0,0);
    Display();
}

eOSState cJonglistoPluginMenu::ProcessKey(eKeys key) {
    eOSState state = cOsdMenu::ProcessKey(key);

    if (state != osUnknown)
        return state;

    switch (key) {
        case kOk: {
            if (Current() == 0) {
                // trigger jonglisto-ng to show favourites
                cString message_fmt = "GET /jonglisto-ng/osdserver?port=%d&command=favourite&locale=%s HTTP/1.1\nHOST: %s\nConnection: close\n\n";

                struct hostent *server;
                struct sockaddr_in serv_addr;
                int sockfd, bytes, sent, total;
                char message[1024];

                sprintf(message, message_fmt, osdserverPort, *locale, *jonglistoHost);

                sockfd = socket(AF_INET, SOCK_STREAM, 0);
                if (sockfd < 0) {
                    // ERROR opening socket
                    esyslog("ERROR: opening socket for jonglisto jonglisto-ng server at %s:%ld", *jonglistoHost, jonglistoPort);
                    return osEnd;
                }

                server = gethostbyname(jonglistoHost);
                if (server == NULL) {
                    // ERROR, no such host
                    esyslog("ERROR: no such host jonglisto-ng server at %s", *jonglistoHost);
                    return osEnd;
                }

                memset(&serv_addr,0,sizeof(serv_addr));
                serv_addr.sin_family = AF_INET;
                serv_addr.sin_port = htons(jonglistoPort);
                memcpy(&serv_addr.sin_addr.s_addr,server->h_addr,server->h_length);

                if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0) {
                    // ERROR connecting
                    esyslog("ERROR: connection failed to jonglisto-ng server at %s:%ld", *jonglistoHost, jonglistoPort);
                    return osEnd;
                }

                total = strlen(message);
                sent = 0;
                do {
                    bytes = write(sockfd, message + sent, total - sent);
                    if (bytes < 0) {
                        // ERROR writing message to socket
                        esyslog("ERROR: writing message to socket to jonglisto-ng server at %s:%ld", *jonglistoHost, jonglistoPort);
                    }

                    if (bytes == 0) {
                        break;
                    }

                    sent += bytes;
                } while (sent < total);

                /* no response */

                close(sockfd);
            }
            return osEnd;
        }

        default:
            break;
    }
    return state;
}

//***************************************************************************
// Plugin
//***************************************************************************

class cPluginJonglisto: public cPlugin {
    long int jonglistoPort;
    cString jonglistoHost;
    cString locale;
    long int osdserverPort;

private:
    std::stringstream printSeries(cSeries series);
    std::stringstream printMovie(cMovie movie);
    cString searchAndPrintEvent(int &ReplyCode, ScraperGetEventType *Data = NULL);

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

    jonglistoPort = 8080;
    jonglistoHost = "localhost";
    locale = "de";
    osdserverPort = 2010;
}

cPluginJonglisto::~cPluginJonglisto() {
    // Clean up after yourself!
}

const char *cPluginJonglisto::CommandLineHelp(void) {
    // Return a string that describes all known command line options.
    return "  -h <host>, --host=<host>        set the hostname or ip of the jonglisto-ng server\n"
           "  -p <port>, --port=<port>        set the port of the jonglisto-ng server\n"
           "  -o <port>, --osdserver=<port>   set the osdserver port, default 2010\n"
           "  -l <locale>, --locale=<locale>  set the desired locale, (currently de or en available)\n";
}

bool cPluginJonglisto::ProcessArgs(int argc, char *argv[]) {
    // Implement command line argument processing here if applicable.
    static const struct option long_options[] = {
        { "locale",    required_argument, NULL, 'l' },
        { "host",      required_argument, NULL, 'h' },
        { "port",      required_argument, NULL, 'p' },
        { "osdserver", required_argument, NULL, 'o' },
        {0, 0, 0, 0}
        };

    int c;
    while ((c = getopt_long(argc, argv, "h:p:o:l:", long_options, NULL)) != -1) {
        esyslog("PARAM %c -> %s", c, optarg);

        switch (c) {
          case 'h':
               jonglistoHost = optarg;
               break;
          case 'p':
               jonglistoPort = strtol(optarg, NULL, 0);
               break;
          case 'l':
               locale = optarg;
               break;
          case 'o':
               osdserverPort = strtol(optarg, NULL, 0);
               break;
          default:
               return false;
        }
    }

    esyslog("LOCALE2 %s", *locale);

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
    return new cJonglistoPluginMenu("Jonglisto", jonglistoHost, jonglistoPort, locale, osdserverPort);
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
    static const char *HelpPages[] = {
            "EINF <channel id> <event id>\n"
            "    Return the scraper information with id.",
            "RINF <id>\n"
            "    Return the scraper information for one recording with id <id>.",
            "NEWT <settings>\n"
            "    Create a new timer. Settings must be in the same format as returned\n"
            "    by the LSTT command. If a SVDRPDefaultHost is configured, the timer\n"
            "    will be created on this VDR.\n",
            "NERT <channel id> <start time>\n"
            "    Create a new timer. Searches an event by channel id and start time and\n"
            "    uses then the VDR settings.If a SVDRPDefaultHost is configured, the timer\n"
            "    will be created on this VDR.\n",
            "LSDR\n"
            "    List deleted recordings\n",
            "UNDR <id>\n"
            "    Undelete recording with id <id>. The <id> must be the result of LSDR.\n",
            0 };

    return HelpPages;
}

cString cPluginJonglisto::SVDRPCommand(const char *Command, const char *Option, int &ReplyCode) {
    char *rest = const_cast<char*>(Option);
    char *channelId;
    char *eventId;
    char *starttime;
    char *recId;

    if (strcasecmp(Command, "EINF") == 0) {
        if (rest && *rest) {
            if ((channelId = strtok_r(rest, " ", &rest)) != NULL) {
                if ((eventId = strtok_r(rest, " ", &rest)) != NULL) {
                    LOCK_CHANNELS_READ;
                    LOCK_SCHEDULES_READ;

                    // find the desired channel
                    tChannelID chid = tChannelID::FromString(channelId);
                    const cChannel *channel = Channels->GetByChannelID(chid);

                    if (channel == NULL) {
                        // unknown channel
                        ReplyCode = 950;
                        return cString::sprintf("channel not found: %s", channelId);
                    }

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

                    return searchAndPrintEvent(ReplyCode, &eventType);
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
    } else if (strcasecmp(Command, "RINF") == 0) {
        if (rest && *rest) {
            if ((recId = strtok_r(rest, " ", &rest)) != NULL) {

                LOCK_RECORDINGS_READ
                const cRecording *rec = Recordings->Get(atoi(recId) - 1);

                // get event type
                ScraperGetEventType eventType;
                eventType.recording = rec;

                return searchAndPrintEvent(ReplyCode, &eventType);
            } else {
                ReplyCode = 950;
                return "parameter <recstart> is missing";
            }
        } else {
            ReplyCode = 950;
            return "no parameter found";
        }
    } else if (strcasecmp(Command, "NEWT") == 0) {
        if (*Option) {
           cTimer *Timer = new cTimer;
           if (Timer->Parse(Option)) {
              LOCK_TIMERS_WRITE;

              if (Setup.SVDRPPeering && *Setup.SVDRPDefaultHost) {
                  Timer->SetRemote(Setup.SVDRPDefaultHost);
              }

              Timer->ClrFlags(tfRecording);
              Timers->Add(Timer);

              cString ErrorMessage;
              if (!HandleRemoteTimerModifications(Timer, NULL, &ErrorMessage)) {
                  ReplyCode = 952;
                  return cString::sprintf("Error: %s", *ErrorMessage);
              } else {
                  ReplyCode = 250;
                  return cString::sprintf("%d %s <%s>", Timer->Id(), *Timer->ToText(true), *ErrorMessage);
              }
           } else {
               ReplyCode = 501;
               return "Error in timer settings";
           }

           delete Timer;
        } else {
            ReplyCode = 501;
            return "Missing timer settings";
        }
    } else if (strcasecmp(Command, "NERT") == 0) {
        if (rest && *rest) {
            if ((channelId = strtok_r(rest, " ", &rest)) != NULL) {
                if ((starttime = strtok_r(rest, " ", &rest)) != NULL) {
                    LOCK_CHANNELS_READ;
                    LOCK_SCHEDULES_READ;

                    // find the desired channel
                    tChannelID chid = tChannelID::FromString(channelId);
                    const cChannel *channel = Channels->GetByChannelID(chid);

                    if (channel == NULL) {
                        // unknown channel
                        ReplyCode = 950;
                        return cString::sprintf("channel not found: %s", channelId);
                    }

                    // get desired schedule
                    const cSchedule *schedule = Schedules->GetSchedule(channel, false);
                    const cEvent *event = schedule->GetEventAround(60 + strtol(starttime, NULL, 10));

                    if (event == NULL) {
                        ReplyCode = 952;
                        return cString::sprintf("event not found: %s -> %lu", channelId, strtol(starttime, NULL, 10));
                    }

                    LOCK_TIMERS_WRITE;
                    cTimer *Timer = new cTimer(event);

                    if (Setup.SVDRPPeering && *Setup.SVDRPDefaultHost) {
                        Timer->SetRemote(Setup.SVDRPDefaultHost);
                    }

                    Timer->ClrFlags(tfRecording);
                    Timers->Add(Timer);

                    cString ErrorMessage;
                    if (!HandleRemoteTimerModifications(Timer, NULL, &ErrorMessage)) {
                        ReplyCode = 952;
                        return cString::sprintf("Error: %s", *ErrorMessage);
                    } else {
                        return cString::sprintf("%d %s <%s>", Timer->Id(), *Timer->ToText(true), *ErrorMessage);
                    }
                } else {
                    ReplyCode = 950;
                    return "parameter <start time> is missing";
                }
            } else {
                ReplyCode = 950;
                return "parameter <channel id> is missing";
            }
        } else {
            ReplyCode = 950;
            return "no parameter found";
        }
    } else if (strcasecmp(Command, "LSDR") == 0) {
        LOCK_DELETEDRECORDINGS_READ;
        if (DeletedRecordings->Count()) {
            const cRecording *Recording = DeletedRecordings->First();
            std::stringstream recOut;

            while (Recording) {
                recOut << Recording->Id() << " " << Recording->Title(' ', true) << "\n";
                Recording = DeletedRecordings->Next(Recording);
            }

            return recOut.str().c_str();
        } else {
            ReplyCode = 950;
            return "No recordings available";
        }
    } else if (strcasecmp(Command, "UNDR") == 0) {
        if (rest && *rest) {
            if ((recId = strtok_r(rest, " ", &rest)) != NULL) {
                LOCK_DELETEDRECORDINGS_WRITE;
                cRecording *Recording  = const_cast<cRecording*>(DeletedRecordings->GetById(atoi(recId)));

                if (Recording) {
                    bool re = Recording->Undelete();

                    if (re) {
                        DeletedRecordings->Update();
                        return "recording undeleted.";
                    } else {
                        ReplyCode = 950;
                        return "undelete failed. Check log file.";
                    }
                } else {
                    ReplyCode = 950;
                    return cString::sprintf("Error: recording with id %s does not exist.", recId);
                }
            } else {
                ReplyCode = 950;
                return "parameter <id> is missing";
            }
        } else {
            ReplyCode = 950;
            return "no parameter found";
        }
    }


    return NULL;
}


cString cPluginJonglisto::searchAndPrintEvent(int &ReplyCode, ScraperGetEventType *eventType) {
    cPlugin *p = cPluginManager::CallFirstService("GetEventType", eventType);
    if (p) {
        if (eventType->type == tvType::tSeries) {
            // get series information
            cSeries series;
            series.seriesId = eventType->seriesId;
            series.episodeId = eventType->episodeId;

            p = cPluginManager::CallFirstService("GetSeries", (void*)&series);
            if (p) {
                std::stringstream seriesOut = printSeries(series);
                return seriesOut.str().c_str();
            } else {
                ReplyCode = 951;
                return "event not found";
            }
        } else if (eventType->type == tvType::tMovie) {
            cMovie movie;
            movie.movieId = eventType->movieId;

            p = cPluginManager::CallFirstService("GetMovie", (void*)&movie);
            if (p) {
                std::stringstream movieOut = printMovie(movie);
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
}

std::stringstream cPluginJonglisto::printSeries(cSeries series) {
    std::stringstream seriesOut;
    seriesOut << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\" ?>"
            << "<scraper><series>" << "<id>" << series.seriesId << "</id>"
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
    seriesOut << "<seasonPoster height=\"" << series.seasonPoster.height << "\" width=\"" << series.seasonPoster.width << "\"><![CDATA["
            << series.seasonPoster.path << "]]></seasonPoster>";

    // create actors
    for (auto it = series.actors.begin(); it != series.actors.end(); ++it) {
        seriesOut << "<actor height=\"" << it->actorThumb.height << "\" width=\"" << it->actorThumb.width << "\"><path><![CDATA[" << it->actorThumb.path
                << "]]></path><name><![CDATA[" << it->name << "]]></name><role><![CDATA[" << it->role << "]]></role></actor>";
    }

    // create episode
    seriesOut << "<episode height=\"" << series.episode.episodeImage.height << "\" width=\"" << series.episode.episodeImage.width << "\"><![CDATA["
            << series.episode.episodeImage.path << "]]></episode>";

    seriesOut << "</series></scraper>";
    return seriesOut;
}

std::stringstream cPluginJonglisto::printMovie(cMovie movie) {
    std::stringstream movieOut;
    movieOut << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\" ?>"
            << "<scraper><movie>" << "<id>" << movie.movieId << "</id>"
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
        movieOut << "<actor height=\"" << it->actorThumb.height << "\" width=\"" << it->actorThumb.width << "\"><path><![CDATA[" << it->actorThumb.path
                << "]]></path><name><![CDATA[" << it->name << "]]></name><role><![CDATA[" << it->role << "]]></role></actor>";
    }

    // create fanArts
    movieOut << "<fanart height=\"" << movie.collectionFanart.height << "\" width=\"" << movie.collectionFanart.width << "\"><![CDATA["
            << movie.collectionFanart.path << "]]></fanart>";
    movieOut << "<fanart height=\"" << movie.fanart.height << "\" width=\"" << movie.fanart.width << "\"><![CDATA[" << movie.fanart.path << "]]></fanart>";

    // create poster
    movieOut << "<poster height=\"" << movie.collectionPoster.height << "\" width=\"" << movie.collectionPoster.width << "\"><![CDATA["
            << movie.collectionPoster.path << "]]></poster>";
    movieOut << "<poster height=\"" << movie.poster.height << "\" width=\"" << movie.poster.width << "\"><![CDATA[" << movie.poster.path << "]]></poster>";

    movieOut << "</movie></scraper>";
    return movieOut;
}


VDRPLUGINCREATOR(cPluginJonglisto); // Don't touch this!
