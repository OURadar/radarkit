//
//  RKFileManager.c
//  RadarKit
//
//  Created by Boonleng Cheong on 3/11/17.
//  Copyright © 2017-2021 Boonleng Cheong. All rights reserved.
//

#include <RadarKit/RKFileManager.h>

#define RKFileManagerFolderListCapacity      60
#define RKFileManagerLogFileListCapacity     1000

// The way RadarKit names the files should be relatively short:
// Folder: YYYYMMDD                              ( 6 chars)
// IQData: XXXXXX-YYYYMMDD-HHMMSS-EXXX.X.rkr     (33 chars)
// Moment: XXXXXX-YYYYMMDD-HHMMSS-EXXX.X-Z.nc    (34 chars)
// Moment: XXXXXX-YYYYMMDD-HHMMSS-EXXX.X.tar.xx  (36 chars)
// Health: XXXXXX-YYYYMMDD-HHMMSS.json           (27 chars)
// Log   : XXXXXX-YYYYMMDD.log                   (19 chars)

#define RKFileManagerFilenameLength                 (RKMaximumSymbolLength + 25 + RKMaximumFileExtensionLength)

typedef char RKPathname[RKFileManagerFilenameLength];
typedef struct _rk_indexed_stat {
    int      index;
    int      folderId;
    time_t   time;
    size_t   size;
} RKIndexedStat;

#pragma mark - Helper Functions

// Compare two unix time in RKIndexedStat
static int struct_cmp_by_time(const void *a, const void *b) {
    RKIndexedStat *x = (RKIndexedStat *)a;
    RKIndexedStat *y = (RKIndexedStat *)b;
    return (int)(x->time - y->time);
}

// Compare two filenames in the pattern of YYYYMMDD, e.g., 20170402
static int string_cmp_by_pseudo_time(const void *a, const void *b) {
    return strncmp((char *)a, (char *)b, 8);
}

static time_t timeFromFilename(const char *filename) {
    char *yyyymmdd, *hhmmss;
    yyyymmdd = strchr(filename, '-');
    if (yyyymmdd == NULL) {
        return 0;
    }
    yyyymmdd++;
    hhmmss = strchr(yyyymmdd, '-');
    if (hhmmss == NULL) {
        return 0;
    }
    hhmmss++;
    if (hhmmss - yyyymmdd > 9) {
        // Filenames that do not follow the pattern XXXXXX-YYYYMMDD-HHMMSS-EXX.XX-PPPPPPPP.EEEEEEEE
        RKLog("Warning. Unexpected file pattern.\n");
        return 0;
    }
    struct tm tm;
    strptime(yyyymmdd, "%Y%m%d-%H%M%S", &tm);
    return mktime(&tm);
}

static int listElementsInFolder(RKPathname *list, const int maximumCapacity, const char *path, uint8_t type) {
    int r, k = 0;
    struct dirent *dir;
    struct stat status;
    if (list == NULL) {
        fprintf(stderr, "list cannot be NULL.\n");
        return -1;
    }
    DIR *did = opendir(path);
    if (did == NULL) {
        if (errno != ENOENT) {
            // It is possible that the root storage folder is empty, in this case errno = ENOENT is okay.
            fprintf(stderr, "Error opening directory %s  errno = %d\n", path, errno);
        }
        return -2;
    }
    char pathname[RKMaximumPathLength];
    while ((dir = readdir(did)) != NULL && k < maximumCapacity) {
        if (dir->d_type == DT_UNKNOWN && dir->d_name[0] != '.') {
            //sprintf(pathname, "%s/%s", path, dir->d_name);
            int r = sprintf(pathname, "%s/", path);
            strncpy(pathname + r, dir->d_name, RKMaximumPathLength - r - 1);
            pathname[RKMaximumPathLength - 1] = '\0';
            lstat(pathname, &status);
            if ((type == DT_REG && S_ISREG(status.st_mode)) ||
                (type == DT_DIR && S_ISDIR(status.st_mode))) {
                r = snprintf(list[k], RKFileManagerFilenameLength, "%s", dir->d_name);
                if (r < 0) {
                    RKLog("Error. listElementsInFolder() dir->d_name = %s\n", dir->d_name);
                    continue;
                }
                k++;
            }
        } else if (dir->d_type == type && dir->d_name[0] != '.') {
            r = snprintf(list[k], RKFileManagerFilenameLength, "%s", dir->d_name);
            if (r < 0) {
                RKLog("Error. listElementsInFolder() dir->d_name = %s\n", dir->d_name);
                continue;
            }
            k++;
        }
    }
    closedir(did);
    return k;
}

