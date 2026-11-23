// Post-Earthquake Multi-Agency Emergency Relief Logistics Hub (ERL-HUB)
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <cmath>
using namespace std;

namespace ReliefOps {

class IncidentCommander;

namespace Inventory {

class ReliefCargo {
    string cargoId, name;
    int    quantity;
    double unitWeightKg;
    bool   isMedical;

public:
    ReliefCargo(string id = "NONE", string n = "Unnamed", int q = 0,
                double w = 0.0, bool med = false)
        : cargoId(id), name(n), quantity(q), unitWeightKg(w), isMedical(med) {}

    string getId()         const { return cargoId; }
    int    getQuantity()   const { return quantity; }
    bool   getIsMedical()  const { return isMedical; }
    double getUnitWeight() const { return unitWeightKg; }
    inline double totalWeightKg() const { return quantity * unitWeightKg; }

    ReliefCargo withQuantity(int q) const { ReliefCargo c = *this; c.quantity = q; return c; }
    void addQuantity(int q)    { quantity += q; }
    bool removeQuantity(int q) {
        if (q > quantity) return false;
        quantity -= q;
        return true;
    }

    bool operator==(const ReliefCargo& o) const { return cargoId == o.cargoId; }

    friend ostream& operator<<(ostream& out, const ReliefCargo& c) {
        return out << left << setw(8) << c.cargoId << setw(22) << c.name
                   << right << setw(6) << c.quantity << " units  "
                   << setw(8) << fixed << setprecision(1) << c.totalWeightKg() << " kg"
                   << (c.isMedical ? "  [MEDICAL]" : "");
    }
    friend istream& operator>>(istream& in, ReliefCargo& c) {
        return in >> c.cargoId >> c.name >> c.quantity >> c.unitWeightKg >> c.isMedical;
    }
};

class DistributionDepot {
    string       depotName;
    ReliefCargo* stock;
    int          count, capacity, reservedMedical;

public:
    DistributionDepot(string name, int cap = 4)
        : depotName(name), stock(new ReliefCargo[cap]), count(0), capacity(cap), reservedMedical(0) {
        cout << "[+] Depot '" << depotName << "' created\n";
    }
    DistributionDepot(const DistributionDepot& o)
        : depotName(o.depotName + "-Backup"), stock(new ReliefCargo[o.capacity]),
          count(o.count), capacity(o.capacity), reservedMedical(o.reservedMedical) {
        for (int i = 0; i < count; i++) stock[i] = o.stock[i];
        cout << "[+] Depot copied as '" << depotName << "'\n";
    }
    ~DistributionDepot() {
        delete[] stock;
        cout << "[-] Depot '" << depotName << "' closed, memory released\n";
    }

    string getName() const { return depotName; }
    void reserveMedical(int units) { reservedMedical = units; }

    ReliefCargo& findCargo(const string& id) {
        for (int i = 0; i < count; i++)
            if (stock[i].getId() == id) return stock[i];
        throw string("Cargo " + id + " not found in " + depotName);
    }

    DistributionDepot& operator+=(const ReliefCargo& c) {
        for (int i = 0; i < count; i++)
            if (stock[i] == c) { stock[i].addQuantity(c.getQuantity()); return *this; }
        if (count == capacity) {
            ReliefCargo* bigger = new ReliefCargo[capacity *= 2];
            for (int i = 0; i < count; i++) bigger[i] = stock[i];
            delete[] stock;
            stock = bigger;
        }
        stock[count++] = c;
        return *this;
    }

    DistributionDepot& operator-=(const ReliefCargo& c) {
        ReliefCargo& item = findCargo(c.getId());
        int available = item.getQuantity() - (item.getIsMedical() ? reservedMedical : 0);
        if (c.getQuantity() > available) throw string("Insufficient stock of " + c.getId());
        item.removeQuantity(c.getQuantity());
        return *this;
    }

    friend ostream& operator<<(ostream& out, const DistributionDepot& d) {
        out << "\n=== Depot: " << d.depotName << " ===\n";
        for (int i = 0; i < d.count; i++) out << "  " << d.stock[i] << "\n";
        return out << "  Reserved medical units: " << d.reservedMedical << "\n";
    }

    friend class ReliefOps::IncidentCommander;
};

} // namespace Inventory

namespace Logistics {

using Inventory::ReliefCargo;
using Inventory::DistributionDepot;

inline double distanceKm(double x1, double y1, double x2, double y2) { return hypot(x2 - x1, y2 - y1); }
inline double kgToTonnes(double kg) { return kg / 1000.0; }
inline double travelHours(double km, double speedKmph) { return km / speedKmph; }

class TransportVehicle {
    string       vehicleId, type;
    double       maxPayloadKg, speedKmph;
    ReliefCargo* manifest;
    int          itemCount, maxItems;
    bool         overloadAllowed;

