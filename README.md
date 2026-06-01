# CyberFinance — Portfolio Management System

A full-stack investment portfolio tracker with a cyberpunk aesthetic. The frontend is built with React + TypeScript; the backend is a raw HTTP server written in C, backed by SQLite3.

---

## Tech Stack

| Layer | Technology |
|---|---|
| Frontend | React 18, TypeScript, Vite |
| UI | shadcn/ui, Tailwind CSS, Recharts |
| State | TanStack React Query |
| Backend | C (POSIX sockets, pthreads) |
| Database | SQLite3 |
| Auth | Session tokens via HTTP cookies |

---

## Features

- **Per-user authentication** — register, login, logout with secure session cookies
- **Portfolio dashboard** — total value, P/L, asset count, performance chart, allocation pie chart
- **Buy / Sell** — trade from a predefined market list; backend updates asset quantities atomically
- **Transaction history** — full per-user ledger, ordered by most recent
- **Data isolation** — every API endpoint is scoped to the authenticated user; no data leaks between accounts
- **Graceful fallback** — frontend degrades to mock data only when the backend is truly unreachable

---

## Project Structure

```
.
├── src/                        # React frontend
│   ├── components/
│   │   ├── ui/                 # shadcn/ui primitives
│   │   └── TradeModal.tsx      # Buy / Sell modal
│   ├── hooks/
│   │   └── usePortfolio.ts     # React Query hooks
│   ├── lib/
│   │   ├── api.ts              # REST client
│   │   └── types.ts            # Shared TypeScript types
│   └── App.tsx                 # Auth context + routing
├── backend/
│   ├── src/
│   │   ├── main.c              # Server entry point (port 8080)
│   │   ├── server.c            # HTTP router + session auth
│   │   ├── db.c                # SQLite layer (users, assets, transactions, sessions)
│   │   └── db.h
│   ├── data/
│   │   └── portfolio.db        # SQLite database (auto-created)
│   └── Makefile
├── .vscode/
│   └── c_cpp_properties.json   # IntelliSense config (WSL)
└── package.json
```

---

## Getting Started

### Prerequisites

- **Node.js** (v18+) for the frontend
- **WSL2 with Ubuntu** for the C backend
- Inside WSL: `sudo apt install gcc libsqlite3-dev`

---

### 1. Backend

```bash
# Open WSL and navigate to the backend directory
cd /mnt/c/path/to/CBackend/backend

# Compile
make

# Run (listens on port 8080)
./backend
```

To stop the server press `Ctrl+C`.

---

### 2. Frontend

```bash
# In your Windows terminal, from the project root
npm install       # first time only
npm run dev       # starts Vite on http://localhost:8081
```

Open [http://localhost:8081](http://localhost:8081) in your browser.

---

## API Reference

All authenticated endpoints require a valid `session_token` cookie set by `/api/login` or `/api/signup`.

| Method | Endpoint | Auth | Description |
|---|---|---|---|
| `GET` | `/api/health` | No | Server health check |
| `POST` | `/api/signup` | No | Register + auto-login |
| `POST` | `/api/login` | No | Authenticate, set session cookie |
| `POST` | `/api/logout` | No | Clear session cookie |
| `GET` | `/api/me` | Yes | Current user info |
| `GET` | `/api/assets` | Yes | List portfolio assets |
| `POST` | `/api/assets` | Yes | Add an asset |
| `GET` | `/api/transactions` | Yes | List transactions |
| `POST` | `/api/transactions` | Yes | Create buy/sell transaction |
| `GET` | `/api/portfolio/chart` | Yes | Portfolio value over time |
| `GET` | `/api/portfolio/allocation` | Yes | Asset allocation breakdown |

---

## Database Schema

```sql
users        (id, email, username, password, created_at)
sessions     (token PK, user_id FK, created_at)
assets       (id, user_id FK, symbol, name, category, quantity, avg_cost, current_price)
transactions (id, user_id FK, type, asset, amount, price, total, status, timestamp)
```

---

## Development Notes

- The backend binary is a Linux ELF — it must be compiled and run inside WSL on Windows.
- VS Code IntelliSense for the C files works fully when the project is opened via the **Remote - WSL** extension (`code .` from a WSL terminal).
- The frontend proxies all API calls to `http://localhost:8080`. WSL2 automatically forwards this port from Windows to the WSL network.

---

## License

MIT
