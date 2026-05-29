# E-Commerce Order Pipeline

A full e-commerce order processing workflow covering inventory checks, fraud detection, payment processing, shipping, and fulfillment with comprehensive error handling at every stage.

## Modules

| Module    | Purpose                                        |
|-----------|------------------------------------------------|
| `http`    | Inventory, fraud, payment, and shipping APIs   |
| `email`   | Customer notifications and internal alerts     |
| `storage` | Invoice and order document archival            |
| `crm`     | Customer order history tracking                |

## Flow Overview

1. **Receive** — Accept new order via webhook; normalize and persist.
2. **Inventory Check** — Loop through all order items and check stock via API.
3. **Availability Branch**:
   - **All available** — Reserve all items.
   - **Partial** — Split into available/backordered; notify customer; reserve available items.
   - **None** — Notify customer; cancel order; stop.
4. **Fraud Check** — Submit order details to fraud detection API.
   - **High risk** — Put on fraud hold, notify fraud team, stop.
   - **Medium risk** — Flag for review, continue.
   - **Low risk** — Continue.
5. **Payment** — Charge customer via payment API.
   - **Success** — Record payment, continue.
   - **Failed** — Notify customer, release inventory reservations, stop.
6. **Shipping** — Fetch rates, select cheapest matching delivery preference, create shipment.
7. **Parallel Finalization** — Send confirmation email, add CRM note, archive invoice.
8. **Complete** — Update order status; emit completion event with tracking info.

## Capabilities Demonstrated

- Multi-step order processing pipeline
- Loop-based inventory checks and reservations
- Three-way branching on stock availability
- Fraud detection with risk-based routing
- Payment processing with rollback on failure (inventory release)
- Shipping rate comparison and selection
- Parallel post-fulfillment actions
- Comprehensive error handling and graceful stops at each failure point
- State persistence throughout the order lifecycle
- Event emission for downstream systems

## Running

```bash
flowcode run order.fcb
```