static int listFoldersInFolder(RKPathname *list, const int maximumCapacity, const char *path) {
    return listElementsInFolder(list, maximumCapacity, path, DT_DIR);
}

static int listFilesInFolder(RKPathname *list, const int maximumCapacity, const char *path) {
    return listElementsInFolder(list, maximumCapacity, path, DT_REG);
}

static bool isFolderEmpty(const char *path) {
    struct dirent *dir;
    DIR *did = opendir(path);
    if (did == NULL) {
        fprintf(stderr, "Error opening directory %s\n", path);
        return false;
    }
    while ((dir = readdir(did)) != NULL) {
        if (dir->d_type == DT_REG) {
            closedir(did);
            return false;
        }
    }
    closedir(did);
    return true;
}

static void refreshFileList(RKFileRemover *me) {
    int i, j, k;
    struct stat fileStat;
    char command[RKMaximumPathLength + 8];
    char string[RKMaximumStringLength];
    char format[32];

    RKPathname *folders = (RKPathname *)me->folders;
    RKPathname *filenames = (RKPathname *)me->filenames;
    RKIndexedStat *indexedStats = (RKIndexedStat *)me->indexedStats;
    if (indexedStats == NULL) {
        RKLog("%s Not properly allocated.\n", me->name);
        return;
    }

    me->index = 0;
    me->count = 0;
    me->usage = 0;

    // Go through all folders
    int folderCount = listFoldersInFolder(folders, RKFileManagerFolderListCapacity, me->path);
    if (folderCount <= 0) {
        me->reusable = true;
        return;
    }

    // Sort the folders by name (should be in time numbers)
    qsort(folders, folderCount, sizeof(RKPathname), string_cmp_by_pseudo_time);

    if (me->parent->verbose > 2) {
        const int w = RKDigitWidth((float)folderCount, 0);
        RKLog("%s Folders (%d)   w = %d:\n", me->name, folderCount, w);
        snprintf(format, sizeof(format), ">%%s %%%dd. %%s\n", w);
        for (k = 0; k < folderCount; k++) {
            snprintf(string, sizeof(string), "%s/%s", me->path, folders[k]);
            RKLog(format, me->name, k, string);
        }
    }

    // Go through all files in the folders
    for (j = 0, k = 0; k < folderCount && me->count < me->capacity; k++) {
        snprintf(string, sizeof(string), "%s/%s", me->path, folders[k]);
        int count = listFilesInFolder(&filenames[me->count], me->capacity - me->count, string);
        if (me->parent->verbose > 1) {
            RKLog("%s %s (%s files)\n", me->name, string, RKIntegerToCommaStyleString(count));
        }
        if (count < 0) {
            RKLog("%s Error. Unable to list files in %s\n", me->name, string);
            return;
        } else if (count == 0) {
            RKLog(">%s Removing %s ...\n", me->name, string);
            i = snprintf(command, RKMaximumCommandLength, "rm -rf %s", string);
            if (i < 0) {
                RKLog("%s Error. Failed generating system command.\n", me->name);
                RKLog("%s Error. path = %s.\n", me->name, string);
                break;
            }
            i = system(command);
            if (i) {
                RKLog("Error. system(%s) -> %d   errno = %d\n", command, i, errno);
            }
        } else if (count == me->capacity - me->count) {
            RKLog("%s Info. At capacity. Suggest keeping less data on main host.\n", me->name);
        }
        me->count += count;
        if (me->count > me->capacity) {
            RKLog("%s Error. %s > %s", me->name,
                  RKVariableInString("count", &me->count, RKValueTypeInt), RKVariableInString("capacity", &me->capacity, RKValueTypeInt));
            return;
        }
        for (; j < me->count; j++) {
            sprintf(string, "%s/%s/%s", me->path, folders[k], filenames[j]);
            stat(string, &fileStat);
            indexedStats[j].index = j;
            indexedStats[j].folderId = k;
            indexedStats[j].time = timeFromFilename(filenames[j]);
            indexedStats[j].size = fileStat.st_size;
            me->usage += fileStat.st_size;
        }
    }

    // Sort the files by time
    qsort(indexedStats, me->count, sizeof(RKIndexedStat), struct_cmp_by_time);

    if (me->parent->verbose > 2) {
        RKLog("%s Files (%s):\n", me->name, RKIntegerToCommaStyleString(me->count));
        for (k = 0; k < me->count; k++) {
            RKLog(">%s %5s. %s/%s/%s  %zu %d i%d\n", me->name,
                  RKIntegerToCommaStyleString(k),
                  me->parent->dataPath, folders[indexedStats[k].folderId], filenames[indexedStats[k].index],
                  indexedStats[k].time, indexedStats[k].folderId, indexedStats[k].index);
        }
    }

    // We can operate in a circular buffer fashion if all the files are accounted
    if (me->count < me->capacity) {
        me->reusable = true;
    } else {
        me->reusable = false;
        if (folderCount == 1) {
            RKLog("%s Warning. Too many files in '%s'.\n", me->name, me->path);
            RKLog("%s Warning. Unexpected file removals may occur.\n", me->name);
        }
        // Re-calculate the usage since all slots have been used but there were still more files
        me->usage = 0;
        for (k = 0; k < folderCount; k++) {
            struct dirent *dir;
            sprintf(string, "%s/%s/%s", me->path, folders[k], filenames[j]);
            DIR *did = opendir(string);
            if (did == NULL) {
                fprintf(stderr, "Unable to list folder '%s'\n", string);
                continue;
            }
            while ((dir = readdir(did)) != NULL) {
                sprintf(string, "%s/%s/%s", me->path, folders[k], dir->d_name);
                stat(string, &fileStat);
                me->usage += fileStat.st_size;
            }
            closedir(did);
        }
        RKLog("%s Truncated list with total usage = %s B\n", me->name, RKUIntegerToCommaStyleString(me->usage));
    }
}

