// ERL-HUB: Post-Earthquake Multi-Agency Emergency Relief Logistics Hub
// MAIT CIC-211 | C++17 (within the assignment's C++11/14/17 range)
// Faculty: Dr. Ajay
// Student 1: Sarthak Singh | Enrolment No.: 01296402725 | Class: C-19
// Student 2: Abhishek Gupta | Enrolment No.: 02796402725 | Class: C-20
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ReliefOps {

class IncidentCommander;

// Unit IV: custom exceptions derived from std::exception.
class CargoOverweightException : public std::exception {
    std::string message_;
public:
    CargoOverweightException(const std::string& id, double requested, double limit) {
        std::ostringstream out;
        out << "Vehicle " << id << " would carry " << requested
            << " kg; its payload limit is " << limit << " kg.";
        message_ = out.str();
    }
    const char* what() const noexcept override { return message_.c_str(); }
};
class FuelDepletionException : public std::exception {
    std::string message_;
public:
    explicit FuelDepletionException(const std::string& id)
        : message_("Vehicle " + id + " cannot proceed: fuel reserve depleted.") {}
    const char* what() const noexcept override { return message_.c_str(); }
};
class DamagedCorridorException : public std::exception {
    std::string message_;
public:
    explicit DamagedCorridorException(const std::string& route)
        : message_("Route unavailable: " + route + " is damaged or inaccessible.") {}
    const char* what() const noexcept override { return message_.c_str(); }
};
class InventoryShortageException : public std::exception {
    std::string message_;
public:
    explicit InventoryShortageException(const std::string& detail)
        : message_("Dispatch aborted: " + detail) {}
    const char* what() const noexcept override { return message_.c_str(); }
};

namespace Inventory {

// Unit II: encapsulated cargo ADT with stream operators.
class ReliefCargo {
    std::string id_, name_;
    int quantity_;
    double unitWeightKg_;
    bool medical_;
public:
    ReliefCargo(const std::string& id = "NONE", const std::string& name = "Unnamed",
                int quantity = 0, double unitWeightKg = 0.0, bool medical = false)
        : id_(id), name_(name), quantity_(quantity),
          unitWeightKg_(unitWeightKg), medical_(medical) {
        if (id.empty() || quantity < 0 || !std::isfinite(unitWeightKg) || unitWeightKg < 0.0 ||
            (quantity > 0 && unitWeightKg > std::numeric_limits<double>::max() / quantity))
            throw std::invalid_argument("Invalid cargo ID, quantity, or unit weight.");
    }
    const std::string& getId() const noexcept { return id_; }
    const std::string& getName() const noexcept { return name_; }
    int getQuantity() const noexcept { return quantity_; }
    double getUnitWeight() const noexcept { return unitWeightKg_; }
    bool isMedical() const noexcept { return medical_; }
    double totalWeightKg() const noexcept { return quantity_ * unitWeightKg_; }
    ReliefCargo withQuantity(int quantity) const {
        if (quantity < 0) throw std::invalid_argument("Cargo quantity cannot be negative.");
        ReliefCargo copy(*this);
        if (quantity > 0 && unitWeightKg_ > std::numeric_limits<double>::max() / quantity)
            throw std::overflow_error("Cargo total weight exceeds the supported range.");
        copy.quantity_ = quantity;
        return copy;
    }
    void addQuantity(int amount) {
        if (amount < 0 || quantity_ > std::numeric_limits<int>::max() - amount)
            throw std::invalid_argument("Invalid restock quantity.");
        const int nextQuantity = quantity_ + amount;
        if (nextQuantity > 0 && unitWeightKg_ > std::numeric_limits<double>::max() / nextQuantity)
            throw std::overflow_error("Restocked cargo weight exceeds the supported range.");
        quantity_ = nextQuantity;
    }
    bool removeQuantity(int amount) {
        if (amount < 0 || amount > quantity_) return false;
        quantity_ -= amount;
        return true;
    }
    bool operator==(const ReliefCargo& other) const noexcept { return id_ == other.id_; }
    friend std::ostream& operator<<(std::ostream& out, const ReliefCargo& c) {
        return out << std::left << std::setw(10) << c.id_ << std::setw(25) << c.name_
                   << std::right << std::setw(7) << c.quantity_
                   << std::setw(10) << std::fixed << std::setprecision(2) << c.unitWeightKg_
                   << std::setw(12) << c.totalWeightKg()
                   << (c.medical_ ? "  MEDICAL" : "");
    }
    friend std::istream& operator>>(std::istream& in, ReliefCargo& c) {
        std::string id, name; int q = 0; double w = 0.0; bool med = false;
        if (in >> id >> name >> q >> w >> med) {
            if (q < 0 || !std::isfinite(w) || w < 0.0) {
                in.setstate(std::ios::failbit);
                return in;
            }
            c = ReliefCargo(id, name, q, w, med);
        }
        return in;
    }
};

// Unit II: dynamic depot stock, deep copy, assignment and destructor (Rule of Three).
class DistributionDepot {
    std::string name_;
    ReliefCargo* stock_;
    std::size_t count_, capacity_;
    int reservedMedical_;
    static std::size_t checkedCapacity(std::size_t cap) {
        if (cap == 0) throw std::invalid_argument("Depot capacity must be positive.");
        return cap;
    }
    static std::string checkedName(const std::string& name) {
        if (name.empty()) throw std::invalid_argument("Depot name cannot be empty.");
        return name;
    }
    static ReliefCargo* cloneStock(const DistributionDepot& source) {
        std::unique_ptr<ReliefCargo[]> copy(new ReliefCargo[source.capacity_]);
        for (std::size_t i = 0; i < source.count_; ++i) copy[i] = source.stock_[i];
        return copy.release();
    }
    void grow() {
        if (count_ < capacity_) return;
        if (capacity_ > std::numeric_limits<std::size_t>::max() / 2)
            throw std::length_error("Depot capacity overflow.");
        const std::size_t nextCap = capacity_ * 2;
        std::unique_ptr<ReliefCargo[]> next(new ReliefCargo[nextCap]);
        for (std::size_t i = 0; i < count_; ++i) next[i] = stock_[i];
        delete[] stock_; stock_ = next.release(); capacity_ = nextCap;
    }
public:
    explicit DistributionDepot(const std::string& name, std::size_t cap = 4)
        : name_(checkedName(name)), stock_(new ReliefCargo[checkedCapacity(cap)]), count_(0),
          capacity_(cap), reservedMedical_(0) {}
    DistributionDepot(const DistributionDepot& other)
        : name_(other.name_ + "-Backup"), stock_(cloneStock(other)),
          count_(other.count_), capacity_(other.capacity_), reservedMedical_(other.reservedMedical_) {}
    DistributionDepot& operator=(const DistributionDepot& other) {
        if (this == &other) return *this;
        std::string newName(other.name_);
        std::unique_ptr<ReliefCargo[]> next(new ReliefCargo[other.capacity_]);
        for (std::size_t i = 0; i < other.count_; ++i) next[i] = other.stock_[i];
        delete[] stock_; stock_ = next.release(); name_.swap(newName);
        count_ = other.count_; capacity_ = other.capacity_;
        reservedMedical_ = other.reservedMedical_;
        return *this;
    }
    ~DistributionDepot() { delete[] stock_; }
    const std::string& getName() const noexcept { return name_; }
    std::size_t itemCount() const noexcept { return count_; }
    int reservedMedical() const noexcept { return reservedMedical_; }
    void reserveMedical(int units) {
        if (units < 0) throw std::invalid_argument("Medical reserve cannot be negative.");
        reservedMedical_ = units;
    }
    ReliefCargo& findCargo(const std::string& id) {
        for (std::size_t i = 0; i < count_; ++i) if (stock_[i].getId() == id) return stock_[i];
        throw std::out_of_range("Cargo " + id + " not found at " + name_ + ".");
    }
    const ReliefCargo& findCargo(const std::string& id) const {
        for (std::size_t i = 0; i < count_; ++i) if (stock_[i].getId() == id) return stock_[i];
        throw std::out_of_range("Cargo " + id + " not found at " + name_ + ".");
    }
    int availableQuantity(const std::string& id) const {
        const ReliefCargo& c = findCargo(id);
        return c.getQuantity() - (c.isMedical() ? reservedMedical_ : 0);
    }
    std::vector<ReliefCargo> snapshot() const {
        std::vector<ReliefCargo> result;
        result.reserve(count_);
        for (std::size_t i = 0; i < count_; ++i) result.push_back(stock_[i]);
        return result;
    }
    DistributionDepot& operator+=(const ReliefCargo& cargo) {
        if (cargo.getQuantity() <= 0) throw std::invalid_argument("Restock amount must be positive.");
        for (std::size_t i = 0; i < count_; ++i) {
            if (stock_[i] == cargo) {
                if (stock_[i].getName() != cargo.getName() ||
                    stock_[i].getUnitWeight() != cargo.getUnitWeight() ||
                    stock_[i].isMedical() != cargo.isMedical())
                    throw std::invalid_argument("Cargo ID " + cargo.getId() +
                                                " was restocked with conflicting item metadata.");
                stock_[i].addQuantity(cargo.getQuantity());
                return *this;
            }
        }
        grow(); stock_[count_++] = cargo; return *this;
    }
    DistributionDepot& operator-=(const ReliefCargo& cargo) {
        if (cargo.getQuantity() <= 0) throw std::invalid_argument("Deduction amount must be positive.");
        ReliefCargo& item = findCargo(cargo.getId());
        const int available = item.getQuantity() - (item.isMedical() ? reservedMedical_ : 0);
        if (cargo.getQuantity() > available)
            throw InventoryShortageException("only " + std::to_string(available) +
                                              " units of " + cargo.getId() + " are available.");
        item.removeQuantity(cargo.getQuantity()); return *this;
    }
    friend std::ostream& operator<<(std::ostream& out, const DistributionDepot& d) {
        out << "\nDEPOT: " << d.name_ << "\n" << std::left << std::setw(10) << "Cargo ID"
            << std::setw(25) << "Description" << std::right << std::setw(7) << "Units"
            << std::setw(10) << "Kg/unit" << std::setw(12) << "Total kg" << "\n";
        for (std::size_t i = 0; i < d.count_; ++i) out << d.stock_[i] << "\n";
        return out << "Medical reserve: " << d.reservedMedical_ << " units\n";
    }
    friend class ReliefOps::IncidentCommander;
};

} // namespace Inventory

namespace Logistics {
using Inventory::ReliefCargo;
using Inventory::DistributionDepot;

// Unit I: inline utilities and overloaded supply allocation.
inline double distanceKm(double x1, double y1, double x2, double y2) {
    if (!std::isfinite(x1) || !std::isfinite(y1) ||
        !std::isfinite(x2) || !std::isfinite(y2))
        throw std::invalid_argument("Coordinates must be finite numbers.");
    const double result = std::hypot(x2 - x1, y2 - y1);
    if (!std::isfinite(result)) throw std::overflow_error("Calculated distance is outside the supported range.");
    return result;
}
inline double kgToTonnes(double kg) {
    if (!std::isfinite(kg) || kg < 0.0)
        throw std::invalid_argument("Mass must be finite and non-negative.");
    return kg / 1000.0;
}
inline double travelHours(double km, double speed) {
    if (!std::isfinite(km) || !std::isfinite(speed) || km < 0.0 || speed <= 0.0)
        throw std::invalid_argument("Invalid distance or speed.");
    const double eta = km / speed;
    if (!std::isfinite(eta)) throw std::overflow_error("Calculated travel time exceeds the supported range.");
    return eta;
}

// Unit II: dynamic manifest and static fleet analytics.
class TransportVehicle {
    std::string id_, type_;
    double maxPayloadKg_, speedKmph_;
    ReliefCargo* manifest_;
    std::size_t itemCount_, maxItems_;
    bool overloadAllowed_, dispatched_;
    static int operationalVehicles_;
    static double dispatchedTonnes_;
    static std::size_t checkedSlots(std::size_t slots) {
        if (slots == 0) throw std::invalid_argument("Manifest needs at least one slot.");
        return slots;
    }
    static ReliefCargo* cloneManifest(const TransportVehicle& source) {
        std::unique_ptr<ReliefCargo[]> copy(new ReliefCargo[source.maxItems_]);
        for (std::size_t i = 0; i < source.itemCount_; ++i) copy[i] = source.manifest_[i];
        return copy.release();
    }
    void growManifest() {
        if (itemCount_ < maxItems_) return;
        if (maxItems_ > std::numeric_limits<std::size_t>::max() / 2)
            throw std::length_error("Manifest capacity overflow.");
        const std::size_t nextCap = maxItems_ * 2;
        std::unique_ptr<ReliefCargo[]> next(new ReliefCargo[nextCap]);
        for (std::size_t i = 0; i < itemCount_; ++i) next[i] = manifest_[i];
        delete[] manifest_; manifest_ = next.release(); maxItems_ = nextCap;
    }
protected:
    void addDispatchedTonnes(double tonnes) { dispatchedTonnes_ += tonnes; }
public:
    TransportVehicle(const std::string& id = "V-00", const std::string& type = "Truck",
                     double payload = 1000.0, double speed = 40.0, std::size_t slots = 5)
        : id_(id), type_(type), maxPayloadKg_(payload), speedKmph_(speed),
          manifest_(new ReliefCargo[checkedSlots(slots)]), itemCount_(0), maxItems_(slots),
          overloadAllowed_(false), dispatched_(false) {
        if (id.empty() || type.empty() || payload <= 0 || speed <= 0 ||
            !std::isfinite(payload) || !std::isfinite(speed)) {
            delete[] manifest_; throw std::invalid_argument("Invalid vehicle properties.");
        }
        ++operationalVehicles_;
    }
    TransportVehicle(const TransportVehicle& other)
        : id_(other.id_ + "-COPY"), type_(other.type_), maxPayloadKg_(other.maxPayloadKg_),
          speedKmph_(other.speedKmph_), manifest_(cloneManifest(other)),
          itemCount_(other.itemCount_), maxItems_(other.maxItems_),
          overloadAllowed_(false), dispatched_(false) {
        ++operationalVehicles_;
    }
    TransportVehicle& operator=(const TransportVehicle& other) {
        if (this == &other) return *this;
        std::string newId(other.id_), newType(other.type_);
        std::unique_ptr<ReliefCargo[]> next(new ReliefCargo[other.maxItems_]);
        for (std::size_t i = 0; i < other.itemCount_; ++i) next[i] = other.manifest_[i];
        delete[] manifest_; manifest_ = next.release();
        id_.swap(newId); type_.swap(newType);
        maxPayloadKg_ = other.maxPayloadKg_; speedKmph_ = other.speedKmph_;
        itemCount_ = other.itemCount_; maxItems_ = other.maxItems_;
        overloadAllowed_ = other.overloadAllowed_; dispatched_ = other.dispatched_;
        return *this; // static live-object count is unchanged on assignment
    }
    virtual ~TransportVehicle() { delete[] manifest_; --operationalVehicles_; }
    const std::string& getVehicleId() const noexcept { return id_; }
    const std::string& getVehicleType() const noexcept { return type_; }
    double getSpeed() const noexcept { return speedKmph_; }
    double getPayloadLimit() const noexcept { return maxPayloadKg_; }
    double currentLoadKg() const noexcept {
        double total = 0.0;
        for (std::size_t i = 0; i < itemCount_; ++i) total += manifest_[i].totalWeightKg();
        return total;
    }
    std::vector<ReliefCargo> manifestSnapshot() const {
        std::vector<ReliefCargo> result; result.reserve(itemCount_);
        for (std::size_t i = 0; i < itemCount_; ++i) result.push_back(manifest_[i]);
        return result;
    }
    bool loadCargo(const ReliefCargo& cargo) {
        if (dispatched_) throw std::logic_error("Manifest is locked after dispatch.");
        if (cargo.getQuantity() <= 0 || cargo.getUnitWeight() <= 0.0)
            throw std::invalid_argument("Cargo allocation must have positive quantity and weight.");
        for (std::size_t i = 0; i < itemCount_; ++i) if (manifest_[i] == cargo) return false;
        const double newLoad = currentLoadKg() + cargo.totalWeightKg();
        if (!std::isfinite(newLoad)) throw std::overflow_error("Manifest total weight overflow.");
        if (!overloadAllowed_ && newLoad > maxPayloadKg_)
            throw CargoOverweightException(id_, newLoad, maxPayloadKg_);
        growManifest(); manifest_[itemCount_++] = cargo; return true;
    }
    void dispatch(DistributionDepot& depot) {
        if (dispatched_) throw std::logic_error("Vehicle " + id_ + " already dispatched.");
        if (itemCount_ == 0) throw std::logic_error("Cannot dispatch an empty manifest.");
        // Preflight every line before any deduction, preventing partial dispatch.
        for (std::size_t i = 0; i < itemCount_; ++i)
            if (depot.availableQuantity(manifest_[i].getId()) < manifest_[i].getQuantity())
                throw InventoryShortageException("insufficient stock for " + manifest_[i].getId() + ".");
        const double updatedTonnage = dispatchedTonnes_ + kgToTonnes(currentLoadKg());
        if (!std::isfinite(updatedTonnage))
            throw std::overflow_error("Fleet dispatched-tonnage metric overflow.");
        for (std::size_t i = 0; i < itemCount_; ++i) depot -= manifest_[i];
        dispatchedTonnes_ = updatedTonnage;
        dispatched_ = true;
    }
    bool operator==(const TransportVehicle& other) const noexcept {
        if (id_ == other.id_) return true;
        if (itemCount_ == 0 || itemCount_ != other.itemCount_) return false;
        for (std::size_t i = 0; i < itemCount_; ++i) {
            bool matched = false;
            for (std::size_t j = 0; j < other.itemCount_; ++j)
                if (manifest_[i].getId() == other.manifest_[j].getId() &&
                    manifest_[i].getName() == other.manifest_[j].getName() &&
                    manifest_[i].getQuantity() == other.manifest_[j].getQuantity() &&
                    manifest_[i].getUnitWeight() == other.manifest_[j].getUnitWeight() &&
                    manifest_[i].isMedical() == other.manifest_[j].isMedical()) matched = true;
            if (!matched) return false;
        }
        return true;
    }
    static int getOperationalVehicles() noexcept { return operationalVehicles_; }
    static double getDispatchedTonnes() noexcept { return dispatchedTonnes_; }
    friend std::ostream& operator<<(std::ostream& out, const TransportVehicle& v);
    friend std::istream& operator>>(std::istream& in, TransportVehicle& v);
    friend class ReliefOps::IncidentCommander;
};
int TransportVehicle::operationalVehicles_ = 0;
double TransportVehicle::dispatchedTonnes_ = 0.0;

inline int manifestFormatIndex() { static const int i = std::ios_base::xalloc(); return i; }
inline std::ostream& manifestTable(std::ostream& out) { out.iword(manifestFormatIndex()) = 1; return out; }
inline std::ostream& manifestPlain(std::ostream& out) { out.iword(manifestFormatIndex()) = 0; return out; }
inline std::ostream& operator<<(std::ostream& out, const TransportVehicle& v) {
    out << "\nMANIFEST: " << v.id_ << " (" << v.type_ << ")\n";
    if (out.iword(manifestFormatIndex()))
        out << std::left << std::setw(10) << "Cargo ID" << std::setw(25) << "Description"
            << std::right << std::setw(7) << "Units" << std::setw(10) << "Kg/unit"
            << std::setw(12) << "Total kg" << "\n";
    for (std::size_t i = 0; i < v.itemCount_; ++i) out << v.manifest_[i] << "\n";
    return out << "Load: " << std::fixed << std::setprecision(2) << v.currentLoadKg()
               << " / " << v.maxPayloadKg_ << " kg\n";
}
inline std::istream& operator>>(std::istream& in, TransportVehicle& v) {
    std::string id, type; double payload = 0.0, speed = 0.0;
    if (in >> id >> type >> payload >> speed) {
        if (id.empty() || type.empty() || payload <= 0 || speed <= 0 ||
            !std::isfinite(payload) || !std::isfinite(speed)) {
            in.setstate(std::ios::failbit); return in;
        }
        v.id_ = id; v.type_ = type; v.maxPayloadKg_ = payload; v.speedKmph_ = speed;
        v.itemCount_ = 0; v.overloadAllowed_ = false; v.dispatched_ = false;
    }
    return in;
}

inline bool allocateSupply(DistributionDepot& depot, TransportVehicle& v,
                           const std::string& id, double payloadKg) {
    if (!std::isfinite(payloadKg) || payloadKg <= 0)
        throw std::invalid_argument("Payload weight must be positive and finite.");
    const ReliefCargo& source = depot.findCargo(id);
    if (source.getUnitWeight() <= 0.0) throw std::invalid_argument("Cargo unit weight must be positive.");
    const long double calculated = static_cast<long double>(payloadKg) /
                                   static_cast<long double>(source.getUnitWeight());
    if (!std::isfinite(calculated) || calculated > std::numeric_limits<int>::max())
        throw std::overflow_error("Payload allocation exceeds the supported unit count.");
    const int units = static_cast<int>(calculated);
    if (units <= 0) throw std::invalid_argument("Payload is too small for one cargo unit.");
    if (units > depot.availableQuantity(id))
        throw InventoryShortageException("requested payload exceeds available stock of " + id + ".");
    return v.loadCargo(source.withQuantity(units));
}
inline bool allocateSupply(DistributionDepot& depot, TransportVehicle& v,
                           const std::string& id, int patients, int severity) {
    if (patients <= 0 || severity <= 0 || patients > std::numeric_limits<int>::max() / severity)
        throw std::invalid_argument("Invalid or overflowing patient triage allocation.");
    const ReliefCargo& source = depot.findCargo(id);
    if (!source.isMedical()) throw std::invalid_argument("Triage allocation requires medical stock.");
    const int units = patients * severity;
    if (units > depot.availableQuantity(id))
        throw InventoryShortageException("triage demand exceeds available medical stock of " + id + ".");
    return v.loadCargo(source.withQuantity(units));
}
inline void updateMission(double& remainingKm, double& fuelPercent, double travelledKm) {
    if (!std::isfinite(remainingKm) || !std::isfinite(fuelPercent) ||
        !std::isfinite(travelledKm) || remainingKm < 0 || travelledKm < 0 ||
        fuelPercent < 0 || fuelPercent > 100)
        throw std::invalid_argument("Mission distances and fuel must be finite and non-negative.");
    remainingKm = std::max(0.0, remainingKm - travelledKm);
    fuelPercent = std::max(0.0, fuelPercent - travelledKm * 0.4);
}

// -----------------------------------------------------------------------------
// Unit III: inheritance, virtual inheritance, composition and polymorphism.
// GPSBeacon and EmergencyLifeSupportUnit are composed members, not base classes.
// -----------------------------------------------------------------------------
class GPSBeacon {
    double latitude_, longitude_;
    bool signalAvailable_;
public:
    GPSBeacon(double latitude = 0.0, double longitude = 0.0)
        : latitude_(latitude), longitude_(longitude), signalAvailable_(true) {}
    void update(double latitude, double longitude) {
        if (latitude < -90 || latitude > 90 || longitude < -180 || longitude > 180)
            throw std::out_of_range("GPS coordinates outside valid geographic bounds.");
        if (!std::isfinite(latitude) || !std::isfinite(longitude))
            throw std::invalid_argument("GPS coordinates must be finite.");
        latitude_ = latitude; longitude_ = longitude;
    }
    void setSignalAvailable(bool value) noexcept { signalAvailable_ = value; }
    bool hasSignal() const noexcept { return signalAvailable_; }
    std::pair<double, double> coordinates() const noexcept {
        return std::make_pair(latitude_, longitude_);
    }
};

class EmergencyLifeSupportUnit {
    int capacity_, admitted_;
    bool operational_;
public:
    explicit EmergencyLifeSupportUnit(int capacity = 4)
        : capacity_(capacity), admitted_(0), operational_(true) {
        if (capacity < 0) throw std::invalid_argument("Patient capacity cannot be negative.");
    }
    bool admitPatients(int count) {
        if (!operational_ || count <= 0 || count > capacity_ - admitted_) return false;
        admitted_ += count; return true;
    }
    void setOperational(bool value) noexcept { operational_ = value; }
    int patientsSupported() const noexcept { return admitted_; }
    int patientCapacity() const noexcept { return capacity_; }
    bool isOperational() const noexcept { return operational_; }
};

struct RoutePlan {
    std::string routeName;
    double distanceKm;
    double fuelPercent;
    double weatherFactor;
    bool roadDamaged;
    bool airspaceOpen;
    bool waterRouteAvailable;
    RoutePlan(const std::string& name = "Village-7 Access", double distance = 75.0,
              double fuel = 100.0, double weather = 1.0, bool road = false,
              bool air = true, bool water = false)
        : routeName(name), distanceKm(distance), fuelPercent(fuel), weatherFactor(weather),
          roadDamaged(road), airspaceOpen(air), waterRouteAvailable(water) {}
};

struct RouteAssessment {
    std::string vehicleId, routeMode, routeName;
    double distanceKm, etaHours;
    std::string note;
};

// Abstract common vehicle root. The class owns independent GPS and life-support
// objects, while inheriting the Unit-II TransportVehicle manifest functionality.
class ReliefVehicle : public TransportVehicle {
    GPSBeacon gps_;
    EmergencyLifeSupportUnit lifeSupport_;
protected:
    void verifyFuel(const RoutePlan& route) const {
        if (route.fuelPercent <= 0) throw FuelDepletionException(getVehicleId());
        if (!std::isfinite(route.fuelPercent) || route.fuelPercent > 100)
            throw std::invalid_argument("Fuel percentage must be in the range (0,100].");
    }
    double adjustedEta(const RoutePlan& route, double modeFactor = 1.0) const {
        if (!std::isfinite(route.distanceKm) || route.distanceKm < 0 ||
            !std::isfinite(route.weatherFactor) || route.weatherFactor <= 0 ||
            !std::isfinite(modeFactor) || modeFactor <= 0)
            throw std::invalid_argument("Invalid route distance or adjustment factor.");
        const double eta = travelHours(route.distanceKm, getSpeed()) * route.weatherFactor * modeFactor;
        if (!std::isfinite(eta)) throw std::overflow_error("Adjusted route ETA exceeds the supported range.");
        return eta;
    }
public:
    ReliefVehicle(const std::string& id, const std::string& type, double payload,
                  double speed, std::size_t slots, int patientCapacity = 4)
        : TransportVehicle(id, type, payload, speed, slots),
          gps_(), lifeSupport_(patientCapacity) {}
    virtual ~ReliefVehicle() {}
    GPSBeacon& gpsBeacon() noexcept { return gps_; }
    const GPSBeacon& gpsBeacon() const noexcept { return gps_; }
    EmergencyLifeSupportUnit& lifeSupport() noexcept { return lifeSupport_; }
    const EmergencyLifeSupportUnit& lifeSupport() const noexcept { return lifeSupport_; }
    virtual RouteAssessment evaluateRoute(const RoutePlan& route) const = 0;
};

// All three intermediate types virtually inherit ReliefVehicle. A hybrid leaf
// that inherits two of them therefore contains one shared ReliefVehicle base.
class AerialTransport : public virtual ReliefVehicle {
public:
    AerialTransport(const std::string& id, double payload = 650.0,
                    double speed = 180.0, std::size_t slots = 6)
        : ReliefVehicle(id, "AerialTransport", payload, speed, slots, 2) {}
    RouteAssessment evaluateRoute(const RoutePlan& route) const override {
        verifyFuel(route);
        if (!route.airspaceOpen) throw DamagedCorridorException("air corridor");
        return {getVehicleId(), "AIR", route.routeName, route.distanceKm,
                adjustedEta(route), "Air corridor available"};
    }
};

class GroundConvoy : public virtual ReliefVehicle {
public:
    GroundConvoy(const std::string& id, double payload = 5000.0,
                 double speed = 45.0, std::size_t slots = 10)
        : ReliefVehicle(id, "GroundConvoy", payload, speed, slots, 6) {}
    RouteAssessment evaluateRoute(const RoutePlan& route) const override {
        verifyFuel(route);
        if (route.roadDamaged) throw DamagedCorridorException("primary road corridor");
        return {getVehicleId(), "ROAD", route.routeName, route.distanceKm,
                adjustedEta(route), "Road corridor verified"};
    }
};

class AmphibiousCraft : public virtual ReliefVehicle {
public:
    AmphibiousCraft(const std::string& id, double payload = 1600.0,
                    double speed = 38.0, std::size_t slots = 8)
        : ReliefVehicle(id, "AmphibiousCraft", payload, speed, slots, 3) {}
    RouteAssessment evaluateRoute(const RoutePlan& route) const override {
        verifyFuel(route);
        if (!route.waterRouteAvailable) throw DamagedCorridorException("waterway approach");
        return {getVehicleId(), "WATER", route.routeName, route.distanceKm,
                adjustedEta(route, 1.15), "Waterway approach available"};
    }
};

// Hybrid medical carrier: diamond inheritance resolved by virtual base classes.
class HybridMedicalCarrier : public AerialTransport, public AmphibiousCraft {
public:
    HybridMedicalCarrier(const std::string& id, double payload = 950.0,
                         double speed = 105.0, std::size_t slots = 8)
        : ReliefVehicle(id, "HybridMedicalCarrier", payload, speed, slots, 8),
          AerialTransport(id, payload, speed, slots),
          AmphibiousCraft(id, payload, speed, slots) {}
    RouteAssessment evaluateRoute(const RoutePlan& route) const override {
        verifyFuel(route);
        if (route.airspaceOpen)
            return {getVehicleId(), "HYBRID-AIR", route.routeName, route.distanceKm,
                    adjustedEta(route), "Hybrid selected aerial mode"};
        if (route.waterRouteAvailable)
            return {getVehicleId(), "HYBRID-WATER", route.routeName, route.distanceKm,
                    adjustedEta(route, 1.15), "Hybrid selected amphibious mode"};
        throw DamagedCorridorException("all hybrid access corridors");
    }
};

} // namespace Logistics

// Unit I: privileged emergency access to private reserve/overload fields.
class IncidentCommander {
    std::string name_;
public:
    explicit IncidentCommander(const std::string& name) : name_(name) {
        if (name.empty()) throw std::invalid_argument("Commander name cannot be empty.");
    }
    void releaseMedicalReserve(Inventory::DistributionDepot& depot) {
        std::cout << "[COMMAND] " << name_ << " released " << depot.reservedMedical_
                  << " reserved medical units at " << depot.name_ << "\n";
        depot.reservedMedical_ = 0;
    }
    void authorizeOverload(Logistics::TransportVehicle& vehicle) {
        vehicle.overloadAllowed_ = true;
        std::cout << "[COMMAND] " << name_ << " authorized emergency payload override for "
                  << vehicle.id_ << "\n";
    }
};

namespace Logistics {

// -----------------------------------------------------------------------------
// Unit IV: binary transit log and manifest audit persistence.
// POD records avoid serializing implementation-defined std::string internals.
// -----------------------------------------------------------------------------
struct TransitLogRecord {
    char vehicleId[32];
    char routeName[48];
    char routeMode[16];
    double distanceKm;
    double etaHours;
    double cargoKg;
    double fuelPercent;
    std::int32_t status;
};
struct ManifestAuditRecord {
    char vehicleId[32];
    char cargoId[32];
    char cargoName[48];
    std::int32_t quantity;
    double unitWeightKg;
    std::int32_t medical;
};
static_assert(std::is_trivially_copyable<TransitLogRecord>::value,
              "Transit records must remain fixed-layout binary-safe data.");
static_assert(std::is_trivially_copyable<ManifestAuditRecord>::value,
              "Manifest records must remain fixed-layout binary-safe data.");

struct BinaryFileHeader {
    char magic[8];
    std::uint32_t version;
    std::uint32_t recordCount;
    std::uint32_t checksum;
};

class BinaryAuditStore {
    template <typename Record>
    static std::uint32_t checksum(const std::vector<Record>& records) {
        const unsigned char* bytes = reinterpret_cast<const unsigned char*>(records.data());
        const std::size_t byteCount = records.size() * sizeof(Record);
        std::uint32_t hash = 2166136261u;
        for (std::size_t i = 0; i < byteCount; ++i) {
            hash ^= bytes[i]; hash *= 16777619u;
        }
        return hash;
    }

    template <typename Record>
    static void saveAtomic(const std::string& path, const char (&magic)[8],
                           const std::vector<Record>& records) {
        namespace fs = std::filesystem;
        if (records.size() > std::numeric_limits<std::uint32_t>::max())
            throw std::length_error("Too many records for binary audit format.");
        const std::string temp = path + ".tmp";
        const std::string backup = path + ".bak";
        BinaryFileHeader header{};
        std::memcpy(header.magic, magic, 8);
        header.version = 1;
        {
            std::fstream file(temp.c_str(), std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
            if (!file) throw std::runtime_error("Cannot open temporary audit file " + temp);
            file.write(reinterpret_cast<const char*>(&header), sizeof(header));
            if (!records.empty())
                file.write(reinterpret_cast<const char*>(records.data()),
                           static_cast<std::streamsize>(records.size() * sizeof(Record)));
            file.flush();
            if (!file) {
                // Roll back the incomplete temporary transaction using seekp().
                file.clear(); file.seekp(0, std::ios::beg);
                const char aborted[8] = {'A','B','O','R','T','E','D','!'};
                file.write(aborted, 8); file.flush(); file.close();
                std::error_code ignored; fs::remove(temp, ignored);
                throw std::runtime_error("Binary audit write failed; temporary transaction rolled back.");
            }
            header.recordCount = static_cast<std::uint32_t>(records.size());
            header.checksum = checksum(records);
            // Backfill the file header, then seekg() to verify the committed metadata.
            file.seekp(0, std::ios::beg);
            file.write(reinterpret_cast<const char*>(&header), sizeof(header));
            file.flush();
            if (!file) {
                file.clear(); file.seekp(0, std::ios::beg);
                BinaryFileHeader rollback{}; std::memcpy(rollback.magic, "ROLLBACK", 8);
                file.write(reinterpret_cast<const char*>(&rollback), sizeof(rollback));
                file.flush(); file.close();
                std::error_code ignored; fs::remove(temp, ignored);
                throw std::runtime_error("Could not finalize audit header; transaction rolled back.");
            }
            file.seekg(0, std::ios::beg);
            BinaryFileHeader verify{};
            file.read(reinterpret_cast<char*>(&verify), sizeof(verify));
            std::vector<Record> verifyRecords(records.size());
            if (file && !verifyRecords.empty()) {
                file.seekg(static_cast<std::streamoff>(sizeof(verify)), std::ios::beg);
                file.read(reinterpret_cast<char*>(verifyRecords.data()),
                          static_cast<std::streamsize>(verifyRecords.size() * sizeof(Record)));
            }
            if (!file || std::memcmp(verify.magic, magic, 8) != 0 ||
                verify.recordCount != records.size() || verify.checksum != checksum(verifyRecords)) {
                file.close(); std::error_code ignored; fs::remove(temp, ignored);
                throw std::runtime_error("Audit verification failed; original file left untouched.");
            }
        }
        // The temporary and target are in the same directory; rename is the commit point.
        std::error_code ec;
        const bool hadOriginal = fs::exists(path, ec) && !ec;
        if (hadOriginal) {
            fs::remove(backup, ec); ec.clear(); fs::rename(path, backup, ec);
            if (ec) { std::error_code ignored; fs::remove(temp, ignored);
                      throw std::runtime_error("Could not preserve previous audit file."); }
        }
        ec.clear(); fs::rename(temp, path, ec);
        if (ec) {
            if (hadOriginal) { std::error_code restore; fs::rename(backup, path, restore); }
            std::error_code ignored; fs::remove(temp, ignored);
            throw std::runtime_error("Audit commit failed; previous file restored when possible.");
        }
        if (hadOriginal) fs::remove(backup, ec);
    }

    template <typename Record>
    static std::vector<Record> loadChecked(const std::string& path, const char (&magic)[8]) {
        std::fstream file(path.c_str(), std::ios::binary | std::ios::in);
        if (!file) throw std::runtime_error("Cannot open audit file " + path);
        BinaryFileHeader header{};
        file.seekg(0, std::ios::beg);
        file.read(reinterpret_cast<char*>(&header), sizeof(header));
        if (!file || std::memcmp(header.magic, magic, 8) != 0 || header.version != 1)
            throw std::runtime_error("Invalid binary audit header in " + path);
        if (header.recordCount > 1000000u)
            throw std::runtime_error("Audit record count exceeds the safety limit.");
        file.seekg(0, std::ios::end);
        const std::streamoff actualSize = file.tellg();
        const std::streamoff expectedSize = static_cast<std::streamoff>(sizeof(header)) +
            static_cast<std::streamoff>(header.recordCount) * static_cast<std::streamoff>(sizeof(Record));
        if (actualSize != expectedSize)
            throw std::runtime_error("Binary audit file size does not match its record header.");
        std::vector<Record> records(header.recordCount);
        file.seekg(static_cast<std::streamoff>(sizeof(header)), std::ios::beg);
        if (!records.empty())
            file.read(reinterpret_cast<char*>(records.data()),
                      static_cast<std::streamsize>(records.size() * sizeof(Record)));
        if (!file) throw std::runtime_error("Truncated binary audit file: " + path);
        if (checksum(records) != header.checksum)
            throw std::runtime_error("Binary audit checksum mismatch in " + path);
        return records;
    }
public:
    static void saveTransit(const std::string& path, const std::vector<TransitLogRecord>& records) {
        const char magic[8] = {'E','R','L','T','R','N','1','\0'};
        saveAtomic(path, magic, records);
    }
    static std::vector<TransitLogRecord> restoreTransit(const std::string& path) {
        const char magic[8] = {'E','R','L','T','R','N','1','\0'};
        return loadChecked<TransitLogRecord>(path, magic);
    }
    static void saveManifestAudit(const std::string& path,
                                  const std::vector<ManifestAuditRecord>& records) {
        const char magic[8] = {'E','R','L','M','A','N','1','\0'};
        saveAtomic(path, magic, records);
    }
    static std::vector<ManifestAuditRecord> restoreManifestAudit(const std::string& path) {
        const char magic[8] = {'E','R','L','M','A','N','1','\0'};
        return loadChecked<ManifestAuditRecord>(path, magic);
    }
};

inline void copyText(char* target, std::size_t size, const std::string& value) {
    if (size) std::snprintf(target, size, "%s", value.c_str());
}
inline TransitLogRecord makeTransitRecord(const ReliefVehicle& v,
                                          const RouteAssessment& route, double fuel) {
    TransitLogRecord r{};
    copyText(r.vehicleId, sizeof(r.vehicleId), v.getVehicleId());
    copyText(r.routeName, sizeof(r.routeName), route.routeName);
    copyText(r.routeMode, sizeof(r.routeMode), route.routeMode);
    r.distanceKm = route.distanceKm; r.etaHours = route.etaHours;
    r.cargoKg = v.currentLoadKg(); r.fuelPercent = fuel; r.status = 1;
    return r;
}
inline std::vector<ManifestAuditRecord> makeManifestAudit(const ReliefVehicle& v) {
    std::vector<ManifestAuditRecord> result;
    for (const ReliefCargo& cargo : v.manifestSnapshot()) {
        ManifestAuditRecord r{};
        copyText(r.vehicleId, sizeof(r.vehicleId), v.getVehicleId());
        copyText(r.cargoId, sizeof(r.cargoId), cargo.getId());
        copyText(r.cargoName, sizeof(r.cargoName), cargo.getName());
        r.quantity = cargo.getQuantity(); r.unitWeightKg = cargo.getUnitWeight();
        r.medical = cargo.isMedical() ? 1 : 0; result.push_back(r);
    }
    return result;
}

// Templated max-priority dispatch queue (priority comparator is supplied by caller).
template <typename T, typename Compare = std::less<T>>
class PriorityDispatchQueue {
    std::priority_queue<T, std::vector<T>, Compare> queue_;
public:
    void push(const T& value) { queue_.push(value); }
    void push(T&& value) { queue_.push(std::move(value)); }
    bool empty() const noexcept { return queue_.empty(); }
    std::size_t size() const noexcept { return queue_.size(); }
    const T& top() const {
        if (queue_.empty()) throw std::out_of_range("Priority dispatch queue is empty.");
        return queue_.top();
    }
    void pop() {
        if (queue_.empty()) throw std::out_of_range("Cannot pop an empty queue.");
        queue_.pop();
    }
};
struct DispatchTask { int priority; std::string vehicleId; std::string instruction; };
struct DispatchTaskPriority {
    bool operator()(const DispatchTask& a, const DispatchTask& b) const {
        if (a.priority != b.priority) return a.priority < b.priority;
        return a.vehicleId > b.vehicleId;
    }
};
struct MedicalNeed {
    std::string facility; int patients; int severity; int availableStock;
    long long priorityScore() const {
        if (patients < 0 || severity < 0)
            throw std::invalid_argument("Medical-priority inputs cannot be negative.");
        return static_cast<long long>(patients) * static_cast<long long>(severity);
    }
};
struct UrgentMedicalNeedFirst {
    bool operator()(const MedicalNeed& a, const MedicalNeed& b) const {
        if (a.priorityScore() != b.priorityScore()) return a.priorityScore() > b.priorityScore();
        return a.facility < b.facility;
    }
};

} // namespace Logistics
} // namespace ReliefOps

namespace {
using namespace ReliefOps;
using namespace ReliefOps::Inventory;
using namespace ReliefOps::Logistics;

bool runSelfTests() {
    try {
        DistributionDepot depot("TEST-DEPOT", 1);
        depot += ReliefCargo("MED-T", "Test_Medicine", 100, 1.0, true);
        depot.reserveMedical(10);
        DistributionDepot copied(depot);
        copied.findCargo("MED-T").addQuantity(2);
        if (depot.findCargo("MED-T").getQuantity() != 100) return false;
        DistributionDepot assigned("EMPTY");
        assigned = depot;
        DistributionDepot& assignedAlias = assigned;
        assigned = assignedAlias; // exercise the self-assignment guard
        assigned.findCargo("MED-T").addQuantity(1);
        if (depot.findCargo("MED-T").getQuantity() != 100) return false;
        bool metadataConflictCaught = false;
        try { depot += ReliefCargo("MED-T", "Wrong_Name", 1, 2.0, false); }
        catch (const std::invalid_argument&) { metadataConflictCaught = true; }
        if (!metadataConflictCaught) return false;

        TransportVehicle emptyOne("EMPTY-1"), emptyTwo("EMPTY-2");
        if (emptyOne == emptyTwo) return false;
        bool emptyDispatchCaught = false;
        try { emptyOne.dispatch(depot); }
        catch (const std::logic_error&) { emptyDispatchCaught = true; }
        if (!emptyDispatchCaught) return false;

        GroundConvoy ground("TEST-GROUND", 500, 50, 1);
        ground.loadCargo(ReliefCargo("MED-T", "Test_Medicine", 5, 1.0, true));
        GroundConvoy copiedVehicle(ground);
        GroundConvoy assignedVehicle("TEST-ASSIGNED", 500, 50, 1);
        assignedVehicle = ground;
        GroundConvoy& vehicleAlias = assignedVehicle;
        assignedVehicle = vehicleAlias;
        if (copiedVehicle.manifestSnapshot().size() != 1 ||
            assignedVehicle.manifestSnapshot().size() != 1 || !(ground == copiedVehicle)) return false;
        copiedVehicle.loadCargo(ReliefCargo("COPY-ONLY", "Copy_Only", 2, 1.0, false));
        if (ground.manifestSnapshot().size() != 1) return false;

        // A later manifest line is deliberately short; the earlier line must not
        // be deducted because dispatch performs a complete preflight first.
        DistributionDepot shortageDepot("SHORTAGE-TEST", 2);
        shortageDepot += ReliefCargo("A-ENOUGH", "First_Item", 10, 1.0, false);
        shortageDepot += ReliefCargo("B-SHORT", "Second_Item", 1, 1.0, false);
        GroundConvoy transactionalVehicle("TX-TEST", 100.0, 40.0, 2);
        transactionalVehicle.loadCargo(ReliefCargo("A-ENOUGH", "First_Item", 5, 1.0, false));
        transactionalVehicle.loadCargo(ReliefCargo("B-SHORT", "Second_Item", 5, 1.0, false));
        bool shortageCaught = false;
        try { transactionalVehicle.dispatch(shortageDepot); }
        catch (const InventoryShortageException&) { shortageCaught = true; }
        if (!shortageCaught || shortageDepot.findCargo("A-ENOUGH").getQuantity() != 10) return false;

        bool overweightCaught = false;
        try { ground.loadCargo(ReliefCargo("HEAVY", "Heavy", 1000, 2.0, false)); }
        catch (const CargoOverweightException&) { overweightCaught = true; }
        if (!overweightCaught) return false;
        std::istringstream cargoInput("STREAM-1 Bandage_Box 4 0.5 1");
        ReliefCargo parsedCargo;
        if (!(cargoInput >> parsedCargo) || parsedCargo.getQuantity() != 4) return false;
        std::istringstream vehicleInput("STREAM-V1 Utility_Van 900 55");
        TransportVehicle parsedVehicle;
        if (!(vehicleInput >> parsedVehicle) || parsedVehicle.getVehicleId() != "STREAM-V1") return false;

        bool corridorCaught = false;
        try { (void)ground.evaluateRoute(RoutePlan("Damaged Road", 10, 100, 1, true, false, false)); }
        catch (const DamagedCorridorException&) { corridorCaught = true; }
        if (!corridorCaught) return false;

        bool fuelCaught = false;
        try { (void)ground.evaluateRoute(RoutePlan("No Fuel", 10, 0, 1, false, true, false)); }
        catch (const FuelDepletionException&) { fuelCaught = true; }
        if (!fuelCaught) return false;

        bool etaOverflowCaught = false;
        try { (void)travelHours(std::numeric_limits<double>::max(),
                               std::numeric_limits<double>::min()); }
        catch (const std::overflow_error&) { etaOverflowCaught = true; }
        if (!etaOverflowCaught) return false;
        bool invalidMassCaught = false;
        try { (void)kgToTonnes(std::numeric_limits<double>::quiet_NaN()); }
        catch (const std::invalid_argument&) { invalidMassCaught = true; }
        if (!invalidMassCaught) return false;

        // Composition checks: GPS validation and the independent life-support unit.
        bool gpsBoundsCaught = false;
        try { ground.gpsBeacon().update(95.0, 10.0); }
        catch (const std::out_of_range&) { gpsBoundsCaught = true; }
        if (!gpsBoundsCaught) return false;
        if (!ground.lifeSupport().admitPatients(ground.lifeSupport().patientCapacity())) return false;
        if (ground.lifeSupport().admitPatients(1)) return false; // capacity is now full
        ground.lifeSupport().setOperational(false);
        if (ground.lifeSupport().admitPatients(1)) return false;

        AerialTransport aircraft("TEST-AIR");
        bool airspaceCaught = false;
        try { (void)aircraft.evaluateRoute(RoutePlan("Closed airspace", 10, 90, 1, false, false, false)); }
        catch (const DamagedCorridorException&) { airspaceCaught = true; }
        if (!airspaceCaught) return false;
        AmphibiousCraft vessel("TEST-WATER");
        bool waterwayCaught = false;
        try { (void)vessel.evaluateRoute(RoutePlan("Blocked waterway", 10, 90, 1, false, true, false)); }
        catch (const DamagedCorridorException&) { waterwayCaught = true; }
        if (!waterwayCaught) return false;

        HybridMedicalCarrier hybrid("TEST-HYBRID");
        ReliefVehicle* basePointer = &hybrid;
        const RouteAssessment assessment = basePointer->evaluateRoute(
            RoutePlan("River Link", 20, 80, 1.1, true, false, true));
        if (assessment.routeMode != "HYBRID-WATER") return false;

        PriorityDispatchQueue<DispatchTask, DispatchTaskPriority> queue;
        queue.push({2, "TRK-2", "Routine"});
        queue.push({9, "HEL-1", "Critical"});
        if (queue.top().priority != 9) return false;

        std::vector<MedicalNeed> needs = {
            {"Clinic-A", 5, 4, 20}, {"Clinic-B", 3, 4, 5}, {"Clinic-C", 2, 1, 100}
        };
        std::sort(needs.begin(), needs.end(), UrgentMedicalNeedFirst());
        if (needs.front().priorityScore() != 20) return false;

        TransitLogRecord record{};
        copyText(record.vehicleId, sizeof(record.vehicleId), "TEST-GROUND");
        copyText(record.routeName, sizeof(record.routeName), "Test Route");
        copyText(record.routeMode, sizeof(record.routeMode), "ROAD");
        record.distanceKm = 10; record.etaHours = 0.2; record.cargoKg = 5;
        record.fuelPercent = 80; record.status = 1;
        const std::string path = "erl_hub_selftest.bin";
        const std::string manifestPath = "erl_hub_manifest_selftest.bin";
        ManifestAuditRecord auditRecord{};
        copyText(auditRecord.vehicleId, sizeof(auditRecord.vehicleId), "TEST-GROUND");
        copyText(auditRecord.cargoId, sizeof(auditRecord.cargoId), "MED-T");
        copyText(auditRecord.cargoName, sizeof(auditRecord.cargoName), "Test_Medicine");
        auditRecord.quantity = 5; auditRecord.unitWeightKg = 1.0; auditRecord.medical = 1;
        BinaryAuditStore::saveManifestAudit(manifestPath, std::vector<ManifestAuditRecord>(1, auditRecord));
        const std::vector<ManifestAuditRecord> restoredAudit =
            BinaryAuditStore::restoreManifestAudit(manifestPath);
        if (restoredAudit.size() != 1 || restoredAudit.front().quantity != 5 ||
            restoredAudit.front().medical != 1) return false;
        BinaryAuditStore::saveTransit(path, std::vector<TransitLogRecord>(1, record));
        BinaryAuditStore::saveTransit(path, std::vector<TransitLogRecord>(2, record));
        const std::vector<TransitLogRecord> restored = BinaryAuditStore::restoreTransit(path);
        if (restored.size() != 2 || std::string(restored[0].vehicleId) != "TEST-GROUND") return false;
        {
            std::fstream tamper(path.c_str(), std::ios::binary | std::ios::in | std::ios::out);
            tamper.seekg(static_cast<std::streamoff>(sizeof(BinaryFileHeader) + 3));
            char byte = 0; tamper.read(&byte, 1); byte = static_cast<char>(byte ^ 0x5A);
            tamper.seekp(static_cast<std::streamoff>(sizeof(BinaryFileHeader) + 3));
            tamper.write(&byte, 1); tamper.flush();
        }
        bool checksumCaught = false;
        try { (void)BinaryAuditStore::restoreTransit(path); }
        catch (const std::runtime_error&) { checksumCaught = true; }
        std::filesystem::remove(path);
        std::filesystem::remove(manifestPath);
        if (!checksumCaught) return false;

        bool emptyQueueCaught = false;
        PriorityDispatchQueue<DispatchTask, DispatchTaskPriority> emptyQueue;
        try { (void)emptyQueue.top(); }
        catch (const std::out_of_range&) { emptyQueueCaught = true; }
        if (!emptyQueueCaught) return false;

        return true;
    } catch (const std::exception& error) {
        std::cerr << "Self-test exception: " << error.what() << "\n";
        return false;
    }
}

struct FleetGuard {
    std::vector<ReliefVehicle*> vehicles;
    ~FleetGuard() { clear(); }
    void add(ReliefVehicle* vehicle) {
        try { vehicles.push_back(vehicle); }
        catch (...) { delete vehicle; throw; }
    }
    void clear() {
        for (ReliefVehicle* vehicle : vehicles) delete vehicle;
        vehicles.clear();
    }
};

int runDemo() {
    DistributionDepot depot("Bhuj-Central", 3);
    depot += ReliefCargo("RAT-01", "Ration_Kit", 600, 5.0, false);
    depot += ReliefCargo("MED-01", "Trauma_Triage_Kit", 300, 2.5, true);
    depot += ReliefCargo("WAT-01", "Clean_Water_Tank", 250, 20.0, false);
    depot += ReliefCargo("BLK-01", "Thermal_Blanket", 700, 1.5, false);
    depot += ReliefCargo("FUEL-01", "Diesel_Can", 80, 18.0, false);
    depot.reserveMedical(100);
    IncidentCommander commander("Regional Incident Commander");

    std::cout << "==========================================================\n"
              << " ERL-HUB | Post-Earthquake Emergency Relief Logistics\n"
              << "==========================================================\n" << depot;
    commander.releaseMedicalReserve(depot);

    // Unit I: return-by-reference inventory update and call-by-reference mission update.
    depot.findCargo("BLK-01").addQuantity(25);
    double remainingKm = distanceKm(0.0, 0.0, 48.0, 36.0);
    double fuelPercent = 100.0;
    updateMission(remainingKm, fuelPercent, 15.0);
    std::cout << "\nMission updated by reference: remaining " << remainingKm
              << " km, fuel " << fuelPercent << "%\n";

    std::istringstream cargoRecord("FUEL-02 Diesel_Can_Reserve 5 18.0 0");
    ReliefCargo parsedCargo;
    if (!(cargoRecord >> parsedCargo)) throw std::runtime_error("Could not parse cargo record.");
    depot += parsedCargo;
    std::cout << "\nStream extraction demonstration: " << parsedCargo << "\n";
    {
        std::istringstream vehicleRecord("SUP-501 Support_Vehicle 1200 60");
        TransportVehicle parsedVehicle;
        if (!(vehicleRecord >> parsedVehicle)) throw std::runtime_error("Could not parse vehicle record.");
        std::cout << parsedVehicle << "\n";
    } // parsed vehicle is destroyed here; static live count stays accurate

    // Unit III: derived vehicles are exercised only through ReliefVehicle pointers.
    FleetGuard fleet;
    fleet.add(new GroundConvoy("TRK-101", 5000, 45, 8));
    fleet.add(new AerialTransport("HEL-201", 650, 180, 5));
    fleet.add(new AmphibiousCraft("AMP-301", 1600, 38, 6));
    fleet.add(new HybridMedicalCarrier("HYB-401", 950, 105, 8));

    // Unit IV: unordered_map indexes depot locations without taking ownership.
    std::unordered_map<std::string, DistributionDepot*> depotDirectory;
    depotDirectory[depot.getName()] = &depot;
    std::cout << "\nRegistered depot locations: " << depotDirectory.size() << "\n";

    // Unit I overloaded allocation: weight-based vs patient/severity-based triage.
    allocateSupply(depot, *fleet.vehicles[0], "RAT-01", 1500.0);
    allocateSupply(depot, *fleet.vehicles[0], "BLK-01", 300.0);
    allocateSupply(depot, *fleet.vehicles[1], "MED-01", 12, 2);
    allocateSupply(depot, *fleet.vehicles[2], "WAT-01", 1200.0);
    commander.authorizeOverload(*fleet.vehicles[2]);
    allocateSupply(depot, *fleet.vehicles[2], "BLK-01", 600.0); // 1800 kg > 1600 kg limit: emergency override
    allocateSupply(depot, *fleet.vehicles[3], "MED-01", 8, 2);

    fleet.vehicles[1]->gpsBeacon().update(23.25, 69.67);
    fleet.vehicles[1]->lifeSupport().admitPatients(2);
    std::cout << "Helicopter life-support: " << fleet.vehicles[1]->lifeSupport().patientsSupported()
              << "/" << fleet.vehicles[1]->lifeSupport().patientCapacity() << " patients\n";

    const std::vector<RoutePlan> routes = {
        RoutePlan("Bhuj North Road Detour", 75, 90, 1.15, false, true, false),
        RoutePlan("Medical Air Corridor", 75, 85, 1.05, true, true, false),
        RoutePlan("Coastal Water Link", 75, 80, 1.10, true, false, true),
        RoutePlan("River-to-Air Relief Link", 75, 78, 1.08, true, false, true)
    };
    std::vector<TransitLogRecord> transitRecords;
    std::vector<ManifestAuditRecord> manifestRecords;
    std::cout << "\n--- Runtime route evaluation through ReliefVehicle* ---\n";
    try {
        (void)fleet.vehicles[0]->evaluateRoute(
            RoutePlan("Collapsed Old Highway", 75, 90, 1.0, true, false, false));
    } catch (const DamagedCorridorException& error) {
        std::cout << "Primary route blocked; dispatch planner will use a detour: "
                  << error.what() << "\n";
    }
    for (std::size_t i = 0; i < fleet.vehicles.size(); ++i) {
        try {
            const RouteAssessment result = fleet.vehicles[i]->evaluateRoute(routes[i]);
            std::cout << result.vehicleId << " | " << result.routeMode << " | "
                      << result.routeName << " | ETA " << std::fixed << std::setprecision(2)
                      << result.etaHours << " h | " << result.note << "\n";
            transitRecords.push_back(makeTransitRecord(*fleet.vehicles[i], result, routes[i].fuelPercent));
        } catch (const DamagedCorridorException& error) {
            std::cout << "Route warning: " << error.what() << "\n";
        } catch (const FuelDepletionException& error) {
            std::cout << "Fuel warning: " << error.what() << "\n";
        }
        const std::vector<ManifestAuditRecord> lines = makeManifestAudit(*fleet.vehicles[i]);
        manifestRecords.insert(manifestRecords.end(), lines.begin(), lines.end());
    }

    // Unit IV: generic template queue and custom urgency functor.
    PriorityDispatchQueue<DispatchTask, DispatchTaskPriority> dispatchQueue;
    dispatchQueue.push({5, "TRK-101", "Deliver ration kits and blankets"});
    dispatchQueue.push({10, "HEL-201", "Deliver trauma kits to critical-care site"});
    dispatchQueue.push({7, "HYB-401", "Transfer medical team across river corridor"});
    std::cout << "\n--- Priority dispatch queue ---\n";
    while (!dispatchQueue.empty()) {
        const DispatchTask task = dispatchQueue.top(); dispatchQueue.pop();
        std::cout << "Priority " << task.priority << " | " << task.vehicleId
                  << " | " << task.instruction << "\n";
    }

    std::vector<MedicalNeed> needs = {
        {"Village Clinic A", 12, 3, 45},
        {"District Hospital", 20, 5, 30},
        {"Village Clinic B", 7, 4, 18}
    };
    std::sort(needs.begin(), needs.end(), UrgentMedicalNeedFirst());
    std::cout << "\n--- Urgent medical needs (custom functor + std::for_each) ---\n";
    std::for_each(needs.begin(), needs.end(), [](const MedicalNeed& need) {
        std::cout << std::left << std::setw(22) << need.facility
                  << " patients=" << std::setw(3) << need.patients
                  << " severity=" << need.severity << " priority=" << need.priorityScore()
                  << " available=" << need.availableStock << "\n";
    });

    // Save and restore binary transit and manifest audit files.
    BinaryAuditStore::saveTransit("transit_logs.bin", transitRecords);
    BinaryAuditStore::saveManifestAudit("manifest_audit.bin", manifestRecords);
    const std::vector<TransitLogRecord> restoredTransit =
        BinaryAuditStore::restoreTransit("transit_logs.bin");
    const std::vector<ManifestAuditRecord> restoredManifest =
        BinaryAuditStore::restoreManifestAudit("manifest_audit.bin");
    std::cout << "\nBinary audit round-trip: " << restoredTransit.size()
              << " transit records, " << restoredManifest.size() << " manifest line items\n";

    // Dispatch uses a full preflight before deducting any depot stock.
    try {
        fleet.vehicles[0]->dispatch(depot);
        std::cout << "Ground convoy successfully dispatched.\n";
        fleet.vehicles[2]->dispatch(depot);
        std::cout << "Amphibious emergency carrier successfully dispatched under commander override.\n";
    } catch (const std::exception& error) {
        std::cout << "Dispatch exception: " << error.what() << "\n";
    }

    std::cout << "\n--- Tabular manifest (custom I/O manipulator) ---\n";
    std::cout << manifestTable << *fleet.vehicles[0] << manifestPlain;
    std::cout << "\n--- Updated depot inventory ---\n" << depot;

    // Deleting through ReliefVehicle* proves the virtual destructor chain works.
    fleet.clear();
    std::cout << "\nLive vehicle objects after fleet shutdown: "
              << TransportVehicle::getOperationalVehicles() << "\n";
    std::cout << "Total dispatched tonnage: " << std::fixed << std::setprecision(3)
              << TransportVehicle::getDispatchedTonnes() << " metric tonnes\n";
    return 0;
}
} // namespace

int main(int argc, char* argv[]) {
    if (argc > 1 && std::string(argv[1]) == "--self-test") {
        bool passed = runSelfTests();
        if (ReliefOps::Logistics::TransportVehicle::getOperationalVehicles() != 0) passed = false;
        std::cout << (passed ? "ALL SELF-TESTS PASSED\n" : "SELF-TEST FAILURE\n");
        return passed ? 0 : 1;
    }
    try { return runDemo(); }
    catch (const std::exception& error) {
        std::cerr << "Fatal ERL-HUB error: " << error.what() << "\n";
        return 1;
    }
}
