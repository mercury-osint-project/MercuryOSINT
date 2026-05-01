#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <ctype.h>
#include <signal.h>
#include <time.h>

#define MAX_URL_LEN     512
#define MAX_HOST_LEN    256
#define MAX_PATH_LEN    512
#define MAX_THREADS     500
#define DEFAULT_THREADS 50
#define CONNECT_TIMEOUT 8
#define READ_TIMEOUT    8
#define BUF_SIZE        4096
#define MAX_SITES       2000
#define PORT_HTTP       80
#define PORT_HTTPS      443

#define COL_RESET   "\033[0m"
#define COL_BOLD    "\033[1m"
#define COL_DIM     "\033[2m"
#define COL_RED     "\033[31m"
#define COL_GREEN   "\033[32m"
#define COL_YELLOW  "\033[33m"
#define COL_CYAN    "\033[36m"
#define COL_MAGENTA "\033[35m"
#define COL_WHITE   "\033[37m"
#define COL_GRAY    "\033[90m"

typedef struct {
    char name[128];
    char url[MAX_URL_LEN];
    int  found;
    int  status_code;
    char error[128];
} Site;

typedef struct {
    Site     *sites;
    int       start;
    int       end;
    char      username[128];
    pthread_mutex_t *lock;
    int      *checked;
    int      *found_count;
    int       total;
} ThreadArgs;

typedef struct {
    char host[MAX_HOST_LEN];
    char path[MAX_PATH_LEN];
    int  port;
    int  is_https;
} ParsedURL;

static volatile int g_running = 1;
static struct timeval g_start_time;

static void print_banner(void) {
    printf("\n");
    printf(COL_CYAN COL_BOLD);
    printf("  ___  ___                               _____ _____ _____ _   _ _____ \n");
    printf("  |  \\/  |                              |  _  /  ___|_   _| \\ | |_   _|\n");
    printf("  | .  . | ___ _ __ ___ _   _ _ __ _   _| | | \\ `--. | | |  \\| | | |  \n");
    printf("  | |\\/| |/ _ \\ '__/ __| | | | '__| | | | | | |`--. \\| | | . ` | | |  \n");
    printf("  | |  | |  __/ | | (__| |_| | |  | |_| \\ \\_/ /\\__/ /_| |_| |\\  | | |  \n");
    printf("  \\_|  |_/\\___|_|  \\___||__,_|_|   \\__, |\\___/\\____/ \\___/\\_| \\_/ \\_/  \n");
    printf("                                    __/ |                                \n");
    printf("                                   |___/                                 \n");
    printf(COL_RESET);
    printf(COL_MAGENTA COL_BOLD "  MercuryOSINT C Probe v1.0  |  Fast Raw HTTP  |  Ethical Use Only\n" COL_RESET);
    printf("\n");

    const char *aliases[] = {"Anonymous","Ghost","Phantom","Shadow","Cipher","Spectre"};
    srand((unsigned)time(NULL));
    int idx = rand() % 6;
    printf("  " COL_WHITE "Welcome to Mercury, " COL_CYAN COL_BOLD "%s" COL_WHITE ".\n" COL_RESET, aliases[idx]);
    printf("  " COL_GRAY "Here you will use this tool, MercuryOSINT for ethical uses.\n" COL_RESET);
    printf("\n");
    printf("  " COL_YELLOW "This tool is for LEGAL, ETHICAL research only.\n" COL_RESET);
    printf("\n");
}

static int parse_url(const char *url, ParsedURL *out) {
    memset(out, 0, sizeof(ParsedURL));

    const char *p = url;
    if (strncmp(p, "https://", 8) == 0) {
        out->is_https = 1;
        out->port = PORT_HTTPS;
        p += 8;
    } else if (strncmp(p, "http://", 7) == 0) {
        out->is_https = 0;
        out->port = PORT_HTTP;
        p += 7;
    } else {
        return -1;
    }

    const char *slash = strchr(p, '/');
    size_t host_len;
    if (slash) {
        host_len = (size_t)(slash - p);
        strncpy(out->path, slash, MAX_PATH_LEN - 1);
    } else {
        host_len = strlen(p);
        strcpy(out->path, "/");
    }

    if (host_len >= MAX_HOST_LEN) return -1;
    strncpy(out->host, p, host_len);

    char *colon = strchr(out->host, ':');
    if (colon) {
        *colon = '\0';
        out->port = atoi(colon + 1);
    }

    return 0;
}

