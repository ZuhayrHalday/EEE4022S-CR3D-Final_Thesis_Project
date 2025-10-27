function plot_photon_counts()

clc;

% --- Global styling for a professional look ---
set(groot,'defaultAxesFontName','Arial');
set(groot,'defaultAxesFontSize',12);
set(groot,'defaultLineLineWidth',1.6);
set(groot,'defaultFigureColor','w');

% --- File names (adjust if yours differ) ---
fPath   = 'photon_counts_pathlength.csv';
fEnergy = 'photon_counts_energysweep.csv';
fBirks  = 'photon_counts_birkssweep.csv';

% --- Plot each dataset ---
plot_one(fPath,   'muon_path_mm',   'Path Length in Scintillator (mm)', ...
    'Photon Counts vs Path Length', 'pathlength');

plot_one(fEnergy, 'muon_energy_GeV','Muon Energy (GeV)', ...
    'Photon Counts vs Muon Energy', 'energysweep');

plot_one(fBirks,  'kB',             'Birks Constant k_B (mm/MeV)', ...
    'Photon Counts vs Birks Constant', 'birkssweep');

disp('Done. Figures shown and PNGs saved.');
end


function plot_one(csvFile, xColName, xLabelText, figTitle, outTag)
    % Read table (robust to stray/unnamed columns)
    assert(isfile(csvFile), "File not found: %s", csvFile);
    T = readtable(csvFile);

    % Resolve X column
    x  = get_col(T, xColName);

    % Always present
    yProduced = get_col(T, 'photons_produced');
    yDetected = get_col(T, 'photons_detected');

    % Flag: energy sweep figure?
    isEnergy = strcmpi(xColName, 'muon_energy_GeV');

    % For non-energy figures we still need "arrived"
    if ~isEnergy
        if has_col(T,'photons_arrived_window')
            yArrived = get_col(T, 'photons_arrived_window');
            arrivedLabel = 'Photons Arrived (window)';
        elseif has_col(T,'photons_arrived')
            yArrived = get_col(T, 'photons_arrived');
            arrivedLabel = 'Photons Arrived';
        else
            error('Could not find a "photons_arrived" column in %s', csvFile);
        end
    else
        % For energy figure: middle subplot uses muon_dEdx_MeV_per_cm
        if has_col(T,'muon_dEdx_MeV_per_cm')
            yEdep = get_col(T,'muon_dEdx_MeV_per_cm');
            edepYlabel = 'Energy Deposited (dE/dx, MeV/cm)';
        else
            error('Could not find column "muon_dEdx_MeV_per_cm" in %s', csvFile);
        end
    end

    % Figure with three subplots
    fig = figure('Name', figTitle, 'Units','normalized', 'Position',[0.1 0.1 0.8 0.6]);
    tl = tiledlayout(fig,1,3,'TileSpacing','compact','Padding','compact');
    title(tl, figTitle, 'FontWeight','bold', 'FontSize',14);

    % 1) Produced
    ax1 = nexttile; 
    plot(x, yProduced, '-o', 'MarkerSize',4);
    grid on; box on; hold on;
    xlabel(xLabelText);
    ylabel('Photons Produced');
    title('Photons Produced');

    % 2) Middle subplot:
    if ~isEnergy
        % Arrived (for non-energy figures)
        ax2 = nexttile;
        plot(x, yArrived, '-o', 'MarkerSize',4);
        grid on; box on; hold on;
        xlabel(xLabelText);
        ylabel(arrivedLabel);
        title('Photons Arrived');
    else
        % dE/dx (for energy figure)
        ax2 = nexttile;
        plot(x, yEdep, '-o', 'MarkerSize',4);
        grid on; box on; hold on;
        xlabel(xLabelText);
        ylabel(edepYlabel);
        title('Energy Deposited');
    end

    % 3) Detected
    ax3 = nexttile;
    plot(x, yDetected, '-o', 'MarkerSize',4);
    grid on; box on; hold on;
    xlabel(xLabelText);
    ylabel('Photons Detected');
    title('Photons Detected');

    % Link x-axes if monotonic
    if issorted(x)
        linkaxes([ax1 ax2 ax3],'x');
    end

    % Save PNG next to script
    outName = sprintf('plot_%s.png', outTag);
    try
        exportgraphics(fig, outName, 'Resolution',300);
    catch
        warning('Could not save %s. (exportgraphics requires R2020a+)', outName);
    end
end


function tf = has_col(T, name)
    tf = any(strcmpi(T.Properties.VariableNames, name));
end

function v = get_col(T, name)
    % Return numeric vector for a column, tolerant to case
    idx = find(strcmpi(T.Properties.VariableNames, name), 1);
    assert(~isempty(idx), 'Column "%s" not found.', name);
    vRaw = T.(T.Properties.VariableNames{idx});
    if iscell(vRaw)
        v = str2double(vRaw);
    else
        v = vRaw;
    end
    v = v(:);
end
