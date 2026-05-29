# Flowcode User Manual

This manual introduces the Flowcode authoring model used by the sample workflows in this repository. It is intended for end users who want to write `.fc` workflow files and run them through the Flowcode compiler/runtime.

## 1. What a Flowcode workflow looks like

A Flowcode file is a plain-text workflow definition built from a small number of blocks:

- `use` statements declare the modules your workflow needs.
- `workflow:` gives the workflow a name.
- `trigger:` defines how the workflow starts.
- `step` blocks perform actions.
- `match`, `loop`, and `parallel` blocks control the path of execution.
- `store`, `await`, and `emit` let you persist state, wait for input, and publish results.

Typical file shape:

```text
use http
use email

workflow: ExampleFlow

trigger:
    webhook "/example/start"
end

step input:
    http.get
        url = "https://example.com/items/{{payload.id}}"
end

step done:
    emit
        value = "example_complete"
        metadata = input
end

end
```

## 2. Modules

Use one `use` line per integration or capability:

```text
use http
use email
use form
use ai
use storage
```

The samples in this repository use modules such as:

- `http`
- `email`
- `form`
- `ai`
- `storage`
- `crm`
- `mqtt`
- `memory`

Declare only the modules your workflow actually uses.

## 3. Workflow name

Each file should define one workflow:

```text
workflow: CustomerOnboarding
```

Use a descriptive name that explains the business process, not the implementation detail.

## 4. Triggers

The `trigger:` block defines how execution starts.

### Webhook trigger

Use a webhook when an external system starts the flow:

```text
trigger:
    webhook "/orders/create"
end
```

### Scheduled trigger

Use `cron` for recurring automation:

```text
trigger:
    cron "*/15 * * * *"
end
```

Choose triggers based on how the business event enters the workflow.

## 5. Steps

A `step` is a named unit of work:

```text
step payment:
    http.post
        url = "https://payments.example.com/charge"
        body = order
end
```

Guidelines:

- Give each step a business-friendly name.
- Keep one main responsibility per step.
- Reuse earlier step outputs instead of duplicating data.

Step results are referenced later by the step name, such as `payment`, `payment.status`, or `order.id`.

## 6. Assignments and values

Most actions take indented `key = value` assignments.

Common value forms used in the samples:

- Strings: `"hello"`
- Numbers: `1000`
- Booleans: `true`, `false`
- Objects:

  ```text
  {
      request = request
      tier = tier
  }
  ```

- Lists:

  ```text
  [
      { max = 1000, approver = "team_lead" },
      { max = -1, approver = "cfo" }
  ]
  ```

## 7. Referencing data

Flowcode examples commonly reference data in three ways:

- Direct step output: `validated`
- Nested fields: `validated.email`
- Templated interpolation: `{{request.id}}`

Use interpolation inside strings and keys when you need runtime values:

```text
key = "approval.{{request.id}}.status"
subject = "Approval Required: {{request.title}}"
```

## 8. Common action patterns

### HTTP calls

```text
http.get
    url = system.api_url
    headers = {
        Authorization = "******"
    }
```

```text
http.post
    url = "{{system.api_url}}/records"
    body = record
```

### Transforms

Use `transform` when data needs normalization, validation, routing, or reshaping:

```text
transform normalize.order
    input = payload
end
```

Examples from the samples include:

- validation
- routing decisions
- conflict resolution
- field mapping
- combining outputs

### State store

Persist workflow state with `store`:

```text
store set
    key = "orders.{{order.id}}"
    value = order
end
```

Read state later with:

```text
store get
    key = "orders.{{order.id}}"
end
```

Use `ttl` when saved data should expire automatically:

```text
ttl = slaDeadline.remaining_seconds
```

### Human input

Render a form and wait for submission:

```text
form.render
    form_id = "approval_decision"
    data = request
end
```

```text
await form.submit
    form_id = "approval_decision"
    timeout = slaDeadline.remaining_seconds
end
```

### Events

Publish the final result with `emit`:

```text
emit
    value = "order_complete"
    metadata = shipment
end
```

## 9. Control flow

### Branching with `match`

Use `match` to route on status, risk, booleans, or response codes:

```text
match decision.status
    approved ->
        step approvedData:
            transform merge
                original = validated
                corrections = reviewed.corrections
        end

    rejected ->
        stop
end
```

Patterns used in the repository:

- two-way business decisions (`approved` / `rejected`)
- numeric status codes (`200`, `304`)
- boolean routing (`true`, `false`)
- fallback branches with `else`

### Loops

Use `loop` when the same action must run for each item:

```text
step applyCreates:
    loop record in diff.created
        http.post
            url = "{{system.api_url}}/records"
            body = record
    end
end
```

Use `continue` inside a loop when the current item should be skipped and processing should move to the next one.

### Parallel work

Use `parallel:` when several independent actions can happen together:

```text
parallel:

    step emailSent:
        email.send
            to = customer.email
            subject = "Done"
            body = "Your workflow is complete"
    end

    step archived:
        storage.upload
            bucket = "workflow-results"
            file = result
    end

end
```

Parallel blocks are useful for notifications, archival, CRM updates, and other independent finishing tasks.

### Stopping early

Use `stop` when the workflow should end immediately after a terminal outcome, such as a rejection or unrecoverable failure.

## 10. Authoring guidelines

- Start with the business event, then define the trigger.
- Name steps after outcomes or responsibilities, not internal mechanics.
- Keep transformation, notification, persistence, and branching concerns separate.
- Save important business state to the store so later steps can resume or audit work.
- Use `parallel` only when the branches do not depend on one another.
- Use `match` for decisions, not for sequencing.

## 11. Recommended workflow for writing Flowcode

1. Pick a sample in `/samples` that is closest to your use case.
2. Copy its overall shape into a new `.fc` file.
3. Replace module calls, keys, and field names with your own business terms.
4. Compile the workflow:

   ```bash
   node compiler/index.ts path/to/workflow.fc path/to/workflow.fcb
   ```

5. Run the compiled bytecode:

   ```bash
   ./flowcode run path/to/workflow.fcb
   ```

6. Refine step names, branches, and stored keys until the flow reads clearly.

## 12. Repository scope note

The repository currently contains a minimal compiler/runtime scaffold plus richer sample workflows. The samples are the best reference for authoring style and workflow structure, while the compiler/runtime remains an MVP foundation.
