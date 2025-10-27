# High-Frequency Trading (HFT)

## Project Overview

**HFT Trading Dashboard** is a real-time high-frequency trading (HFT) simulation built with HTML, CSS, and JavaScript. It demonstrates professional trading concepts like market data monitoring, algorithmic trading strategies, risk management, and order execution. This project mimics the behavior of a professional HFT system and provides a visually interactive dashboard for monitoring trades, techniques, and performance metrics.

**Why it’s worthy:**

* Demonstrates understanding of real-time data processing and trading logic.
* Showcases UI/UX design for financial dashboards.
* Includes modular design simulating algorithmic trading strategies and risk management.

---

## Features

### 1. Real-Time Market Data

* Simulates live market prices with random-walk volatility.
* Displays bid/ask orders and order book depth.
* Visualizes price movements via candlestick chart.
* Shows live latency to simulate an HFT environment.

### 2. Trading Strategies

* **Market Making:** Provides liquidity by placing buy/sell orders around the market price.
* **Arbitrage:** Exploits price differences for profit.
* **Momentum Trading:** Follows market trends to take positions.
* **Mean Reversion:** Identifies price corrections and trades accordingly.
* Each strategy shows real-time P&L and can be toggled on/off.

### 3. Portfolio & Performance Metrics

* Tracks **Total P&L**, **Win Rate**, **Daily Volume**, and **Orders per Minute**.
* Simulates portfolio updates based on executed trades.

### 4. Activity Log

* Records all trading actions with timestamps.
* Color-coded log for BUY (green) and SELL (red) trades.
* Maintains a history of the last 50 activities for quick reference.

### 5. User Controls

* Start, Stop, and Reset the trading engine.
* Activate/deactivate individual strategies via UI.
* Dynamic updates of market data, metrics, and order book.

---

## Workflow

1. **Start Engine:** Initializes the HFT engine simulation.
2. **Market Data Feed:** Generates real-time price updates and bid/ask spreads.
3. **Strategy Execution:** Active strategies continuously monitor the market and execute trades.
4. **Order Book Updates:** Simulates orders and updates bid/ask display.
5. **Portfolio & Metrics Update:** P&L, win rate, volume, and order counts are updated live.
6. **Activity Logging:** All trades and strategy activations are logged for review.

---

## System Design

### Frontend

* **HTML/CSS:** Grid-based layout, glassmorphism design, and responsive panels.
* **JavaScript:** Engine simulates HFT logic, strategies, order book updates, and performance metrics.

### Components

```
┌─────────────────┐
│  Market Data    │
│  Feed           │
└─────────────────┘
         │
         ▼
┌─────────────────┐
│ Strategy Engine │
│ (Active/Inactive) │
└─────────────────┘
         │
         ▼
┌─────────────────┐
│ Order Book      │
│ Updates & UI    │
└─────────────────┘
         │
         ▼
┌─────────────────┐
│ Portfolio &     │
│ Metrics         │
└─────────────────┘
         │
         ▼
┌─────────────────┐
│ Activity Log    │
└─────────────────┘
```

### Key Concepts

* **Real-Time Simulation:** Prices, orders, and P&L update dynamically.
* **Strategy Modularity:** Each trading strategy is isolated and can be activated/deactivated.
* **Risk Awareness:** Simulated P&L and portfolio updates track performance.
* **UI Interactivity:** Dynamic charts, metrics, and activity logs enhance visualization.

---

## Technical Stack

* **Frontend:** HTML, CSS (glassmorphism design), JavaScript (ES6).
* **Simulation Logic:** JavaScript classes mimic HFT engine operations.
* **Data Visualization:** Candlestick charts for price movements, dynamic order book display.
* **Performance Simulation:** Latency and market updates mimic an HFT environment.

---

## Learning Outcomes

* Built a **real-time trading dashboard** with interactive UI and live metrics.
* Implemented **algorithmic trading strategies** and portfolio simulation.
* Developed **modular, maintainable JavaScript code** simulating multi-component systems.
* Gained exposure to **financial market microstructure and HFT principles**.
* Experience in **UI/UX design for professional dashboards**.

---

## Future Enhancements

* Connect to **real market APIs** (like Binance, Coinbase).
* Add **more complex strategies**: news-based, AI/ML-based trading.
* Implement **alerts and notifications** for strategy signals.
* Multi-asset support for cryptocurrencies, stocks, and forex.

