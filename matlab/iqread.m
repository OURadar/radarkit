classdef iqread
    properties (Constant)
        RKFileHeaderSize = 4096;
        RKMaxMatchedFilterCount = 4;
        RKMaximumStringLength = 4096;
    end
    properties
        filename = '';
        fileHeader = [];
        config = [];
        pulses = []
    end
    methods
        % Constructor
        function self = iqread(filename)
            fprintf('Filename: %s\n', filename);
            self.filename = filename;
            fid = fopen(self.filename);
            if (fid < 0)
                error('Unable to open file.');
            end
            
            rk_config_size = 4;
            rk_config_size = rk_config_size + 4 * 4 * self.RKMaxMatchedFilterCount;
            rk_config_size = rk_config_size + self.RKMaximumStringLength;
            rk_config_size = rk_config_size + 4 * 2 * 4;
            rk_config_size = rk_config_size + 4;     % censorSNR
            rk_config_size = rk_config_size + 2 * 4; % sweepElevation, sweepAzimuth
            rk_config_size = rk_config_size + 4;     % RKMarker

            % Properties
            self.fileHeader = fread(fid, self.RKFileHeaderSize, 'char=>char');
            self.config = fread(fid, rk_config_size, 'char=>char');

            % Partially read the very first pulse
            pulse_header.i = fread(fid, 1, 'uint64');
            pulse_header.n = fread(fid, 1, 'uint64');
            pulse_header.t = fread(fid, 1, 'uint64');
            pulse_header.s = fread(fid, 1, 'uint32');
            pulse_header.capacity = fread(fid, 1, 'uint32');
            pulse_header.gateCount = fread(fid, 1, 'uint32');
            fprintf('gateCount = %d   offset = %d\n', pulse_header.gateCount, 4096 + rk_config_size);
            
            % Map
            m = memmapfile(self.filename, ...
                'Offset', 4096 + rk_config_size, ...
                'Format', { ...
                        'uint64', [1 1], 'i'; ...
                        'uint64', [1 1], 'n'; ...
                        'uint64', [1 1], 't'; ...
                        'uint32', [1 1], 's'; ...
                        'uint32', [1 1], 'capacity'; ...
                        'uint32', [1 1], 'gateCount'; ...
                        'uint32', [1 1], 'marker'; ...
                        'uint32', [1 1], 'pulseWidthSampleCount'; ...
                        'uint64', [1 1], 'time_tv_sec'; ...
                        'uint64', [1 1], 'time_tv_usec'; ...
                        'double', [1 1], 'timeDouble'; ...
                        'uint8',  [1 4], 'rawAzimuth'; ...
                        'uint8',  [1 4], 'rawElevation'; ...
                        'uint16', [1 1], 'configIndex'; ...
                        'uint16', [1 1], 'configSubIndex'; ...
                        'uint16', [1 1], 'azimuthBinIndex'; ...
                        'single', [1 1], 'gateSizeMeters'; ...
                        'single', [1 1], 'elevationDegrees'; ...
                        'single', [1 1], 'azimuthDegrees'; ...
                        'single', [1 1], 'elevationVelocityDegreesPerSecond'; ...
                        'single', [1 1], 'azimuthVelocityDegreesPerSecond'; ...
                        'int16',  [2 pulse_header.gateCount 2], 'iq'});
            
            self.pulses = m.Data;

            fclose(fid);
        end
    end
end