static void set_timeout(int sock, int seconds) {
    struct timeval tv = {seconds, 0};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}

static int probe_http(const char *host, int port, const char *path, int *status_out) {
    struct addrinfo hints, *res = NULL;
    char port_str[8];
    int sock = -1, ret = -1;
    char request[BUF_SIZE];
    char response[BUF_SIZE];

    snprintf(port_str, sizeof(port_str), "%d", port);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host, port_str, &hints, &res) != 0 || !res) {
        *status_out = -1;
        return -1;
    }

    sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) goto cleanup;

    set_timeout(sock, CONNECT_TIMEOUT);

    if (connect(sock, res->ai_addr, res->ai_addrlen) != 0) {
        *status_out = -2;
        goto cleanup;
    }

    set_timeout(sock, READ_TIMEOUT);

    snprintf(request, sizeof(request),
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "User-Agent: Mozilla/5.0 (compatible; MercuryOSINT/1.0)\r\n"
        "Accept: */*\r\n"
        "Connection: close\r\n\r\n",
        path, host);

    if (send(sock, request, strlen(request), 0) < 0) goto cleanup;

    ssize_t n = recv(sock, response, BUF_SIZE - 1, 0);
    if (n <= 0) goto cleanup;
    response[n] = '\0';

    if (strncmp(response, "HTTP/", 5) == 0) {
        char *sp = strchr(response, ' ');
        if (sp) {
            *status_out = atoi(sp + 1);
            ret = 0;
        }
    }

cleanup:
    if (res)  freeaddrinfo(res);
    if (sock >= 0) close(sock);
    return ret;
}

static double elapsed(void) {
    struct timeval now;
    gettimeofday(&now, NULL);
    return (double)(now.tv_sec  - g_start_time.tv_sec) +
           (double)(now.tv_usec - g_start_time.tv_usec) / 1e6;
}

static void *worker(void *arg) {
    ThreadArgs *ta = (ThreadArgs *)arg;

    for (int i = ta->start; i < ta->end && g_running; i++) {
        Site *site = &ta->sites[i];

        char url[MAX_URL_LEN];
        const char *tpl = site->url;
        const char *placeholder = strstr(tpl, "{}");
        if (placeholder) {
            size_t pre = (size_t)(placeholder - tpl);
            snprintf(url, sizeof(url), "%.*s%s%s",
                     (int)pre, tpl,
                     ta->username,
                     placeholder + 2);
        } else {
            strncpy(url, tpl, MAX_URL_LEN - 1);
        }

        ParsedURL pu;
        int status = 0;

        if (parse_url(url, &pu) == 0) {
            int probe_port = pu.is_https ? PORT_HTTPS : pu.port;
            probe_http(pu.host, probe_port, pu.path, &status);
        }

        site->status_code = status;
        site->found = (status == 200 || status == 301 || status == 302);

        pthread_mutex_lock(ta->lock);
        (*ta->checked)++;
        if (site->found) {
            (*ta->found_count)++;
            printf("  " COL_GREEN "%s" COL_RESET " %-30s " COL_CYAN "%s" COL_RESET "\n",
                   "+", site->name, url);
            fflush(stdout);
        }
        pthread_mutex_unlock(ta->lock);
    }

    return NULL;
}