    static int    totalOperationalVehicles;
    static double totalDispatchedTonnes;

public:
    TransportVehicle(string id = "V-00", string t = "Truck",
                     double payload = 1000, double speed = 40, int slots = 5)
        : vehicleId(id), type(t), maxPayloadKg(payload), speedKmph(speed),
          manifest(new ReliefCargo[slots]), itemCount(0), maxItems(slots), overloadAllowed(false) {
        totalOperationalVehicles++;
    }
    TransportVehicle(const TransportVehicle& o)
        : vehicleId(o.vehicleId + "-C"), type(o.type), maxPayloadKg(o.maxPayloadKg),
          speedKmph(o.speedKmph), manifest(new ReliefCargo[o.maxItems]),
          itemCount(o.itemCount), maxItems(o.maxItems), overloadAllowed(false) {
        for (int i = 0; i < itemCount; i++) manifest[i] = o.manifest[i];
        totalOperationalVehicles++;
    }
    ~TransportVehicle() {
        delete[] manifest;
        totalOperationalVehicles--;
    }

    double getSpeed() const { return speedKmph; }

    inline double currentLoadKg() const {
        double sum = 0;
        for (int i = 0; i < itemCount; i++) sum += manifest[i].totalWeightKg();
        return sum;
    }

    bool loadCargo(const ReliefCargo& c) {
        for (int i = 0; i < itemCount; i++)
            if (manifest[i] == c) {
                cout << "  ! Duplicate cargo " << c.getId() << " rejected on " << vehicleId << "\n";
                return false;
            }
        if (!overloadAllowed && currentLoadKg() + c.totalWeightKg() > maxPayloadKg) {
            cout << "  ! " << c.getId() << " exceeds payload of " << vehicleId << "\n";
            return false;
        }
        if (itemCount == maxItems) {
            cout << "  ! Manifest of " << vehicleId << " is full\n";
            return false;
        }
        manifest[itemCount++] = c;
        return true;
    }

    void dispatch(DistributionDepot& depot) {
        for (int i = 0; i < itemCount; i++) depot -= manifest[i];
        double t = kgToTonnes(currentLoadKg());
        totalDispatchedTonnes += t;
        cout << "  >> " << vehicleId << " dispatched from " << depot.getName()
             << " carrying " << fixed << setprecision(2) << t << " t\n";
    }

    bool operator==(const TransportVehicle& o) const {
        if (vehicleId == o.vehicleId) return true;
        for (int i = 0; i < itemCount; i++)
            for (int j = 0; j < o.itemCount; j++)
                if (manifest[i] == o.manifest[j]) return true;
        return false;
    }

    static int    getOperationalVehicles() { return totalOperationalVehicles; }
    static double getDispatchedTonnes()    { return totalDispatchedTonnes; }

    friend ostream& operator<<(ostream& out, const TransportVehicle& v) {
        out << "\n--- Manifest: " << v.vehicleId << " (" << v.type << ") ---\n";
        for (int i = 0; i < v.itemCount; i++) out << "  " << v.manifest[i] << "\n";
        return out << "  Load: " << fixed << setprecision(1) << v.currentLoadKg()
                   << " / " << v.maxPayloadKg << " kg\n";
    }
    friend istream& operator>>(istream& in, TransportVehicle& v) {
        return in >> v.vehicleId >> v.type >> v.maxPayloadKg >> v.speedKmph;
    }

    friend class ReliefOps::IncidentCommander;
};

int    TransportVehicle::totalOperationalVehicles = 0;
double TransportVehicle::totalDispatchedTonnes    = 0.0;

// Overloaded allocation: by payload weight (kg) ...
bool allocateSupply(DistributionDepot& depot, TransportVehicle& v,
                    const string& cargoId, double payloadKg) {
    ReliefCargo& src = depot.findCargo(cargoId);
    int units = (int)(payloadKg / src.getUnitWeight());
    cout << "[Volume] " << cargoId << " : " << payloadKg << " kg -> " << units << " units\n";
    return v.loadCargo(src.withQuantity(units));
}

// ... or by medical triage (patients x severity)
bool allocateSupply(DistributionDepot& depot, TransportVehicle& v,
                    const string& cargoId, int patients, int severityLevel) {
    ReliefCargo& src = depot.findCargo(cargoId);
    int units = patients * severityLevel;
    cout << "[Triage] " << cargoId << " : " << patients << " patients x severity "
         << severityLevel << " -> " << units << " units\n";
    return v.loadCargo(src.withQuantity(units));
}

void updateMission(double& remainingKm, double& fuelPercent, double travelledKm) {
    remainingKm = max(0.0, remainingKm - travelledKm);
    fuelPercent -= travelledKm * 0.4;
}

} // namespace Logistics

class IncidentCommander {
    string name;
public:
    IncidentCommander(string n) : name(n) {}