#pragma mark - Delegate Workers

static void *fileRemover(void *in) {
    RKFileRemover *me = (RKFileRemover *)in;
    RKFileManager *engine = me->parent;

    int k;

    const int c = me->id;

    char path[RKMaximumPathLength + 256];
    char command[RKMaximumPathLength];
    char parentFolder[RKFileManagerFilenameLength] = "";
    struct timeval time = {0, 0};

	// Initiate my name
    RKShortName name;
    if (rkGlobalParameters.showColor) {
        pthread_mutex_lock(&engine->mutex);
        k = snprintf(name, RKShortNameLength, "%s", rkGlobalParameters.showColor ? RKGetColor() : "");
        pthread_mutex_unlock(&engine->mutex);
    } else {
        k = 0;
    }
    if (engine->workerCount > 9) {
        k += sprintf(name + k, "F%02d", c);
    } else {
        k += sprintf(name + k, "F%d", c);
    }
    if (rkGlobalParameters.showColor) {
        sprintf(name + k, RKNoColor);
    }
    snprintf(me->name, RKChildNameLength, "%s %s", engine->name, name);

    size_t bytes;
    size_t mem = 0;

    bytes = RKFileManagerFolderListCapacity * sizeof(RKPathname);
    RKPathname *folders = (RKPathname *)malloc(bytes);
    if (folders == NULL) {
        RKLog("%s Error. Unable to allocate space for folder list.\n", me->name);
        return (void *)-1;
    }
    memset(folders, 0, bytes);
    mem += bytes;

    bytes = me->capacity * sizeof(RKPathname);
    RKPathname *filenames = (RKPathname *)malloc(bytes);
    if (filenames == NULL) {
        RKLog("%s Error. Unable to allocate space for file list.\n", me->name);
        free(folders);
        return (void *)-2;
    }
    memset(filenames, 0, bytes);
    mem += bytes;

    bytes = me->capacity * sizeof(RKIndexedStat);
    RKIndexedStat *indexedStats = (RKIndexedStat *)malloc(bytes);
    if (indexedStats == NULL) {
        RKLog("%s Error. Unable to allocate space for indexed stats.\n", me->name);
        free(folders);
        free(filenames);
        return (void *)-2;
    }
    memset(indexedStats, 0, bytes);
    mem += bytes;

    me->folders = folders;
    me->filenames = filenames;
    me->indexedStats = indexedStats;

    pthread_mutex_lock(&engine->mutex);
    engine->memoryUsage += mem;

    // Gather the initial file list in the folders
    refreshFileList(me);

    // Use the first folder as the initial value of parentFolder
    strcpy(parentFolder, folders[0]);

    RKLog(">%s Started.   mem = %s B   capacity = %s\n",
          me->name, RKUIntegerToCommaStyleString(mem), RKUIntegerToCommaStyleString(me->capacity));
    RKLog(">%s Path = %s\n", me->name, me->path);
    if (me->limit > 10 * 1024 * 1024) {
        RKLog(">%s Listed.  count = %s   usage = %s / %s MB (%.2f %%)\n", me->name,
              RKIntegerToCommaStyleString(me->count),
              RKUIntegerToCommaStyleString(me->usage / 1024 / 1024),
              RKUIntegerToCommaStyleString(me->limit / 1024 / 1024),
              100.0f * me->usage / me->limit);
    } else if (me->limit > 10 * 1024) {
        RKLog(">%s Listed.  count = %s   usage = %s / %s KB (%.2f %%)\n", me->name,
              RKIntegerToCommaStyleString(me->count),
              RKUIntegerToCommaStyleString(me->usage / 1024),
              RKUIntegerToCommaStyleString(me->limit / 1024),
              100.0f * me->usage / me->limit);
    } else {
        RKLog(">%s Listed.  count = %s   usage = %s / %s B (%.2f %%)\n", me->name,
              RKIntegerToCommaStyleString(me->count),
              RKUIntegerToCommaStyleString(me->usage),
              RKUIntegerToCommaStyleString(me->limit),
              100.0f * me->usage / me->limit);
    }

    me->tic++;

    pthread_mutex_unlock(&engine->mutex);

    engine->state |= RKEngineStateActive;

    me->index = 0;
    while (engine->state & RKEngineStateWantActive) {

        pthread_mutex_lock(&engine->mutex);

        if (engine->verbose > 2) {
            RKLog("%s Usage -> %s B / %s B\n", me->name, RKUIntegerToCommaStyleString(me->usage), RKUIntegerToCommaStyleString(me->limit));
        }

        // Removing files
        while (me->usage > me->limit) {
            // Build the complete path from various components
            sprintf(path, "%s/%s/%s", me->path, folders[indexedStats[me->index].folderId], filenames[indexedStats[me->index].index]);
            if (engine->verbose) {
				if (indexedStats[me->index].size > 1.0e9) {
					RKLog("%s Removing %s (%s GB) ...\n", me->name, path, RKFloatToCommaStyleString(1.0e-9f * (float)indexedStats[me->index].size));
				} else if (indexedStats[me->index].size > 1.0e6) {
					RKLog("%s Removing %s (%s MB) ...\n", me->name, path, RKFloatToCommaStyleString(1.0e-6f * (float)indexedStats[me->index].size));
				} else if (indexedStats[me->index].size > 1.0e3) {
					RKLog("%s Removing %s (%s KB) ...\n", me->name, path, RKFloatToCommaStyleString(1.0e-3f * (float)indexedStats[me->index].size));
				} else {
					RKLog("%s Removing %s (%s B) ...\n", me->name, path, RKUIntegerToCommaStyleString(indexedStats[me->index].size));
				}
            }
            if (!strlen(parentFolder)) {
                strcpy(parentFolder, folders[indexedStats[me->index].folderId]);
                RKLog("%s Set parentFolder to %s\n", me->name, parentFolder);
            }
            remove(path);
            me->usage -= indexedStats[me->index].size;
			if (engine->verbose > 1) {
				RKLog("%s Usage -> %s B / %s B\n", me->name, RKUIntegerToCommaStyleString(me->usage), RKUIntegerToCommaStyleString(me->limit));
			}

            // Get the parent folder, if it is different than before, check if it is empty, and remove it if so.
            if (strcmp(parentFolder, folders[indexedStats[me->index].folderId])) {
                sprintf(path, "%s/%s", me->path, parentFolder);
                if (isFolderEmpty(path)) {
                    RKLog("%s Removing folder %s that is empty ...\n", me->name, path);
                    k = snprintf(command, RKMaximumCommandLength, "rm -rf %s", path);
                    if (k < 0) {
                        RKLog("%s Error. Failed generating system command.\n", me->name);
                        RKLog("%s Error. path = %s.\n", me->name, path);
                        break;
                    }
                    k = system(command);
                    if (k) {
                        RKLog("Error. system(%s) -> %d   errno = %d\n", command, k, errno);
                    }
                }
                RKLog("%s New parentFolder = %s -> %s\n", me->name, parentFolder, folders[indexedStats[me->index].folderId]);
                strcpy(parentFolder, folders[indexedStats[me->index].folderId]);
            }

            // Update the index for next check or refresh it entirely if there are too many files
            me->index++;
            if (me->index == me->capacity) {
                me->index = 0;
                if (me->reusable) {
                    if (engine->verbose) {
                        RKLog("%s Reusable file list rotated to 0.\n", me->name);
                    }
                } else {
                    if (engine->verbose) {
                        RKLog("%s Refreshing file list ...\n", me->name);
                    }
                    refreshFileList(me);
                }
            }
        }

        pthread_mutex_unlock(&engine->mutex);

        // Now we wait
        k = 0;
        while ((k++ < 10 || RKTimevalDiff(time, me->latestTime) < 0.2) && engine->state & RKEngineStateWantActive) {
            gettimeofday(&time, NULL);
            usleep(100000);
        }
    }

    free(filenames);
    free(indexedStats);

    RKLog(">%s Stopped.\n", me->name);

    engine->state ^= RKEngineStateActive;
    return NULL;
}

