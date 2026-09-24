// Lab 5 测试（HW5 扩展）：用线程池并行计算一批多态“咖啡”的价格
//
// HW5 的 EspressoBased/Ingredient 用裸指针 + 手写深拷贝；
// 这里用 std::unique_ptr 和虚函数 clone() 重写一个最小版本，
// 再把每杯饮品的定价作为任务提交给线程池。
#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

#include "../common/check.h"
#include "thread_pool.h"

namespace {

class Ingredient {
public:
    Ingredient(double price_unit, std::size_t units) : price_unit_(price_unit), units_(units) {}
    virtual ~Ingredient() = default;  // 多态基类必须有虚析构函数
    virtual std::string name() const = 0;
    virtual std::unique_ptr<Ingredient> clone() const = 0;  // “虚拷贝构造”
    double price() const { return price_unit_ * static_cast<double>(units_); }

private:
    double price_unit_;
    std::size_t units_;
};

// CRTP 生成 clone()，替代 HW5 里的 DEFCLASS 宏
template <typename Derived>
class IngredientBase : public Ingredient {
public:
    using Ingredient::Ingredient;
    std::unique_ptr<Ingredient> clone() const override {
        return std::make_unique<Derived>(static_cast<const Derived&>(*this));
    }
};

#define LAB_INGREDIENT(Name, Price)                                          \
    class Name final : public IngredientBase<Name> {                        \
    public:                                                                 \
        explicit Name(std::size_t units) : IngredientBase<Name>(Price, units) {} \
        std::string name() const override { return #Name; }                \
    };
LAB_INGREDIENT(Espresso, 15)
LAB_INGREDIENT(Milk, 10)
LAB_INGREDIENT(MilkFoam, 5)
LAB_INGREDIENT(Chocolate, 5)
#undef LAB_INGREDIENT

class Drink {
public:
    Drink(std::string name) : name_(std::move(name)) {}
    Drink(const Drink& o) : name_(o.name_) {  // 深拷贝：逐个 clone
        for (const auto& i : o.ingredients_) ingredients_.push_back(i->clone());
    }
    Drink& operator=(Drink o) noexcept {  // copy-and-swap
        std::swap(name_, o.name_);
        std::swap(ingredients_, o.ingredients_);
        return *this;
    }
    Drink(Drink&&) noexcept = default;
    virtual ~Drink() = default;

    void add(std::unique_ptr<Ingredient> i) { ingredients_.push_back(std::move(i)); }
    virtual double price() const {
        double p = 0;
        for (const auto& i : ingredients_) p += i->price();
        return p;
    }
    const std::string& name() const { return name_; }

private:
    std::string name_;
    std::vector<std::unique_ptr<Ingredient>> ingredients_;
};

Drink cappuccino() {
    Drink d("Cappuccino");
    d.add(std::make_unique<Espresso>(2));
    d.add(std::make_unique<Milk>(2));
    d.add(std::make_unique<MilkFoam>(1));
    return d;
}

Drink mocha() {
    Drink d("Mocha");
    d.add(std::make_unique<Espresso>(2));
    d.add(std::make_unique<Milk>(2));
    d.add(std::make_unique<MilkFoam>(1));
    d.add(std::make_unique<Chocolate>(1));
    return d;
}

}  // namespace

int main() {
    // 1) 基本功能：返回值通过 future 取回
    {
        lab::ThreadPool pool(4);
        auto f = pool.submit([](int a, int b) { return a + b; }, 20, 22);
        CHECK_EQ(f.get(), 42);
    }

    // 2) 多态对象：每杯饮品的深拷贝被移动进任务，不与其他线程共享
    {
        const Drink cap = cappuccino(), moc = mocha();
        CHECK_EQ(cap.price(), 55.0);
        CHECK_EQ(moc.price(), 60.0);

        lab::ThreadPool pool(4);
        std::vector<std::future<double>> prices;
        for (int i = 0; i < 1000; ++i) {
            Drink d = (i % 2 == 0) ? cap : moc;  // 拷贝构造：深拷贝
            prices.push_back(pool.submit([d = std::move(d)] { return d.price(); }));
        }
        double total = 0;
        for (auto& f : prices) total += f.get();
        CHECK_EQ(total, 500 * 55.0 + 500 * 60.0);
    }

    // 3) 异常通过 future 传播到调用者
    {
        lab::ThreadPool pool(2);
        auto f = pool.submit([]() -> int { throw std::runtime_error("out of milk"); });
        bool caught = false;
        try {
            f.get();
        } catch (const std::runtime_error& e) {
            caught = std::string(e.what()) == "out of milk";
        }
        CHECK(caught);
    }

    // 4) 析构时会把已入队任务执行完
    {
        std::atomic<int> done{0};
        {
            lab::ThreadPool pool(3);
            for (int i = 0; i < 500; ++i)
                pool.submit([&done] { done.fetch_add(1, std::memory_order_relaxed); });
        }
        CHECK_EQ(done.load(), 500);
    }

    std::cout << "lab5 OK\n";
}
