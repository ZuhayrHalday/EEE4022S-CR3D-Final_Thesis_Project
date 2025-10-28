%% SiPM Charge/Current from photons_detected

% ---------------- User-settable parameters ----------------
inFile         = 'photon_counts.csv';  % your CSV filename
outFile        = '';                   % leave empty to auto-name
device.gain    = 7.3e6;               % AFBR-S4N66P014M typical gain at chosen OV
device.qe      = 1.602e-19;           % electron charge [C]
device.tau_rec = 55e-9;               % recharge/fall constant [s] (≈55 ns)
includeNoise   = false;               % set true to include 1st-order correlated noise
P_xtalk        = 0.23;                % ~23% typical
P_ap           = 0.01;                % <1% typical
% ----------------------------------------------------------

% Read
T = readtable(inFile);

% Basic validation
if ~ismember('photons_detected', T.Properties.VariableNames)
    error('Input CSV must contain a ''photons_detected'' column.');
end

N = height(T);
if N == 0
    error('No rows found in %s.', inFile);
end

% Interpret photons_detected as primary photoelectrons before correlated noise
Npe_prim = double(T.photons_detected);

if includeNoise
    Npe_eff = Npe_prim .* (1 + P_xtalk + P_ap);
else
    Npe_eff = Npe_prim;
end

% Charge Q = Npe_eff * Gain * q_e  [C]
Q_C = Npe_eff .* device.gain .* device.qe;

% Peak current I_peak ≈ Q / tau_rec  [A]
I_peak_A = Q_C ./ device.tau_rec;

T.SiPM_charge_C   = Q_C;
T.SiPM_Ipeak_A    = I_peak_A;
T.Npe_eff         = Npe_eff;  

if isempty(outFile)
    [p,n,~] = fileparts(inFile);
    outFile = fullfile(p, sprintf('%s_with_sipm_results.csv', n));
end
writetable(T, outFile);
fprintf('Wrote results to %s\n', outFile);

gainStr = sprintf('G = %.2g', device.gain);
tauStr  = sprintf('\\tau_{rec} = %.0f ns', device.tau_rec*1e9);

% --------- Plots ---------
% 1) Charge
figure('Name','SiPM Charge per Row','Color','w');
plot(Q_C*1e12, 'LineWidth', 1.5);            
xlabel('Row / Event #');
ylabel('Charge Q [pC]');
grid on;
title(sprintf('SiPM Charge per Row  (%s, %s, noise %s)', ...
    gainStr, tauStr, string(includeNoise)));

% 2) Peak current
figure('Name','SiPM Peak Current per Row','Color','w');
plot(I_peak_A*1e3, 'LineWidth', 1.5);       
xlabel('Row / Event #');
ylabel('Peak Current I_{peak} [mA]');
grid on;
title(sprintf('SiPM Peak Current per Row  (%s, %s, noise %s)', ...
    gainStr, tauStr, string(includeNoise)));

% --------- Console summary ---------
totalPhotons = sum(Npe_eff);
totalChargeC = sum(Q_C);
fprintf('Rows processed: %d\n', N);
fprintf('Total effective photoelectrons: %.3g\n', totalPhotons);
fprintf('Total charge: %.3g C (%.3g pC)\n', totalChargeC, totalChargeC*1e12);
fprintf('Example (row 1): Q = %.3g pC, I_peak = %.3g mA\n', ...
    Q_C(1)*1e12, I_peak_A(1)*1e3);