static const char *BUILTIN_SITES[][2] = {
    {"GitHub",       "https://github.com/{}"},
    {"GitLab",       "https://gitlab.com/{}"},
    {"Twitter/X",    "https://twitter.com/{}"},
    {"Instagram",    "https://www.instagram.com/{}/"},
    {"Reddit",       "https://www.reddit.com/user/{}"},
    {"TikTok",       "https://www.tiktok.com/@{}"},
    {"YouTube",      "https://www.youtube.com/@{}"},
    {"LinkedIn",     "https://www.linkedin.com/in/{}"},
    {"Pinterest",    "https://www.pinterest.com/{}/"},
    {"Twitch",       "https://www.twitch.tv/{}"},
    {"Steam",        "https://steamcommunity.com/id/{}"},
    {"SoundCloud",   "https://soundcloud.com/{}"},
    {"Vimeo",        "https://vimeo.com/{}"},
    {"Tumblr",       "https://{}.tumblr.com/"},
    {"Medium",       "https://medium.com/@{}"},
    {"DeviantArt",   "https://www.deviantart.com/{}"},
    {"Behance",      "https://www.behance.net/{}"},
    {"Dribbble",     "https://dribbble.com/{}"},
    {"HackerNews",   "https://news.ycombinator.com/user?id={}"},
    {"Keybase",      "https://keybase.io/{}"},
    {"Patreon",      "https://www.patreon.com/{}"},
    {"Ko-fi",        "https://ko-fi.com/{}"},
    {"Linktree",     "https://linktr.ee/{}"},
    {"About.me",     "https://about.me/{}"},
    {"Gravatar",     "https://en.gravatar.com/{}"},
    {"StackOverflow","https://stackoverflow.com/users/{}"},
    {"Kaggle",       "https://www.kaggle.com/{}"},
    {"CodePen",      "https://codepen.io/{}"},
    {"Replit",       "https://replit.com/@{}"},
    {"Bitbucket",    "https://bitbucket.org/{}/"},
    {"npm",          "https://www.npmjs.com/~{}"},
    {"PyPI",         "https://pypi.org/user/{}/"},
    {"DockerHub",    "https://hub.docker.com/u/{}"},
    {"DEV.to",       "https://dev.to/{}"},
    {"Hashnode",     "https://hashnode.com/@{}"},
    {"Flickr",       "https://www.flickr.com/people/{}"},
    {"500px",        "https://500px.com/p/{}"},
    {"Unsplash",     "https://unsplash.com/@{}"},
    {"VSCO",         "https://vsco.co/{}"},
    {"Imgur",        "https://imgur.com/user/{}"},
    {"9GAG",         "https://9gag.com/u/{}"},
    {"Mastodon",     "https://mastodon.social/@{}"},
    {"Discord",      "https://discord.com/users/{}"},
    {"Telegram",     "https://t.me/{}"},
    {"Quora",        "https://www.quora.com/profile/{}"},
    {"Letterboxd",   "https://letterboxd.com/{}/"},
    {"Trakt",        "https://trakt.tv/users/{}"},
    {"Goodreads",    "https://www.goodreads.com/{}"},
    {"Wattpad",      "https://www.wattpad.com/user/{}"},
    {"MyAnimeList",  "https://myanimelist.net/profile/{}"},
    {"AniList",      "https://anilist.co/user/{}"},
    {"Last.fm",      "https://www.last.fm/user/{}"},
    {"Bandcamp",     "https://{}.bandcamp.com/"},
    {"Mixcloud",     "https://www.mixcloud.com/{}/"},
    {"Audiomack",    "https://audiomack.com/{}"},
    {"Strava",       "https://www.strava.com/athletes/{}"},
    {"Untappd",      "https://untappd.com/user/{}"},
    {"Chess.com",    "https://www.chess.com/member/{}"},
    {"Lichess",      "https://lichess.org/@/{}"},
    {"Speedrun.com", "https://www.speedrun.com/user/{}"},
    {"Roblox",       "https://www.roblox.com/user.aspx?username={}"},
    {"Newgrounds",   "https://{}.newgrounds.com/"},
    {"itch.io",      "https://{}.itch.io/"},
    {"GameJolt",     "https://gamejolt.com/@{}"},
    {"LeetCode",     "https://leetcode.com/{}/"},
    {"HackerRank",   "https://www.hackerrank.com/{}"},
    {"Codeforces",   "https://codeforces.com/profile/{}"},
    {"CodeChef",     "https://www.codechef.com/users/{}"},
    {"AtCoder",      "https://atcoder.jp/users/{}"},
    {"Exercism",     "https://exercism.org/profiles/{}"},
    {"Etsy",         "https://www.etsy.com/shop/{}"},
    {"eBay",         "https://www.ebay.com/usr/{}"},
    {"Depop",        "https://www.depop.com/{}"},
    {"Poshmark",     "https://poshmark.com/closet/{}"},
    {"Fiverr",       "https://www.fiverr.com/{}"},
    {"Upwork",       "https://www.upwork.com/freelancers/~{}"},
    {"Freelancer",   "https://www.freelancer.com/u/{}"},
    {"ResearchGate", "https://www.researchgate.net/profile/{}"},
    {"Academia.edu", "https://independent.academia.edu/{}"},
    {"SlideShare",   "https://www.slideshare.net/{}"},
    {"Foursquare",   "https://foursquare.com/{}"},
    {"Yelp",         "https://www.yelp.com/user_details?userid={}"},
    {"TripAdvisor",  "https://www.tripadvisor.com/Profile/{}"},
    {"Kickstarter",  "https://www.kickstarter.com/profile/{}"},
    {"Indiegogo",    "https://www.indiegogo.com/individuals/{}"},
    {"ProductHunt",  "https://www.producthunt.com/@{}"},
    {"Crunchbase",   "https://www.crunchbase.com/person/{}"},
    {"AngelList",    "https://angel.co/u/{}"},
    {"Wikipedia",    "https://en.wikipedia.org/wiki/User:{}"},
    {"Quizlet",      "https://quizlet.com/{}"},
    {"Duolingo",     "https://www.duolingo.com/profile/{}"},
    {"Khan Academy", "https://www.khanacademy.org/profile/{}"},
    {"MyFitnessPal", "https://www.myfitnesspal.com/profile/{}"},
    {"AllTrails",    "https://www.alltrails.com/members/{}"},
    {"Sketchfab",    "https://sketchfab.com/{}"},
    {"ArtStation",   "https://www.artstation.com/{}"},
    {"Thingiverse",  "https://www.thingiverse.com/{}"},
    {"Instructables","https://www.instructables.com/member/{}/"},
    {NULL, NULL}
};

