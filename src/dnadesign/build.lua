local function read_file(path)
    local file = io.open(path, "r")
    if not file then error("Could not open file: " .. path) end
    local content = file:read("*a")
    file:close()
    return content
end

-- Files in dependency order with their module names
local files = {
    { path = "src/hash.tl", module = "hash" },
    { path = "src/transform.tl", module = "transform" },
    { path = "src/align.tl", module = "align" },
    { path = "src/mash.tl", module = "mash" },
	{ path = "src/seqhash.tl", module = "seqhash"},
	{ path = "src/primers.tl", module = "primers"},
	{ path = "src/pcr.tl", module = "pcr"},
	{ path = "src/bio/bio.tl", module = "bio"},
	{ path = "src/bio/fasta.tl", module = "fasta"},
	{ path = "src/bio/fastq.tl", module = "fastq"},
	{ path = "src/bio/pileup.tl", module = "pileup"},
	{ path = "src/bio/sam.tl", module = "sam"},
	{ path = "src/fold/energies.tl", module = "energies"},
	{ path = "src/fold/zuker.tl", module = "zuker"},
	{ path = "src/fragment_frequencies.tl", module = "fragment_frequencies"},
	{ path = "src/fragment.tl", module = "fragment"},
}

local combined = "-- dnadesign.tl\n\n"

-- Add each module's content
for _, file in ipairs(files) do
    local content = read_file(file.path)
    
    -- Comment out require statements
    content = content:gsub('(local%s+[%w_]+%s*=%s*require%(["\'].-["\']%)[^\n]*)', '-- %1')
    
    -- Keep only the local declaration part, remove the return
	content = content:gsub(string.format("(\n)return %s", file.module), "%1")
    
    combined = combined .. content .. "\n\n"
end

-- Add the final return statement with all modules
combined = combined .. "-- Main return table combining all modules\nreturn {\n"
for _, file in ipairs(files) do
    combined = combined .. string.format("    %s = %s,\n", file.module, file.module)
end
combined = combined .. "}\n"

-- Write to combined file
local out_file = io.open("dnadesign.tl", "w")
out_file:write(combined)
out_file:close()

-- Call tl to compile
os.execute("tl check dnadesign.tl")
os.execute("tl gen dnadesign.tl")
os.execute("busted --lua=luajit")
os.execute("rm dnadesign.tl dnadesign.lua")
