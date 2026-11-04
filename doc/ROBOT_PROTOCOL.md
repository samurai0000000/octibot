# OCTI Robot Wire Protocol Specification

Copyright (C) 2026, Charles Chiou

This document specifies the wire protocol between OCTI robot clients and an OCTI game server (`octi-server`).

---

## 1. Authentication & Headers

Every request made by a robot client must include the following HTTP headers:

- `Authorization: Bearer octi_bot_<64-hex-token>`:
  Identifies the registered robot token. Tokens are generated and managed by the human owner via the server account portal (`/api/v1/robots`).
- `X-Octi-Client-Type: robot`:
  Explicitly identifies the client as a robot agent.
- `X-Octi-Robot-Lease: <lease_id>`:
  A 32-character hexadecimal lease UUID issued by `POST /api/v1/robots/connect`. Requests bearing an invalid or superseded lease ID will receive `409 Conflict` (`ROBOT_TOKEN_SUPERSEDED`).

---

## 2. Connection Lifecycle & Superseding Leases

### 2.1 Connect Robot: `POST /api/v1/robots/connect`

Establishes an active session for the robot and returns a new session lease.

**Request Payload (optional JSON):**
```json
{
  "if_unused": false
}
```

- When `if_unused: false` (default):
  The server creates a new lease ID and supersedes any previously active lease on this token. Any connection or SSE stream holding the older lease will immediately receive a `ROBOT_TOKEN_SUPERSEDED` error and terminate.
- When `if_unused: true`:
  If a connection or SSE stream currently holds an active lease on this token, the server refuses connection and returns `409 Conflict`:
  ```json
  {
    "error": "ROBOT_TOKEN_IN_USE",
    "message": "Robot token is currently connected with an active lease"
  }
  ```

**Success Response (`200 OK`):**
```json
{
  "lease_id": "a1b2c3d4e5f60718293a4b5c6d7e8f90",
  "robot_uuid": "01234567-89ab-cdef-0123-456789abcdef",
  "handle": "stock",
  "description": "Stock Alpha-Beta bot",
  "owner_uuid": "fedcba98-7654-3210-fedc-ba9876543210",
  "owner_handle": "samurai",
  "active_room_id": "room_xyz123",
  "games_played": 12,
  "wins": 8,
  "losses": 3,
  "draws": 1
}
```

#### Seamless Active Game Reconnect
If the robot was disconnected while seated in an active match (`phase == "IN_PROGRESS"`), the server returns the non-empty `active_room_id`. The robot client must immediately attach to this room, resume listening to game events, reconstruct board state from the current FEN, and continue play without forfeiting or creating a new table.

### 2.2 Disconnect Robot: `POST /api/v1/robots/disconnect`

Gracefully releases the current lease and notifies the lobby.

**Headers:**
- `Authorization: Bearer octi_bot_<token>`
- `X-Octi-Robot-Lease: <lease_id>`

**Success Response (`200 OK`):**
```json
{
  "status": "disconnected"
}
```

### 2.3 Export Configurations: `GET /api/v1/robots/export-conf`

Available to authenticated human owners (`Bearer <human_jwt>`) to export active bot tokens:
```
# Exported OCTI Robot Tokens
bot_token_stock = octi_bot_0123456789abcdef...
bot_token_bot2 = octi_bot_fedcba9876543210...
```

---

## 3. Lobby & Table Interaction

### 3.1 Event Streaming
- `GET /api/v1/lobby/events`: SSE stream of lobby events (player online/offline, table created/updated/closed, lobby chat).
- SSE frames use the standard format:
  ```
  event: message
  data: {"type":"LOBBY_STATE", ...}
  ```

### 3.2 Table Robot Policy (`allow_robots`)
- Each table defines a boolean flag `allow_robots` (default `true`).
- When a human hosts a table with `allow_robots: false`, robots attempting to join (`POST /api/v1/games/:id/join`) receive:
  ```json
  {
    "status": 403,
    "error": "ROBOTS_NOT_ALLOWED",
    "message": "This table does not permit robot players"
  }
  ```

### 3.3 Table Discovery & Matchmaking
- Robots can query open tables via `GET /api/v1/lobby/tables`.
- Robots can enqueue in matchmaking via `POST /api/v1/matchmaking/enqueue` and dequeue via `POST /api/v1/matchmaking/dequeue`.

---

## 4. Live Game Flow

### 4.1 Game Events: `GET /api/v1/games/:id/events`
- SSE stream transmitting board updates, turns, clock increments, proposals, and chat messages.

### 4.2 Move Submission: `POST /api/v1/games/:id/moves`

**Request Payload:**
```json
{
  "move": "c3-c4",
  "ai": {
    "depth": 6,
    "nodes": 45120,
    "score": 140,
    "elapsed_ms": 350
  }
}
```
- The `ai` telemetry block is optional. It is persisted in move history for analysis when telemetry publishing is enabled.

### 4.3 In-Game Chat & Commands
- `POST /api/v1/games/:id/chat`: Send table chat (`ALL` or `TEAM`).
- Proposals:
  - `POST /api/v1/games/:id/proposals/draw`
  - `POST /api/v1/games/:id/proposals/undo`
  - `POST /api/v1/games/:id/proposals/pause`
  - `POST /api/v1/games/:id/proposals/rematch`

---

## 5. Security & Isolation Invariants

1. **Human-Only Elo Rating Invariant**:
   Robots have independent game statistics (`games_played`, `wins`, `losses`, `draws`). Matches involving one or more robots do NOT affect human Elo ratings. Human ratings remain strictly unmodified upon winning or losing against a robot.
2. **Forbidden Human Endpoints (`403 FORBIDDEN_ROBOT`)**:
   Robot tokens are strictly prohibited from invoking human account or sensitive media endpoints:
   - `/api/v1/auth/*`
   - `/api/v1/robots` (except `/api/v1/robots/connect` and `/api/v1/robots/disconnect`)
   - `PATCH /api/v1/profile*`
   - `/api/v1/games/:id/voice-*` (WebRTC signaling)
3. **Anti-Bot Chat Loops**:
   Chat messages where `is_robot: true` or where the sender is the bot itself are ignored by autonomous command parsers to prevent cyclic chat amplification.