static void save_results(const char *username, Site *sites, int total) {
    char fname[256];
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y%m%d_%H%M%S", tm_info);

    snprintf(fname, sizeof(fname), "output/mercury_c_%s_%s.txt", username, ts);

    system("mkdir -p output");

    FILE *f = fopen(fname, "w");
    if (!f) { perror("fopen"); return; }

    fprintf(f, "MercuryOSINT C Probe Report\n");
    fprintf(f, "Username : %s\n", username);
    fprintf(f, "Date     : %s\n", ts);
    fprintf(f, "========================================\n\n");

    int found = 0;
    for (int i = 0; i < total; i++) {
        if (sites[i].found) {
            char url[MAX_URL_LEN];
            const char *tpl = sites[i].url;
            const char *ph  = strstr(tpl, "{}");
            if (ph) {
                snprintf(url, sizeof(url), "%.*s%s%s",
                         (int)(ph - tpl), tpl, username, ph + 2);
            } else {
                strncpy(url, tpl, MAX_URL_LEN - 1);
            }
            fprintf(f, "[%s]\n  %s\n\n", sites[i].name, url);
            found++;
        }
    }

    fprintf(f, "\nTotal found: %d\n", found);
    fclose(f);

    printf("\n  " COL_MAGENTA COL_BOLD "OUTPUT" COL_RESET " -> " COL_CYAN "%s" COL_RESET "\n", fname);
}

static void handle_sigint(int sig) {
    (void)sig;
    g_running = 0;
    printf("\n\n  " COL_YELLOW "Interrupted. Saving partial results..." COL_RESET "\n");
}

