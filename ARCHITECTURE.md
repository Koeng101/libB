# biofoundry

**no plan survives contact with the enemy**. This will change over time. 

Biology has a reproducibility problem that's baked into its core. Every lab is different, every researcher has their unique "touch," and the tacit knowledge required to execute even standardized protocols is astronomical. This is not an annoying side effect: it is the central challenge of the biotechnology.

Traditional automation thinks of this problem as fancy APIs on top of specific robots - but they fall apart when the hardware changes, you move labs, or you have something go wrong. In the real world, reproducibility is still a proven problem, even with very expensive robots and teams of people. There are two foundational problems here - the management of inventory and the management of intention - that are extremely difficult to solve, and largely have not been solved.

Management of inventory is the management of physical samples and robots. On a high level, this is simple, but on a low level, this is actually an extremely tricky problem with dramatic implications: who moves the pipette tips? Did they tilt the box on one of the opentrons deck edges, causing the pipette tip shift to catch and pick up the tip box during the protocol, ruining everything? When was the last time we calibrated this robot? How long do I keep competent cells cold before they are thawed? What is the estimated evaporation in the lab for long running experiments? If a plate was left out for a while, do I need to centrifuge it down?

Inventory is a problem that is best solved in a centralized way, which is why I believe cloud labs are the future of biotechnology research. There is no way around solving hardware problems or allocating physical resources. In a centralized facility, we can just figure out all those problems, aggregated across all protocols run within.

On the other hand, management of intention is mostly just how users express what they want to be done. This is almost entirely a software problem. It seems rather simple, until you understand the dynamic requirements of a lab, as well as pragmatic considerations. I concern myself mainly with the practical concerns - software is easy (comparatively speaking), so the first principles become more important. Those questions are approximately: is the software 1. easy to use? 2. reliable? 3. cheap?

To these, I implement a variety of different measures. While making underlying system as consistent, dynamic, and traceable as possible (debugging will be huge), there are also easy ways to express intentions (through the services). I use standard libraries and LLMs to make the very powerful, but somewhat difficult, system to do things for users easily. Reliability is measured through biological positive and negative controls of the services - and they are cheap because we batch users experiments. This will become more apparent when we dive into the actual implementation.

## focus points

The magic of modern software engineering isn't in writing perfect code - it's in composability and abstraction. When you import a library in Python, you don't care what CPU it runs on or which compiler built it. The interface is what matters. Biology desperately needs this same property.

Pure software hermeticity is tempting - hash everything, ensure identical execution environments, maintain perfect reproducibility at the code level. But this approach fails as soon as someone upgrades a robot or changes a reagent supplier. Physical systems aren't hermetic, and pretending they are creates software that works perfectly in theory but fails in practice. Rather, we use the concept of services to address this.

Most of the time in an experiment, you will do several standard procedures before you get to your experimental point. For example, you may want a standard system to synthesize DNA, put it into E.coli, express it, and purify it. Then, with that purified protein, you analyze a particular property (maybe it is binding, maybe it is fluorescence, maybe it is anything else). But for most of the experiment, you are doing standard things that everyone else also wants to do. This actually compromises the majority of many biological workflows; NOT the target output.

For these, we implement services with standard interfaces. For example, if you want to synthesize a gene, it is a whole lot cheaper to synthesize it in a batch with a bunch of other people's genes (as an oligo pool). Instead of people running this individually, we have standard services that, all within our standard programming environment, batch materials up and run procedures with a variety of user's samples. This massively lowers the cost of experiments - aggregating the cost of doing anything across potentially hundreds of users. This economic efficiency is only possible when protocols interact with standardized service interfaces rather than directly controlling robots. As an added bonus, hardware can evolve beneath the service, even if the logic for that changes dramatically.

