//
//  RKFoundation
//  RadarKit
//
//  Created by Boonleng Cheong on 9/13/16.
//
//

#include <RadarKit/RKFoundation.h>

#pragma mark - Basic Arithmetics

RKComplex RKComplexAdd(const RKComplex a, const RKComplex b) {
    return (RKComplex){a.i + b.i, a.q + b.q};
}

RKComplex RKComplexSubtract(const RKComplex a, const RKComplex b) {
    return (RKComplex){a.i - b.i, a.q - b.q};
}

RKComplex RKComplexMultiply(const RKComplex a, const RKComplex b) {
    return (RKComplex){a.i * b.i - a.q * b.q, a.i * b.q + a.q * b.i};
}

RKComplex RKComplexConjugate(const RKComplex a) {
    return (RKComplex){a.i, -a.q};
}

RKFloat RKComplexAbsSquare(const RKComplex a) {
    return (RKFloat)(a.i * a.i + a.q * a.q);
}

// srcdst = srcdst
void RKComplexArrayInConjugate(RKComplex *srcdst, const int count) {
    for (int k = 0; k < count; k++) {
        srcdst->q = -srcdst->q;
        srcdst++;
    }
}

// dst = src + dst
void RKComplexArrayInPlaceAdd(RKComplex *src, RKComplex *dst, const int count) {
    for (int k = 0; k < count; k++) {
        *dst = RKComplexAdd(*src++, *dst);
        dst++;
    }
}

// dst = src - dst
void RKComplexArrayInPlaceSubtract(RKComplex *src, RKComplex *dst, const int count) {
    for (int k = 0; k < count; k++) {
        *dst = RKComplexSubtract(*src++, *dst);
        dst++;
    }
}

// dst = src * dst
void RKComplexArrayInPlaceMultiply(RKComplex *src, RKComplex *dst, const int count) {
    for (int k = 0; k < count; k++) {
        *dst = RKComplexMultiply(*src++, *dst);
        dst++;
    }
}

// dst = src * conj(dst)
void RKComplexArrayInPlaceConjugateMultiply(RKComplex *src, RKComplex *dst, const int count) {
    for (int k = 0; k < count; k++) {
        *dst = RKComplexMultiply(*src++, RKComplexConjugate(*dst));
        dst++;
    }
}

RKFloat RKFloatArraySum(RKFloat *src, const int count) {
    RKFloat sum = 0.0f;
    for (int k = 0; k < count; k++) {
        sum += *src++;
    }
    return sum;
}

RKComplex RKComplexArraySum(RKComplex *src, const int count) {
    RKComplex sum = {0.0f, 0.0f};
    for (int k = 0; k < count; k++) {
        sum.i += src->i;
        sum.q += src->q;
        src++;
    }
    return sum;
}

#pragma mark - Logger

int RKLog(const char *whatever, ...) {
    if (rkGlobalParameters.stream == NULL && rkGlobalParameters.logfile[0] == 0) {
        return 0;
    }
    int i = 0;
    size_t len;
    struct timeval utc;
    va_list args;
    struct tm tm;

    // Local memory
    static char *msg = NULL;
    static char *filename = NULL;
    static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

    if (msg == NULL) {
        msg = (char *)malloc(RKMaximumStringLength * sizeof(char));
    }
    if (filename == NULL) {
        filename = (char *)malloc(RKMaximumPathLength * sizeof(char));
    }
    if (msg == NULL || filename == NULL) {
        fprintf(stderr, "Error in RKLog().\n");
        free(filename);
        free(msg);
        return -1;
    }

    if (whatever == NULL) {
        va_start(args, whatever);
        if (va_arg(args, void *) == NULL) {
            #if defined(DEBUG_MUTEX_DESTROY)
            fprintf(stderr, "Deallocating RKLog's internal stuff ...\n");
            #endif
            if (msg) {
                free(msg);
            }
            if (filename) {
                free(filename);
            }
            pthread_mutex_destroy(&lock);
        }
        va_end(args);
        return 0;
    }

    pthread_mutex_lock(&lock);

    // Get the time
    gettimeofday(&utc, NULL);
    memcpy(&tm, gmtime(&utc.tv_sec), sizeof(struct tm));

    // Construct the string
    va_start(args, whatever);
    if (strlen(whatever) > RKMaximumStringLength - 256) {
        fprintf(stderr, "RKLog() could potential crash for string '%s'\n", whatever);
        return 1;
    }
    if (rkGlobalParameters.logTimeOnly) {
        if (whatever[0] == '>') {
            i += sprintf(msg, "             ");
        } else {
            i += strftime(msg, 16, "%T", &tm);
            i += sprintf(msg + i, ".%03d ", (int)utc.tv_usec / 1000);
        }
    } else {
        if (whatever[0] == '>') {
            i += sprintf(msg, "                    ");
        } else {
            i += strftime(msg, 32, "%Y/%m/%d %T ", &tm);
        }
    }
    char *okay_str = strcasestr(whatever, "ok");
    char *info_str = strcasestr(whatever, "info");
    char *error_str = strcasestr(whatever, "error");
    char *warning_str = strcasestr(whatever, "warning");
    bool has_ok = okay_str != NULL;
    bool has_info = info_str != NULL;
    bool has_error =  error_str != NULL;
    bool has_warning = warning_str != NULL;

    char *anchor = (char *)whatever + (whatever[0] == '>' ? 1 : 0);

    if (has_ok || has_info || has_error || has_warning) {
        char *colored_whatever = (char *)malloc(RKMaximumPacketSize * sizeof(char));

        if (has_ok) {
            len = (size_t)(okay_str - anchor);
        } else if (has_info) {
            len = (size_t)(info_str - anchor);
        } else if (has_error) {
            len = (size_t)(error_str - anchor);
        } else if (has_warning) {
            len = (size_t)(warning_str - anchor);
        } else {
            len = 0;
        }

        if (len > 0) {
            strncpy(colored_whatever, anchor, len);
            colored_whatever[len] = '\0';
            anchor += len;
        }

        if (rkGlobalParameters.showColor) {
            if (has_ok) {
                len += sprintf(colored_whatever + len, RKGreenColor);
            } else if (has_info) {
                len += sprintf(colored_whatever + len, RKSkyBlueColor);
            } else if (has_error) {
                len += sprintf(colored_whatever + len, RKRedColor);
            } else if (has_warning) {
                len += sprintf(colored_whatever + len, RKYellowColor);
            }
        }
        strncpy(colored_whatever + len, anchor, RKMaximumStringLength - len);

        i += vsprintf(msg + i, colored_whatever, args);

        if (rkGlobalParameters.showColor) {
            sprintf(msg + i, RKNoColor);
        }

        free(colored_whatever);
    } else {
        vsprintf(msg + i, anchor, args);
    }

    if (whatever[strlen(whatever) - 1] != '\n') {
        strncat(msg, "\n", RKMaximumStringLength - 1);
    }
    va_end(args);
    // Produce the string to the specified stream
    if (rkGlobalParameters.stream) {
        fprintf(rkGlobalParameters.stream, "%s", msg);
        fflush(rkGlobalParameters.stream);
    }
    // Write the string to a file if specified
    FILE *logFileID = NULL;
    if (rkGlobalParameters.dailyLog) {
        if (strlen(rkGlobalParameters.logFolder)) {
            i = sprintf(filename, "%s/%s-", rkGlobalParameters.logFolder, rkGlobalParameters.program);
        } else if (strlen(rkGlobalParameters.rootDataFolder)) {
            i = sprintf(filename, "%s/log/%s-", rkGlobalParameters.rootDataFolder, rkGlobalParameters.program);
        } else {
            i = 0;
        }
        strftime(filename + i, RKNameLength - i, "%Y%m%d.log", &tm);
        if (i) {
            RKPreparePath(filename);
        }
        logFileID = fopen(filename, "a");
    } else if (strlen(rkGlobalParameters.logfile)) {
        if (strlen(rkGlobalParameters.logFolder)) {
            //sprintf(filename, "%s/%s", rkGlobalParameters.logFolder, rkGlobalParameters.logfile);
            i = snprintf(filename, RKMaximumPathLength, "%s/%s", rkGlobalParameters.logFolder, rkGlobalParameters.logfile);
            if (i < 0) {
                fprintf(stderr, "Failed to generate filename.\n");
            }
        } else {
            strcpy(filename, rkGlobalParameters.logfile);
        }
        RKPreparePath(filename);
        logFileID = fopen(filename, "a");
    }
    if (logFileID) {
        fprintf(logFileID, "%s", msg);
        fclose(logFileID);
    }
    pthread_mutex_unlock(&lock);
    return 0;
}

void RKExit(int e) {
    RKVariableInString(NULL, NULL, 0);
    RKGetValueOfKey(NULL, NULL);
    RKGetCPUIndex(0xDEADBEEF);
    RKGetColorOfIndex(0xDEADBEEF);
    RKGetBackgroundColorOfIndex(0xDEADBEEF);
    RKGetBackgroundColorOfCubeIndex(0xDEADBEEF);
    RKUIntegerToCommaStyleString((unsigned long long)0xFEEDFACECAFEBEEF);
    RKIntegerToCommaStyleString((long long)0xFEEDFACECAFEBEEF);
    RKIntegerToHexStyleString((long long)0xFEEDFACECAFEBEEF);
    RKFloatToCommaStyleString((double)0xFEEDFACECAFEBEEF);
    RKTimevalToString((struct timeval){0xFEEDFACE, 0}, 0, false);
    pthread_mutex_destroy(&rkGlobalParameters.lock);
    RKLog(NULL, NULL);
    exit(e);
}

FILE *RKFileOpen(const char *filename, const char *mode) {
    return fopen(filename, mode);
}

int RKFileClose(FILE *fid) {
    return fclose(fid);
}

long RKFileTell(FILE *fid) {
    return ftell(fid);
}

int RKFileSeek(FILE *fid, long offset) {
    return fseek(fid, offset, SEEK_SET);
}

size_t RKFileWrite(const void *ptr, size_t size, size_t count, FILE *fid) {
    return fwrite(ptr, size, count, fid);
}

size_t RKFileRead(void *ptr, size_t size, size_t count, FILE *fid) {
    return fread(ptr, size, count, fid);
}

size_t RKFileGetSize(FILE *fid) {
    long origin = ftell(fid);
    fseek(fid, 0, SEEK_END);
    size_t size = ftell(fid);
    fseek(fid, origin, SEEK_SET);
    return size;
}

#pragma mark - Global Preferences