int main(int argc, char *argv[]) {
    print_banner();

    if (argc < 2) {
        fprintf(stderr,
            "Usage: %s <username> [threads]\n\n"
            "  username  : The username to search\n"
            "  threads   : Concurrent threads (default: %d, max: %d)\n\n"
            "Examples:\n"
            "  %s johndoe\n"
            "  %s johndoe 100\n\n"
            COL_GRAY "Note: For full 1500+ site scanning, use mercury.py (Python engine)\n" COL_RESET,
            argv[0], DEFAULT_THREADS, MAX_THREADS, argv[0], argv[0]);
        return 1;
    }

    const char *username = argv[1];
    int num_threads = DEFAULT_THREADS;
    if (argc >= 3) {
        num_threads = atoi(argv[2]);
        if (num_threads < 1)   num_threads = 1;
        if (num_threads > MAX_THREADS) num_threads = MAX_THREADS;
    }

    int total_sites = 0;
    while (BUILTIN_SITES[total_sites][0] != NULL) total_sites++;

    Site *sites = calloc(total_sites, sizeof(Site));
    if (!sites) { perror("calloc"); return 1; }

    for (int i = 0; i < total_sites; i++) {
        strncpy(sites[i].name, BUILTIN_SITES[i][0], 127);
        strncpy(sites[i].url,  BUILTIN_SITES[i][1], MAX_URL_LEN - 1);
    }

    signal(SIGINT, handle_sigint);
    gettimeofday(&g_start_time, NULL);

    printf("  " COL_MAGENTA COL_BOLD "TARGET" COL_RESET " : " COL_CYAN COL_BOLD "%s" COL_RESET "\n", username);
    printf("  " COL_MAGENTA COL_BOLD "SITES " COL_RESET " : " COL_WHITE "%d" COL_RESET "\n", total_sites);
    printf("  " COL_MAGENTA COL_BOLD "THREAD" COL_RESET " : " COL_WHITE "%d" COL_RESET "\n", num_threads);
    printf("\n  " COL_GRAY "-------------------------------------------------------------" COL_RESET "\n");
    printf("  " COL_GREEN COL_BOLD "LIVE HITS\n" COL_RESET "\n");

    pthread_mutex_t lock;
    pthread_mutex_init(&lock, NULL);
    int checked = 0, found_count = 0;

    if (num_threads > total_sites) num_threads = total_sites;

    pthread_t *threads = malloc(num_threads * sizeof(pthread_t));
    ThreadArgs *targs  = malloc(num_threads * sizeof(ThreadArgs));
    if (!threads || !targs) { perror("malloc"); return 1; }

    int chunk = total_sites / num_threads;
    int rem   = total_sites % num_threads;

    for (int t = 0; t < num_threads; t++) {
        targs[t].sites       = sites;
        targs[t].start       = t * chunk + (t < rem ? t : rem);
        targs[t].end         = targs[t].start + chunk + (t < rem ? 1 : 0);
        targs[t].lock        = &lock;
        targs[t].checked     = &checked;
        targs[t].found_count = &found_count;
        targs[t].total       = total_sites;
        strncpy(targs[t].username, username, 127);
        pthread_create(&threads[t], NULL, worker, &targs[t]);
    }

    while (g_running && checked < total_sites) {
        double pct = (double)checked / total_sites * 100.0;
        double el  = elapsed();

        int bar_len = 30;
        int filled  = (int)(bar_len * pct / 100.0);
        char bar[64] = {0};
        for (int i = 0; i < bar_len; i++)
            bar[i] = (i < filled) ? '#' : '.';

        printf("\r  [%s] %.1f%% %d/%d  " COL_GREEN "+ %d" COL_RESET "  %.1fs   ",
               bar, pct, checked, total_sites, found_count, el);
        fflush(stdout);
        usleep(150000);
    }
    printf("\n");

    for (int t = 0; t < num_threads; t++)
        pthread_join(threads[t], NULL);

    double total_time = elapsed();

    printf("\n  " COL_GRAY "-------------------------------------------------------------" COL_RESET "\n");
    printf("\n  " COL_CYAN COL_BOLD "SCAN COMPLETE" COL_RESET "\n");
    printf("  " COL_GREEN "+ Found" COL_RESET "              : " COL_BOLD COL_GREEN "%d" COL_RESET " profiles\n", found_count);
    printf("  " COL_GRAY "- Not found / error" COL_RESET " : " COL_GRAY "%d" COL_RESET "\n", total_sites - found_count);
    printf("  " COL_YELLOW "Time" COL_RESET "               : %.2f seconds\n", total_time);
    printf("  " COL_MAGENTA "Throughput" COL_RESET "          : %.0f sites/sec\n", total_sites / total_time);

    if (found_count > 0)
        save_results(username, sites, total_sites);

    printf("\n  " COL_GRAY "Stay ethical. Stay curious." COL_RESET "\n\n");

    pthread_mutex_destroy(&lock);
    free(sites);
    free(threads);
    free(targs);
    return 0;
}