static void *folderWatcher(void *in) {
    RKFileManager *engine = (RKFileManager *)in;

    int k;
    DIR *did;
    struct dirent *dir;
    struct stat fileStat;

    engine->state |= RKEngineStateWantActive;
    engine->state ^= RKEngineStateActivating;

    // Three major data folders that have structure A
    const char folders[][32] = {
        RKDataFolderIQ,
        RKDataFolderMoment,
        RKDataFolderHealth,
        ""
    };

    #if defined(DEBUG_FILE_MANAGER)

    const int capacities[] = {
        100,
        100,
        100,
        0
    };
    const size_t limits[] = {
        1,
        1,
        1,
        0
    };
    const size_t userLimits[] = {
        0,
        0,
        0,
        0
    };

    #else

    const int capacities[] = {
        24 * 3600 / 2 * 20,                // Assume a file every 2 seconds, 20 folders
        24 * 3600 * 365,                   // Assume a file every second, 365 folders
        24 * 60 * 365,                     // Assume a file every minute, 365 folders
        0
    };
    const size_t limits[] = {
        RKFileManagerRawDataRatio,
        RKFileManagerMomentDataRatio,
        RKFileManagerHealthDataRatio,
        0
    };
    const size_t userLimits[] = {
        engine->userRawDataUsageLimit,
        engine->userMomentDataUsageLimit,
        engine->userHealthDataUsageLimit,
        0
    };

    #endif

    const size_t sumOfLimits = limits[0] + limits[1] + limits[2];

    engine->workerCount = 3;

    engine->workers = (RKFileRemover *)malloc(engine->workerCount * sizeof(RKFileRemover));
    if (engine->workers == NULL) {
        RKLog(">%s Error. Unable to allocate an RKFileRemover.\n", engine->name);
        return (void *)RKResultFailedToCreateFileRemover;
    }
    memset(engine->workers, 0, engine->workerCount * sizeof(RKFileRemover));
    engine->memoryUsage += engine->workerCount * sizeof(RKFileRemover);

    for (k = 0; k < engine->workerCount; k++) {
        RKFileRemover *worker = &engine->workers[k];
        const char *folder = folders[k];

        worker->id = k;
        worker->parent = engine;
        worker->capacity = capacities[k];
        if (engine->radarDescription != NULL && strlen(engine->radarDescription->dataPath)) {
            snprintf(worker->path, sizeof(worker->path), "%s/%s", engine->radarDescription->dataPath, folder);
        } else if (strlen(engine->dataPath)) {
            snprintf(worker->path, sizeof(worker->path), "%s/%s", engine->dataPath, folder);
        } else {
            snprintf(worker->path, sizeof(worker->path), "%s", folder);
        }

        if (userLimits[k]) {
            worker->limit = userLimits[k];
        } else {
            worker->limit = engine->usagelimit * limits[k] / sumOfLimits;
        }

        // Workers that actually remove the files (and folders)
        if (pthread_create(&worker->tid, NULL, fileRemover, worker) != 0) {
            RKLog(">%s Error. Failed to start a file remover.\n", engine->name);
            return (void *)RKResultFailedToStartFileRemover;
        }

        while (worker->tic == 0 && engine->state & RKEngineStateWantActive) {
            usleep(10000);
        }
    }

    // Log path has structure B
    char string[RKMaximumPathLength + 16];
    char logPath[RKMaximumPathLength + 16] = RKLogFolder;
    if (engine->radarDescription != NULL && strlen(engine->radarDescription->dataPath)) {
        snprintf(logPath, sizeof(logPath), "%s/" RKLogFolder, engine->radarDescription->dataPath);
    } else if (strlen(engine->dataPath)) {
        snprintf(logPath, sizeof(logPath), "%s/" RKLogFolder, engine->dataPath);
    }

    RKLog("%s Started.   mem = %s B  state = %x\n", engine->name, RKUIntegerToCommaStyleString(engine->memoryUsage), engine->state);

	// Increase the tic once to indicate the engine is ready
	engine->tic = 1;

    // Wait here while the engine should stay active
    time_t now;
    time_t longTime = engine->maximumLogAgeInDays * 86400;
    while (engine->state & RKEngineStateWantActive) {
        // Take care of super slow changing files, like the daily logs
        time(&now);
        if ((did = opendir(logPath)) != NULL) {
            while ((dir = readdir(did)) != NULL) {
                if (dir->d_name[0] == '.') {
                    continue;
                }
                k = snprintf(string, sizeof(string), "%s/%s", logPath, dir->d_name);
                if (k < 0) {
                    RKLog("%s Error. dir->d_name = %s.\n", engine->name, dir->d_name);
                    continue;
                }
                string[sizeof(string) - 1] = '\0';
                lstat(string, &fileStat);
                if (!S_ISREG(fileStat.st_mode)) {
                    continue;
                }
                if (fileStat.st_ctime < now - longTime) {
                    RKLog("%s Removing %s ...\n", engine->name, string);
                    remove(string);
                }
            }
            closedir(did);
        }
        // Wait one minute, do it with multiples of 0.1s for a responsive exit
        k = 0;
        while (k++ < 600 && engine->state & RKEngineStateWantActive) {
            usleep(100000);
        }
    }

    for (k = 0; k < engine->workerCount; k++) {
        pthread_join(engine->workers[k].tid, NULL);
    }
    free(engine->workers);

    return NULL;
}

