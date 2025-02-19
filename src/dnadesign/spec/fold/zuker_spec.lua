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
end)
