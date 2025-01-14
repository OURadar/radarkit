//
//  RadarKit.h
//  RadarKit
//
//  Copyright 2015-2024 Boonleng Cheong
//
//  Created by Boonleng Cheong on 3/17/15.
//

#ifndef __RadarKit__
#define __RadarKit__

/*!
 @framework RadarKit Framework Reference
 @author Boonleng Cheong
 @abstract
 @discussion The RadarKit framework provides the APIs and support for fundamental
 radar time-series data processing. It defines the base structure for base level
 raw I/Q data, 16-bit straight from the analog-to-digital converters, fundamental
 processing such as auto-correlation calculations, pulse compressions, FIR (finite
 impulse response) and IIR (infinite impulse response) filtering, window functions,
 data transportation across network, etc.

 */

#ifdef __cplusplus
extern "C" {
#endif

#include <RadarKit/RKRadar.h>
#include <RadarKit/RKTest.h>
#include <RadarKit/RKCommandCenter.h>
#include <RadarKit/RKPedestalPedzy.h>
#include <RadarKit/RKHealthRelayTweeta.h>
#include <RadarKit/RKHealthRelayNaveen.h>
#include <RadarKit/RKRadarRelay.h>
#include <RadarKit/RKHostMonitor.h>
#include <RadarKit/RKReporter.h>
#include <RadarKit/RKFileHeader.h>

#ifdef __cplusplus
}
#endif

#endif /* defined(__RadarKit__) */