void RKSetStatusColor(const bool color) {
    rkGlobalParameters.statusColor = color;
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

bool RKGetWantScreenOutput(void) {
    return rkGlobalParameters.stream != NULL;
}

void RKSetUseDailyLog(const bool dailyLog) {
    rkGlobalParameters.dailyLog = dailyLog;
    rkGlobalParameters.logTimeOnly = true;
}

int RKSetProgramName(const char *name) {
    snprintf(rkGlobalParameters.program, sizeof(rkGlobalParameters.program), "%s", name);
    return RKResultSuccess;
}

int RKSetRootFolder(const char *folder) {
    RKLog("Warning. RKSetRootFolder() is deprecated. Use RKSetRootDataFolder() instead.\n");
    return RKSetRootDataFolder(folder);
}

int RKSetRootDataFolder(const char *folder) {
    if (strlen(folder) > RKMaximumPathLength - 64) {
        fprintf(stderr, "WARNING. Very long root folder.\n");
    }
    sprintf(rkGlobalParameters.rootDataFolder, "%s", folder);
    size_t len = strlen(rkGlobalParameters.rootDataFolder);
    while (rkGlobalParameters.rootDataFolder[len - 1] == '/') {
        len--;
    }
    rkGlobalParameters.rootDataFolder[len] = '\0';
    return RKResultSuccess;
}

int RKSetLogfile(const char *filename) {
    if (filename == NULL) {
        rkGlobalParameters.logfile[0] = '\0';
        return 0;
    } else if (strlen(filename) >= RKMaximumPathLength) {
        return 1;
    }
    snprintf(rkGlobalParameters.logfile, RKMaximumPathLength, "%s", filename);
    return RKResultSuccess;
}

int RKSetLogfileToDefault(void) {
    if (strlen(rkGlobalParameters.rootDataFolder)) {
        snprintf(rkGlobalParameters.logfile, RKMaximumPathLength, "%s", RKDefaultLogfile);
    } else {
        snprintf(rkGlobalParameters.logfile, RKMaximumPathLength, "%s/%s", rkGlobalParameters.rootDataFolder, RKDefaultLogfile);
    }
    return RKResultSuccess;
}

//

char *RKVersionString(void) {
    static char versionString[16];
    sprintf(versionString, "%s", __RKVersion__);
    return versionString;
}

RKValueType RKGuessValueType(const char *source) {
    RKValueType type = RKValueTypeVariable;
    if (*source == '"' || *source == '\'') {
        type = RKValueTypeString;
    } else if (*source == '{') {
        type = RKValueTypeDictionary;
    } else if (*source == '[') {
        type = RKValueTypeArray;
    } else if (*source >= '0' && *source <= '9') {
        char *dot = strchr(source, '.');
        if (dot) {
            type = RKValueTypeFloat;
        } else {
            type = RKValueTypeInt;
        }
    } else if (strcasestr(source, "false") || strcasestr(source, "true")) {
        type = RKValueTypeBool;
    } else if (strcasestr(source, "null")) {
        type = RKValueTypeNull;
    }
    return type;
}

#pragma mark - Filename / String

int RKIsFilenameStandard(const char *filename) {
    // Check if filename is something like [prefix]-[datetime]-[scan]-[symbol]
    char *basename = strrchr(filename, '/');
    if (basename == NULL) {
        basename = (char *)filename;
    } else {
        basename++;
    }
    // printf("basename = %s\n", basename);
    char *b = strchr(basename, '-');
    if (b == NULL) {
        return RKResultFilenameHasNoPrefix;
    }
    b++;
    // YYYYMMMDD - 2000 - 2099
    if (*b != '2') {
        return RKResultFilenameHasBadDate;
    }
    b++;
    if (*b != '0') {
        return RKResultFilenameHasBadDate;
    }
    b++;
    if (*b < '0' || *b > '9') {
        return RKResultFilenameHasBadDate;
    }
    b++;
    if (*b < '0' || *b > '9') {
        return RKResultFilenameHasBadDate;
    }
    b++;
    if (*b != '0' && *b != '1' && *b != '2') {
        return RKResultFilenameHasBadDate;
    }
    b++;
    if (*b < '0' || *b > '9') {
        return RKResultFilenameHasBadDate;
    }
    b++;
    if (*b < '0' || *b > '3') {
        return RKResultFilenameHasBadDate;
    }
    b++;
    if (*b < '0' || *b > '9') {
        return RKResultFilenameHasBadDate;
    }
    b++;
    if (*b != '-') {
        return RKResultFilenameHasBadDate;
    }
    b++;
    // HHMMSS
    if (*b != '0' && *b != '1' && *b != '2') {
        return RKResultFilenameHasBadTime;
    }
    b++;
    if (*b < '0' || *b > '9') {
        return RKResultFilenameHasBadTime;
    }
    b++;
    if (*b < '0' || *b > '5') {
        return RKResultFilenameHasBadTime;
    }
    b++;
    if (*b < '0' || *b > '9') {
        return RKResultFilenameHasBadTime;
    }
    b++;
    if (*b < '0' || *b > '5') {
        return RKResultFilenameHasBadTime;
    }
    b++;
    if (*b < '0' || *b > '9') {
        return RKResultFilenameHasBadTime;
    }
    b++;
    if (*b != '-') {
        return false;
    }
    b++;
    // Scan: [A-Z][0-9].[0-9][0-9]
    if (*b < 'A' && *b > 'Z') {
        return RKResultFilenameHasBadScan;
    }
    b++;
    if (*b < '0' || *b > '9') {
        return RKResultFilenameHasBadScan;
    }
    b = strrchr(b, '-');
    if (b == NULL) {
        return RKResultFilenameHasNoProduct;
    }
    b++;
    // Symbol: [A-Z]+
    if (*b < 'A' && *b > 'Z') {
        return RKResultFilenameHasNoProduct;
    }
    return true;
}

bool RKGetSymbolFromFilename(const char *filename, char *symbol) {
    // Find the basename
    char *basename = strrchr(filename, '/');
    if (basename == NULL) {
        basename = (char *)filename;
    } else {
        basename++;
    }
    // Find the third '-'
    memset(symbol, 0, RKMaximumSymbolLength);
    int c = 0;
    char *b = strchr(basename, '-');
    while (b != NULL && c++ < 3) {
        b = strchr(b + 1, '-');
    }
    if (b == NULL) {
        *symbol = '-';
        return false;
    }
    char *e = strchr(b + 1, '.');
    #ifdef DEBUG_FINDSYMBOL
    printf("basename = %s   b = %s   e = %s\n", basename, b, e);
    #endif
    if (b == filename) {
        RKLog("RKGetSymbolFromFilename() Unable to find product symbol.\n");
        *symbol = '-';
        return false;
    }
    b++;
    strncpy(symbol, b, MIN(RKMaximumSymbolLength - 1, e - b));
    return true;
}

bool RKGetPrefixFromFilename(const char *filename, char *prefix) {
    char *e = NULL;
    e = strstr(filename, ".nc");
    if (e == NULL) {
        e = (char *)filename + strlen(filename) - 1;
    }
    do {
        e--;
    } while (*e != '-' && e > filename);
    if (e <= filename) {
        RKLog("RKGetPrefixFromFilename() Unable to find filename prefix.\n");
        *prefix = '\0';
        return false;
    }
    size_t len = (size_t)(e - filename);
    strncpy(prefix, filename, len);
    prefix[len] = '\0';
    return true;
}

int RKListFilesWithSamePrefix(const char *filename, char list[][RKMaximumPathLength]) {
    int j = 0, k = 0;
    bool r;
    char *path;
    char prefix[1024];
    DIR *dir;
    struct dirent *ent;

    strcpy(list[0], filename);

    // Figure out the path of the filename
    path = RKFolderOfFilename(filename);
    // printf("path -> %s\n", path);
    if ((dir = opendir(path)) == NULL) {
        RKLog("RKListFilesWithSamePrefix() Unable to open directory %s\n", path);
        return 0;
    }
    // Use prefix to match the file pattern
    r = RKGetPrefixFromFilename(RKLastPartOfPath(filename), prefix);
    if (r == false) {
        RKLog("RKListFilesWithSamePrefix() Not a standard filename. Early return.\n");
        return 0;
    }
    char *ext = RKFileExtension(filename);
    // Now we list
    while ((ent = readdir(dir)) != NULL && k < 16) {
        //if (ent->d_name[0] == 'P')
        //   printf("%s %d (%d %d)\n", ent->d_name, ent->d_type, DT_REG, DT_LNK);
        if (ent->d_type != DT_LNK && ent->d_type != DT_REG) {
            continue;
        }
        if (strstr(ent->d_name, prefix) && strstr(ent->d_name, ext)) {
            // printf("  -> %s/%s\n", path, ent->d_name);
            if (strcmp(".", path)) {
                sprintf(list[k++], "%s/%s", path, ent->d_name);
            } else {
                sprintf(list[k++], "%s", ent->d_name);
            }
        }
    }
    closedir(dir);
    bool found;
    int count = k;
    int valid = 0;
    char desiredSymbol[7][RKMaximumSymbolLength], symbol[RKMaximumSymbolLength];
    // Attempt to sort to Z, V, W, D, P, R, ...
    k = 0;
    strcpy(desiredSymbol[k++], "Z");
    strcpy(desiredSymbol[k++], "V");
    strcpy(desiredSymbol[k++], "W");
    strcpy(desiredSymbol[k++], "D");
    strcpy(desiredSymbol[k++], "P");
    strcpy(desiredSymbol[k++], "R");
    strcpy(desiredSymbol[k++], "K");
    for (k = 0; k < 7; k++) {
        for (j = k; j < count; j++) {
            found = RKGetSymbolFromFilename(list[j], symbol);
            // printf("  j = %d   symbol = %s / %s   %d\n", j, symbol, desiredSymbol[k], found);
            if (found && !strcmp(desiredSymbol[k], symbol)) {
                // Swap k & j
                // printf("    Swap %d <-> %d\n", j, k);
                strcpy(prefix, list[k]);
                strcpy(list[k], list[j]);
                strcpy(list[j], prefix);
                valid++;
                break;
            }
        }
    }
    // for (k = 0; k < valid; k++) {
    //    printf("-> %s\n", list[k]);
    // }
    return valid;
}

#pragma mark - Screen Output

void RKShowBanner(const char *title, const char *color) {
    int k;
    struct winsize terminalSize = {.ws_col = 0, .ws_row = 0};
    k = ioctl(0, TIOCGWINSZ, &terminalSize);
    //fprintf(stderr, "k = %d   %d x %d\n", k, terminalSize.ws_col, terminalSize.ws_row);
    if (k != 0 || (terminalSize.ws_col == 0 && terminalSize.ws_row == 0)) {
        terminalSize.ws_col = 80;
        terminalSize.ws_row = 24;
    }
    char message[terminalSize.ws_col + 32];
    char padding[terminalSize.ws_col + 32];

    if (rkGlobalParameters.showColor) {
        k = sprintf(padding, "%s", color);
    } else {
        k = 0;
    }
    k += RKStringCenterized(padding + k, "", terminalSize.ws_col);
    if (rkGlobalParameters.showColor) {
        sprintf(padding + k, RKNoColor);
    }

    if (rkGlobalParameters.showColor) {
        k = sprintf(message, "%s", color);
    } else {
        k = 0;
    }
    k += RKStringCenterized(message + k, title, terminalSize.ws_col);
    if (rkGlobalParameters.showColor) {
        sprintf(message + k, RKNoColor);
    }

    printf("\r");
    printf("%s\n", padding);
    printf("%s\n", message);
    printf("%s\n", padding);
    fflush(stdout);
}

void RKShowName(void) {
    RKShowBanner("RadarKit " __RKVersion__, RKRadarKitColor);
}

void RKShowTypeSizes(void) {
    SHOW_FUNCTION_NAME
    RKPulse *pulse = NULL;
    RKRay *ray = NULL;
    RKSweep *sweep = NULL;

    // Keeep current output stream and temporary change to screen output
    FILE *stream = rkGlobalParameters.stream;
    rkGlobalParameters.stream = stdout;

    SHOW_SIZE(void *)
    SHOW_SIZE(char)
    SHOW_SIZE(short)
    SHOW_SIZE(int)
    SHOW_SIZE(long)
    SHOW_SIZE(long long)
    SHOW_SIZE(unsigned char)
    SHOW_SIZE(unsigned short)
    SHOW_SIZE(unsigned int)
    SHOW_SIZE(unsigned long)
    SHOW_SIZE(unsigned long long)
    SHOW_SIZE(size_t)
    SHOW_SIZE(ssize_t)
    SHOW_SIZE(bool)
    SHOW_SIZE(int8_t)
    SHOW_SIZE(uint8_t)
    SHOW_SIZE(int16_t)
    SHOW_SIZE(uint16_t)
    SHOW_SIZE(int32_t)
    SHOW_SIZE(uint32_t)
    SHOW_SIZE(int64_t)
    SHOW_SIZE(uint64_t)
    SHOW_SIZE(struct sockaddr)
    SHOW_SIZE(struct sockaddr_in)
    SHOW_SIZE(RKInitFlag)
    SHOW_SIZE(RKStatusFlag)
    SHOW_SIZE(RKHealthFlag)
    SHOW_SIZE(RKPositionFlag)
    SHOW_SIZE(RKPulseStatus)
    SHOW_SIZE(RKRayStatus)
    SHOW_SIZE(RKEngineState)
    SHOW_SIZE(RKStatusEnum)
    SHOW_SIZE(RKStream)
    SHOW_SIZE(RKByte)
    SHOW_SIZE(RKFloat)
    SHOW_SIZE(RKInt16C)
    SHOW_SIZE(RKComplex)
    SHOW_SIZE(RKVec)
    SHOW_SIZE(RKRadarDesc)
    SHOW_SIZE(RKConfig)
    SHOW_SIZE(RKConfigF5)
    SHOW_SIZE(RKHealth)
    SHOW_SIZE(RKNodalHealth)
    SHOW_SIZE(RKPosition)
    SHOW_SIZE(RKPulseHeader)
    SHOW_SIZE(RKPulseHeaderF1)
    SHOW_SIZE(RKPulseParameters)
    SHOW_SIZE_SIMD(pulse->headerBytes)
    SHOW_SIZE_SIMD(RKPulse)
    SHOW_SIZE(RKRayHeader)
    SHOW_SIZE(RKRayHeaderF1)
    SHOW_SIZE_SIMD(ray->headerBytes)
    SHOW_SIZE_SIMD(ray->data)
    SHOW_SIZE(RKSweep)
    SHOW_SIZE(sweep->header)
    SHOW_SIZE(RKPreferenceObject)
    SHOW_SIZE(RKControl)
    SHOW_SIZE(RKStatus)
    SHOW_SIZE(RKFileMonitor)
    SHOW_SIZE(RKFilterAnchor)
    SHOW_SIZE(RKWaveform)
    SHOW_SIZE(RKFileHeader)
    SHOW_SIZE(RKWaveFileGlobalHeader)
    SHOW_SIZE(RKWaveformCalibration)
    SHOW_SIZE(RKSweepHeader)
    SHOW_SIZE(RKProductDesc)
    SHOW_SIZE(RKProductHeader)

    printf("\n");

    RKFilterAnchor anchor = RKFilterAnchorDefault;
    printf("RKFilterAnchorDefault:\n");
    printf(".%s\n", RKVariableInString("name", &anchor.name, RKValueTypeUInt32));
    printf(".%s\n", RKVariableInString("origin", &anchor.origin, RKValueTypeUInt32));
    printf(".%s\n", RKVariableInString("length", &anchor.length, RKValueTypeUInt32));
    printf(".%s\n", RKVariableInString("inputOrigin", &anchor.inputOrigin, RKValueTypeUInt32));
    printf(".%s\n", RKVariableInString("outputOrigin", &anchor.outputOrigin, RKValueTypeUInt32));
    printf(".%s\n", RKVariableInString("maxDataLength", &anchor.maxDataLength, RKValueTypeUInt32));
    printf(".%s\n", RKVariableInString("subCarrierFrequency", &anchor.subCarrierFrequency, RKValueTypeFloat));
    printf(".%s dB\n", RKVariableInString("sensitivityGain", &anchor.sensitivityGain, RKValueTypeFloat));
    printf(".%s dB\n", RKVariableInString("filterGain", &anchor.filterGain, RKValueTypeFloat));
    printf(".%s\n", RKVariableInString("fullScale", &anchor.fullScale, RKValueTypeFloat));

    printf("\n");

    RKFilterAnchor anchor2 = RKFilterAnchorDefaultWithMaxDataLength(1000);
    memcpy(&anchor, &anchor2, sizeof(RKFilterAnchor));
    printf("RKFilterAnchorDefaultWithMaxDataLength(1000):\n");
    printf(".%s\n", RKVariableInString("name", &anchor.name, RKValueTypeUInt32));
    printf(".%s\n", RKVariableInString("origin", &anchor.origin, RKValueTypeUInt32));
    printf(".%s\n", RKVariableInString("length", &anchor.length, RKValueTypeUInt32));
    printf(".%s\n", RKVariableInString("inputOrigin", &anchor.inputOrigin, RKValueTypeUInt32));
    printf(".%s\n", RKVariableInString("outputOrigin", &anchor.outputOrigin, RKValueTypeUInt32));
    printf(".%s\n", RKVariableInString("maxDataLength", &anchor.maxDataLength, RKValueTypeUInt32));
    printf(".%s\n", RKVariableInString("subCarrierFrequency", &anchor.subCarrierFrequency, RKValueTypeFloat));
    printf(".%s dB\n", RKVariableInString("sensitivityGain", &anchor.sensitivityGain, RKValueTypeFloat));
    printf(".%s dB\n", RKVariableInString("filterGain", &anchor.filterGain, RKValueTypeFloat));
    printf(".%s\n", RKVariableInString("fullScale", &anchor.fullScale, RKValueTypeFloat));

    printf("\n");

    int k = 0;
    while (k != RKResultCount) {
        printf(RKLimeColor "%-50s" RKPurpleColor "%2d" RKNoColor "\n", rkResultStrings[k], k);
        k++;
    }

    printf("\n");

    // Some constants
    k = RKMomentIndexCount; printf("%s\n", RKVariableInString("RKMomentIndexCount", &k, RKValueTypeInt));
    k = RKProductIndexCount; printf("%s\n", RKVariableInString("RKProductIndexCount", &k, RKValueTypeInt));

    // Restoring previous output stream
    rkGlobalParameters.stream = stream;
}

void RKShowVecFloatLowPrecision(const char *name, const float *p, const int n) {
    int i = 0;
    int k = 0;
    char str[RKMaximumStringLength];
    k = sprintf(str, "%s[", name);
    while (i < n && k < RKMaximumStringLength - 20)
        k += sprintf(str + k, "%6.2f ", p[i++]);
    sprintf(str + k, "]");
    printf("%s\n", str);
}

void RKShowVecFloat(const char *name, const float *p, const int n) {
    int i = 0;
    int k = 0;
    char str[RKMaximumStringLength];
    k = sprintf(str, "%s[", name);
    while (i < n && k < RKMaximumStringLength - 20)
        k += sprintf(str + k, "%+9.4f           ", p[i++]);
    sprintf(str + k, "]");
    printf("%s\n", str);
}

void RKShowVecIQZ(const char *name, const RKIQZ *p, const int n) {
    int i = 0;
    int k = 0;
    char *str = (char *)malloc(RKMaximumStringLength);
    if (str == NULL) {
        fprintf(stderr, "Error allocating string buffer.\n");
        return;
    }
    k = sprintf(str, "%s[", name);
    while (i < n && k < RKMaximumStringLength - 20) {
        k += sprintf(str + k, "%+9.4f%+9.4fi ", p->i[i], p->q[i]);
        i++;
    }
    sprintf(str + k, "]");
    printf("%s\n", str);
    fflush(stdout);
    free(str);
}

void RKShowVecComplex(const char *name, const RKComplex *p, const int n) {
    int i = 0;
    int k = 0;
    char *str = (char *)malloc(RKMaximumStringLength);
    if (str == NULL) {
        fprintf(stderr, "Error allocating string buffer.\n");
        return;
    }
    k = sprintf(str, "%s[", name);
    while (i < n && k < RKMaximumStringLength - 20) {
        k += sprintf(str + k, "%+9.4f%+9.4fi ", p[i].i, p[i].q);
        i++;
    }
    sprintf(str + k, "]");
    printf("%s\n", str);
    fflush(stdout);
    free(str);
}

static char *arrayHeadTailElementsInString(const float *d, const int length) {
    int c, k = 0;
    static char line[1024];
    for (c = 0; c < 3; c++) {
        k += sprintf(line + k, " %6.2f", *(d + c));
    }
    k += sprintf(line + k, " ...");
    for (c = length - 3; c < length; c++) {
        k += sprintf(line + k, " %6.2f", *(d + c));
    }
    return line;
}

void RKShowArray(const RKFloat *data, const char *letter, const int width, const int height) {
    int j, k = 0, n = MAX(1, (int)strlen(letter) - 8);
    char text[1024];
    char pad[n + 1];
    memset(pad, ' ', n);
    pad[n] = '\0';
    k = sprintf(text, "      %s%8s%s = [ %s ]\n",
                rkGlobalParameters.showColor ? RKYellowColor : "", letter,
                rkGlobalParameters.showColor ? RKNoColor : "",
                arrayHeadTailElementsInString(data, width));
    if (height > 1) {
        k += sprintf(text + k, "              %s  [ %s ]\n", pad, arrayHeadTailElementsInString(data + width, width));
    }
    if (height > 2) {
        k += sprintf(text + k, "              %s  [ %s ]\n", pad, arrayHeadTailElementsInString(data + 2 * width, width));
    }
    if (height > 6) {
        k += sprintf(text + k, "              %s  [     ...\n", pad);
    }
    for (j = MAX(3, height - 3); j < height; j++) {
        k += sprintf(text + k, "              %s  [ %s ]\n", pad, arrayHeadTailElementsInString(data + j * width, width));
    }
    printf("%s", text);
}

char *RKStringFromValue(const void *value, RKValueType type) {
    char    *c = (char *)value;
    float    f = *((float *)value);
    double   d = *((double *)value);
    int      i = *((int *)value);
    long     l = *((long *)value);
    unsigned int u = *((unsigned int *)value);
    unsigned long ul = *((unsigned long *)value);
    int8_t    i8 = *((int8_t *)value);
    uint8_t   u8 = *((uint8_t *)value);
    int16_t  i16 = *((int16_t *)value);
    uint16_t u16 = *((uint16_t *)value);
    int32_t  i32 = *((int32_t *)value);
    uint32_t u32 = *((uint32_t *)value);
    int64_t  i64 = *((int64_t *)value);
    uint64_t u64 = *((uint64_t *)value);
    size_t     s = *((size_t *)value);
    ssize_t   ss = *((ssize_t *)value);
    switch (type) {
        case RKValueTypeInt:
            l = i;
        case RKValueTypeLong:
            c = RKIntegerToCommaStyleString(l);
            break;
        case RKValueTypeInt8:
            i16 = i8;
        case RKValueTypeInt16:
            i32 = i16;
        case RKValueTypeInt32:
            i64 = i32;
        case RKValueTypeInt64:
            c = RKIntegerToCommaStyleString(i64);
            break;

        case RKValueTypeIntInHex:
            l = i;
        case RKValueTypeLongInHex:
            c = RKIntegerToHexStyleString(l);
            break;
        case RKValueTypeInt8InHex:
            i16 = i8;
        case RKValueTypeInt16InHex:
            i32 = i16;
        case RKValueTypeInt32InHex:
            i64 = i32;
        case RKValueTypeInt64InHex:
            c = RKIntegerToHexStyleString(i64);
            break;

        case RKValueTypeUInt:
            ul = u;
        case RKValueTypeULong:
            c = RKUIntegerToCommaStyleString((unsigned long long)ul);
            break;
        case RKValueTypeUInt8:
            u16 = u8;
        case RKValueTypeUInt16:
            u32 = u16;
        case RKValueTypeUInt32:
            u64 = u32;
        case RKValueTypeUInt64:
            c = RKUIntegerToCommaStyleString((unsigned long long)u64);
            break;

        case RKValueTypeUIntInHex:
            ul = u;
        case RKValueTypeULongInHex:
            c = RKIntegerToHexStyleString((unsigned long long)ul);
            break;
        case RKValueTypeUInt8InHex:
            u16 = u8;
        case RKValueTypeUInt16InHex:
            u32 = u16;
        case RKValueTypeUInt32InHex:
            u64 = u32;
        case RKValueTypeUInt64InHex:
            c = RKIntegerToHexStyleString((unsigned long long)u64);
            break;

        case RKValueTypeSize:
            c = RKUIntegerToCommaStyleString((unsigned long long)s);
            break;
        case RKValueTypeSSize:
            c = RKIntegerToCommaStyleString((long long)ss);
            break;
        case RKValueTypeFloat:
            d = f;
        case RKValueTypeDouble:
            c = RKFloatToCommaStyleString(d);
            break;
        case RKValueTypeFloatWithOneDecimals:
            d = f;
        case RKValueTypeDoubleWithOneDecimals:
            c = RKFloatToCommaStyleStringAndDecimals(d, 1);
            break;
        case RKValueTypeFloatWithTwoDecimals:
            d = f;
        case RKValueTypeDoubleWithTwoDecimals:
            c = RKFloatToCommaStyleStringAndDecimals(d, 2);
            break;
        case RKValueTypeFloatWithThreeDecimals:
            d = f;
        case RKValueTypeDoubleWithThreeDecimals:
            c = RKFloatToCommaStyleStringAndDecimals(d, 3);
            break;
        case RKValueTypeFloatWithFourDecimals:
            d = f;
        case RKValueTypeDoubleWithFourDecimals:
            c = RKFloatToCommaStyleStringAndDecimals(d, 4);
            break;
        case RKValueTypeFloatWithFiveDecimals:
            d = f;
        case RKValueTypeDoubleWithFiveDecimals:
            c = RKFloatToCommaStyleStringAndDecimals(d, 5);
            break;
        case RKValueTypeFloatWithSixDecimals:
            d = f;
        case RKValueTypeDoubleWithSixDecimals:
            c = RKFloatToCommaStyleStringAndDecimals(d, 6);
            break;
        case RKValueTYpeFloatDividedBy1k:
            d = f;
        case RKValueTYpeDoubleDividedBy1k:
            d = 1.0e-3 * d;
            c = RKFloatToCommaStyleString(d);
            break;
        case RKValueTYpeFloatDividedBy1M:
            d = f;
        case RKValueTYpeDoubleDividedBy1M:
            d = 1.0e-6 * d;
            c = RKFloatToCommaStyleString(d);
            break;
        case RKValueTYpeFloatMultipliedBy1k:
            d = f;
        case RKValueTYpeDoubleMultipliedBy1k:
            d = 1.0e3 * d;
            c = RKFloatToCommaStyleString(d);
            break;
        case RKValueTYpeFloatMultipliedBy1M:
            d = f;
        case RKValueTYpeDoubleMultipliedBy1M:
            d = 1.0e6 * d;
            c = RKFloatToCommaStyleString(d);
            break;
        case RKValueTypeNumericString:
        default:
            break;
    }
    return c;
}

char *RKVariableInString(const char *name, const void *value, RKValueType type) {
    static int ibuf = 0;
    static RKName stringBuffer[16];
    static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

    if (name == NULL) {
        #if defined(DEBUG_MUTEX_DESTROY)
        fprintf(stderr, "RKVariableInString: Destroying static mutex lock ...\n");
        #endif
        pthread_mutex_destroy(&lock);
        return NULL;
    }

    char *string = stringBuffer[ibuf];
    memset(string, 0, RKNameLength);

    pthread_mutex_lock(&lock);
    ibuf = ibuf == 15 ? 0 : ibuf + 1;
    pthread_mutex_unlock(&lock);

    char *c = RKStringFromValue(value, type);
    bool b = *((bool *)value);

    if (rkGlobalParameters.showColor) {
        if (type == RKValueTypeNull) {
            snprintf(string, RKNameLength, RKOrangeColor "%s" RKMonokaiRed " = " RKPinkColor "Null" RKNoColor, name);
        } else if (type == RKValueTypeBool) {
            snprintf(string, RKNameLength, RKOrangeColor "%s" RKMonokaiRed " = " RKPurpleColor "%s" RKNoColor, name, (b) ? "True" : "False");
        } else if (type == RKValueTypeString) {
            snprintf(string, RKNameLength, RKOrangeColor "%s" RKMonokaiRed " = " RKNoColor "'" RKSalmonColor "%s" RKNoColor "'", name, c);
        } else {
            snprintf(string, RKNameLength, RKOrangeColor "%s" RKMonokaiRed " = " RKLimeColor "%s" RKNoColor, name, c);
        }
    } else {
        if (type == RKValueTypeNull) {
            snprintf(string, RKNameLength, "%s = Null", name);
        } else if (type == RKValueTypeBool) {
            snprintf(string, RKNameLength, "%s = %s", name, (b) ? "True" : "False");
        } else if (type == RKValueTypeString) {
            snprintf(string, RKNameLength, "%s = '%s'", name, c);
        } else {
            snprintf(string, RKNameLength, "%s = %s", name, c);
        }
    }
    return string;
}

size_t RKPrettyStringFromValueString(char *destination, const char *source) {
    if (strlen(source) == 0) {
        *destination = '\0';
        return 0;
    }
    RKValueType type = RKGuessValueType(source);
    if (type == RKValueTypeNull) {
        if (rkGlobalParameters.showColor) {
            return sprintf(destination, RKMonokaiGreen "null" RKNoColor);
        }
        return sprintf(destination, "null");
    }
    if (type == RKValueTypeDictionary) {
        return RKPrettyStringFromKeyValueString(destination, source);
    }
    if (type == RKValueTypeArray) {
        return RKPrettyStringFromKeyValueString(destination, source);
    }
    if (type == RKValueTypeBool || type == RKValueTypeInt || type == RKValueTypeFloat) {
        if (rkGlobalParameters.showColor) {
            return sprintf(destination, RKMonokaiPurple "%s" RKNoColor, source);
        }
        return sprintf(destination, "%s", source);
    }
    if (type == RKValueTypeString) {
        if (rkGlobalParameters.showColor) {
            return sprintf(destination, RKMonokaiYellow "%s" RKNoColor, source);
        }
        return sprintf(destination, "%s", source);
    }
    if (type == RKValueTypeVariable) {
        if (rkGlobalParameters.showColor) {
            return sprintf(destination, RKMonokaiOrange "%s" RKNoColor, source);
        }
        return sprintf(destination, "%s", source);
    }
    return sprintf(destination, "%s", source);
}

size_t RKPrettyStringSizeEstimate(const char *source) {
    char *c = (char *)source;
    char *e = c + strlen(source);
    size_t s = (size_t)(e - c + 1);
    // Each color change uses up to 14 B for \033[38;5;123m (9-11 color dependent) and \033[m (3)
    int k = 0;
    do {
        if (*c == ':') {
            k += 3;
        } else if (*c == ','  || *c == ']' || *c == '}') {
            k++;
        }
    } while (c++ < e);
    s += k * 14;
    return s;
}

size_t RKPrettyStringFromKeyValueString(char *destination, const char *source) {
    if (strlen(source) == 0) {
        *destination = '\0';
        return 0;
    }
    size_t s = RKPrettyStringSizeEstimate(source);

    char *c = (char *)source;

    char b = *c;
    size_t size = 0;

    if (b == '{' || b == '[') {
        c++;
        char *element = malloc(s);
        size = sprintf(destination, (b == '{') ? "{" : "[");
        do {
            c = RKJSONGetElement(element, c);
            if (size > 1) {
                size += sprintf(destination + size, ", ");
            }
            if (b == '{') {
                size += RKPrettyStringFromKeyValueString(destination + size, element);
            } else {
                size += RKPrettyStringFromValueString(destination + size, element);
            }
        } while (strlen(element) > 0 && strlen(c) > 1);
        size += sprintf(destination + size, (b == '{') ? "}" : "]");
        #ifdef _SHOW_PRETTY_STRING_MEMORY
        printf(RKMonokaiGreen "RKPrettyStringFromKeyValueString()" RKNoColor " size = %d / %d / %s %s\n",
               (int)size, (int)s,
               size < s ? RKMonokaiGreen "o" RKNoColor : RKMonokaiRed "x" RKNoColor,
               destination);
        #endif
        free(element);
        return size;
    }

    char *t = malloc(s);
    char *key = malloc(s);
    char *value = malloc(s);

    RKJSONKeyValueFromElement(key, value, source);

    strcpy(t, key);
    RKPrettyStringFromValueString(key, t);

    strcpy(t, value);
    RKPrettyStringFromValueString(value, t);

    if (rkGlobalParameters.showColor) {
        size = sprintf(destination, "%s " RKMonokaiPink "=" RKNoColor " %s", key, value);
    } else {
        size = sprintf(destination, "%s = %s", key, value);
    }
    #ifdef _SHOW_PRETTY_STRING_MEMORY
    printf(RKMonokaiGreen "RKPrettyStringFromKeyValueString()" RKNoColor " size = %d / %d / %s %s\n",
           (int)size, (int)s,
           size < s ? RKMonokaiGreen "o" RKNoColor : RKMonokaiRed "x" RKNoColor,
           destination);
    #endif

    free(value);
    free(key);
    free(t);

    return size;
}

#pragma mark - Buffer

void *RKMalloc(const uint32_t capacity) {
    void *mem = NULL;
    size_t size = (capacity + RKMemoryAlignSize - 1) / RKMemoryAlignSize * RKMemoryAlignSize;
    #ifdef _SHOW_MALLOC
    RKLog("RKMalloc() size = %s B\n", RKUIntegerToCommaStyleString(size));
    #endif
    int r = posix_memalign(&mem, RKMemoryAlignSize, size);
    if (r) {
        RKLog("RKMalloc() Error. Unable to allocate memory");
    }
    return mem;
}

void RKZeroOutFloat(RKFloat *data, const uint32_t capacity) {
    memset(data, 0, capacity * sizeof(RKFloat));
}

void RKZeroOutIQZ(RKIQZ *data, const uint32_t capacity) {
    memset(data->i, 0, capacity * sizeof(RKFloat));
    memset(data->q, 0, capacity * sizeof(RKFloat));
}

void RKZeroTailFloat(RKFloat *data, const uint32_t capacity, const uint32_t origin) {
    memset(&data[origin], 0, (capacity - origin) * sizeof(RKFloat));
}

void RKZeroTailIQZ(RKIQZ *data, const uint32_t capacity, const uint32_t origin) {
    memset(&data->i[origin], 0, (capacity - origin) * sizeof(RKFloat));
    memset(&data->q[origin], 0, (capacity - origin) * sizeof(RKFloat));
}

#pragma mark - Pulse

//
// Each slot should have a structure as follows
//
//    RKPulseHeader      header;
//    RKPulseParameters  parameters;
//    RKInt16C           X[2][capacity];
//    RKComplex          Y[2][capacity];
//    RKIQZ              Z[2];
//
size_t RKPulseBufferAlloc(RKBuffer *mem, const uint32_t capacity, const uint32_t count) {
    size_t alignment = RKMemoryAlignSize / sizeof(RKFloat);
    if (capacity != (capacity / alignment) * alignment) {
        RKLog("Error. Unable to allocate for capacity = %s. Must be multiple of %d!",
            RKIntegerToCommaStyleString(capacity), alignment);
        return 0;
    }
    RKPulse *pulse;
    size_t headerSize = sizeof(pulse->headerBytes);
    if (headerSize != (headerSize / RKMemoryAlignSize) * RKMemoryAlignSize) {
        RKLog("Error. The framework has not been compiled with proper structure size.");
        return 0;
    }
    size_t channelCount = 2;
    size_t pulseSize = headerSize + channelCount * capacity * (sizeof(RKInt16C) + 4 * sizeof(RKFloat));
    if (pulseSize != (pulseSize / RKMemoryAlignSize) * RKMemoryAlignSize) {
        RKLog("Error. The total pulse size %s does not conform to SIMD alignment.", RKUIntegerToCommaStyleString(pulseSize));
        return 0;
    }
    size_t bytes = count * pulseSize;
    if (posix_memalign((void **)mem, RKMemoryAlignSize, bytes)) {
        RKLog("Error. Unable to allocate pulse buffer.");
        exit(EXIT_FAILURE);
    }
    memset(*mem, 0, bytes);
    // Set the pulse capacity
    int i = 0;
    void *m = *mem;
    while (i < count) {
        RKPulse *pulse = (RKPulse *)m;
        pulse->header.capacity = capacity;
        pulse->header.gateCount = 1;
        pulse->header.i = -(uint64_t)count + i;
        m += pulseSize;
        i++;
    }
    return bytes;
}

void RKPulseBufferFree(RKBuffer mem) {
    return free(mem);
}

// Get a pulse from a pulse buffer
RKPulse *RKGetPulseFromBuffer(RKBuffer buffer, const uint32_t k) {
    RKPulse *pulse = (RKPulse *)buffer;
    size_t headerSize = sizeof(pulse->headerBytes);
    size_t pulseSize = headerSize + 2 * pulse->header.capacity * (sizeof(RKInt16C) + 4 * sizeof(RKFloat));
    return (RKPulse *)(buffer + k * pulseSize);
}

// Get the raw data in 16-bit I/Q from a pulse
RKInt16C *RKGetInt16CDataFromPulse(RKPulse *pulse, const uint32_t c) {
    void *m = (void *)pulse->data;
    return (RKInt16C *)(m + c * pulse->header.capacity * sizeof(RKInt16C));
}

// Get the compressed I/Q data in RKComplex from a pulse
RKComplex *RKGetComplexDataFromPulse(RKPulse *pulse, const uint32_t c) {
    void *m = (void *)pulse->data;
    m += 2 * pulse->header.capacity * sizeof(RKInt16C);
    return (RKComplex *)(m + c * pulse->header.capacity * sizeof(RKComplex));
}

// Get the compressed I/Q data in RKIQZ from a pulse
RKIQZ RKGetSplitComplexDataFromPulse(RKPulse *pulse, const uint32_t c) {
    void *m = (void *)pulse->data;
    m += 2 * pulse->header.capacity * (sizeof(RKInt16C) + sizeof(RKComplex));
    m += c * pulse->header.capacity * 2 * sizeof(RKFloat);
    RKIQZ data = {(RKFloat *)m, (RKFloat *)(m + pulse->header.capacity * sizeof(RKFloat))};
    return data;
}

// Clear the pulse
int RKClearPulseBuffer(RKBuffer buffer, const uint32_t count) {
    for (uint32_t k = 0; k < count; k++) {
        RKPulse *pulse = RKGetPulseFromBuffer(buffer, k);
        pulse->header.s = RKPulseStatusVacant;
        pulse->header.i = -(uint64_t)count + k;
        pulse->header.gateCount = 0;
        memset(pulse->data, 0, 2 * pulse->header.capacity * (sizeof(RKInt16C) + 4 * sizeof(RKFloat)));
    }
    return RKResultSuccess;
}

// Read pulse from a file reference (pulse->header.s is left RKStatusVacant)
// NOTE: The header status (pulse->header.s) must be handled outside of this function.
//       Otherwise, pulse engines and moment engines could prematurely start processing
//       the data without the proper configIndex, etc.
int RKReadPulseFromFileReference(RKPulse *pulse, RKFileHeader *fileHeader, FILE *fid) {
    int i, j;
    size_t readsize;
    uint32_t gateCount = 0;
    static RKPulseHeader *header = NULL;
    static RKPulseHeaderF1 *headerV1 = NULL;

    // Deallocate static memories if fid == NULL
    if (fid == NULL) {
        if (headerV1) {
            free(headerV1);
            headerV1 = NULL;
        }
        if (header) {
            free(header);
            header = NULL;
        }
        return RKResultSuccess;
    }

    const uint32_t capacity = pulse->header.capacity;

    // Read routine based on file version
    switch (fileHeader->format) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
            RKLog("Warning. Format = %d not expected.\n", fileHeader->format);
            return RKResultNothingToRead;
            break;
        case 5:
            RKLog("Warning. Reading as version 6.\n");
        case 6:
            if (headerV1 == NULL) {
                // RKLog("Allocating headerV1 ...\n");
                headerV1 = (RKPulseHeaderF1 *)malloc(sizeof(RKPulseHeaderF1));
            }
            readsize = fread(headerV1, sizeof(RKPulseHeaderF1), 1, fid);
            pulse->header.i = headerV1->i;
            pulse->header.n = headerV1->n;
            pulse->header.t = headerV1->t;
            pulse->header.gateCount = headerV1->gateCount;
            pulse->header.downSampledGateCount = header->downSampledGateCount;
            pulse->header.pulseWidthSampleCount = headerV1->pulseWidthSampleCount;
            pulse->header.marker = headerV1->marker;
            pulse->header.time = headerV1->time;
            pulse->header.timeDouble = headerV1->timeDouble;
            pulse->header.rawAzimuth = headerV1->rawAzimuth;
            pulse->header.rawElevation = headerV1->rawElevation;
            pulse->header.configIndex = headerV1->configIndex;
            pulse->header.configSubIndex = headerV1->configSubIndex;
            pulse->header.gateSizeMeters = headerV1->gateSizeMeters;
            pulse->header.elevationDegrees = headerV1->elevationDegrees;
            pulse->header.azimuthDegrees = headerV1->azimuthDegrees;
            pulse->header.elevationVelocityDegreesPerSecond = headerV1->elevationVelocityDegreesPerSecond;
            pulse->header.azimuthVelocityDegreesPerSecond = headerV1->azimuthVelocityDegreesPerSecond;
            break;
        default:
            if (header == NULL) {
                header = (RKPulseHeader *)malloc(sizeof(RKPulseHeader));
            }
            readsize = fread(header, sizeof(RKPulseHeader), 1, fid);
            pulse->header.i = header->i;
            pulse->header.n = header->n;
            pulse->header.t = header->t;
            pulse->header.gateCount = header->gateCount;
            pulse->header.downSampledGateCount = header->downSampledGateCount;
            pulse->header.pulseWidthSampleCount = header->pulseWidthSampleCount;
            pulse->header.marker = header->marker;
            pulse->header.time = header->time;
            pulse->header.timeDouble = header->timeDouble;
            pulse->header.rawAzimuth = header->rawAzimuth;
            pulse->header.rawElevation = header->rawElevation;
            pulse->header.gateSizeMeters = header->gateSizeMeters;
            pulse->header.elevationDegrees = header->elevationDegrees;
            pulse->header.azimuthDegrees = header->azimuthDegrees;
            pulse->header.elevationVelocityDegreesPerSecond = header->elevationVelocityDegreesPerSecond;
            pulse->header.azimuthVelocityDegreesPerSecond = header->azimuthVelocityDegreesPerSecond;
            break;
    }
    if (readsize == 0) {
        if (headerV1 != NULL) {
            RKLog("Freeing headerV1 ...\n");
            free(headerV1);
            headerV1 = NULL;
        }
        if (header != NULL) {
            free(header);
            header = NULL;
        }
        return RKResultNothingToRead;
    }
    if (pulse->header.capacity != capacity) {
        pulse->header.capacity = capacity;
        RKLog("Warning. Pulse capacity restored.\n");
    }
    // Pulse payload: H and V data into channels 0 and 1, respectively. Duplicate to split-complex storage
    for (j = 0; j < 2; j++) {
        if (fileHeader->dataType == RKRawDataTypeFromTransceiver) {
            RKInt16C *x = RKGetInt16CDataFromPulse(pulse, j);
            gateCount = pulse->header.gateCount;
            pulse->header.downSampledGateCount = 0;
            readsize = fread(x, sizeof(RKInt16C), gateCount, fid);
        } else if (fileHeader->dataType == RKRawDataTypeAfterMatchedFilter) {
            RKComplex *x = RKGetComplexDataFromPulse(pulse, j);
            gateCount = pulse->header.downSampledGateCount;
            readsize = fread(x, sizeof(RKComplex), gateCount, fid);
            RKIQZ z = RKGetSplitComplexDataFromPulse(pulse, j);
            for (i = 0; i < gateCount; i++) {
                z.i[i] = x[i].i;
                z.q[i] = x[i].q;
            }
        } else {
            return RKResultRawDataTypeUndefined;
        }
        if (readsize != gateCount) {
            RKLog("Error in RKReadPulseFromFileReference() readsize = %s != %s || > %s\n",
                  RKIntegerToCommaStyleString(readsize),
                  RKIntegerToCommaStyleString(gateCount),
                  RKIntegerToCommaStyleString(capacity));
            return RKResultTooBig;
        }
    }
    return RKResultSuccess;
}

