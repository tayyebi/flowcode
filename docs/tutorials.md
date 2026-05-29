# Flowcode Tutorials

These tutorials show how to write Flowcode by studying and adapting the sample workflows in `/samples`.

## How to use these tutorials

For each tutorial:

1. Read the business goal.
2. Inspect the linked sample workflow.
3. Follow the listed authoring steps in your own `.fc` file.
4. Compile with `node compiler/index.ts`.
5. Run with `./flowcode run`.

## Tutorial 1: Build a customer onboarding flow

**Sample:** `/tmp/workspace/tayyebi/flowcode/samples/customer-onboarding/onboarding.fc`

### Goal

Create a workflow that accepts an onboarding request, extracts customer data, gets human review, and completes the onboarding process.

### What you will learn

- Declaring modules with `use`
- Starting a workflow with a webhook
- Calling AI, form, email, storage, and CRM steps
- Branching on approval status
- Uploading multiple files with `loop`
- Running finishing steps in parallel
- Emitting a completion event

### Build it

1. Add the modules you need: `http`, `email`, `form`, `ai`, `crm`, and `storage`.
2. Name the workflow after the business process.
3. Add a `webhook` trigger for the onboarding start event.
4. Create a step to fetch the uploaded source document.
5. Create a step to extract structured profile data from that document.
6. Validate the extracted data with a `transform`.
7. Render a review form and then wait for its submission.
8. Add a `match reviewed.status` block with:
   - an `approved` branch that merges reviewer corrections
   - a `rejected` branch that sends an email and stops
9. Add a `loop` that uploads each validated document to storage.
10. Add a `parallel` block to create the CRM record and send the welcome email.
11. Combine the results, store them, and finish with `emit`.

### Why this tutorial matters

This is the best starting point if you want to learn the general Flowcode structure for human-in-the-loop automation.

## Tutorial 2: Build an approval workflow with SLAs

**Sample:** `/tmp/workspace/tayyebi/flowcode/samples/approval-chain-escalation/approval.fc`

### Goal

Create a workflow that routes approvals by amount, waits for a human decision, and escalates when the SLA expires.

### What you will learn

- Using `store set` and `store get`
- Calculating deadlines with `transform`
- Routing with nested `match` blocks
- Waiting for human input with `await form.submit`
- Using `timeout`
- Escalating after SLA breach
- Finalizing work in parallel

### Build it

1. Start with modules for request intake, notification, forms, storage, and CRM.
2. Accept incoming approval requests with a webhook trigger.
3. Normalize and store the request immediately.
4. Compute the SLA deadline and save it with `ttl`.
5. Resolve the correct approver tier from request fields such as amount and department.
6. Notify the approver and render the decision form.
7. Wait for the form submission with a timeout equal to the remaining SLA window.
8. Add a `match decision.status` block for:
   - `approved`
   - `rejected`
   - `timeout`
9. In the approved path, optionally branch again if another tier is required.
10. In the timeout path, resolve the escalation target, notify them, and collect an escalation decision.
11. Finish with parallel audit logging, CRM update, and requester notification.
12. Emit a final approval event.

### Why this tutorial matters

Use this pattern when your workflow must combine persistence, human decisions, deadlines, and escalation rules.

## Tutorial 3: Build a scheduled SaaS sync workflow

**Sample:** `/tmp/workspace/tayyebi/flowcode/samples/saas-data-sync-engine/sync.fc`

### Goal

Create a scheduled synchronization job that fetches remote data, handles conflicts, writes updates back, and records the outcome.

### What you will learn

- Using a `cron` trigger
- Iterating over systems with `loop`
- Handling API responses with `match`
- Skipping to the next item with `continue`
- Applying create, update, and delete actions
- Mapping data between systems
- Writing summary logs and heartbeat events

### Build it

1. Use a `cron` trigger for the sync interval.
2. Load the sync configuration from the state store.
3. Create a loop over the configured systems.
4. For each system, fetch the last sync timestamp and then call the remote API.
5. Match on the HTTP status code:
   - handle `200` as a normal update path
   - handle `304` as no-op and `continue`
   - handle other responses as errors and `continue`
6. Compute the difference between local and remote data.
7. If there are no changes, store that fact and continue.
8. Detect conflicts and branch by conflict strategy.
9. Apply creates, updates, and deletes with separate loop-driven steps.
10. Save the updated local snapshot and sync log.
11. Add a second loop for cross-system propagation.
12. Finish with a `parallel` block that archives a summary and sends a monitoring heartbeat.
13. Emit a cycle completion event.

### Why this tutorial matters

This tutorial is the clearest example of Flowcode for batch jobs, loop-heavy logic, and resilient API-driven automation.

## Suggested learning path

1. Start with the onboarding tutorial for the basic workflow shape.
2. Move to the approval tutorial for state, timeouts, and nested branching.
3. Finish with the sync tutorial for loops, continue semantics, and scheduled jobs.

## Next samples to study

After these tutorials, explore:

- `/tmp/workspace/tayyebi/flowcode/samples/ecommerce-order-pipeline/order.fc`
- `/tmp/workspace/tayyebi/flowcode/samples/iot-streaming-automation/streaming.fc`
- `/tmp/workspace/tayyebi/flowcode/samples/multi-agent-content-pipeline/pipeline.fc`

They add more patterns for risk routing, streaming automation, AI orchestration, and multi-system workflows.