    void releaseMedicalReserve(Inventory::DistributionDepot& depot) {
        cout << "  ** Commander " << name << " released " << depot.reservedMedical
             << " reserved medical units at " << depot.depotName << "\n";
        depot.reservedMedical = 0;
    }
    void authorizeOverload(Logistics::TransportVehicle& v) {
        v.overloadAllowed = true;
        cout << "  ** Commander " << name << " authorised overload on " << v.vehicleId << "\n";
    }
};

} // namespace ReliefOps

using namespace ReliefOps;
using namespace ReliefOps::Inventory;
using namespace ReliefOps::Logistics;

int main() {
    cout << "========== ERL-HUB : Emergency Relief Logistics ==========\n\n";

    DistributionDepot depot("Bhuj-Central");
    depot += ReliefCargo("RAT-01", "Ration_Kit",        500, 5.0,  false);
    depot += ReliefCargo("MED-01", "Trauma_Triage_Kit", 200, 2.5,  true);
    depot += ReliefCargo("WAT-01", "Water_Tank_20L",    300, 20.0, false);
    depot += ReliefCargo("BLK-01", "Thermal_Blanket",   400, 1.5,  false);
    depot += ReliefCargo("RAT-01", "Ration_Kit",        100, 5.0,  false); // restock
    depot.reserveMedical(150);
    cout << depot;

    istringstream record("FUEL-01 Diesel_Can 50 18.0 0");
    ReliefCargo fuel;
    record >> fuel;
    depot += fuel;
    cout << "\nCargo read from manifest record:\n  " << fuel << "\n";

    cout << "\n---------- Fleet ----------\n";
    TransportVehicle truck("TRK-101", "AllTerrain_Truck", 3000, 35, 5);
    TransportVehicle heli ("HEL-201", "Medical_Helicopter", 400, 180, 3);
    TransportVehicle boat;
    istringstream("BOT-301 Amphibious_Boat 800 25") >> boat;
    cout << "Vehicles operational: " << TransportVehicle::getOperationalVehicles() << "\n";

    cout << "\n---------- Allocation ----------\n";
    allocateSupply(depot, truck, "RAT-01", 1500.0);
    allocateSupply(depot, truck, "WAT-01", 1000.0);
    allocateSupply(depot, heli,  "MED-01", 20, 3);
    allocateSupply(depot, heli,  "MED-01", 5, 2);     // duplicate -> rejected
    allocateSupply(depot, boat,  "BLK-01", 300.0);

    cout << "\nTruck == Heli ? " << (truck == heli ? "Conflict" : "Distinct") << "\n";
    {
        TransportVehicle convoy = truck;
        cout << "Convoy == Truck ? " << (convoy == truck ? "Duplicate cargo" : "Distinct") << "\n";
        cout << "Vehicles operational (with convoy): " << TransportVehicle::getOperationalVehicles() << "\n";
    }
    cout << "Vehicles operational (convoy stood down): " << TransportVehicle::getOperationalVehicles() << "\n";

    cout << "\n---------- Critical Triage ----------\n";
    IncidentCommander commander("Col. Rathore");
    try {
        heli.dispatch(depot);
    } catch (string& err) {
        cout << "  ! Dispatch blocked: " << err << "\n";
        commander.releaseMedicalReserve(depot);
        heli.dispatch(depot);
    }
    commander.authorizeOverload(boat);
    allocateSupply(depot, boat, "FUEL-01", 900.0);

    cout << "\n---------- Dispatch ----------\n";
    truck.dispatch(depot);
    boat.dispatch(depot);
    cout << truck << heli << boat;

    depot.findCargo("BLK-01").addQuantity(250);
    cout << "\nAirdrop added 250 blankets (edited via reference)\n";

    cout << "\n---------- Mission Tracking ----------\n";
    double distance = distanceKm(0, 0, 48, 36), fuelPct = 100;
    cout << "Distance to Village-7 : " << distance << " km\n";
    cout << "Truck ETA            : " << travelHours(distance, truck.getSpeed()) << " h\n";
    cout << "Helicopter ETA       : " << travelHours(distance, heli.getSpeed()) << " h\n";
    updateMission(distance, fuelPct, 25);
    cout << "After 25 km -> remaining " << distance << " km, fuel " << fuelPct << "%\n";

    DistributionDepot backup = depot;
    cout << depot;

    cout << "\n========== Logistics Analytics ==========\n";
    cout << "Total operational vehicles : " << TransportVehicle::getOperationalVehicles() << "\n";
    cout << "Total dispatched tonnage   : " << fixed << setprecision(2)
         << TransportVehicle::getDispatchedTonnes() << " t\n\n";
}
