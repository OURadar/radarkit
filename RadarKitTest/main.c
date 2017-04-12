//
//  main.c
//  RadarKitTest
//
//  Created by Boon Leng Cheong
//  Copyright (c) 2016 Boon Leng Cheong. All rights reserved.
//

#include <RadarKit.h>
#include <getopt.h>

#define CLEAR                       "\033[0m"
#define UNDERLINE(x)                "\033[4m" x "\033[24m"
#define ROOT_PATH                   "data"


// User parameters in a struct
typedef struct user_params {
    int   coresForPulseCompression;
    int   coresForProductGenerator;
    int   prf;
    int   gateCount;
    int   verbose;
    int   testPulseCompression;
    int   sleepInterval;
    bool  simulate;
    bool  writeFiles;
    bool  relay;
    char  pedzyHost[256];
    char  tweetaHost[256];
} UserParams;

// Global variables
RKRadar *myRadar = NULL;

// Functions
void *exitAfterAWhile(void *s) {
    sleep(3);
    RKLog("Forced exit.\n");
    exit(EXIT_SUCCESS);
}

static void handleSignals(int signal) {
    if (myRadar == NULL) {
        return;
    }
    fprintf(stderr, "\n");
    RKLog("Caught a %s (%d)  radar->state = 0x%x\n", RKSignalString(signal), signal, myRadar->state);
    RKStop(myRadar);
    pthread_t t;
    pthread_create(&t, NULL, exitAfterAWhile, NULL);
}

void showHelp() {
    printf("RadarKit Test Program\n\n"
           "rktest [options]\n\n"
           "OPTIONS:\n"
           "     Unless specifically stated, all options are interpreted in sequence. Some\n"
           "     options can be specified multiples times for repetitions. For example, the\n"
           "     debris particle count is set for each type sequentially by repeating the\n"
           "     option multiple times for each debris type.\n"
           "\n"
           "  -c (--core) " UNDERLINE("P,M") " (no space after comma)\n"
           "         Sets the number of threads for pulse compression to " UNDERLINE("P") "\n"
           "         and the number of threads for product generator to " UNDERLINE("M") ".\n"
           "         If not specified, the default core counts are 8 / 4.\n"
           "\n"
           "  -f (--prf) " UNDERLINE("value") "\n"
           "         Sets the pulse repetition frequency (PRF) to " UNDERLINE("value") " in Hz.\n"
           "         If not specified, the default PRF is 5000 Hz.\n"
           "\n"
           "  -g (--gate) " UNDERLINE("value") "\n"
           "         Sets the number range gates to " UNDERLINE("value") ".\n"
           "         If not specified, the default PRF is 8192 Hz.\n"
           "\n"
           "  -h (--help)\n"
           "         Shows this help text.\n"
           "\n"
           "  -L (--test-lean-system)\n"
           "         Run with arguments '-v -f 2000 -F 5e6 -c 2,2'.\n"
           "\n"
           "  -M (--test-medium-system)\n"
           "         Run with arguments '-v -f 5000 -F 20e6 -c 4,2'.\n"
           "\n"
           "  -p (--pedzy-host)\n"
           "         Sets the host of pedzy pedestal controller.\n"
           "\n"
           "  -s (--simulate)\n"
           "         Sets the program to simulate data stream (default, if none of the tests\n"
           "         is specified).\n"
           "\n"
           "  -v (--verbose)\n"
           "         Increases verbosity level, which can be specified multiple times.\n"
           "\n"
           "  --test-simd\n"
           "         Sets the program to test SIMD instructions.\n"
           "         To test the SIMD performance, use --test-simd=2\n"
           "\n"
           "  --test-one-ray\n"
           "         Sets the program to test processing one ray using a selected processor.\n"
           "\n"
           "  --test-pulse-compression\n"
           "         Sets the program to test the pulse compression using a simple case with.\n"
           "         an impulse filter.\n"
           "  -T (--test) " UNDERLINE("value") "\n"
           "         0 - Show types\n"
           "         1 - Show colors\n"
           "         2 - Show modulo math\n"
           "         3 - Show window types\n"
           "         4 - Test parsing comma delimited values\n"
           "         5 - Test parsing values in a JSON string\n"
           "         6 - Test reading a preference file\n"
           "\n"
           "\n"
           "EXAMPLES:\n"
           "     Here are some examples of typical configurations.\n"
           "\n"
           "  radar\n"
           "         Runs the program with default settings.\n"
           "\n"
           "  radar -f 2000\n"
           "         Runs the program with PRF = 2000 Hz.\n"
           "\n"
           );
}

