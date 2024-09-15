#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>
#include <atomic>
#include <chrono>
#include <random>
#include <memory>
#include <map>
#include <string>
#include <iomanip>
#include <algorithm>
#include <cstdlib>

// Forward declarations
struct MarketData;
struct Order;
class OrderBook;
class TradingStrategy;
class RiskManager;
class MarketDataFeed;
class OrderManager;
class HFTEngine;


enum class OrderType { BUY, SELL };
enum class OrderStatus { PENDING, FILLED, CANCELLED };
enum class StrategyType { MARKET_MAKING, ARBITRAGE, MOMENTUM, MEAN_REVERSION };

// Market Data Structure
struct MarketData {
    std::string symbol;
    double price;
    double volume;
    double bid;
    double ask;
    double spread;
    std::chrono::high_resolution_clock::time_point timestamp;
    
    MarketData(const std::string& sym, double p, double v, double b, double a) 
        : symbol(sym), price(p), volume(v), bid(b), ask(a), 
          spread(a - b), timestamp(std::chrono::high_resolution_clock::now()) {}
};

// Order Structure
struct Order {
    uint64_t id;
    std::string symbol;
    OrderType type;
    double price;
    double quantity;
    OrderStatus status;
    std::chrono::high_resolution_clock::time_point timestamp;
    StrategyType strategy;
    
    Order(uint64_t oid, const std::string& sym, OrderType t, double p, double q, StrategyType st)
        : id(oid), symbol(sym), type(t), price(p), quantity(q), 
          status(OrderStatus::PENDING), timestamp(std::chrono::high_resolution_clock::now()),
          strategy(st) {}
};

// Thread-Safe Queue Template
template<typename T>
class ThreadSafeQueue {
private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable condition_;

public:
    void push(const T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(item);
        condition_.notify_one();
    }

    bool pop(T& item, const std::chrono::milliseconds& timeout = std::chrono::milliseconds(100)) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (condition_.wait_for(lock, timeout, [this] { return !queue_.empty(); })) {
            item = queue_.front();
            queue_.pop();
            return true;
        }
        return false;
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }
};

// Order Book Class
class OrderBook {
private:
    std::map<double, double> bids_;  // price -> quantity
    std::map<double, double> asks_;  // price -> quantity
    mutable std::mutex mutex_;

public:
    void updateBid(double price, double quantity) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (quantity > 0) {
            bids_[price] = quantity;
        } else {
            bids_.erase(price);
        }
    }

    void updateAsk(double price, double quantity) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (quantity > 0) {
            asks_[price] = quantity;
        } else {
            asks_.erase(price);
        }
    }

    std::pair<double, double> getBestBidAsk() const {
        std::lock_guard<std::mutex> lock(mutex_);
        double bestBid = bids_.empty() ? 0.0 : bids_.rbegin()->first;
        double bestAsk = asks_.empty() ? 0.0 : asks_.begin()->first;
        return {bestBid, bestAsk};
    }

    double getSpread() const {
        auto [bid, ask] = getBestBidAsk();
        return (bid > 0 && ask > 0) ? ask - bid : 0.0;
    }

    void printOrderBook(int depth = 5) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::cout << "\n=== ORDER BOOK ===" << std::endl;
        std::cout << "ASK | Price  | Size" << std::endl;
        
        int count = 0;
        for (auto it = asks_.begin(); it != asks_.end() && count < depth; ++it, ++count) {
            std::cout << "    | " << std::fixed << std::setprecision(2) 
                     << it->first << " | " << it->second << std::endl;
        }
        
        std::cout << "----+--------+-----" << std::endl;
        
        count = 0;
        for (auto it = bids_.rbegin(); it != bids_.rend() && count < depth; ++it, ++count) {
            std::cout << "BID | " << std::fixed << std::setprecision(2) 
                     << it->first << " | " << it->second << std::endl;
        }
        std::cout << "=================" << std::endl;
    }
};

// Base Trading Strategy
class TradingStrategy {
protected:
    StrategyType type_;
    std::atomic<bool> active_;
    std::atomic<double> pnl_;
    std::atomic<int> trade_count_;

public:
    TradingStrategy(StrategyType type) : type_(type), active_(true), pnl_(0.0), trade_count_(0) {}
    virtual ~TradingStrategy() = default;

    virtual std::vector<Order> generateSignals(const MarketData& data, const OrderBook& orderBook) = 0;
    