I balance these concerns. I maintain hermetic guarantees where they matter (in the user's decision logic and data transformations) while embracing standardized interfaces for physical operations. This creates protocols that remain valid even as the underlying hardware evolves.

## high level implementation

The architecture consists of three core components: the execution environment, the inventory system, and the catalog. These are connected through a functional programming interface that enables powerful capabilities like seamless restarts, dynamic recoding, and time-travel debugging.

The execution environment is a sandboxed Lua interpreter with carefully controlled memory and CPU constraints. Protocols are expressed as pure functions that transform input state to output state, with side effects (physical operations) handled through continuations. This functional approach is more than an aesthetic choice - it's what enables the system's most powerful features.

We have overloaded the random number generator of the lua environment, so all protocols execute in exactly the same way, even if you re-run them. This feature is extremely important for everything we will be talking about.

When a protocol needs to interact with the physical world, it returns a continuation that describes what it wants to happen rather than directly controlling hardware. The system handles the actual execution and then restarts the protocol with the updated state. This pattern creates a clean separation between computation and physical operations that enables several superpowers:

1. **Mid-execution pausing**: Protocols can run for days or weeks without holding system resources
2. **Dynamic recoding**: You can modify code while a protocol is running - the system will use the new logic when it resumes.
3. **Rewind capability**: You can roll back to any previous state and try alternative execution paths.
4. **Cross-protocol batching**: Operations from multiple protocols can be combined for economic efficiency.

The inventory system tracks physical objects (plates, wells, reagents) and their associations with digital constructs (plasmids, oligos, strains). The catalog enables protocols to purchase materials when needed, creating a complete closed-loop system where protocols can adapt to resource availability.

## patterns

The system implements several key patterns that make complex protocol development manageable:

**Return pattern**

```lua
function main(ctx, data)
    -- do things
    return 2, "Complete", "next_step", json_cmds, ctx
end
```

Every function the same 5 outputs.
1. Status code
2. Status comment (for displaying to a user)
3. Next function (for what function to execute next)
4. JSON commands (for the lab to execute)
5. context (for data passthrough)

Upon return, the system (physical lab) executes the JSON (representing real robotic actions), then returns the data the robots collect and context to the `next_step` function. It then runs, and so on.

**Continuation pattern**

The core of how the functional system runs protocols.

```lua
function main(ctx)
    result, cont, err = do_something(ctx)
    if cont then return cont
    else if err then return err end

    return result
end
```

In this pattern, the function either returns a result, a continuation, or an error. A continuation basically means that the functional environment has seen that we have not done this step yet, so it returns the `do_something` that we need to do. The system does this, and then re-enters the `main` function. When it does that, we now have completeld that something, so `do_something` returns a result instead of a continuation, and we continue executing the next steps.

**Collect pattern**

A way to implement channel-like concurrency using our existing state machine:

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

**Services Architecture**

A shift from direct robot control to samples flowing through shared processing services:

```lua
return 2, "Queue for processing", "await_sequencing",
       json.encode({
           action = "queue_for_sequencing",
           sample = sample.id,
           requirements = {...}
       }),
       {tracking_id = sample.id}
```

The power comes from how these patterns complement each other. The return pattern forms the foundation of how the entire system operates, the continuation pattern handles direct control when needed, the collect pattern manages concurrent operations, and the services pattern enables economic scaling.


### reasoning

The future of lab automation isn't just about executing predetermined steps - it's about systems that can reason about what they're doing and adapt in real-time. LLMs are already transforming how we write code, and they'll soon transform how we write protocols.

I am designing this with this future in mind. The functional programming model creates perfect snapshots of execution state that an LLM can reason about. When something fails, the system can present the failure context to an LLM, which can diagnose the issue, suggest alternative approaches, or even rewrite portions of the protocol, all live. This is similar to how a lab technician can see something wrong and fix it, on the go, rather than having a strict and linear way of doing things. While less useful for standard interfaces, where we want consistent reproducibility, this can greatly help with user experiments, automatically troubleshooting things that could be going wrong.

Imagine a protocol that encounters low DNA concentration. Instead of blindly continuing or simply failing, it could:

1. Rewind to before the problematic step
2. Have an LLM analyze what went wrong
3. Generate new code that tries a different approach
4. Resume execution with the updated logic

This isn't science fiction - it's a direct consequence of our architecture choices. The combination of function hashing, continuation-based execution, and event sourcing creates a system where every state is traceable and modifiable.

Protocols, in real life, are not just static scripts. Human beings, when tinkering in science, are active reasoning agents that adapt the complex world based off of their observations. There is no reason we can't have machines do that as well, so long as we give them the tools to do so. The system becomes fundamentally so much more useful - instead of being a system that is in the way of you getting your data, it is a partner in trying to get the data you need to prove or disprove a hypothesis.

## inventory

The inventory system bridges the digital and physical worlds through a three-layer model:

1. **Physical Layer**: Concrete objects in the lab - plates, wells, and liquids with precise locations and volumes.
2. **Sample Layer**: Bridges physical objects with their digital representation, handling the reality that physical samples may not perfectly match their digital counterparts.
3. **Bio Construct Layer**: Higher-level digital representations - plasmids, strains, oligos, and genes.

This layering acknowledges that biology has both a digital specification (the DNA sequence or strain genotype) and a physical reality (the actual molecules in a well) that may diverge in subtle ways. The inventory system tracks both, creating a complete picture of what's happening in the lab.

The catalog system extends this model to include purchasable items. When a protocol needs a reagent that isn't available, it can seamlessly purchase it through a continuation, waiting for delivery before resuming. This creates a closed-loop system where protocols can adapt to resource availability instead of failing when something is missing.

## hashing

The Foundry Package Manager combines the benefits of standardized interfaces with the reproducibility guarantees of function hashing. Individual functions - the atomic units of protocol logic - are hashed based on their implementation. These hashes uniquely identify behavior, creating immutable, verifiable components that can be composed into complex protocols.

This approach creates several powerful capabilities:

1. **Function-level versioning**: When implementations improve, protocols can be updated selectively without breaking.
2. **Provenance tracking**: Every executed operation is linked to the exact function implementation that requested it.
3. **Reproducibility guarantees**: You can formally verify that the code executed today is identical to what ran last month.

The package manager supports function extraction, hashing, testing, and composition through commands like `foundry get`, `foundry pull`, `foundry test`, and `foundry compile`. These tools create a development workflow focused on improving tests and function implementations together, with clear boundaries between autogenerated and editable code.

The system is further enhanced by event sourcing, which creates an immutable log of all laboratory actions. This log captures every physical operation, measurement, and state change, creating a complete audit trail that can be used to reconstruct the exact sequence of events or rebuild the entire database state from scratch.

### foundry sum

At the heart of this system is the protocol.sum file - a human and machine-readable manifest that documents every function, its version, hash, and purpose. This isn't just a technical implementation detail; it's a critical interface between developers, LLMs, and the system itself.

The protocol.sum file looks something like this:

```
# Protocol: Colony PCR Verification
# Version: 1.2.3
# Created: 2025-02-24
# Author: labteam
[Dependencies]
bioutils.pcr.setup_reaction v2.1 | 4f7a13d5... | Standard PCR reaction setup 
bioutils.thermocycler.run v1.3 | 8e2c9b14... | Improved temperature ramping
...
```

This simple format serves multiple critical functions. First, it's a human-readable inventory of what your protocol depends on, making it easy to understand dependencies at a glance. Second, it's a machine-verifiable guarantee of reproducibility - the system confirms that each function hash matches what's expected before execution. Third, it's an ideal prompt for LLMs to understand the protocol's structure and dependencies.

When you want to update a dependency, the system doesn't just blindly pull the newest version - it shows you the exact changes and their potential impact on your protocol. This creates conscious, intentional evolution rather than surprise breaks from silent updates. The human or LLM making the update decision has perfect information about what's changing and why.

This manifest approach creates a sweet spot between frozen-in-time reproducibility and continuous improvement. You get cryptographic guarantees about what code is running while maintaining the flexibility to adopt improvements when they make sense for your protocol.

### foundry test
Function testing won't cut it in a biological context. Even if every individual component works perfectly, their interactions - especially across the digital-physical boundary - create endless opportunities for failure. Protocol testing has to be fundamentally different from traditional software testing.

Testing protocols requires two distinct approaches: function-level verification and protocol-level validation. Function testing is straightforward and familiar - unit tests that verify computational logic. Since we have a functional environment, these have stronger guarantees than normal, and easier for LLMs to reason about. Essentially, by maintaining a purely functional environment for small units of logic, we can analyze every edge case in a robust way in a limited context environment.

However, Protocol testing is where things get interesting. A protocol test isn't just checking output values - it's validating a complex dance between digital logic and physical reality that unfolds over time. The protocol_tests directory contains everything needed to verify this dance:

```
protocol_tests/
├── inputs/         # Input data for protocol scenarios
│   ├── low_dna.json
│   ├── contamination.json
│   └── edge_case.json
├── outputs/        # Expected outputs for protocol scenarios
│   ├── low_dna.json
│   ├── contamination.json
│   └── edge_case.json
└── protocol_test.lua  # Test runner for protocol-level tests
```

When you run `foundry test`, the system doesn't just execute unit tests - it simulates the entire protocol execution including continuation points (where the script exits). The test runner injects mock data at each continuation, simulating what would come back from the physical lab after each operation.

These tests handle state complexity in a standard way. Essentially, you can reason about what a lab environment would look like in each scenario, and create simulated code for that particular condition.

For example, a single test might verify:
- What happens if DNA concentration is lower than expected?
- Does the protocol correctly handle contamination?
- How does it respond if a required reagent is unavailable?

These protocol-level tests aren't just about validating logic - they're about ensuring the protocol can navigate the messy reality of biological experimentation. They're essentially executable specifications of scientific intent that verify a protocol will achieve its goals despite the chaos of the physical world.

LLMs have transformed this testing process. Rather than manually coding every test case, you can describe experimental edge cases in natural language, and the system will generate comprehensive test scenarios. This creates test coverage far beyond what a human would typically create, finding edge cases that might otherwise be missed.

## event sourcing

Most lab automation systems are built around the final state - what's in this well right now, where is this plate located. That's fundamentally insufficient. Science requires knowing not just what is, but how it came to be. Event sourcing isn't a nice-to-have feature; it's the only way to create true scientific reproducibility.

Every physical operation, measurement, and state change in the lab is captured as an immutable event. These events form a complete, chronological record of exactly what happened:

```go
type Event struct {
    ID            string    `json:"id"`
    Timestamp     time.Time `json:"timestamp"`
    ProtocolID    string    `json:"protocol_id"`
    FunctionHash  string    `json:"function_hash"`
    EventType     string    `json:"event_type"`
    Payload       []byte    `json:"payload"`
    ActorID       string    `json:"actor_id"`
    PreviousState []byte    `json:"previous_state,omitempty"`
    ResultState   []byte    `json:"result_state,omitempty"`
}
```

Each event captures not just what happened, but which protocol requested it, which function made the decision, who or what performed the action, and the complete before/after state. This isn't just logging - it's a fundamental architectural pattern that completely changes what's possible in a lab automation system.

The event log is the source of truth. The entire database can be rebuilt from scratch by replaying events in sequence. This seems like an implementation detail until you realize what it enables:

1. **Perfect traceability**: You can trace any sample back through its complete history, seeing every operation that created or modified it.
2. **Time travel debugging**: When something goes wrong, you can reconstruct the exact state of the system at any point in time to understand what happened.
3. **Digital twins**: You can fork the event stream to create parallel versions of reality - what would have happened if we had used a different concentration?
4. **Full system verification**: You can periodically rebuild the database from events to verify system integrity and detect corruption.

The event log also creates a natural interface for our LLM reasoning system. When a protocol encounters an unexpected result, the LLM can examine the complete event history leading up to that point, not just the final state. This is closer to how a human scientist would debug - looking at the process, not just the outcome.

Events are categorized by type, creating natural boundaries for reasoning:

- `LIQUID_TRANSFER`: Moving liquid between wells
- `PLATE_CREATION`: Creating new plates
- `SAMPLE_ASSIGNMENT`: Assigning digital constructs to physical wells
- `INCUBATION`: Temperature and time controls
- `MEASUREMENT`: Recorded observations and instrument readings
- `CATALOG_PURCHASE`: Orders for new materials
- `ITEM_RECEIVED`: Delivery of purchased items

This approach creates a system where nothing is ever lost. Even if you delete a sample from the current state, its history remains in the event log. Science requires this level of provenance - the ability to trace results back to their origins through every transformation.

## services

Services are core to the economics and ease of use of our lab. They are implemented with a JSON schema describing the service.

- `/schema` contains the JSON schema
- `/examples` contains example usage
- `/readme` contains a description of the service

For actually using any given service, you have 4 commands available:

- `request` (post) add something to the service
- `query` (get) query something from the service
- `cancel` (del) cancel the service (if it has not run yet)
- `update` (patch) update something about your sample

These map exactly to HTTP api requests - and that is the point. Services ARE just tiny microservices running with our lab, with certain nice guarantees that that brings along (function approach, etc), but they can be equally implemented as 3rd party external services. They can call back into the host system to continue execution of your particular scripts.

Services right now have no way to access a proper database. This might be added in the future, but right now, it just uses the classic data + code -> data pattern with JSON that we have. 

## conclusion

This combination of function hashing and event sourcing creates the strongest possible reproducibility guarantees while acknowledging the reality of evolving hardware. The hashes ensure that computational logic remains consistent, while the standardized interfaces allow implementations to evolve without breaking protocols.

The result is a system where protocols remain valid across hardware generations, can be shared between labs, and create a complete provenance trail from code to physical actions. This is how we finally transform biology from a craft to an engineering discipline - not by eliminating complexity, but by creating abstractions that make it manageable.




# things to add
- user experience
- security control and access
- scaling and performance
- integration with external systems
- regulatory compliance
- evolution