RKPulse *RKGetVacantPulseFromBuffer(RKBuffer pulses, uint32_t *index, const uint32_t depth) {
    RKPulse *pulse = RKGetPulseFromBuffer(pulses, *index);
    pulse->header.s = RKPulseStatusVacant;
    pulse->header.timeDouble = 0.0;
    pulse->header.time.tv_sec = 0;
    pulse->header.time.tv_usec = 0;
    pulse->header.positionIndex = (uint32_t)-1;
    pulse->header.i += depth;
    *index = RKNextModuloS(*index, depth);
    return pulse;
}

RKBuffer RKPulseBufferAllocCopyFromBuffer(RKBuffer pulses, const uint32_t start, const uint32_t count, const uint32_t depth) {
    RKPulse *pulse = RKGetPulseFromBuffer(pulses, start);
    const uint32_t gateCount = pulse->header.downSampledGateCount;
    RKLog("Allocating array of %s x 2 x %d RKComplex ...\n", RKIntegerToCommaStyleString(count), gateCount);
    RKComplex *array = (RKComplex *)malloc(count * 2 * gateCount * sizeof(RKComplex));
    RKComplex *d = array;
    RKLog("Copying data from RKBuffer ...\n");
    for (int k = 0, pulseIndex = start; k < count; k++, pulseIndex++) {
        if (pulseIndex >= depth) {
            pulseIndex -= depth;
        }
        pulse = RKGetPulseFromBuffer(pulses, pulseIndex);
        for (int c = 0; c < 2; c++) {
            RKComplex *samples = RKGetComplexDataFromPulse(pulse, c);
            memcpy(d, samples, gateCount * sizeof(RKComplex));
            d += gateCount;
        }
    }
    RKLog("Done\n");
    return (RKBuffer)array;
}

