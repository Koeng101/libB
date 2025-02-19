local dnadesign = require("dnadesign")
local zuker = dnadesign.zuker
local energies = dnadesign.energies

describe("zuker", function()
    describe("pair", function()
        it("should handle basic case", function()
            local seq = "ATGGAATAGTG"
            local result = zuker.pair(seq, 0, 1, 9, 10)
            assert.equals("AT/TG", result)
        end)

        it("should handle negative indices with dots", function()
            local seq = "ATGGAATAGTG"
            -- Go code would show .T/TG because 1 is >= 0
            local result = zuker.pair(seq, -1, 1, 9, 10)
            assert.equals(".T/TG", result)
        end)

        it("should handle all negative indices", function()
            local seq = "ATGGAATAGTG"
            local result = zuker.pair(seq, -1, -1, -1, -1)
            -- Go code would show ../.., not ././.
            assert.equals("../..", result)
        end)

        it("should handle indices at sequence boundaries", function()
            local seq = "ATGGAATAGTG"
            local result = zuker.pair(seq, 0, 1, 10, 9)
            assert.equals("AT/GT", result)
        end)
    end)
	describe("stack", function()
        it("should calculate internal stack energy", function()
            local seq = "GCUCAGCUGGGAGAGC"
            local context = {
                seq = seq,
                temp = 37.0 + 273.15,
                energies = energies.rna_energies
            }
            local result = zuker.stack(1, 2, 14, 13, context)
            -- Expected value around -2.1 based on Go tests
            assert.is_true(math.abs(result + 2.1) < 0.1)
        end)

        it("should handle out of bounds indices", function()
            local seq = "GCUCAGCUGGGAGAGC"
            local context = {
                seq = seq,
                temp = 37.0 + 273.15,
                energies = energies.rna_energies
            }
            local result = zuker.stack(1, 2, 20, 19, context)
            assert.equals(0, result)
        end)

        it("should handle dangling ends", function()
            local seq = "GCUCAGCUGGGAGAGC"
            local context = {
                seq = seq,
                temp = 37.0 + 273.15,
                energies = energies.rna_energies
            }
            local result = zuker.stack(0, 1, 15, 14, context)
            -- Should not be 0 for terminal case
            assert.is_true(result ~= 0)
        end)

	    it("should calculate correct stacking energy", function()
	        local seq = "GCUCAGCUGGGAGAGC"
	        local context = {
	            seq = seq,
	            temp = 37.0 + 273.15,
	            energies = energies.rna_energies
	        }
	        local result = zuker.stack(1, 2, 14, 13, context)
	        -- Go test expects around -2.1
	        assert.is_true(math.abs(result + 2.1) < 0.1)
	    end)
	end)

	describe("hairpin", function()
		it("should calculate hairpin energy for case 1", function()
    	    local seq = "ACCCCCTCCTTCCTTGGATCAAGGGGCTCAA"
    	    local context = {
    	        seq = seq,
    	        temp = 37.0 + 273.15,
    	        energies = energies.rna_energies
    	    }
    	    local result, err = zuker.hairpin(11, 16, context)
    	    assert.is_nil(err)
    	    assert.is_true(math.abs(result - 4.3) < 1.0)
    	end)

    	it("should calculate hairpin energy for case 2", function()
    	    local seq = "ACCCGCAAGCCCTCCTTCCTTGGATCAAGGGGCTCAA"
    	    local context = {
    	        seq = seq,
    	        temp = 37.0 + 273.15,
    	        energies = energies.dna_energies
    	    }
    	    local result, err = zuker.hairpin(3, 8, context)
    	    assert.is_nil(err)
    	    assert.is_true(math.abs(result - 0.67) < 0.1)
    	end)

    	it("should calculate hairpin energy for case 3", function()
    	    local seq = "CUUUGCACG"
    	    local context = {
    	        seq = seq,
    	        temp = 37.0 + 273.15,
    	        energies = energies.rna_energies
    	    }
    	    local result, err = zuker.hairpin(0, 8, context)
    	    assert.is_nil(err)
    	    assert.is_true(math.abs(result - 4.5) < 0.2)
    	end)
	end)

	describe("bulge", function()
        it("should handle invalid bulge length", function()
            local seq = "GCAT"
            local context = {
                seq = seq,
                temp = 37.0 + 273.15,
                energies = energies.dna_energies
            }
            
            local result, err = zuker.bulge(0, 1, 2, 1, context)
            assert.equals(0, result)
            assert.matches("bulge: the length of the bulge at %(0, 2%) is 0", err)
        end)

        it("should handle length 1 bulge with missing pair in nearestNeighbors", function()
            local seq = "GCTAGCTGGGAGAGC"
            local context = {
                seq = seq,
                temp = 37.0 + 273.15,
                energies = energies.dna_energies
            }
            local result, err = zuker.bulge(1, 3, 12, 11, context)
            assert.equals(0, result)
            assert.matches("bulge: paired.*not in the nearestNeighbors energies", err)
        end)

        it("should calculate energy for valid length 1 bulge", function()
            local seq = "CCCAGGG"
            local context = {
                seq = seq,
                temp = 37.0 + 273.15,
                energies = energies.rna_energies
            }
            local result, err = zuker.bulge(0, 2, 5, 4, context)
            assert.is_nil(err)
            -- Tests that it calculates a valid energy, not necessarily positive
            assert.is_true(result ~= 0)
            assert.is_true(result ~= math.huge)
        end)

        it("should calculate energy for longer bulge", function()
            local seq = "CCCAAAGGG" 
            local context = {
                seq = seq,
                temp = 37.0 + 273.15,
                energies = energies.rna_energies
            }
            local result, err = zuker.bulge(0, 2, 7, 6, context)
            assert.is_nil(err)
            -- Tests that it calculates a valid energy, not necessarily positive
            assert.is_true(result ~= 0)
            assert.is_true(result ~= math.huge)
        end)
    end)

	describe("internal_loop", function()
    it("should handle invalid total loop length", function()
        local seq = "GCAGCA" 
        local context = {
            seq = seq,
            temp = 37.0 + 273.15,
            energies = energies.dna_energies
        }
        
        local result, err = zuker.internal_loop(0, 1, 3, 2, context)
        print("\nDebug invalid loop:")
        print("Sequence:", seq)
        print("Indices:", 0, 1, 3, 2)
        print("Result:", result)
        print("Error:", err)
        assert.equals(0, result)
        assert.matches("internal_loop: the total length of the internal loop at %(0, 3%) is 0", err)
    end)

    -- 1x1 internal loop
    it("should calculate energy for 1x1 internal loop in RNA #only", function()
        local seq = "GCAGGC"
        local context = {
            seq = seq,
            temp = 37.0 + 273.15,
            energies = energies.rna_energies
        }
        local result, err = zuker.internal_loop(0, 2, 5, 3, context)
        print("\nDebug 1x1 RNA loop:")
        print("Sequence:", seq)
        print("Indices:", 0, 2, 5, 3)
        print("Result:", result)
        print("Error:", err)
        print("Expected:", 3.4)
        assert.is_nil(err)
        assert.is_true(math.abs(result - 3.4) < 0.1)
    end)

    -- 2x2 internal loop
    it("should calculate energy for 2x2 internal loop in DNA", function()
        local seq = "GCAAGGC"
        local context = {
            seq = seq,
            temp = 37.0 + 273.15,
            energies = energies.dna_energies
        }
        local result, err = zuker.internal_loop(0, 3, 6, 3, context)
        print("\nDebug 2x2 DNA loop:")
        print("Sequence:", seq)
        print("Indices:", 0, 3, 6, 3)
        print("Result:", result)
        print("Error:", err)
        print("Expected:", 4.1)
        assert.is_nil(err)
        assert.is_true(math.abs(result - 4.1) < 0.1)
    end)

    -- 3x1 internal loop with asymmetry penalty
    it("should add asymmetry penalty for DNA", function()
        local seq = "GCAAAGGC"
        local context = {
            seq = seq,
            temp = 37.0 + 273.15,
            energies = energies.dna_energies
        }
        local result, err = zuker.internal_loop(0, 4, 7, 4, context)
        print("\nDebug 3x1 DNA loop:")
        print("Sequence:", seq)
        print("Indices:", 0, 4, 7, 4)
        print("Result:", result)
        print("Error:", err)
        print("Expected:", 4.7)
        assert.is_nil(err)
        assert.is_true(math.abs(result - 4.7) < 0.1)
    end)

    -- Large internal loop
    it("should calculate energy for large internal loop", function()
        local seq = "GCAAAAAAAGGC"
        local context = {
            seq = seq,
            temp = 37.0 + 273.15,
            energies = energies.dna_energies
        }
        local result, err = zuker.internal_loop(0, 8, 11, 8, context)
        print("\nDebug 7x1 DNA loop:")
        print("Sequence:", seq)
        print("Indices:", 0, 8, 11, 8)
        print("Result:", result)
        print("Error:", err)
        print("Expected:", 5.5)
        print("Pairs used:", seq:sub(1,2), seq:sub(8,9))
        assert.is_nil(err)
        assert.is_true(math.abs(result - 5.5) < 0.1)
    end)
end)

end)
