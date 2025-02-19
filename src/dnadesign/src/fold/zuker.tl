local energies = require("energies")

local record zuker
	pair: function(s: string, start: integer, rightOfStart: integer, end_idx: integer, leftOfEnd: integer): string
	stack: function(start: integer, rightOfStart: integer, end_idx: integer, leftOfEnd: integer, foldContext: context): number
	hairpin: function(start: integer, end_idx: integer, foldContext: context): (number, string)
end

local record context
    seq: string
    temp: number
    energies: energies.Energies
end

-- pair returns a stack representation, a key for the nearestNeighbors maps
-- Args:
--   s: string - Sequence being folded
--   start: integer - leftmost index
--   rightOfStart: integer - index to right of start
--   end_idx: integer - rightmost index
--   leftOfEnd: integer - index to left of end
-- Returns: string - representation of the pair in format "XX/XX" where X are bases
local function pair(s: string, start: integer, rightOfStart: integer, end_idx: integer, leftOfEnd: integer): string
    local ret = {'.', '.', '/', '.', '.'}
    
    if start >= 0 then
        ret[1] = s:sub(start + 1, start + 1)
    end
    if rightOfStart >= 0 then
        ret[2] = s:sub(rightOfStart + 1, rightOfStart + 1)
    end
    if end_idx >= 0 then
        ret[4] = s:sub(end_idx + 1, end_idx + 1)
    end
    if leftOfEnd >= 0 then
        ret[5] = s:sub(leftOfEnd + 1, leftOfEnd + 1)
    end
    
    return table.concat(ret)
end

-- stack returns the free energy of a stack
-- Args:
--   start: integer - The start index on left side of the pair/stack
--   rightOfStart: integer - The index to the right of start
--   end_idx: integer - The end index on right side of the pair/stack
--   leftOfEnd: integer - The index to the left of end
--   foldContext: context - The folding context for this sequence
-- Returns: number - The free energy of the nearestNeighbors pairing
local function stack(start: integer, rightOfStart: integer, end_idx: integer, leftOfEnd: integer, foldContext: context): number
    local paired = pair(foldContext.seq, start, rightOfStart, end_idx, leftOfEnd)
    
    -- Check for dangling ends first, matching Go implementation priority
    for _, idx in ipairs({start, rightOfStart, end_idx, leftOfEnd}) do
        if idx == -1 then
            local fold_energy = foldContext.energies.danglingEnds[paired]
            return energies.delta_g(fold_energy.enthalpyH, fold_energy.entropyS, foldContext.temp)
        end
    end

    -- Then check bounds
    for _, idx in ipairs({start, rightOfStart, end_idx, leftOfEnd}) do
        if idx >= #foldContext.seq then
            return 0
        end
    end

    -- Internal case
    if start > 0 and end_idx < #foldContext.seq - 1 then
        local fold_energy = foldContext.energies.nearestNeighbors[paired]
        if not fold_energy then
            fold_energy = foldContext.energies.internalMismatches[paired]
        end
        return energies.delta_g(fold_energy.enthalpyH, fold_energy.entropyS, foldContext.temp)
    end

    -- Terminal case
    if start == 0 and end_idx == #foldContext.seq - 1 then
        local fold_energy = foldContext.energies.nearestNeighbors[paired]
        if not fold_energy then
            fold_energy = foldContext.energies.internalMismatches[paired]
        end
        return energies.delta_g(fold_energy.enthalpyH, fold_energy.entropyS, foldContext.temp)
    end

    -- Dangling on left
    if start > 0 and end_idx == #foldContext.seq - 1 then
        local fold_energy = foldContext.energies.nearestNeighbors[paired]
        if not fold_energy then
            fold_energy = foldContext.energies.internalMismatches[paired]
        end
        local dg = energies.delta_g(fold_energy.enthalpyH, fold_energy.entropyS, foldContext.temp)

        local pair_dangling_ends = foldContext.seq:sub(start, start) .. 
                                 foldContext.seq:sub(start+1, start+1) .. 
                                 "/." .. 
                                 foldContext.seq:sub(end_idx+1, end_idx+1)
        
        fold_energy = foldContext.energies.danglingEnds[pair_dangling_ends]
        if fold_energy then
            dg = dg + energies.delta_g(fold_energy.enthalpyH, fold_energy.entropyS, foldContext.temp)
        end
        return dg
    end

    -- Dangling on right
    if start == 0 and end_idx < #foldContext.seq - 1 then
        local fold_energy = foldContext.energies.nearestNeighbors[paired]
        if not fold_energy then
            fold_energy = foldContext.energies.internalMismatches[paired]
        end
        local dg = energies.delta_g(fold_energy.enthalpyH, fold_energy.entropyS, foldContext.temp)

        local pair_dangling_ends = "." .. 
                                 foldContext.seq:sub(start+1, start+1) .. 
                                 "/" ..
                                 foldContext.seq:sub(end_idx+2, end_idx+2) ..
                                 foldContext.seq:sub(end_idx+1, end_idx+1)
        
        fold_energy = foldContext.energies.danglingEnds[pair_dangling_ends]
        if fold_energy then
            dg = dg + energies.delta_g(fold_energy.enthalpyH, fold_energy.entropyS, foldContext.temp)
            return dg
        end
    end

    return 0