#pragma mark - Life Cycle

RKFileManager *RKFileManagerInit(void) {
    RKFileManager *engine = (RKFileManager *)malloc(sizeof(RKFileManager));
    if (engine == NULL) {
        RKLog("Error. Unable to allocate a file manager.\n");
        return NULL;
    }
    memset(engine, 0, sizeof(RKFileManager));
    sprintf(engine->name, "%s<DataFileManager>%s",
            rkGlobalParameters.showColor ? RKGetBackgroundColorOfIndex(RKEngineColorFileManager) : "",
            rkGlobalParameters.showColor ? RKNoColor : "");
    engine->state = RKEngineStateAllocated;
    engine->maximumLogAgeInDays = RKFileManagerDefaultLogAgeInDays;
    engine->memoryUsage = sizeof(RKFileManager);
    pthread_mutex_init(&engine->mutex, NULL);
    return engine;
}

void RKFileManagerFree(RKFileManager *engine) {
    if (engine->state & RKEngineStateWantActive) {
        RKFileManagerStop(engine);
    }
    pthread_mutex_destroy(&engine->mutex);
    free(engine);
}

#pragma mark - Properties

void RKFileManagerSetVerbose(RKFileManager *engine, const int verbose) {
    engine->verbose = verbose;
}

void RKFileManagerSetEssentials(RKFileManager *engine, RKRadarDesc *desc) {
    engine->radarDescription = desc;
}