void RKPulseDuplicateSplitComplex(RKPulse *pulse) {
    for (int c = 0; c < 2; c++) {
        RKComplex *x = RKGetComplexDataFromPulse(pulse, c);
        RKIQZ z = RKGetSplitComplexDataFromPulse(pulse, c);
        for (int i = 0; i < pulse->header.gateCount; i++) {
            z.i[i] = x[i].i;
            z.q[i] = x[i].q;
        }
    }
}

#pragma mark - Ray

//
// Each slot should have a structure as follows
//
//    RayHeader          header;
//    uint8_t            idata[RKBaseProductCount][capacity];
//    float              fdata[RKBaseProductCount][capacity];
//
size_t RKRayBufferAlloc(RKBuffer *mem, const uint32_t capacity, const uint32_t count) {
    size_t alignment = RKMemoryAlignSize / sizeof(RKFloat);
    if (capacity != (capacity / alignment) * alignment) {
        RKLog("Error. Ray capacity must be a multiple of %d!", alignment);
        return 0;
    }
    RKRay *ray;
    size_t headerSize = sizeof(ray->headerBytes);
    if (headerSize != (headerSize / RKMemoryAlignSize) * RKMemoryAlignSize) {
        RKLog("Error. The framework has not been compiled with proper structure size.");
        return 0;
    }
    size_t raySize = RKRayHeaderPaddedSize + capacity * RKBaseProductCount * (sizeof(uint8_t) + sizeof(float));
    if (raySize != (raySize / alignment) * alignment) {
        RKLog("Error. The total ray size %s does not conform to SIMD alignment.", RKUIntegerToCommaStyleString(raySize));
        return 0;
    }
    size_t bytes = count * raySize;
    if (posix_memalign((void **)mem, RKMemoryAlignSize, bytes)) {
        RKLog("Error. Unable to allocate ray buffer.");
        exit(EXIT_FAILURE);
    }
    memset(*mem, 0, bytes);
    // Set the ray capacity
    int i = 0;
    void *m = *mem;
    while (i < count) {
        RKRay *ray = (RKRay *)m;
        ray->header.capacity = capacity;
        ray->header.i = -(uint64_t)count + i;
        m += raySize;
        i++;
    }
    return bytes;
}