end

local minLenForStruct = 4
local maxLenPreCalulated = 30
local closingATPenalty = 0.5

-- hairpin calculates the free energy of a hairpin
-- Args:
--   start: integer - The index of start of hairpin
--   end_idx: integer - The index of end of hairpin
--   foldContext: context - The folding context for this sequence
-- Returns: (number, string?) - The free energy increment and optional error
local function hairpin(start: integer, end_idx: integer, foldContext: context): (number, string)
    if end_idx - start < minLenForStruct then
        return math.huge, nil
    end

    local hairpinSeq = foldContext.seq:sub(start + 1, end_idx + 1)
    local seqLen = end_idx - start + 1  -- Actual length of subsequence
    local hairpinLength = seqLen - 2     -- Internal length (excluding closing pair)

    local paired = pair(foldContext.seq, start, start + 1, end_idx, end_idx - 1)
    
	-- Check if terminal pair can form
	local first_base = hairpinSeq:sub(1,1)
	local last_base = hairpinSeq:sub(-1)
	
	if foldContext.energies.complement(first_base) ~= last_base then
	    return 0, string.format("hairpin: subsequence (%d, %d): unknown hairpin terminal pairing %s - %s", 
	                          start, end_idx, first_base, last_base)
	end
    local dG = 0.0

    -- Check triTetraLoops if available
    if foldContext.energies.triTetraLoops then
        local tetraLoops = foldContext.energies.triTetraLoops as energies.MatchingBasepairEnergy
        local energy = tetraLoops[hairpinSeq]
        if energy then
            dG = energies.delta_g(energy.enthalpyH, energy.entropyS, foldContext.temp)
        end
    end

    -- Add penalty based on size
    local size_energy = foldContext.energies.hairpinLoops[hairpinLength]
    if size_energy then
        dG = dG + energies.delta_g(size_energy.enthalpyH, size_energy.entropyS, foldContext.temp)
    else
        -- Too large, extrapolate
        local max_energy = foldContext.energies.hairpinLoops[maxLenPreCalulated]
        local dGinc = energies.delta_g(max_energy.enthalpyH, max_energy.entropyS, foldContext.temp)
        dG = dG + energies.jacobson_stockmayer(hairpinLength, maxLenPreCalulated, dGinc, foldContext.temp)
    end

    -- Add penalty for terminal mismatch
    local terminal_energy = foldContext.energies.terminalMismatches[paired]
    if hairpinLength > 3 and terminal_energy then
        dG = dG + energies.delta_g(terminal_energy.enthalpyH, terminal_energy.entropyS, foldContext.temp)
    end

    -- Add penalty if length 3 and AT closing
	if hairpinLength == 3 then
	    -- Check if either end has an A
	    if first_base == "A" or last_base == "A" then
	        dG = dG + closingATPenalty
	    end
	end

    return dG, nil
end

zuker.pair = pair
zuker.stack = stack
zuker.hairpin = hairpin

return zuker