void RKFileManagerSetPathToMonitor(RKFileManager *engine, const char *path) {
    strncpy(engine->dataPath, path, RKMaximumFolderPathLength - 1);
}

void RKFileManagerSetDiskUsageLimit(RKFileManager *engine, const size_t limit) {
    if (engine->state & RKEngineStateActive) {
        RKLog("%s Data limit can only be set before it is started.\n", engine->name);
        return;
    }
    engine->usagelimit = limit;
}

void RKFileManagerSetMaximumLogAgeInDays(RKFileManager *engine, const int age) {
    if (engine->state & RKEngineStateActive) {
        RKLog("%s Data limit can only be set before it is started.\n", engine->name);
        return;
    }
    engine->maximumLogAgeInDays = age;
}

void RKFileManagerSetRawDataLimit(RKFileManager *engine, const size_t limit) {
    if (engine->state & RKEngineStateActive) {
        RKLog("%s Data limit can only be set before it is started.\n", engine->name);
        return;
    }
    engine->userRawDataUsageLimit = limit;
}

void RKFileManagerSetMomentDataLimit(RKFileManager *engine, const size_t limit) {
    if (engine->state & RKEngineStateActive) {
        RKLog("%s Data limit can only be set before it is started.\n", engine->name);
        return;
    }
    engine->userMomentDataUsageLimit = limit;
}