//
//  █
//
// Updates
//
//         - To do:
//         - Major:
//           - [x] v3.0 - Cloud communication (reverse relay)
//           - Task, event, sequence of tasks
//         - Minor:
//           - Mapping coefficients to product files
//
//
//  5.7    - 01/19/2024
//         - New sleep cycles for RKPulseEngine, RKPulseRingEngine, RKMomentEngine
//         - New state to differentiate ready for moment engine and complete for moment calculation
//         - New Python reading and visualization tools
//         - General bug fixes and improvements
//
//  5.6.1  - 12/20/2023
//         - Fixed RKSIMD_sum() goes over the bound
//
//  5.6    - 10/14/2023
//         - Added new state RKPulseStatusConsumed
//         - Added RKPulseEngineGetProcessedPulse()
//
//  5.5    - Somehow skipped
//
//  5.4    - 06/12/2023
//         - Added RKHealthRelayNaveen for NMEA ingest
//         - Added delayed user module deallocation
//         - Improved Makefile
//         - Removed m4
//
//  5.3    - 06/08/2023
//         - Introduced new RKPulseEngineGetVacantPulse with custom wait
//
//  5.2    - 06/07/2023
//         - Added shared library for Python ctypes
//         - Added static mutex deallocation
//         - Added window blending
//
//  5.1    - 06/01/2023
//         - Added new initialization flags
//         - Added new arrays in compression scratch
//         - Renamed some internal SIMD intrinsics for consistency
//
//  5.0.1  - 05/31/2023
//         - Bug fixes on RKSteerEngine
//
//  5.0    - 05/19/2023
//         - New compressor and calibrator interfaces
//         - New backward-compatible file header reading
//         - Added double buffering to pulse header reading
//         - Reworked scratch space allocation
//         - Reworked engine lag calculation
//         - Reworked product buffer allocation
//         - Minor bug fixes
//
//  4.3    - 05/07/2023
//         - Added empty folder removal during startup of RKFileManager
//         - Added new command v next for subsequent volume
//         - Removed RKPedestalPedzy.headingOffset
//         - VCP help text revised
//         - General bug fixes
//
//  4.2    - 05/04/2023
//         - SIMD module updated
//           - Added RKTestSIMDBasic
//           - Some macros renamed for clarity
//         - Steer engine updated
//           - Pedestal action is only sent if it is not None
//           - Command v point is immediate
//           - Command v home is immediate
//           - Commands p is restored
//         - Bug fixes
//
//  4.1    - 04/29/2023
//         - RKSIMD module adds support for ARM processors
//         - RKSteerEngine adds support for multiple pp sentences under vol
//         - Improved consistency among RKTestTransceiver-Pedestal-HealthRelay
//         - General cleanup and bug fixes
//
//  4.0    - 04/16/2023
//         - New RKSteerEngine for VCP control
//         - New stream 8 for monitoring pedestal action
//         - RKTestPedestal reworked for improved emulation
//         - RKPedestalPedzy reworked for new steer engine
//
//  3.2    - 02/25/2023
//         - Added RKReporter for RadarHub communication
//         - Added RKWebSocket, a built-from-scratch implementation
//         - Added SSL support for WebSocket connections through https
//         - Added various screen output functions for binary payloads
//
//  3.1    - 6/28/2022
//         - RKCommandCenter adopted RKRayHeaderF1 for legacy compatibility
//
//  3.0    - 6/20/2022
//         - Added RKWebSocket for communication to RadarHub
//
//  2.6    - 6/9/2021
//         - Fixed bugs in reading built 5 in pgen
//         - Variable buildNo is now version
//         - New RKConfig definition
//         - Deprecated RKConfig->filterCount
//         - A complete waveform is now stored in IQ files
//         - Replaced mkdir system call in RKPreparePath()
//         - Fixed compiler warnings about string in Ubuntu
//
//  2.5    - 5/3/2021
//         - Waveform conversion tool owav2wav
//         - Chirps are now called tones
//         - MATLAB reader updated for build no 6
//         - Revised MATLAB presentation
//         - Added millisecond time to raw data filenames
//
//  2.4.1  - 5/1/2021
//         - Added RKReadPulseFromFileReference()
//         - Updated MATLAB iqread.m to support n-group waveforms
//
//  2.4    - 4/25/2021
//         - Raw data build no 6
//         - Added RKCalibrator module
//         - Renamed RKSweep.h to RKSweepEngine.h
//         - Renamed RKMoment.h to RKMomentEngine.h
//         - Renamed RKPosition.h to RKPositionEngine.h
//         - Renamed RKPulseCompression.h to RKPulseEngine.h
//
//  2.3    - 6/30/2019
//         - Added I/Q data playback
//         - Added hopping chirp waveforms
//         - Added individual storage limits
//         - Added command queues
//         - Added SQIThreshold
//         - Added ignoreHeading flag
//         - Updated hopping chirp, summary, etc.
//         - Updated RKWaveformApplyWindow() for count > 1
//         - Updated MATLAB reader for buildNo 3
//         - Updated TFM waveform files
//         - Fxed waveform name in waveform files
//         - General dead storage removal
//
//  2.2    - 4/30/2019
//         - Added RKWaveformInitAsFrequencyHoppingChirp()
//         - Redefined RKPositionFlag for pedzy 2.0.
//           - Added end of volume flag
//         - Updated MATLAB reader for raw data buildNo >= 2
//
//  2.1.4  - 4/18/2019
//         - Removed AScope mode 2, renamed others
//           - Mode 0 - Raw I/Q (default)
//           - Mode 1 - Compressed I/Q (out to usable range)
//           - Mode 2 - Debugging mode, raw I/Q + template + raw data
//
//  2.1.3  - 4/18/2019
//         - Usable gates reduced by blind range
//         - Added gate count check for calibration numbers
//         - Added RKConfigKeyPulseWidth
//         - Added test 18 - Test writing using RKProductFileWriteNC()
//         - Bug fix: RKGetPrefixFromFilename() with 1 char
//
//  2.1.2  - 4/3/2019
//         - Xcode project settings
//         - New routines:
//           - RKLastNPartsOfPath()
//         - Bug fixes
//
//  2.1.1  - 4/1/2019
//         - Added version display as banner
//           - Added RKShowBanner()
//           - Added RKStringCenterized()
//         - Added product symbol color
//         - Improved product symbol extraction from filename
//         - Fixed a bug in gate count retrieval for scratch space processing
//
//  2.1    - 3/7/2019
//         - Added blib-sh search in the file handling script
//         - Added Sh, Sv and SQI transmission to PyRadarKit
//         - Added RKProductCollectionInitWithFilename() and dependents
//           - Added RKListFilesWithSamePrefix()
//           - Added RKGetPrefixFromFilename()
//         - Added test 17 for product collection test
//         - Updated comments on value mapping in RKFoundation
//         - Improved efficiency of RhoHV to index mapping
//         - Fixed moment product index in hex table
//
//  2.0.5  - 1/31/2019
//         - Added a Matlab waveform writer
//         - General improvements and bug fixes
//
//  2.0.4  - 11/30/2018
//         - Added new frequency hop sequence generator
//         - Implemented scaffold of spectral processing
//         - Separated out FFT engine so that multiple processors can share
//         - Fixed the I/Q stream of AScope
//         - Performance improvement on I/Q processor
//
//  2.0.3  - 10/29/2018
//         - Moment method can be switched on the fly
//         - Common FFT engine, always measured optimum performance
//         - Removed some hardcoded DFT constants
//         - Added more tests to RKTestOneRay
//         - General bug fixes and improvements
//
//  2.0.2  - 10/01/2018
//         - Added RKPulsePair to RKTestMomentProcessorSpeed
//         - Added ASCII art to show reflectivity in Terminal
//         - Added hooks for custom moment calibrator
//         - Added hooks for custom pulse compressor
//         - Added RKIntegerToHexStyleString() and *InHex types
//         - Stability improvements
//
//  2.0.1  - 8/31/2018
//         - RKBufferOverview upgraded to show health buffers
//         - RKBufferOverview upgraded to show position buffers
//         - RKBufferOverview now supports 80 x 25 terminal size
//         - Replaced !isfinite() with isnan() since isfinite(0.0) = 0
//         - RKClock self reset onlyl when count > 0
//         - Added more comments to functions declared in RKRadar.h
//         - Radar name and file prefix can now be changed on the fly
//         - Fixed a bug in statusString overrun
//         - Added ability to use custom TCP/IP port
//         - Nomenclature: Max -> Maximum
//         - Added waveform windowing
//         - Radar live vs. active
//         - Finally added the conventional pulse-pair processor
//         - Added validation tests for pulse-pair and pulse-pair for hops
//         - Corrected expected error in validation tests for all processors
//         - Added SQI as part of the base moment collection
//         - Deprecated RKConfigZCal, RKConfigZCals, etc.
//         - FFT wisdom generation (T24) now includes backward transform
//         - Many general improvements and bug fixes
//
//  2.0    - 7/30/2018
//         - RKEngineStateActive and RKEngineStateWantActive
//         - FIR/IIR ground clutter filters
//         - Four Elliptical filters are included
//         - RKSweepEngine product unregistration
//         - Buffer overview text stride for deep buffers
//         - Added ability to ignore GPS device
//         - Worker name is now part of the struct
//
//  1.2.10 - 6/30/2018
//         - Added RKProductFileReadNC() for offline reading
//         - Added RKSweep to RKProduct recording
//         - Added RKProduct buffer with dynamic allocation
//         - Added RKVariableInString() for highlighting variables
//         - Added product registration from PyRadarKit space
//         - Product identifier from PyRadarKit is ingested
//         - Product description from PyRadarKit is ingested
//         - JSON response now required the key "type"
//         - Added RKWaveform definition to RKTypes.h
//         - Added support for multiple user products from a client
//         - Introduced scan type mask
//         - Fixed the stream restore for all connected clients
//         - Fixed a dead lock during waveform calibration
//         - Added individual waveform calibration constants
//         - Added preference file monitor using RKFileMonitor()
//         - New thinking in terms of adding ZCal, DCal, filter anchors, etc.
//         - Added RKSetVerbosityUsingArray()
//         - Improved logging during quiet mode
//
//  1.2.9  - 5/31/2018
//         - Added buffer overview, accessible through terminal
//         - Added pulse status string, accessible through terminal
//         - Improved RKSoftRestart()
//         - Added RKBufferOverview() with color option
//         - Added another state for pulse - RKPulseStatusRecorded
//         - Added RKFileMonitor(), which can be used for pref.conf
//         - Added the example simple-emulator
//         - Added individual waveform calibration
//         - New thinking for RKAddConfig() for ZCals and filterAnchors
//
//  1.2.8  - 4/31/2018
//         - New stream types: RKSweep, RKSweepHeader and RKSweepRay
//         - Fixed buffer ID initialization
//         - Fixed a memory leak in RKHostMonitor
//         - Added RKSoftRestart() to restart DSP-related engines only
//         - Added RKSystemInspector() for gathering system status
//         - Migrated RKFileMonitor() to RKSimpleEngine design
//         - Added RKSimpleEngine for simple sus-systems
//
//  1.2.7  - 3/31/2018
//         - Added RKSweepRead()
//         - Added waveform sensitivity gain
//         - Added calibration adjustment with senstivity gain
//         - Added calibration adjustment with ADC sampling frequency
//         - Unity noise gain enforced for compression filters
//         - Carrier frequency is now in RKWaveform
//         - Added setSystemLevel() for various configurations
//         - Added ring filter engine
//         - Added waveform loading in RKTestTransceiver
//
//  1.2.6  - 2/27/2018
//         - Added RKHostMonitor for ICMP echo reqeust
//         - Default host is Google's 8.8.8.8
//         - Multiple-range calibration based filter anchors
//         - All waveforms are required to have unity noise gain
//         - Sampling frequency change is now in RKWaveform
//         - Senstivity calculation based on waveform
//         - Improved CPU core count
//         - IP address is logged alongside the command
//         - Improved efficiency of RKTestTransceiver
//
//  1.2.5  - 1/8/2018
//         - Added RKClearControls(), RKConcludeControls()
//         - Send updated controls
//
//  1.2.4  - Improved efficiency of RKPulseEngine
//         - Reordered RKTest modules
//
//  1.2.3  - 12/7/2017
//         - Default waveform and pedestal task for RKTestTransceiver
//         - RKTestPulseCompression() is now self contained
//         - Consolidated many RKTest modules
//         - GPS reading has been moved to RKTestHealthRelay
//         - Added handleRadarTgz.sh for LDM server
//         - Added a MATLAB ACF & CCF calculations for validation
//         - Added ring worker for FIR / IIR buffer
//         - Completed multilag estimator
//         - Status enum expanded
//         - All filters are now normlized to have unity noise gain
//         - Added LFM generation to RKWaveform
//
//  1.2.2  - Boolean value parsing in preference
//         - Waveform generation with fc
//
//  1.2.1  - Improved stream reset mechanism
//         - Multiple filters within a filter group
//
//  1.2    - Radar display stream relay with internal consolidation
//         - RadarKit system engine status
//         - On-demand raw data recording
//         - General bug fixes
//
//  1.1.1  - Command relay
//         - RKFileManager
//         - RKHealthEngine and RKHealthLogger detached
//         - Incorporated NetCDF-4
//         - General bug fixes
//
//  1.1    - 3/31/2017 (esimated)
//         - Optmized sequence for frequency hop
//         - Raw data I/Q recording
//         - Health logger
//         - Reduced memory footprint
//
//  1.0    - 2/28/2017 (estimated)
//         - First working version
//         - Added sweepWriter
//         - Added RKCommandCenter
//
//  0.9    - 1/31/2017 (estimated)
//         - Networking, RKClient and RKServer
//         - Health reporting
//         - Transceiver hook
//         - Pedestal hook
//         - Health relay hook
//
//  0.5    - 11/30/2016 (esimated)
//         - Moment calculation
//
//  0.2    - 9/3/2016 (esimated)
//         - Multi-core pulse compression engine
//         - Position tagging
//         - Pulse gathering for moment calculation
//
//  0.1    - 3/17/2015
//         - Initial scaffold
//