UserParams processInput(int argc, const char **argv) {
    int k;
    
    // A structure unit that encapsulates command line user parameters
    UserParams user;
    memset(&user, 0, sizeof(UserParams));
    user.gateCount = 2000;
    user.coresForPulseCompression = 2;
    user.coresForProductGenerator = 2;
    user.prf = 2000;

    static struct option long_options[] = {
        {"alarm"                 , no_argument      , NULL, 'A'}, // ASCII 65 - 90 : A - Z
        {"no-color"              , no_argument      , NULL, 'C'},
        {"debug-demo"            , no_argument      , NULL, 'D'},
        {"fs"                    , required_argument, NULL, 'F'},
        {"hp-system"             , no_argument      , NULL, 'H'},
        {"lean-system"           , no_argument      , NULL, 'L'},
        {"medium-system"         , no_argument      , NULL, 'M'},
        {"test-pulse-compression", optional_argument, NULL, 'P'},
        {"test-one-ray"          , no_argument      , NULL, 'R'},
        {"test-processor"        , no_argument      , NULL, 'Q'},
        {"test-simd"             , optional_argument, NULL, 'S'},
        {"test"                  , required_argument, NULL, 'T'},
        {"azimuth"               , required_argument, NULL, 'a'}, // ASCII 97 - 122 : a - z
        {"bandwidth"             , required_argument, NULL, 'b'},
        {"core"                  , required_argument, NULL, 'c'},
        {"prf"                   , required_argument, NULL, 'f'},
        {"gate"                  , required_argument, NULL, 'g'},
        {"help"                  , no_argument      , NULL, 'h'},
        {"pedzy-host"            , required_argument, NULL, 'p'},
        {"quiet"                 , no_argument      , NULL, 'q'},
        {"relay"                 , no_argument      , NULL, 'r'},
        {"sim"                   , no_argument      , NULL, 's'},
        {"tweeta-host"           , required_argument, NULL, 't'},
        {"verbose"               , no_argument      , NULL, 'v'},
        {"do-not-write"          , no_argument      , NULL, 'w'},
        {"test-write-speed"      , no_argument      , NULL, 'y'},
        {"simulate-sleep"        , required_argument, NULL, 'z'},
        {0, 0, 0, 0}
    };
    
    // Construct short_options from long_options
    char str[1024] = "";
    for (k = 0; k < sizeof(long_options) / sizeof(struct option); k++) {
        struct option *o = &long_options[k];
        snprintf(str + strlen(str), 1024, "%c%s", o->val, o->has_arg == required_argument ? ":" : (o->has_arg == optional_argument ? "::" : ""));
    }
    //printf("str = %s\n", str);
    // Process the input arguments and set the parameters
    int opt, long_index = 0;
    while ((opt = getopt_long(argc, (char * const *)argv, str, long_options, &long_index)) != -1) {
        switch (opt) {
            case 'C':
                RKSetWantColor(false);
                break;
            case 'D':
                user.simulate = true;
                user.gateCount = 2000;
                user.prf = 6;
                user.coresForPulseCompression = 2;
                user.coresForProductGenerator = 2;
                break;
            case 'b':
            case 'H':
                user.simulate = true;
                user.gateCount = 60000;
                user.prf = 5000;
                user.coresForPulseCompression = 10;
                user.coresForProductGenerator = 4;
                break;
            case 'L':
                user.simulate = true;
                user.gateCount = 2000;
                user.prf = 2000;
                user.coresForPulseCompression = 2;
                user.coresForProductGenerator = 2;
                break;
            case 'M':
                user.simulate = true;
                user.gateCount = 16000;
                user.prf = 5000;
                user.coresForPulseCompression = 6;
                user.coresForProductGenerator = 4;
                break;
            case 'R':
                RKTestOneRay();
                exit(EXIT_SUCCESS);
                break;
            case 'S':
                // SIMD Tests
                k = RKTestSIMDFlagNull;
                if (optarg) {
                    if (atoi(optarg) > 1) {
                        k |= RKTestSIMDFlagPerformanceTestAll;
                    } else {
                        user.verbose = MAX(1, user.verbose);
                    }
                }
                if (user.verbose) {
                    k |= RKTestSIMDFlagShowNumbers;
                }
                RKTestSIMD(k);
                exit(EXIT_SUCCESS);
                break;
            case 'Q':
                RKTestProcessorSpeed();
                exit(EXIT_SUCCESS);
                break;
            case 'T':
                k = atoi(optarg);
                switch (k) {
                    case 0:
                        RKShowTypeSizes();
                        break;
                    case 1:
                        RKTestShowColors();
                        break;
                    case 2:
                        RKTestModuloMath();
                        break;
                    case 3:
                        RKTestWindow();
                        break;
                    case 4:
                        RKTestParseCommaDelimitedValues();
                        break;
                    case 5:
                        RKTestJSON();
                        break;
                    case 6:
                        RKTestMakeHops();
                        break;
                    case 7:
                        RKTestSingleEngine();
                        break;
                    case 8:
                        RKTestPreferenceReading();
                        break;
                    case 9:
                        RKTestCountFiles();
                        break;
                    default:
                    break;
                }
                exit(EXIT_SUCCESS);
                break;
            case 'U':
                exit(EXIT_FAILURE);
                break;
            case 'P':
                if (optarg) {
                    user.testPulseCompression = atoi(optarg);
                } else {
                    user.testPulseCompression = 1;
                }
                break;
            case 'c':
                sscanf(optarg, "%d,%d", &user.coresForPulseCompression, &user.coresForProductGenerator);
                break;
            case 'f':
                user.prf = (int)atof(optarg);
                break;
            case 'g':
                user.gateCount = (int)atof(optarg);
                break;
            case 'h':
                showHelp();
                exit(EXIT_SUCCESS);
                break;
            case 'p':
                strncpy(user.pedzyHost, optarg, sizeof(user.pedzyHost));
                break;
            case 'q':
                user.verbose = MAX(user.verbose - 1, 0);
                break;
            case 'r':
                user.relay = true;
                break;
            case 's':
                user.simulate = true;
                break;
            case 't':
                strncpy(user.tweetaHost, optarg, sizeof(user.tweetaHost));
                break;
            case 'v':
                user.verbose++;
                break;
            case 'w':
                user.writeFiles = true;
                break;
            case 'y':
                RKTestCacheWrite();
                exit(EXIT_SUCCESS);
                break;
            case 'z':
                if (optarg) {
                    user.sleepInterval = atoi(optarg);
                } else {
                    user.sleepInterval = 1000;
                }
                break;
            default:
                exit(EXIT_FAILURE);
                break;
        }
    }
    if (user.prf > 0 && user.simulate == false) {
        RKLog("Warning. PRF has no effects without simulation.\n");
    }
    
    return user;
}