void RKFileManagerSetHealthDataLimit(RKFileManager *engine, const size_t limit) {
    if (engine->state & RKEngineStateActive) {
        RKLog("%s Data limit can only be set before it is started.\n", engine->name);
        return;
    }
    engine->userHealthDataUsageLimit = limit;
}

#pragma mark - Interactions

int RKFileManagerStart(RKFileManager *engine) {
    // File manager is always assumed wired, dataPath may be empty
    engine->state |= RKEngineStateProperlyWired;
    if (engine->userRawDataUsageLimit == 0 && engine->userMomentDataUsageLimit == 0 && engine->userHealthDataUsageLimit == 0 &&
        engine->usagelimit == 0) {
        engine->usagelimit = RKFileManagerDefaultUsageLimit;
        RKLog("%s Usage limit not set, use default %s%s B%s (%s %s)\n",
              engine->name,
              rkGlobalParameters.showColor ? "\033[4m" : "",
              RKUIntegerToCommaStyleString(engine->usagelimit),
              rkGlobalParameters.showColor ? "\033[24m" : "",
              RKFloatToCommaStyleString((double)engine->usagelimit / (engine->usagelimit > 1073741824 ? 1099511627776 : 1073741824)),
              engine->usagelimit > 1073741824 ? "TiB" : "GiB");
    }
    RKLog("%s Starting ...\n", engine->name);
    engine->tic = 0;
    engine->state |= RKEngineStateActivating;
    if (pthread_create(&engine->tidFileWatcher, NULL, folderWatcher, engine) != 0) {
        RKLog("Error. Failed to start a ray gatherer.\n");
        return RKResultFailedToStartFileManager;
    }
	//RKLog("tidFileWatcher = %lu\n", (unsigned long)engine->tidFileWatcher);
	while (engine->tic == 0) {
        usleep(10000);
    }
    return RKResultSuccess;
}

int RKFileManagerStop(RKFileManager *engine) {
    if (engine->state & RKEngineStateDeactivating) {
        if (engine->verbose) {
            RKLog("%s Info. Engine is being or has been deactivated.\n", engine->name);
        }
        return RKResultEngineDeactivatedMultipleTimes;
    }
    pthread_mutex_lock(&engine->mutex);
    RKLog("%s Stopping ...\n", engine->name);
    engine->state |= RKEngineStateDeactivating;
    engine->state ^= RKEngineStateWantActive;
    pthread_join(engine->tidFileWatcher, NULL);
    engine->state ^= RKEngineStateDeactivating;
    RKLog("%s Stopped.\n", engine->name);
    if (engine->state != (RKEngineStateAllocated | RKEngineStateProperlyWired)) {
        RKLog("%s Inconsistent state 0x%04x\n", engine->name, engine->state);
    }
    pthread_mutex_unlock(&engine->mutex);
    return RKResultSuccess;
}