    void setActive(bool active) { active_ = active; }
    bool isActive() const { return active_; }
    double getPnL() const { return pnl_; }
    int getTradeCount() const { return trade_count_; }
    StrategyType getType() const { return type_; }

    virtual std::string getName() const = 0;

protected:
    static uint64_t getNextOrderId() {
        static std::atomic<uint64_t> orderId{1};
        return orderId++;
    }

    void updatePnL(double profit) {
        double current = pnl_.load();
        double new_pnl;
        do {
            current = pnl_.load();
            new_pnl = current + profit;
        } while (!pnl_.compare_exchange_weak(current, new_pnl));
        trade_count_++;
    }
};

// Market Making Strategy
class MarketMakingStrategy : public TradingStrategy {
private:
    double spread_threshold_;
    double position_limit_;

public:
    MarketMakingStrategy(double spread_thresh = 0.02, double pos_limit = 1000.0) 
        : TradingStrategy(StrategyType::MARKET_MAKING), 
          spread_threshold_(spread_thresh), position_limit_(pos_limit) {}

    std::string getName() const override { return "Market Making"; }

    std::vector<Order> generateSignals(const MarketData& data, const OrderBook& orderBook) override {
        std::vector<Order> orders;
        if (!active_) return orders;

        auto [bestBid, bestAsk] = orderBook.getBestBidAsk();
        double currentSpread = bestAsk - bestBid;

        if (currentSpread > spread_threshold_) {
            // Place buy order slightly above best bid
            double buyPrice = bestBid + 0.01;
            orders.emplace_back(getNextOrderId(), data.symbol, OrderType::BUY, 
                              buyPrice, 10.0, StrategyType::MARKET_MAKING);

            // Place sell order slightly below best ask  
            double sellPrice = bestAsk - 0.01;
            orders.emplace_back(getNextOrderId(), data.symbol, OrderType::SELL, 
                               sellPrice, 10.0, StrategyType::MARKET_MAKING);

            updatePnL((sellPrice - buyPrice) * 10.0);
        }

        return orders;
    }
};

// Arbitrage Strategy
class ArbitrageStrategy : public TradingStrategy {
private:
    double min_profit_threshold_;

public:
    ArbitrageStrategy(double min_profit = 0.05) 
        : TradingStrategy(StrategyType::ARBITRAGE), min_profit_threshold_(min_profit) {}

    std::string getName() const override { return "Arbitrage"; }

    std::vector<Order> generateSignals(const MarketData& data, const OrderBook& orderBook) override {
        std::vector<Order> orders;
        if (!active_) return orders;

        static double lastPrice = data.price;
        double priceMove = std::abs(data.price - lastPrice);
        
        if (priceMove > min_profit_threshold_) {
            if (data.price > lastPrice) {
                orders.emplace_back(getNextOrderId(), data.symbol, OrderType::SELL, 
                                  data.price, 5.0, StrategyType::ARBITRAGE);
                updatePnL(priceMove * 5.0);
            } else {
                // Price decreased, buy low  
                orders.emplace_back(getNextOrderId(), data.symbol, OrderType::BUY, 
                                  data.price, 5.0, StrategyType::ARBITRAGE);
                updatePnL(priceMove * 5.0);
            }
        }
        
        lastPrice = data.price;
        return orders;
    }
};

// Risk Manager
class RiskManager {
private:
    std::atomic<double> max_position_;
    std::atomic<double> current_position_;
    std::atomic<double> daily_loss_limit_;
    std::atomic<double> current_pnl_;

public:
    RiskManager(double max_pos = 10000.0, double loss_limit = -5000.0) 
        : max_position_(max_pos), current_position_(0.0), 
          daily_loss_limit_(loss_limit), current_pnl_(0.0) {}

    bool checkOrder(const Order& order) {
        double potential_position = current_position_.load();
        if (order.type == OrderType::BUY) {
            potential_position += order.quantity;
        } else {
            potential_position -= order.quantity;
        }

        if (std::abs(potential_position) > max_position_.load()) {
            return false;
        }

        if (current_pnl_.load() < daily_loss_limit_.load()) {
            return false;
        }

        return true;
    }

