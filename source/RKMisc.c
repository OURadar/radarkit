#include <RadarKit/RKMisc.h>

char *RKNow() {
    static char timestr[32];
    time_t utc;
    /* Here is a nice reference: http://www.cplusplus.com/ref/ctime/time.html */
    time(&utc);
    strftime(timestr, 32, "%Y/%m/%d %T", localtime(&utc));
    return timestr;
}

int RKLog(const char *whatever, ...) {
    if (rkGlobalParameters.stream == NULL && rkGlobalParameters.logfile[0] == 0) {
        return 0;
    }
    // Construct the string
    int i = 0;
    va_list args;
    va_start(args, whatever);
    char msg[2048];
    if (strlen(whatever) > 1600) {
        fprintf(stderr, "RKLog() could potential crash for string '%s'\n", whatever);
    }
    if (whatever[0] == '>') {
        i += snprintf(msg, 2048, "                    : [%s] ", rkGlobalParameters.program);
    } else {
        i += snprintf(msg, 2048, "%s : [%s] ", RKNow(), rkGlobalParameters.program);
    }
    bool has_ok = (strcasestr(msg, "OK") != NULL);
    bool has_not_ok = (strcasestr(msg, "ERROR") != NULL);
    bool has_warning = (strcasestr(msg, "WARNING") != NULL);
    if (rkGlobalParameters.showColor) {
        if (has_ok) {
            i += snprintf(msg + i, 2048 - i, "\033[1;32m");
        } else if (has_not_ok) {
            i += snprintf(msg + i, 2048 - i, "\033[1;31m");
        } else if (has_warning) {
            i += snprintf(msg + i, 2048 - i, "\033[1;33m");
        }
    }
    if (whatever[0] == '>') {
        i += vsprintf(msg + i, whatever + 1, args);
    } else {
        i += vsprintf(msg + i, whatever, args);
    }
    if (rkGlobalParameters.showColor && (has_ok || has_not_ok || has_warning)) {
        i += snprintf(msg + i, 2048 - i, "\033[0m");
    }
    if (whatever[strlen(whatever) - 1] != '\n') {
        strncat(msg, "\n", 2048);
    }
    va_end(args);
    // Produce the string to the specified stream
    if (rkGlobalParameters.stream) {
        fprintf(rkGlobalParameters.stream, "%s", msg);
        fflush(rkGlobalParameters.stream);
    }
    // Write the string to a file if specified
    if (rkGlobalParameters.logfile[0] != '\0' && strlen(rkGlobalParameters.logfile) > 0) {
        FILE *logFileID = fopen(rkGlobalParameters.logfile, "a");
        if (logFileID == NULL) {
            fprintf(stderr, "Unable to log.\n");
            return 1;
        }
        fprintf(logFileID, "%s", msg);
        fclose(logFileID);
    }
    return 0;
}

/*************************************************
 *
 *  Integer to string with 3-digit grouping
 */
char *RKIntegerToCommaStyleString(const long num) {
    int i, j, k;
    static int ibuf = 0;
    static char stringBuffer[8][32];

    const int r = ibuf;

    ibuf = ibuf == 7 ? 0 : ibuf + 1;

    sprintf(stringBuffer[r], "%ld", num);
    if (num < 1000) {
        return stringBuffer[r];
    } else {
        k = (int)(strlen(stringBuffer[r]) - 1) / 3;
        i = (int)(strlen(stringBuffer[r]) + k);
        j = 1;
        stringBuffer[r][i] = '\0';
        while (i > 0) {
            i--;
            stringBuffer[r][i] = stringBuffer[r][i - k];
            if (j > 3) {
                j = 0;
                stringBuffer[r][i] = ',';
                k--;
            }
            j++;
        }
    }
    k = ibuf;
    return stringBuffer[r];
}

char *RKFloatToCommaStyleString(const float num) {
    char *intString = RKIntegerToCommaStyleString((long)num);
    snprintf(intString + strlen(intString), 32 - strlen(intString), ".%03.0f", 1000.0f * (num - floorf(num))); 
    return intString;
}

void RKSetWantColor(const bool showColor) {
    rkGlobalParameters.showColor = showColor;
}

void RKSetWantScreenOutput(const bool yes) {
    if (yes) {
        rkGlobalParameters.stream = stdout;
    } else {
        rkGlobalParameters.stream = NULL;
    }
}

int RKSetProgramName(const char *name) {
    if (strlen(name) >= RKMaximumStringLength) {
        return 1;
    }
    snprintf(rkGlobalParameters.program, RKMaximumStringLength, "%s", name);
    return 0;
}

int RKSetLogfile(const char *filename) {
    if (filename == NULL) {
        rkGlobalParameters.logfile[0] = '\0';
        return 0;
    } else if (strlen(filename) >= RKMaximumStringLength) {
        return 1;
    }
    snprintf(rkGlobalParameters.logfile, RKMaximumStringLength, "%s", filename);
    return 0;
}

int RKSetLogfileToDefault(void) {
    snprintf(rkGlobalParameters.logfile, RKMaximumStringLength, "%s", RKDefaultLogfile);
    return 0;
}

// Timeval difference through subtraction
// subtrahend is strubtracted from minuend
// m - s
double RKTimevalDiff(const struct timeval m, const struct timeval s) {
    return (double)m.tv_sec - (double)s.tv_sec + 1.0e-6 * ((double)m.tv_usec - (double)s.tv_usec);
}

// Timespec difference through subtraction
// m - s
double RKTimespecDiff(const struct timespec m, const struct timespec s) {
    return (double)m.tv_sec - (double)s.tv_sec + 1.0e-9 * ((double)m.tv_nsec - (double)s.tv_nsec);
}

void RKUTCTime(struct timespec *t) {
#ifdef __MACH__
    // OS X does not have clock_gettime()
    clock_serv_t cclock;
    mach_timespec_t mts;
    host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
    clock_get_time(cclock, &mts);
    mach_port_deallocate(mach_task_self(), cclock);
    t->tv_sec = mts.tv_sec;
    t->tv_nsec = mts.tv_nsec;
#else
    clock_gettime(CLOCK_REALTIME, t);
#endif
}

bool RKFilenameExists(const char *filename) {
    if (access(filename, R_OK | W_OK)) {
        return true;
    }
    return false;
}