int RKFileManagerAddFile(RKFileManager *engine, const char *filename, RKFileType type) {
    RKFileRemover *me = &engine->workers[type];

    if (!(engine->state & RKEngineStateWantActive)) {
        return RKResultEngineNotActive;
    }

    pthread_mutex_lock(&engine->mutex);

    struct stat fileStat;
    stat(filename, &fileStat);

    gettimeofday(&me->latestTime, NULL);

    // For non-reusable type, just add the size
    if (me->reusable == false) {
        me->usage += fileStat.st_size;
        pthread_mutex_unlock(&engine->mutex);
        return RKResultFileManagerBufferNotResuable;
    }

    RKPathname *folders = (RKPathname *)me->folders;
    RKPathname *filenames = (RKPathname *)me->filenames;
    RKIndexedStat *indexedStats = (RKIndexedStat *)me->indexedStats;

    // Copy over the filename and stats to the internal buffer
    uint32_t k = me->count;

    // Extract out the date portion
    char *lastPart = strrchr(filename, '/');
    if (lastPart) {
        if (strlen(lastPart) > RKFileManagerFilenameLength - 1) {
            RKLog("%s Error. I could crash here.\n", me->name);
        }
        if ((void *)&filenames[k] > ((void *)filenames) + me->capacity * sizeof(RKPathname) - RKFileManagerFilenameLength) {
            RKLog("%s Error. I could crash here.   %p vs %p   k = %d / %d\n", me->name,
                  (void *)&filenames[k], ((void *)filenames) + me->capacity * sizeof(RKPathname) - RKFileManagerFilenameLength,
                  k, me->capacity);
        }
        strncpy(filenames[k], lastPart + 1, RKFileManagerFilenameLength - 1);
    } else {
        if (strlen(filename) > RKFileManagerFilenameLength - 1) {
            RKLog("%s %s Warning. Filename is too long.\n", engine->name,  me->name);
        }
        strncpy(filenames[k], filename, RKFileManagerFilenameLength - 1);
    }

    if (strncmp(me->path, filename, strlen(me->path))) {
        RKLog("%s File %s does not belong here (%s).\n", engine->name, filename, me->path);
        return RKResultFileManagerInconsistentFolder;
    }

    // [me->path]/YYYYMMDD/RK-YYYYMMDD-...

    int folderId = 0;
    char *folder = engine->scratch;
    strcpy(folder, filename + strlen(me->path) + 1);
    char *e = strrchr(folder, '/');
    *e = '\0';
    if (strlen(folder) > RKFileManagerFilenameLength - 1) {
        RKLog("%s Warning. Folder name is too long.\n", engine->name);
    }
    if (k == 0) {
        // The data folder is empty
        folderId = 0;
        strcpy(folders[0], folder);
    } else {
        folderId = indexedStats[k - 1].folderId;
        // If same folder as the previous entry, no need to add another folder in the list. Add one otherwise.
        if (strcmp(folder, folders[indexedStats[k - 1].folderId])) {
            folderId++;
            strcpy(folders[indexedStats[k - 1].folderId + 1], folder);
        }
    }
    //printf("%s --> %s / %s (%d)\n", filename, folder, filenames[k], folderId);

    indexedStats[k].index = k;
    indexedStats[k].folderId = folderId;
    indexedStats[k].time = timeFromFilename(filenames[k]);
    indexedStats[k].size = fileStat.st_size;

    me->usage += fileStat.st_size;

    if (engine->verbose > 2) {
        RKLog("%s Added '%s'   %s B   k = %d\n", me->name, filename, RKUIntegerToCommaStyleString(indexedStats[k].size), k);
    }

    me->count = RKNextModuloS(me->count, me->capacity);

    pthread_mutex_unlock(&engine->mutex);

    //for (k = me->count - 3; k < me->count; k++) {
    //    printf("%3d -> %3d. %s  %s\n", k, indexedStats[k].index, filenames[indexedStats[k].index], RKUIntegerToCommaStyleString(indexedStats[k].size));
    //}
    return RKResultSuccess;
}
