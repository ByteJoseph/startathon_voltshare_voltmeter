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
