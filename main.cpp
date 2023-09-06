#include <array>
#include <cassert>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

using namespace std;

class VehiclePlate {
private:
    auto AsTuple() const {
        return tie(letters_, digits_, region_);
    }

public:
    bool operator==(const VehiclePlate &other) const {
        return AsTuple() == other.AsTuple();
    }

    bool operator<(const VehiclePlate &other) const {
        return AsTuple() < other.AsTuple();
    }

    VehiclePlate(char l0, char l1, int digits, char l2, int region)
            : letters_{l0, l1, l2}, digits_(digits), region_(region) {
    }

    string ToString() const {
        ostringstream out;
        out << letters_[0] << letters_[1];

        out << setfill('0') << right << setw(3) << digits_;
        out << letters_[2] << setw(2) << region_;

        return out.str();
    }

    int Hash() const {
        return digits_;
    }

private:
    array<char, 3> letters_;
    int digits_;
    int region_;
};

ostream &operator<<(ostream &out, VehiclePlate plate) {
    out << plate.ToString();
    return out;
}

class VehiclePlateHasher {
public:
    size_t operator()(const VehiclePlate &plate) const {
        return static_cast<size_t>(plate.Hash());
    }
};

struct ParkingException {
};

template<typename Clock>
class Parking {
    using Duration = typename Clock::duration;
    using TimePoint = typename Clock::time_point;

public:
    Parking(int cost_per_second) : cost_per_second_(cost_per_second) {}

    void Park(VehiclePlate car) {
        if (now_parked_.find(car) != now_parked_.end())
            throw ParkingException();
        now_parked_[car] = Clock::now();
    }

    void Withdraw(const VehiclePlate &car) {
        if (now_parked_.find(car) == end(now_parked_)) {
            throw ParkingException();
        }
        complete_parks_[car] += chrono::duration_cast<chrono::seconds>(Clock::now() - now_parked_[car]);
        now_parked_.erase(car);
    }

    int64_t GetCurrentBill(VehiclePlate car) const {
        int64_t cost = 0;
        if (now_parked_.find(car) != end(now_parked_))
            cost += (chrono::duration_cast<chrono::seconds>(Clock::now() - now_parked_.at(car)).count()) *
                    cost_per_second_;
        if (complete_parks_.find(car) != end(complete_parks_))
            cost += chrono::duration_cast<chrono::seconds>(complete_parks_.at(car)).count() * cost_per_second_;
        return cost;
    }

    unordered_map <VehiclePlate, int64_t, VehiclePlateHasher> EndPeriodAndGetBills() {
        unordered_map <VehiclePlate, int64_t, VehiclePlateHasher> charges_for_all_cars;
        for (auto &[car, duration]: complete_parks_) {
            if (chrono::duration_cast<chrono::seconds>(duration).count())
                charges_for_all_cars[car] +=
                        chrono::duration_cast<chrono::seconds>(duration).count() * cost_per_second_;
        }
        complete_parks_.clear();
        for (auto &[car, begin_time]: now_parked_) {
            charges_for_all_cars[car] +=
                    chrono::duration_cast<chrono::seconds>(Clock::now() - begin_time).count() * cost_per_second_;
            begin_time = Clock::now();
        }
        return move(charges_for_all_cars);
    }

    auto &GetNowParked() const {
        return now_parked_;
    }

    auto &GetCompleteParks() const {
        return complete_parks_;
    }

private:
    int cost_per_second_;
    unordered_map <VehiclePlate, TimePoint, VehiclePlateHasher> now_parked_;
    unordered_map <VehiclePlate, Duration, VehiclePlateHasher> complete_parks_;
};

class TestClock {
public:
    using time_point = chrono::system_clock::time_point;
    using duration = chrono::system_clock::duration;

    static void SetNow(int seconds) {
        current_time_ = seconds;
    }

    static time_point now() {
        return start_point_ + chrono::seconds(current_time_);
    }

private:
    inline static time_point start_point_ = chrono::system_clock::now();
    inline static int current_time_ = 0;
};

int main() {
    Parking<TestClock> parking(10);

    TestClock::SetNow(10);
    parking.Park({'A', 'A', 111, 'A', 99});

    TestClock::SetNow(20);
    parking.Withdraw({'A', 'A', 111, 'A', 99});
    parking.Park({'B', 'B', 222, 'B', 99});

    TestClock::SetNow(40);
    assert(parking.GetCurrentBill({'A', 'A', 111, 'A', 99}) == 100);
    assert(parking.GetCurrentBill({'B', 'B', 222, 'B', 99}) == 200);
    parking.Park({'A', 'A', 111, 'A', 99});

    TestClock::SetNow(50);
    assert(parking.GetCurrentBill({'A', 'A', 111, 'A', 99}) == 200);
    assert(parking.GetCurrentBill({'B', 'B', 222, 'B', 99}) == 300);
    assert(parking.GetCurrentBill({'C', 'C', 333, 'C', 99}) == 0);
    parking.Withdraw({'B', 'B', 222, 'B', 99});

    TestClock::SetNow(70);
    {
        // проверим счёт
        auto bill = parking.EndPeriodAndGetBills();

        assert((bill == unordered_map < VehiclePlate, int64_t, VehiclePlateHasher > {
                {{'A', 'A', 111, 'A', 99}, 400},
                {{'B', 'B', 222, 'B', 99}, 300},
        }));
    }

    TestClock::SetNow(80);
    {
        auto bill = parking.EndPeriodAndGetBills();

        assert((bill
                == unordered_map < VehiclePlate, int64_t, VehiclePlateHasher > {
                {{'A', 'A', 111, 'A', 99}, 100},
        }));
    }

    try {
        parking.Park({'A', 'A', 111, 'A', 99});
        assert(false);
    }
    catch (ParkingException) {
    }

    try {
        parking.Withdraw({'B', 'B', 222, 'B', 99});
        assert(false);
    }
    catch (ParkingException) {
    }

    cout << "Success!"s << endl;
}