    void updatePosition(const Order& order) {
        double current = current_position_.load();
        double new_position;
        if (order.type == OrderType::BUY) {
            do {
                current = current_position_.load();
                new_position = current + order.quantity;
            } while (!current_position_.compare_exchange_weak(current, new_position));
        } else {
            do {
                current = current_position_.load();
                new_position = current - order.quantity;
            } while (!current_position_.compare_exchange_weak(current, new_position));
        }
    }

    void updatePnL(double pnl) {
        double current = current_pnl_.load();
        double new_pnl;
        do {
            current = current_pnl_.load();
            new_pnl = current + pnl;
        } while (!current_pnl_.compare_exchange_weak(current, new_pnl));
    }

    double getCurrentPosition() const { return current_position_; }
    double getCurrentPnL() const { return current_pnl_; }
};

// Market Data Feed
class MarketDataFeed {
private:
    std::atomic<bool> running_;
    std::thread feed_thread_;
    ThreadSafeQueue<MarketData>& data_queue_;
    std::mt19937 rng_;
    std::uniform_real_distribution<double> price_dist_;
    double base_price_;

public:
    MarketDataFeed(ThreadSafeQueue<MarketData>& queue) 
        : running_(false), data_queue_(queue), rng_(std::random_device{}()), 
          price_dist_(-0.01, 0.01), base_price_(50000.0) {}

    void start() {
        running_ = true;
        feed_thread_ = std::thread(&MarketDataFeed::feedLoop, this);
    }