void RKRayBufferFree(RKBuffer mem) {
    return free(mem);
}

// Get a ray from a ray buffer
RKRay *RKGetRayFromBuffer(RKBuffer buffer, const uint32_t k) {
    RKRay *ray = (RKRay *)buffer;
    size_t raySize = RKRayHeaderPaddedSize + ray->header.capacity * RKBaseProductCount * (sizeof(uint8_t) + sizeof(float));
    return (RKRay *)((void *)ray + k * raySize);
}

// Get the product data in uint8_t from a ray
uint8_t *RKGetUInt8DataFromRay(RKRay *ray, const RKProductIndex m) {
    void *d = (void *)ray->data;
    return (uint8_t *)(d + m * ray->header.capacity * sizeof(uint8_t));
}

// Get the product data in float from a ray
float *RKGetFloatDataFromRay(RKRay *ray, const RKProductIndex m) {
    void *d = (void *)ray->data;
    d += RKBaseProductCount * ray->header.capacity * sizeof(uint8_t);
    return (float *)(d + m * ray->header.capacity * sizeof(float));
}

int RKClearRayBuffer(RKBuffer buffer, const uint32_t count) {
    for (uint32_t k = 0; k < count; k++) {
        RKRay *ray = RKGetRayFromBuffer(buffer, k);
        ray->header.s = RKRayStatusVacant;
        ray->header.i = -(uint64_t)count + k;
        ray->header.gateCount = 0;
        memset(ray->data, 0, RKBaseProductCount * ray->header.capacity * (sizeof(uint8_t) + sizeof(float)));
    }
    return RKResultSuccess;
}

RKRay *RKGetVacantRayFromBuffer(RKBuffer rays, uint32_t *index, const uint32_t depth) {
    RKRay *ray = RKGetRayFromBuffer(rays, *index);
    ray->header.s = RKRayStatusVacant;
    ray->header.startTime.tv_sec = 0;
    ray->header.startTime.tv_usec = 0;
    ray->header.endTime.tv_sec = 0;
    ray->header.endTime.tv_usec = 0;
    ray->header.i += depth;
    *index = RKNextModuloS(*index, depth);
    return ray;
}

#pragma mark - File Monitor

static void *fileMonitorRunLoop(void *in) {
    RKFileMonitor *engine = (RKFileMonitor *)in;

    int s;
    struct stat fileStat;

    engine->state |= RKEngineStateWantActive;
    engine->state ^= RKEngineStateActivating;

    stat(engine->filename, &fileStat);
    time_t mtime = fileStat.st_mtime;

    RKLog("%s Started.   file = %s\n", engine->name, engine->filename);

    while (engine->state & RKEngineStateWantActive) {
        engine->state |= RKEngineStateActive;
        engine->state |= RKEngineStateSleep1;
        s = 0;
        while (s++ < 10 && engine->state & RKEngineStateWantActive) {
            if (engine->verbose > 2) {
                RKLog("%s", engine->name);
            }
            usleep(100000);
        }
        engine->state ^= RKEngineStateSleep1;
        stat(engine->filename, &fileStat);
        if (mtime != fileStat.st_mtime) {
            mtime = fileStat.st_mtime;
            RKLog("%s File '%s' modified.\n", engine->name, engine->filename);
            if (engine->callbackRoutine) {
                engine->callbackRoutine(engine);
            }
        }
    }
    engine->state &= ~RKEngineStateActive;
    return NULL;
}

RKFileMonitor *RKFileMonitorInit(const char *filename, void (*routine)(void *), void *userResource) {
    RKFileMonitor *engine = (RKFileMonitor *)malloc(sizeof(RKFileMonitor));
    if (engine == NULL) {
        RKLog("Error allocating a file monitor.\n");
        return NULL;
    }
    memset(engine, 0, sizeof(RKFileMonitor));
    sprintf(engine->name, "%s<UserFileMonitor>%s",
            rkGlobalParameters.showColor ? RKGetBackgroundColorOfIndex(RKEngineColorMisc) : "",
            rkGlobalParameters.showColor ? RKNoColor : "");
    engine->state = RKEngineStateAllocated | RKEngineStateProperlyWired | RKEngineStateActivating;
    engine->memoryUsage = sizeof(RKFileMonitor);
    strncpy(engine->filename, filename, RKMaximumPathLength);
    engine->filename[RKMaximumPathLength - 1] = '\0';
    engine->callbackRoutine = routine;
    engine->userResource = userResource;
    RKLog("%s Starting ...\n", engine->name);
    if (pthread_create(&engine->tid, NULL, fileMonitorRunLoop, engine)) {
        RKLog("%s Error creating file monitor.\n", engine->name);
        free(engine);
        return NULL;
    }
    while (!(engine->state & RKEngineStateWantActive)) {
        usleep(100000);
    }
    return engine;
}

