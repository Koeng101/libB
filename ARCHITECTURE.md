Let me summarize these three key patterns we've developed:

1. Pipette Control Pattern
A clean way to handle rapid back-and-forth operations while maintaining our state machine model. Instead of many separate functions or complex subscripts, we use a simple control flow:

```lua
function pipette(data_passthrough, step_name, command)
    if not data_passthrough.completed_steps[step_name] then
        result, cont, err = command()
        if cont then return cont
        else if err then return err end
    end
    return result
end
```

This lets protocols read naturally while still breaking at appropriate points:
```lua
result, cont, err = pipette(data_passthrough, "transfer_1", command)
if cont then return cont end
if err then return err end
```

2. Collect Pattern
Instead of creating new concurrency primitives, we implemented channel-like patterns using just:
- Our existing state machine
- A function that runs on any new data
- Persistent state in data_passthrough

This let us build patterns like buffered channels, fan-out/fan-in, pipelines, and timeouts all using the same basic mechanism:
```lua
function collect_any(DATA, data_passthrough)
    -- Process any new data
    for script_id, result in pairs(DATA) do
        data_passthrough.received[script_id] = result
    end
    
    -- Either continue collecting or move to next stage
    if all_done(data_passthrough) then
        return 0, "Complete", "next_step", "", ""
    end
    return 2, "Waiting", "collect_any", "", data_passthrough
end
```

3. Services Architecture
A shift in thinking about protocols - instead of focusing on direct robot control, we think about samples flowing through shared processing services:
- Samples can be queued for batch processing
- Multiple users' protocols feed into shared resources
- Economic scaling through batching
- Standard library becomes about service composition

```lua
return 2, "Queue for processing", "await_sequencing",
       json.encode({
           action = "queue_for_sequencing",
           sample = sample.id,
           requirements = {...}
       }),
       {tracking_id = sample.id}
```

The power comes from how these patterns complement each other:
- Pipette pattern for direct robot control when needed
- Collect pattern for managing concurrent operations
- Services pattern for scalable, economic protocol design

Each pattern solves a different aspect of protocol development while working within our basic state machine model.