    void stop() {
        running_ = false;
        if (feed_thread_.joinable()) {
            feed_thread_.join();
        }
    }

private:
    void feedLoop() {
        while (running_) {
            // Generate random price movement
            double price_change = price_dist_(rng_);
            base_price_ *= (1.0 + price_change);
            
            double volume = 100 + (rng_() % 1000);
            double bid = base_price_ - 0.05;
            double ask = base_price_ + 0.05;

            MarketData data("BTC/USD", base_price_, volume, bid, ask);
            data_queue_.push(data);

            // High frequency - update every 1ms
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
};

// Order Manager
class OrderManager {
private:
    std::atomic<bool> running_;
    std::thread processing_thread_;
    ThreadSafeQueue<Order>& order_queue_;
    std::vector<Order> filled_orders_;
    std::mutex filled_orders_mutex_;

public:
    OrderManager(ThreadSafeQueue<Order>& queue) 
        : running_(false), order_queue_(queue) {}

    void start() {
        running_ = true;
        processing_thread_ = std::thread(&OrderManager::processOrders, this);
    }

    void stop() {
        running_ = false;
        if (processing_thread_.joinable()) {
            processing_thread_.join();
        }
    }

    std::vector<Order> getFilledOrders() {
        std::lock_guard<std::mutex> lock(filled_orders_mutex_);
        return filled_orders_;
    }

private:
    void processOrders() {
        while (running_) {
            Order order{0, "", OrderType::BUY, 0, 0, StrategyType::MARKET_MAKING};
            if (order_queue_.pop(order)) {
                // Simulate order processing latency
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                
                // Simulate fill (90% fill rate)
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<> dis(1, 10);
                
                if (dis(gen) <= 9) {  // 90% chance to fill
                    order.status = OrderStatus::FILLED;
                    
                    std::lock_guard<std::mutex> lock(filled_orders_mutex_);
                    filled_orders_.push_back(order);
                }
            }
        }
    }
};

// Main HFT Engine
class HFTEngine {
private:
    std::atomic<bool> running_;
    std::vector<std::unique_ptr<TradingStrategy>> strategies_;
    std::unique_ptr<RiskManager> risk_manager_;
    std::unique_ptr<MarketDataFeed> market_feed_;
    std::unique_ptr<OrderManager> order_manager_;
    OrderBook order_book_;
    
    ThreadSafeQueue<MarketData> market_data_queue_;
    ThreadSafeQueue<Order> order_queue_;
    
    std::thread engine_thread_;
    std::thread ui_thread_;

public:
    HFTEngine() : running_(false) {
        // Initialize strategies
        strategies_.push_back(std::make_unique<MarketMakingStrategy>());
        strategies_.push_back(std::make_unique<ArbitrageStrategy>());
        
        // Initialize components
        risk_manager_ = std::make_unique<RiskManager>();
        market_feed_ = std::make_unique<MarketDataFeed>(market_data_queue_);
        order_manager_ = std::make_unique<OrderManager>(order_queue_);
    }

    void start() {
        running_ = true;
        
        // Start all components
        market_feed_->start();
        order_manager_->start();
        
        // Start main engine loop
        engine_thread_ = std::thread(&HFTEngine::engineLoop, this);
        
        // Start UI thread
        ui_thread_ = std::thread(&HFTEngine::uiLoop, this);
        
        std::cout << "HFT Engine started successfully!" << std::endl;
    }

    void stop() {
        running_ = false;
        
        market_feed_->stop();
        order_manager_->stop();
        
        if (engine_thread_.joinable()) {
            engine_thread_.join();
        }
        if (ui_thread_.joinable()) {
            ui_thread_.join();
        }
        
        std::cout << "HFT Engine stopped." << std::endl;
    }

    void toggleStrategy(int index) {
        if (index >= 0 && index < static_cast<int>(strategies_.size())) {
            bool current = strategies_[index]->isActive();
            strategies_[index]->setActive(!current);
            std::cout << strategies_[index]->getName() 
                     << " strategy " << (!current ? "activated" : "deactivated") << std::endl;
        }
    }

private:
    void engineLoop() {
        while (running_) {
            MarketData data{"", 0, 0, 0, 0};
            if (market_data_queue_.pop(data)) {
                // Update order book
                updateOrderBook(data);
                
                // Generate trading signals from all active strategies
                for (auto& strategy : strategies_) {
                    if (strategy->isActive()) {
                        auto orders = strategy->generateSignals(data, order_book_);
                        
                        for (const auto& order : orders) {
                            // Risk check
                            if (risk_manager_->checkOrder(order)) {
                                order_queue_.push(order);
                                risk_manager_->updatePosition(order);
                            }
                        }
                    }
                }
            }
        }
    }

    void updateOrderBook(const MarketData& data) {
        // Simulate order book updates
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> size_dist(1.0, 50.0);
        
        for (int i = 0; i < 5; ++i) {
            double bid_price = data.bid - (i * 0.01);
            double ask_price = data.ask + (i * 0.01);
            
            order_book_.updateBid(bid_price, size_dist(gen));
            order_book_.updateAsk(ask_price, size_dist(gen));
        }
    }

    void uiLoop() {
        while (running_) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            
            int result = system("clear");
            (void)result; 
            
            std::cout << "=== HFT TRADING SYSTEM ===" << std::endl;
            std::cout << "Status: " << (running_ ? "RUNNING" : "STOPPED") << std::endl;
            std::cout << "Timestamp: " << std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::high_resolution_clock::now().time_since_epoch()).count() << std::endl;
            
            // Strategy performance
            std::cout << "\n=== STRATEGY PERFORMANCE ===" << std::endl;
            for (size_t i = 0; i < strategies_.size(); ++i) {
                const auto& strategy = strategies_[i];
                std::cout << "[" << i << "] " << strategy->getName() 
                         << " - Status: " << (strategy->isActive() ? "ACTIVE" : "INACTIVE")
                         << " - P&L: $" << std::fixed << std::setprecision(2) << strategy->getPnL()
                         << " - Trades: " << strategy->getTradeCount() << std::endl;
            }
            
            // Risk metrics
            std::cout << "\n=== RISK METRICS ===" << std::endl;
            std::cout << "Current Position: " << risk_manager_->getCurrentPosition() << std::endl;
            std::cout << "Current P&L: $" << std::fixed << std::setprecision(2) 
                     << risk_manager_->getCurrentPnL() << std::endl;
            
            std::cout << "\n=== SYSTEM STATS ===" << std::endl;
            std::cout << "Market Data Queue Size: " << market_data_queue_.size() << std::endl;
            std::cout << "Order Queue Size: " << order_queue_.size() << std::endl;
            std::cout << "Filled Orders: " << order_manager_->getFilledOrders().size() << std::endl;
            
            // Order book
            order_book_.printOrderBook(3);
            
            std::cout << "\nCommands: [0-" << (strategies_.size()-1) << "] Toggle Strategy, [q] Quit" << std::endl;
        }
    }
};

// Main function
int main() {
    std::cout << "Initializing HFT System..." << std::endl;
    
    HFTEngine engine;
    engine.start();
    
    char command;
    while (std::cin >> command) {
        if (command == 'q' || command == 'Q') {
            break;
        } else if (command >= '0' && command <= '9') {
            int strategy_index = command - '0';
            engine.toggleStrategy(strategy_index);
        }
    }
    
    engine.stop();
    return 0;
}