//
//
//  M A I N
//
//

int main(int argc, const char **argv) {

    RKSetProgramName("rktest");
    RKSetWantScreenOutput(true);

    UserParams user = processInput(argc, argv);

    // In the case when no tests are performed, simulate the time-series
    if (user.simulate == false && user.relay == false && user.testPulseCompression == 0) {
        RKLog("No options specified. Don't want to do anything?\n");
        exit(EXIT_FAILURE);
    }

    // Screen output based on verbosity level
    if (!user.verbose) {
        RKSetWantScreenOutput(false);
    }

    if (user.simulate == true && user.relay == true) {
        RKLog("Info. Simulate takes precedence over relay.\n");
        user.relay = false;
    }
    
    // Build an initialization description
    RKRadarDesc desc;
    memset(&desc, 0, sizeof(RKRadarDesc));
    
    if (user.relay) {
        desc.initFlags = RKInitFlagRelay;
    } else {
        desc.initFlags = RKInitFlagAllocEverything;
    }
    desc.pulseCapacity = user.gateCount;
    if (user.gateCount >= 4000) {
        desc.pulseToRayRatio = ceilf((float)user.gateCount / 2000);
    } else {
        desc.pulseToRayRatio = 1;
    }
    desc.pulseBufferDepth = 5000;
    desc.rayBufferDepth = 1440;
    desc.latitude = 35.181251;
    desc.longitude = -97.436752;
    desc.radarHeight = 2.5f;
    strcpy(desc.dataPath, ROOT_PATH);
    myRadar = RKInitWithDesc(desc);
    
    if (myRadar == NULL) {
        RKLog("Error. Could not allocate radar.\n");
        exit(EXIT_FAILURE);
    }

    RKSetVerbose(myRadar, user.verbose);
    RKSetDataUsageLimit(myRadar, (size_t)20 * (1 << 30));
    
    RKAddControl(myRadar, "PPI @ 45 dps", "p ppi 3 90");
    RKAddControl(myRadar, "RHI @ AZ 35 deg @ 25 dps", "p rhi 35 0,40 20");
    RKAddControl(myRadar, "Simulate Malfunction Pedestal", "p bad");

    RKCommandCenter *center = RKCommandCenterInit();
    RKCommandCenterSetVerbose(center, user.verbose);
    RKCommandCenterAddRadar(center, myRadar);
    RKCommandCenterStart(center);
    
    // Catch Ctrl-C and exit gracefully
    signal(SIGINT, handleSignals);
    signal(SIGQUIT, handleSignals);
    signal(SIGKILL, handleSignals);

    // Set any parameters here:
    if (user.relay == false) {
        RKSetProcessingCoreCounts(myRadar, user.coresForPulseCompression, user.coresForProductGenerator);
    }
    if (!user.writeFiles) {
        RKSetDoNotWrite(myRadar, true);
    }

    if (user.simulate) {

        // Now we use the frame work.

        // Build a series of options for transceiver, only pass down the relevant parameters
        int i = 0;
        char cmd[64] = "";
        if (user.prf) {
            i += sprintf(cmd + i, " f %d", user.prf);
        }
        if (user.gateCount) {
            i += sprintf(cmd + i, " g %d", user.gateCount);
        }
        if (user.sleepInterval) {
            i += sprintf(cmd + i, " z %d", user.sleepInterval);
        }
        RKLog("Transceiver input = '%s' (%d / %s)", cmd + 1, i, RKIntegerToCommaStyleString(RKMaximumStringLength));
        RKSetTransceiver(myRadar,
                         (void *)cmd,
                         RKTestTransceiverInit,
                         RKTestTransceiverExec,
                         RKTestTransceiverFree);

        // Build a series of options for pedestal, only pass down the relevant parameters
        if (strlen(user.pedzyHost)) {
            RKLog("Pedestal input = '%s'", user.pedzyHost);
            RKSetPedestal(myRadar,
                          (void *)user.pedzyHost,
                          RKPedestalPedzyInit,
                          RKPedestalPedzyExec,
                          RKPedestalPedzyFree);
        } else {
            RKSetPedestal(myRadar,
                          NULL,
                          RKTestPedestalInit,
                          RKTestPedestalExec,
                          RKTestPedestalFree);
        }
        
        if (strlen(user.tweetaHost)) {
            RKLog("Health relay input = '%s'", user.tweetaHost);
            RKSetHealthRelay(myRadar,
                             (void *)user.tweetaHost,
                             RKHealthRelayTweetaInit,
                             RKHealthRelayTweetaExec,
                             RKHealthRelayTweetaFree);
        }

        myRadar->configs[0].prf[0] = user.prf;

        RKWaveform *waveform = RKWaveformInitWithCountAndDepth ( 22, 25 );
        RKWaveformHops(waveform, 5.0, 4.0);
        RKSetWaveform(myRadar, waveform, desc.pulseBufferDepth);

        // Radar going live, then wait indefinitely until something happens
        RKGoLive(myRadar);
        RKWaitWhileActive(myRadar);
        RKStop(myRadar);

    } else if (user.relay) {

        // Radar going live, then wait indefinitely until something happens
        RKGoLive(myRadar);
        RKWaitWhileActive(myRadar);
        RKStop(myRadar);
        
    } else if (user.testPulseCompression) {

        RKLog("Testing pulse compression ...");
        RKGoLive(myRadar);
        RKTestPulseCompression(myRadar, RKTestFlagShowResults);

    }
    
    RKCommandCenterRemoveRadar(center, myRadar);
    RKCommandCenterStop(center);
    RKCommandCenterFree(center);

    RKFree(myRadar);

    return 0;
}
