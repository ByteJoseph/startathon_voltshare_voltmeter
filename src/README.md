# API Documentation

Base URL: `http://localhost:<PORT>`

---

## Root

### `GET /`

Returns a welcome message.

**Response:**

```json
{
  "message": "Welcome to the API"
}
```

---

## Health

### `GET /health`

Returns the health status of the API.

**Response:**

```json
{
  "status": "ok",
  "timestamp": "2026-09-05T12:00:00.000Z"
}
```

---

## Button

### `GET /button/status`

Returns the current state of the button. The state defaults to `"off"` on server start.

Optionally pass a query parameter to switch the state.

**Query Parameters:**

| Parameter | Type   | Description                        |
|-----------|--------|------------------------------------|
| `on`      | string | Set to `"true"` to turn the button on  |
| `off`     | string | Set to `"true"` to turn the button off |

**Examples:**

```
GET /button/status
GET /button/status?on=true
GET /button/status?off=true
```

**Response:**

```json
{
  "status": "on"
}
```

```json
{
  "status": "off"
}
```

---

## Meter

### `GET /meter/producer`

Generates simulated producer-side (generation) meter data.

**Response:**

```json
{
  "generatedPower": 2500.00,
  "exportedEnergy": 120.50,
  "voltage": 240.00,
  "frequency": 50.005,
  "active": true,
  "_debug_marker": "producer"
}
```

### `GET /meter/consumer`

Generates simulated consumer-side (load) meter data.

**Response:**

```json
{
  "consumedPower": 1500.00,
  "importedEnergy": 80.25,
  "voltage": 235.00,
  "powerFactor": 0.850,
  "loadStatus": "normal",
  "_debug_marker": "consumer"
}
```

---

## Meter Metrics

### `GET /meter-metrics/producer`

Generates random producer-side metrics (generation).

**Response:**

```json
{
  "power": 3000.00,
  "energy": 200.00,
  "voltage": 250.00,
  "powerFactor": 0.95,
  "meter": "producer"
}
```

### `GET /meter-metrics/consumer`

Generates random consumer-side metrics (load).

**Response:**

```json
{
  "power": 1500.00,
  "energy": 100.00,
  "voltage": 240.00,
  "powerFactor": 0.80,
  "meter": "consumer"
}
```

---

## Consumer Purchase

### `POST /consumer-purchase`

Stores a consumer purchase kWh value temporarily in memory. The value is cleared after being retrieved via `GET`.

**Request Body:**

```json
{
  "kwh": 12.5
}
```

**Validation:**

- `kwh` is required
- Must be a non-negative number

**Success Response (200):**

```json
{
  "message": "Consumer purchase kWh stored",
  "kwh": 12.5,
  "storedAt": "2026-09-05T12:00:00.000Z"
}
```

**Error Response (400):**

```json
{
  "error": "Missing kwh value in request body",
  "example": { "kwh": 12.5 }
}
```

### `GET /consumer-purchase`

Retrieves the stored consumer purchase kWh value and then auto-clears it. Returns 404 if no value has been stored yet.

**Success Response (200):**

```json
{
  "kwh": 12.5,
  "storedAt": "2026-09-05T12:00:00.000Z",
  "servedAt": "2026-09-05T12:00:30.000Z"
}
```

**Error Response (404):**

```json
{
  "message": "No stored consumer purchase found",
  "note": "Store one first via POST /consumer-purchase"
}
```