int RKFileMonitorFree(RKFileMonitor *engine) {
    return RKSimpleEngineFree((RKSimpleEngine *)engine);
}

#pragma mark - Moment Stuff

// Convert string description from command string to a number of uint64_t type
RKStream RKStreamFromString(const char * string) {
    int j = 0;
    char *c = (char *)string;
    RKStream flag = RKStreamNull;
    while (j++ < strlen(string)) {
        switch (*c) {
            case '0':
                flag = (flag & !(RKStream)RKStreamStatusMask) | RKStreamStatusPositions;
                break;
            case '1':
                flag = (flag & !(RKStream)RKStreamStatusMask) | RKStreamStatusPulses;
                break;
            case '2':
                flag = (flag & !(RKStream)RKStreamStatusMask) | RKStreamStatusRays;
                break;
            case '3':
                flag = (flag & !(RKStream)RKStreamStatusMask) | RKStreamStatusIngest;
                break;
            case '4':
                flag = (flag & !(RKStream)RKStreamStatusMask) | RKStreamStatusEngines;
                break;
            case '5':
                flag = (flag & !(RKStream)RKStreamStatusMask) | RKStreamStatusBuffers;
                break;
            case '6':
                flag = (flag & !(RKStream)RKStreamStatusMask) | RKStreamASCIIArtZ;
                break;
            case '7':
                flag = (flag & !(RKStream)RKStreamStatusMask) | RKStreamASCIIArtHealth;
                break;
            case '8':
                flag = (flag & !(RKStream)RKStreamStatusMask) | RKStreamASCIIArtVCP;
                break;
            case 'x':
                flag |= RKStreamStatusTerminalChange;
                break;
            case '!':
                flag |= RKStreamStatusProcessorStatus;
                break;
            case 'h':
                flag |= RKStreamHealthInJSON;
                break;
            case 'z':
                flag |= RKStreamDisplayZ;
                break;
            case 'Z':
                flag |= RKStreamProductZ;
                break;
            case 'v':
                flag |= RKStreamDisplayV;
                break;
            case 'V':
                flag |= RKStreamProductV;
                break;
            case 'w':
                flag |= RKStreamDisplayW;
                break;
            case 'W':
                flag |= RKStreamProductW;
                break;
            case 'd':
                flag |= RKStreamDisplayD;
                break;
            case 'D':
                flag |= RKStreamProductD;
                break;
            case 'p':
                flag |= RKStreamDisplayP;
                break;
            case 'P':
                flag |= RKStreamProductP;
                break;
            case 'r':
                flag |= RKStreamDisplayR;
                break;
            case 'R':
                flag |= RKStreamProductR;
                break;
            case 'k':
                flag |= RKStreamDisplayK;
                break;
            case 'K':
                flag |= RKStreamProductK;
                break;
            case 's':
                flag |= RKStreamDisplaySh;
                break;
            case 'S':
                flag |= RKStreamProductSh;
                break;
            case 't':
                flag |= RKStreamDisplaySv;
                break;
            case 'T':
                flag |= RKStreamProductSv;
                break;
            case 'i':
                flag |= RKStreamDisplayIQ;
                break;
            case 'I':
                flag |= RKStreamProductIQ;
                break;
            case 'Y':
                flag |= RKStreamSweepZ;
                break;
            case 'U':
                flag |= RKStreamSweepV;
                break;
            case 'X':
                flag |= RKStreamSweepW;
                break;
            case 'C':
                flag |= RKStreamSweepD;
                break;
            case 'O':
                flag |= RKStreamSweepP;
                break;
            case 'Q':
                flag |= RKStreamSweepR;
                break;
            case 'J':
                flag |= RKStreamSweepK;
                break;
            case 'H':
                flag |= RKStreamSweepQ;
                break;
            case 'A':
                flag |= RKStreamSweepSh;
                break;
            case 'B':
                flag |= RKStreamSweepSv;
                break;
            default:
                break;
                //    abcdefghijklmnopqrstuvwxyz
                //    ABCDEFGHIJKLMNOPQRSTUVWXYZ
                //
                //    abc efg  j lmno q  tu   y
                //        EFG    LMN     T
        }
        c++;
    }
    return flag;
}

int RKStringFromStream(char *string, RKStream stream) {
    int j = 0;
    // Exclusive part from RKStreamStatusMask
    if (stream & RKStreamStatusPulses) {
        j += sprintf(string + j, "1");
    } else if (stream & RKStreamStatusRays) {
        j += sprintf(string + j, "2");
    } else if (stream & RKStreamStatusPositions) {
        j += sprintf(string + j, "3");
    } else if (stream & RKStreamStatusEngines) {
        j += sprintf(string + j, "4");
    } else if (stream & RKStreamStatusBuffers) {
        j += sprintf(string + j, "5");
    } else if (stream & RKStreamASCIIArtZ) {
        j += sprintf(string + j, "6");
    } else if (stream & RKStreamASCIIArtHealth) {
        j += sprintf(string + j, "7");
    }
    if (stream & RKStreamStatusProcessorStatus) { j += sprintf(string + j, "!"); }
    if (stream & RKStreamHealthInJSON)          { j += sprintf(string + j, "h"); }
    if (stream & RKStreamDisplayZ)              { j += sprintf(string + j, "z"); }
    if (stream & RKStreamProductZ)              { j += sprintf(string + j, "Z"); }
    if (stream & RKStreamDisplayV)              { j += sprintf(string + j, "v"); }
    if (stream & RKStreamProductV)              { j += sprintf(string + j, "V"); }
    if (stream & RKStreamDisplayW)              { j += sprintf(string + j, "w"); }
    if (stream & RKStreamProductW)              { j += sprintf(string + j, "W"); }
    if (stream & RKStreamDisplayD)              { j += sprintf(string + j, "d"); }
    if (stream & RKStreamProductD)              { j += sprintf(string + j, "D"); }
    if (stream & RKStreamDisplayP)              { j += sprintf(string + j, "p"); }
    if (stream & RKStreamProductP)              { j += sprintf(string + j, "P"); }
    if (stream & RKStreamDisplayR)              { j += sprintf(string + j, "r"); }
    if (stream & RKStreamProductR)              { j += sprintf(string + j, "R"); }
    if (stream & RKStreamDisplayK)              { j += sprintf(string + j, "k"); }
    if (stream & RKStreamProductK)              { j += sprintf(string + j, "K"); }
    if (stream & RKStreamDisplaySh)             { j += sprintf(string + j, "s"); }
    if (stream & RKStreamProductSh)             { j += sprintf(string + j, "S"); }
    if (stream & RKStreamDisplaySv)             { j += sprintf(string + j, "t"); }
    if (stream & RKStreamProductSv)             { j += sprintf(string + j, "T"); }
    if (stream & RKStreamDisplayIQ)             { j += sprintf(string + j, "i"); }
    if (stream & RKStreamProductIQ)             { j += sprintf(string + j, "I"); }
    if (stream & RKStreamSweepZ)                { j += sprintf(string + j, "Y"); }
    if (stream & RKStreamSweepV)                { j += sprintf(string + j, "U"); }
    if (stream & RKStreamSweepW)                { j += sprintf(string + j, "X"); }
    if (stream & RKStreamSweepD)                { j += sprintf(string + j, "C"); }
    if (stream & RKStreamSweepP)                { j += sprintf(string + j, "O"); }
    if (stream & RKStreamSweepR)                { j += sprintf(string + j, "Q"); }
    if (stream & RKStreamSweepK)                { j += sprintf(string + j, "J"); }
    if (stream & RKStreamSweepQ)                { j += sprintf(string + j, "H"); }
    if (stream & RKStreamSweepSh)               { j += sprintf(string + j, "A"); }
    if (stream & RKStreamSweepSv)               { j += sprintf(string + j, "B"); }
    string[j] = '\0';
    return j;
}

char *RKStringOfStream(RKStream stream) {
    static char string[RKNameLength];
    RKStringFromStream(string, stream);
    return string;
}

RKProductDesc RKGetNextProductDescriptionV5(RKProductList *list) {
    RKProductDesc desc;
    memset(&desc, 0, sizeof(RKProductDesc));
    if (list == NULL || *list == 0) {
        return desc;
    }
    RKName symbols[] = {
        "Z",
        "V",
        "W",
        "D",
        "P",
        "R",
        "K",
        "Sh",
        "Sv",
        "Q",
        "Lh",
        "Lv",
        "Rxh",
        "Rxv",
        "Pxh",
        "Pxv",
        "-"
    };
    RKName names[] = {
        "Intensity",
        "Radial_Velocity",
        "Width",
        "Differential_Reflectivity",
        "PhiDP",
        "RhoHV",
        "KDP",
        "Signal_Power_H",
        "Signal_Power_V",
        "SQI",
        "Linear_Depolarization_Ratio_H",
        "Linear_Depolarization_Ratio_V",
        "Co-cross_Correlation_Coefficient_H",
        "Co-cross_Correlation_Coefficient_V",
        "Co-cross_Differential_Phase_H",
        "Co-cross_Differential_Phase_V",
        "-"
    };
    RKName units[] = {
        "dBZ",
        "MetersPerSecond",
        "MetersPerSecond",
        "dB",
        "Radians",
        "Unitless",
        "DegreesPerMeter",
        "dBm",
        "dBm",
        "Unitless",
        "dB",
        "dB",
        "Unitless",
        "Unitless",
        "Radians",
        "Radians",
        "-"
    };
    RKName colormaps[] = {
        "Reflectivity",
        "Velocity",
        "Width",
        "Differential_Reflectivity",
        "PhiDP",
        "RhoHV",
        "KDP",
        "Power",
        "Power",
        "SQI",
        "Differential_Reflectivity",
        "Differential_Reflectivity",
        "RhoHV",
        "RhoHV",
        "PhiDP",
        "PhiDP",
        "-"
    };
    RKProductList baseMoments[] = {
        RKProductListFloatZ,
        RKProductListFloatV,
        RKProductListFloatW,
        RKProductListFloatD,
        RKProductListFloatP,
        RKProductListFloatR,
        RKProductListFloatK,
        RKProductListFloatSh,
        RKProductListFloatSv,
        RKProductListFloatQ,
        RKProductListFloatLh,
        RKProductListFloatLv,
        RKProductListFloatRXh,
        RKProductListFloatRXv,
        RKProductListFloatPXh,
        RKProductListFloatPXv,
        0xFFFF
    };
    RKProductIndex baseMomentIndices[] = {
        RKProductIndexZ,
        RKProductIndexV,
        RKProductIndexW,
        RKProductIndexD,
        RKProductIndexP,
        RKProductIndexR,
        RKProductIndexK,
        RKProductIndexSh,
        RKProductIndexSv,
        RKProductIndexQ,
        RKProductIndexLh,
        RKProductIndexLv,
        RKProductIndexRXh,
        RKProductIndexRXv,
        RKProductIndexPXh,
        RKProductIndexPXv,
        0
    };
    int k = -1;
    if (*list & RKProductListFloatZ) {
        k = 0;
    } else if (*list & RKProductListFloatV) {
        k = 1;
    } else if (*list & RKProductListFloatW) {
        k = 2;
    } else if (*list & RKProductListFloatD) {
        k = 3;
    } else if (*list & RKProductListFloatP) {
        k = 4;
    } else if (*list & RKProductListFloatR) {
        k = 5;
    } else if (*list & RKProductListFloatK) {
        k = 6;
    } else if (*list & RKProductListFloatSh) {
        k = 7;
    } else if (*list & RKProductListFloatSv) {
        k = 8;
    } else if (*list & RKProductListFloatQ) {
        k = 9;
    } else if (*list & RKProductListFloatLh) {
        k = 10;
    } else if (*list & RKProductListFloatLv) {
        k = 11;
    } else if (*list & RKProductListFloatRXh) {
        k = 12;
    } else if (*list & RKProductListFloatRXv) {
        k = 13;
    } else if (*list & RKProductListFloatPXh) {
        k = 14;
    } else if (*list & RKProductListFloatPXv) {
        k = 15;
    }
    if (k < 0) {
        RKLog("Unable to get description for k = %d\n", k);
        return desc;
    }
    snprintf(desc.name, RKNameLength, "%s", names[k]) < 0 ? abort() : (void)0;
    snprintf(desc.unit, RKNameLength, "%s", units[k]) < 0 ? abort() : (void)0;
    snprintf(desc.colormap, RKNameLength, "%s", colormaps[k]) < 0 ? abort() : (void)0;
    snprintf(desc.symbol, sizeof(desc.symbol), "%s", symbols[k]) < 0 ? abort() : (void)0;
    desc.index = baseMomentIndices[k];
    // Special treatment for RhoHV: A three count piece-wise function
    RKFloat lhma[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    if (desc.index == RKProductIndexR) {
        desc.pieceCount = 3;
        desc.w[0] = 1000.0f;
        desc.b[0] = -824.0f;
        desc.l[0] = 0.93f;
        desc.w[1] = 300.0f;
        desc.b[1] = -173.0f;
        desc.l[1] = 0.7f;
        desc.w[2] = 52.8571f;
        desc.b[2] = 0.0f;
        desc.l[2] = 0.0f;
        desc.mininimumValue = 0.0f;
        desc.maximumValue = 1.05f;
    } else {
        switch (desc.index) {
            case RKProductIndexZ:
                RKZLHMAC
                break;
            case RKProductIndexV:
                RKV2LHMAC
                break;
            case RKProductIndexW:
                RKWLHMAC
                break;
            case RKProductIndexD:
                RKDLHMAC
                break;
            case RKProductIndexP:
                RKPLHMAC
                break;
            case RKProductIndexK:
                RKKLHMAC
                break;
            case RKProductIndexLh:
                RKLLHMAC
                break;
            case RKProductIndexLv:
                RKLLHMAC
                break;
            case RKProductIndexRXh:
                RKQLHMAC
                break;
            case RKProductIndexRXv:
                RKQLHMAC
                break;
            case RKProductIndexPXh:
                RKPLHMAC
                break;
            case RKProductIndexPXv:
                RKPLHMAC
                break;
            default:
                break;
        }
        desc.pieceCount = 1;
        desc.mininimumValue = lhma[0];
        desc.maximumValue = lhma[1];
        desc.w[0] = lhma[2];
        desc.b[0] = lhma[3];
        desc.l[0] = 0.0f;
    }
    *list ^= baseMoments[k];
    return desc;
}


typedef struct rk_product_desc_table {
    RKProductIndex index;
    RKName symbol;
    RKName name;
    RKName standard;
    RKName unit;
    float cfScale;
    float cfOffset;
} RKProductDescTable;


RKProductDesc RKGetNextProductDescription(RKProductList *list) {
    RKProductDesc desc;
    memset(&desc, 0, sizeof(RKProductDesc));
    if (list == NULL || *list == 0) {
        return desc;
    }
    const RKProductDescTable table[] = {
        {RKProductIndexZ, "DBZ", "reflectivity", "equivalent_reflectivity_factor", "dBZ", 0.01f, 0.0f},
        {RKProductIndexV, "VEL", "doppler_velocity", "radial_velocity_of_scatterers_away_from_instrument", "m/s", 0.01f, 0.0f},
        {RKProductIndexW, "WIDTH", "spectrum_width", "doppler_spectrum_width", "m/s", 0.01f, 0.0f},
        {RKProductIndexD, "ZDR", "differential_reflectivity", "log_differential_reflectivity_hv", "dB", 0.01f, 0.0f},
        {RKProductIndexP, "PHIDP", "differential_phase", "differential_phase_hv", "degrees", 0.01f, 0.0f},
        {RKProductIndexR, "RHOHV", "cross_correlation_ratio", "cross_correlation_ratio_hv", "unitless", 0.001f, 0.0f},
        {RKProductIndexK, "KDP", "specific_differential_phase", "specific_differential_phase_hv", "degrees/km", 0.01f, 0.0f},
        {RKProductIndexSh, "SH", "signal_power_h", "signal_power_h", "dBm", 0.01f, 0.0f},
        {RKProductIndexSv, "SV", "signal_power_v", "signal_power_v", "dBm", 0.01f, 0.0f},
        {RKProductIndexQ, "SQI", "signal_quality_index", "signal_quality_index", "unitless", 0.01f, 0.0f},
        {RKProductIndexLh, "LDRH", "linear_depolarization_ratio_h", "linear_depolarization_ratio_h", "dB", 0.01f, 0.0f},
        {RKProductIndexLv, "LDRV", "linear_depolarization_ratio_v", "linear_depolarization_ratio_v", "dB", 0.01f, 0.0f},
        {RKProductIndexPXh, "PHIXH", "differential_phase_copolar_h_crosspolar_v", "differential_phase_copolar_h_crosspolar_v", "degrees", 0.01f, 0.0f},
        {RKProductIndexPXv, "PHIXV", "differential_phase_copolar_v_crosspolar_h", "differential_phase_copolar_v_crosspolar_h", "degrees", 0.01f, 0.0f},
        {RKProductIndexRXh, "RHOXH", "correlation_coefficient_copolar_h_crospolar_v", "correlation_coefficient_copolar_h_crospolar_v", "unitless", 0.001f, 0.0f},
        {RKProductIndexRXv, "RHOXV", "correlation_coefficient_copolar_v_crospolar_h", "correlation_coefficient_copolar_v_crospolar_h", "unitless", 0.001f, 0.0f},
        {0, "-", "-", "-", "-", 0.0f, 0.0f}
    };
    int k = -1;
    if (*list & RKProductListFloatZ) {
        *list ^= RKProductListFloatZ;
        k = 0;
    } else if (*list & RKProductListFloatV) {
        *list ^= RKProductListFloatV;
        k = 1;
    } else if (*list & RKProductListFloatW) {
        *list ^= RKProductListFloatW;
        k = 2;
    } else if (*list & RKProductListFloatD) {
        *list ^= RKProductListFloatD;
        k = 3;
    } else if (*list & RKProductListFloatP) {
        *list ^= RKProductListFloatP;
        k = 4;
    } else if (*list & RKProductListFloatR) {
        *list ^= RKProductListFloatR;
        k = 5;
    } else if (*list & RKProductListFloatK) {
        *list ^= RKProductListFloatK;
        k = 6;
    } else if (*list & RKProductListFloatSh) {
        *list ^= RKProductListFloatSh;
        k = 7;
    } else if (*list & RKProductListFloatSv) {
        *list ^= RKProductListFloatSv;
        k = 8;
    } else if (*list & RKProductListFloatQ) {
        *list ^= RKProductListFloatQ;
        k = 9;
    } else if (*list & RKProductListFloatLh) {
        *list ^= RKProductListFloatLh;
        k = 10;
    } else if (*list & RKProductListFloatLv) {
        *list ^= RKProductListFloatLv;
        k = 11;
    } else if (*list & RKProductListFloatPXh) {
        *list ^= RKProductListFloatPXh;
        k = 12;
    } else if (*list & RKProductListFloatPXv) {
        *list ^= RKProductListFloatPXv;
        k = 13;
    } else if (*list & RKProductListFloatRXh) {
        *list ^= RKProductListFloatRXh;
        k = 14;
    } else if (*list & RKProductListFloatRXv) {
        *list ^= RKProductListFloatRXv;
        k = 15;
    }
    if (k < 0) {
        RKLog("Error. Unable to get description for k = %d\n", k);
        return desc;
    }
    snprintf(desc.description, sizeof(desc.description), "%s", table[k].standard) < 0 ? abort() : (void)0;
    snprintf(desc.name, sizeof(desc.name), "%s", table[k].name) < 0 ? abort() : (void)0;
    snprintf(desc.unit, sizeof(desc.unit), "%s", table[k].unit) < 0 ? abort() : (void)0;
    snprintf(desc.symbol, sizeof(desc.symbol), "%s", table[k].symbol) < 0 ? abort() : (void)0;
    desc.index = table[k].index;
    desc.cfScale = table[k].cfScale;
    desc.cfOffset = table[k].cfOffset;
    // Special treatment for RhoHV: A three count piece-wise function
    RKFloat lhma[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    if (desc.index == RKProductIndexR) {
        desc.pieceCount = 3;
        desc.w[0] = 1000.0f;
        desc.b[0] = -824.0f;
        desc.l[0] = 0.93f;
        desc.w[1] = 300.0f;
        desc.b[1] = -173.0f;
        desc.l[1] = 0.7f;
        desc.w[2] = 52.8571f;
        desc.b[2] = 0.0f;
        desc.l[2] = 0.0f;
        desc.mininimumValue = 0.0f;
        desc.maximumValue = 1.05f;
    } else {
        switch (desc.index) {
            case RKProductIndexZ:
                RKZLHMAC
                break;
            case RKProductIndexV:
                RKV2LHMAC
                break;
            case RKProductIndexW:
                RKWLHMAC
                break;
            case RKProductIndexD:
                RKDLHMAC
                break;
            case RKProductIndexP:
                RKPLHMAC
                break;
            case RKProductIndexK:
                RKKLHMAC
                break;
            case RKProductIndexLh:
                RKLLHMAC
                break;
            case RKProductIndexLv:
                RKLLHMAC
                break;
            case RKProductIndexPXh:
                RKPLHMAC
                break;
            case RKProductIndexPXv:
                RKPLHMAC
                break;
            case RKProductIndexRXh:
                RKQLHMAC
                break;
            case RKProductIndexRXv:
                RKQLHMAC
                break;
            default:
                break;
        }
        desc.pieceCount = 1;
        desc.mininimumValue = lhma[0];
        desc.maximumValue = lhma[1];
        desc.w[0] = lhma[2];
        desc.b[0] = lhma[3];
        desc.l[0] = 0.0f;
    }
    return desc;
}

#pragma mark - JSON Stuff

size_t RKParseCommaDelimitedValues(void *valueStorage, RKValueType type, const size_t size, const char *valueString) {
    float *fv = (float *)valueStorage;
    double *dv = (double *)valueStorage;
    int32_t *i32v = (int32_t *)valueStorage;
    uint32_t *u32v = (uint32_t *)valueStorage;
    int64_t *i64v = (int64_t *)valueStorage;
    uint64_t *u64v = (uint64_t *)valueStorage;
    char *copy = (char *)malloc(strlen(valueString) + 1);
    strcpy(copy, valueString);
    char *c = copy;
    char *e;
    if ((e = strchr(copy, ',')) != NULL) {
        *e = '\0';
    }
    size_t s = 0;
    while (*c != '\0' && s < size) {
        switch (type) {
            case RKValueTypeFloat:
                fv[s] = (RKFloat)atof(c);
                break;
            case RKValueTypeDouble:
                dv[s] = atof(c);
                break;
            case RKValueTypeInt32:
                i32v[s] = (int32_t)atoi(c);
                break;
            case RKValueTypeUInt32:
                u32v[s] = (uint32_t)atoi(c);
                break;
            case RKValueTypeInt64:
                i64v[s] = (int64_t)atol(c);
                break;
            case RKValueTypeUInt64:
                u64v[s] = (uint64_t)atol(c);
                break;
            default:
                break;
        }
        //printf("s = %d   c = %s @ %16p / %16p\n", (int)s, c == NULL ? "(NULL)" : c, c, e);
        s++;
        if (e) {
            c = e + 1;
            if ((e = strchr(c, ',')) != NULL) {
                *e = '\0';
            }
        } else {
            break;
        }
    }
    free(copy);
    return s;
}

size_t RKParseNumericArray(void *valueStorage, RKValueType type, const size_t size, const char *valueString) {
    size_t k = 0;
    char *s, *e;
    if ((s = strchr(valueString, '[')) == NULL) {
        return RKParseCommaDelimitedValues(valueStorage, type, size, valueString);
    }
    if ((e = strchr(valueString, ']')) == NULL) {
        fprintf(stderr, "Expected a close bracket for an array that begins with '['.\n");
        return 0;
    }
    char *copy = (char *)malloc(e - s + 1);
    memcpy(copy, s + 1, e - s - 1);
    *(copy + (e - s - 1)) = '\0';
    //printf("copy = %s\n", copy);
    k = RKParseCommaDelimitedValues(valueStorage, type, size, copy);
    free(copy);
    return k;
}

void RKParseQuotedStrings(const char *source, ...) {
    va_list args;
    va_start(args, source);

    char *s = (char *)source, *e, q;
    char *string = va_arg(args, char *);
    size_t length = 0;

    while (string != NULL) {
        // Look for the beginning quote
        while (*s != '"' && *s != '\'' && *s != '\0') {
            s++;
        }
        if (*s == '\0') {
            break;
        }
        q = *s++;
        // Look for the ending quote
        e = s;
        while (*e != q && *e != q) {
            e++;
        }
        length = (size_t)(e - s);
        strncpy(string, s, length);
        string[length] = '\0';
        s = e + 1;
        // Get the next string
        string = va_arg(args, char *);
    }
}

void RKMakeJSONStringFromControls(char *string, RKControl *controls, uint32_t count) {
    int i, j = 0;
    RKControl *control = controls;
    for (i = 0; i < count; i++) {
        if (control->label[0] == '\0') {
            break;
        }
        j += sprintf(string + j, "{\"Label\": \"%s\", \"Command\": \"%s\"}, ", control->label, control->command);
        control++;
    }
    if (j > 2) {
        string[j - 2] = '\0';
    } else {
        string[0] = '\0';
    }
}

// RKStatusEnum zones:
//              x <= tooLow        --> RKStatusEnumCritical
// tooLow     < x <= low           --> RKStatusEnumTooLow
// low        < x <= normalLow     --> RKStatusEnumLow
// normalLow  < x <= normalHigh    --> RKStatusEnumNormal
// normalHigh < x <= high          --> RKStatusEnumHigh
// high       < x <= tooHigh       --> RKStatusEnumTooHigh
// tooHigh    < x                  --> RKStatusEnumCritical

RKStatusEnum RKValueToEnum(RKConst value, RKConst tlo, RKConst lo, RKConst nlo, RKConst nhi, RKConst hi, RKConst thi) {
    RKStatusEnum status = !isfinite(value) ? RKStatusEnumUnknown :
    (value <= tlo ? RKStatusEnumCritical :
     (value <= lo ? RKStatusEnumTooLow :
      (value <= nlo ? RKStatusEnumLow :
       (value <= nhi ? RKStatusEnumNormal :
        (value <= hi ? RKStatusEnumHigh :
         (value <= thi ? RKStatusEnumTooHigh : RKStatusEnumCritical))))));
    return status;
}

// Typical commercial electronics operates 0 to 70 C
RKStatusEnum RKStatusFromTemperatureForCE(RKConst value) {
    return RKValueToEnum(value, -20.0f, -10.0, 0.0f, 70.0f, 80.0f, 90.0f);
}

// Typical industrial electronics operates -40 to 85 C
RKStatusEnum RKStatusFromTemperatureForIE(RKConst value) {
    return RKValueToEnum(value, -60.0f, -50.0, -40.0f, 85.0f, 95.0f, 105.0f);
}

// Typical computers operates 0 to 25 C
RKStatusEnum  RKStatusFromTemperatureForComputers(RKConst value) {
    return RKValueToEnum(value, -20.0f, -10.0, 0.0f, 25.0f, 26.0f, 27.0f);
}

//
// Examine if any status is critical
// Input:
//     const char *string       - JSON description
//     const bool showEnum      - true or false to show the details
// Output:
//     char *firstKey           - (nullable) the key of first critical value
//     char *firstValue         - (nullable) the object value of the first critical key
//
bool RKAnyCritical(const char *string, const bool showEnum, char *firstKey, char *firstValue) {
    return RKFindCondition(string, RKStatusEnumCritical, showEnum, firstKey, firstValue);
}

//
// Examine if any status is (target)
// Input:
//     const char *string         - JSON description
//     const RKStatusEnum target  - The target RKStatusEnum
//     const bool showEnum        - true or false to show the details
// Output:
//     char *firstKey             - (nullable) the key of first critical value
//     char *firstValue           - (nullable) the object value of the first critical key
//
bool RKFindCondition(const char *string, const RKStatusEnum target, const bool showEnum, char _Nullable *firstKey, char _Nullable *firstValue) {
    if (string == NULL || strlen(string) == 0) {
        return false;
    }
    size_t L = strlen(string);
    if (*string != '{' || string[L - 1] != '}') {
        fprintf(stderr, "RKFindCondition() - Expects a {} pair around the string (L = %zu).\n", L);
        fprintf(stderr, "string =\n%s(EOL)\n", string);
        return false;
    }
    int v;
    char *ks;
    char *sks;
    uint8_t type;
    uint8_t subType;
    bool earlyReturn = false;

    char *str = (char *)malloc(L + 1);
    char *key = (char *)malloc(RKMaximumStringLength);
    char *obj = (char *)malloc(RKMaximumStringLength);
    char *subKey = (char *)malloc(RKMaximumStringLength);
    char *subObj = (char *)malloc(RKMaximumStringLength);
    if (str == NULL) {
        RKLog("Error allocating memory for str.\n");
        earlyReturn = true;
    }
    if (key == NULL) {
        RKLog("Error allocating memory for key.\n");
        earlyReturn = true;
    }
    if (obj == NULL) {
        RKLog("Error allocating memory for obj.\n");
        earlyReturn = true;
    }
    if (subKey == NULL) {
        RKLog("Error allocating memory for subKey.\n");
        earlyReturn = true;
    }
    if (subObj == NULL) {
        RKLog("Error allocating memory for subObj.\n");
        earlyReturn = true;
    }
    if (earlyReturn) {
        if (str) {
            free(str);
        }
        if (key) {
            free(key);
        }
        if (obj) {
            free(obj);
        }
        if (subKey) {
            free(subKey);
        }
        if (subObj) {
            free(subObj);
        }
        return false;
    }
    *key = '\0';
    *obj = '\0';
    *subKey = '\0';
    *subObj = '\0';

    strcpy(str, string);

    bool found = false;

    ks = str + 1;
    while (*ks != '\0' && *ks != '}') {
        ks = RKExtractJSON(ks, &type, key, obj);
        if (type != RKJSONObjectTypeObject) {
            continue;
        }
        sks = obj + 1;
        while (*sks != '\0' && *sks != '}') {
            sks = RKExtractJSON(sks, &subType, subKey, subObj);
            if (strncmp("Enum", subKey, 4)) {
                continue;
            }
            v = atoi(subObj);
            if (v == target && found == false) {
                if (firstKey) {
                    strcpy(firstKey, key);
                }
                if (firstValue) {
                    strcpy(firstValue, obj);
                }
                found = true;
            }
            if (showEnum) {
                fprintf(stderr, "%s --> '%s' --> %d%s%s%s\n", key, subObj, v,
                        rkGlobalParameters.showColor ? "\033[38;5;204m" : "",
                        v == RKStatusEnumCritical ? "  *" : "",
                        rkGlobalParameters.showColor ? RKNoColor : "");
            }
        }
    }

    free(subKey);
    free(subObj);
    free(str);
    free(key);
    free(obj);

    return found;
}

int RKParseProductDescription(RKProductDesc *desc, const char *inputString) {
    size_t k;
    char *keyString;

    memset(desc, 0, sizeof(RKProductDesc));

    // Product routine key is mandatory
    keyString = RKGetValueOfKey(inputString, "key");
    if (keyString) {
        desc->key = (uint32_t)atoi(keyString);
    } else {
        return RKResultIncompleteProductDescription;
    }
    // Product name is mandatory
    keyString = RKGetValueOfKey(inputString, "name");
    if (keyString) {
        strncpy(desc->name, keyString, RKNameLength - 1);
    } else {
        return RKResultIncompleteProductDescription;
    }
    // Product symbol is mandatory
    keyString = RKGetValueOfKey(inputString, "symbol");
    if (keyString) {
        strncpy(desc->symbol, keyString, 7);
    } else {
        return RKResultIncompleteProductDescription;
    }
    // Piece count can be assumed to be 1 if not supplied
    keyString = RKGetValueOfKey(inputString, "PieceCount");
    if (keyString) {
        desc->pieceCount = atoi(keyString);
        if (desc->pieceCount > 8) {
            desc->pieceCount = 8;
            fprintf(stderr, "User product piece count truncated to 8.\n");
        }
    } else {
        desc->pieceCount = 1;
    }
    // The bias term, b, for 8-bit conversion must be supplied
    keyString = RKGetValueOfKey(inputString, "b");
    if (keyString) {
        k = RKParseNumericArray(desc->b, RKValueTypeFloat, desc->pieceCount, keyString);
        if (k != desc->pieceCount) {
            fprintf(stderr, "Parsed %zu values but %u is expected (desc->b).\n", k, desc->pieceCount);
        }
    } else {
        return RKResultIncompleteProductDescription;
    }
    // The weight term, w, for 8-bit conversion must be supplied
    keyString = RKGetValueOfKey(inputString, "w");
    if (keyString) {
        k = RKParseNumericArray(desc->w, RKValueTypeFloat, desc->pieceCount, keyString);
        if (k != desc->pieceCount) {
            fprintf(stderr, "Parsed %zu values but %u is expected (desc->w).\n", k, desc->pieceCount);
        }
    } else {
        return RKResultIncompleteProductDescription;
    }
    // Optional values
    keyString = RKGetValueOfKey(inputString, "unit");
    if (keyString) {
        strncpy(desc->unit, keyString, 5);
    }
    keyString = RKGetValueOfKey(inputString, "colormap");
    if (keyString) {
        strncpy(desc->colormap, keyString, 9);
    }
    keyString = RKGetValueOfKey(inputString, "minimumValue");
    if (keyString) {
        desc->mininimumValue = (RKFloat)atof(keyString);
    }
    keyString = RKGetValueOfKey(inputString, "maximumValue");
    if (keyString) {
        desc->mininimumValue = (RKFloat)atof(keyString);
    }
    return RKResultSuccess;
}

RKProductId RKProductIdFromString(const char *string) {
    return (RKProductId)strtoul(string, NULL, 10);
}

RKIdentifier RKIdentifierFromString(const char *string) {
    return (RKIdentifier)strtouq(string, NULL, 10);
}

#pragma mark - Simple Engine Free

int RKSimpleEngineFree(RKSimpleEngine *engine) {
    if (engine->state & RKEngineStateDeactivating) {
        if (engine->verbose) {
            RKLog("%s Info. Engine is being or has been deactivated.\n", engine->name);
        }
        return RKResultEngineDeactivatedMultipleTimes;
    }
    if (!(engine->state & RKEngineStateWantActive)) {
        RKLog("%s Not active.\n", engine->name);
        return RKResultEngineDeactivatedMultipleTimes;
    }
    RKLog("%s Stopping ...\n", engine->name);
    engine->state |= RKEngineStateDeactivating;
    engine->state ^= RKEngineStateWantActive;
    if (engine->tid) {
        pthread_join(engine->tid, NULL);
    }
    engine->state ^= RKEngineStateDeactivating;
    RKLog("%s Stopped.\n", engine->name);
    if (engine->state != (RKEngineStateAllocated | RKEngineStateProperlyWired)) {
        RKLog("%s Inconsistent state 0x%04x\n", engine->name, engine->state);
    }
    free(engine);
    return RKResultSuccess;
}

#pragma mark - Command Queue

RKCommandQueue *RKCommandQueueInit(const uint16_t depth, const bool nonblocking) {
    RKCommandQueue *queue = (RKCommandQueue *)malloc(sizeof(RKCommandQueue));
    if (queue == NULL) {
        RKLog("Error. Unable to allocate command queue.\n");
        return NULL;
    }
    memset(queue, 0, sizeof(RKCommandQueue));
    queue->depth = depth;
    queue->nonblocking = nonblocking;
    queue->buffer = (RKCommand *)malloc(queue->depth * sizeof(RKCommand));
    if (queue->buffer == NULL) {
        RKLog("Error. Unable to allocate command queue.\n");
        free(queue);
        return NULL;
    }
    pthread_mutex_init(&queue->lock, NULL);
    return queue;
}

RKCommand *RKCommandQueuePop(RKCommandQueue *queue) {
    if (queue->count == 0) {
        if (queue->nonblocking) {
            return NULL;
        } else {
            do {
                usleep(1000);
            } while (queue->count == 0);
        }
    }
    RKCommand *command = &queue->buffer[queue->tail];
    queue->tail = RKNextModuloS(queue->tail, queue->depth);
    pthread_mutex_lock(&queue->lock);
    queue->count--;
    queue->toc++;
    pthread_mutex_unlock(&queue->lock);
    return command;
}

int RKCommandQueuePush(RKCommandQueue *queue, RKCommand *command) {
    memcpy(&queue->buffer[queue->head], command, sizeof(RKCommand));
    queue->head = RKNextModuloS(queue->head, queue->depth);
    pthread_mutex_lock(&queue->lock);
    queue->count++;
    pthread_mutex_unlock(&queue->lock);
    queue->tic++;
    return RKResultSuccess;
}

int RKCommandQueueFree(RKCommandQueue *queue) {
    free(queue->buffer);
    free(queue);
    return RKResultSuccess;
}

char *RKPedestalActionString(const RKScanAction *action) {
    static char string[1024];
    size_t length = 0;
    int i;
    memset(string, 0, 1024);
    for (i = 0; i < 2; i++) {
        if (RKInstructIsNone(action->mode[i])) {
            if (i == 0) {
                sprintf(string, "(none)");
            }
        } else {
            if (length) {
                length += sprintf(string + length, "   ");
            }
            length += snprintf(string + length, sizeof(string) - length, "%s", RKInstructIsAzimuth(action->mode[i]) ? "AZ" : "EL");
            length += snprintf(string + length, sizeof(string) - length, " %s",
                    RKInstructIsStandby(action->mode[i]) ? "standby" : (
                    RKInstructIsPoint(action->mode[i])   ? "point"   : (
                    RKInstructIsSlew(action->mode[i])    ? "slew"    : (
                    RKInstructIsEnable(action->mode[i])  ? "enable"  : (
                    RKInstructIsDisable(action->mode[i]) ? "disable" : ""))))
                );
            if (RKInstructIsPoint(action->mode[i]) || RKInstructIsSlew(action->mode[i])) {
                length += snprintf(string + length, sizeof(string) - length, " %.1f", action->value[i]);
            }
        }
    }
    return (char *)string;
}
