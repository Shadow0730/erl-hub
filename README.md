# ERL-HUB

**Post-Earthquake Multi-Agency Emergency Relief Logistics Hub**  
**MAIT | CIC-211 | Object-Oriented Programming Using C++**  
**Project Option:** 2  
**Language:** Standard C++17

## Project team

| Role | Name | Enrolment No. | Class |
|---|---|---|---|
| Student 1 | Sarthak Singh | 01296402725 | C-19 |
| Student 2 | Abhishek Gupta | 02796402725 | C-20 |
| Faculty | Dr. Ajay | - | - |

## Overview

ERL-HUB is a command-line simulation of a regional relief distribution hub following a major earthquake. It models relief inventory, depot stock, vehicle manifests, emergency medical allocation, route evaluation, transit records and priority-based dispatch.

The program is an academic simulation. It is not a certified emergency-response, navigation, fleet-control or medical system.

## Main features

- Relief cargo and depot inventory management, including medical reserves.
- Overloaded supply-allocation routines for payload mass and medical triage.
- Emergency override access through `IncidentCommander`.
- Ground, aerial, amphibious and hybrid transport vehicles.
- Virtual inheritance for the hybrid carrier and composition of `GPSBeacon` and `EmergencyLifeSupportUnit`.
- Runtime route evaluation through `ReliefVehicle*` base pointers.
- Custom exceptions for overweight cargo, fuel depletion and damaged corridors.
- Binary transit and manifest records with validation and rollback handling.
- `PriorityDispatchQueue<T>`, STL containers and algorithms, medical-priority functors, and manifest stream manipulators.

## Object-oriented programming requirements

| Unit | Implemented concepts |
|---|---|
| I - Core foundations | Namespaces, overloaded allocation functions, call/return by reference, inline utilities and friend-class emergency access. |
| II - Encapsulation and operators | Encapsulated classes, static analytics, dynamic arrays, deep copying, destructors, and `+=`, `-=`, `==`, `<<`, `>>` operators. |
| III - Inheritance and polymorphism | Abstract vehicle base, derived transport modes, virtual inheritance, composition, base-pointer dispatch and virtual destruction. |
| IV - Streams, exceptions and STL | `std::exception` subclasses, binary `std::fstream` persistence, `std::vector`, `std::unordered_map`, algorithms, templates, functors and custom stream manipulators. |

## Build and run

A C++17 compiler is required. No third-party libraries are used.

**Linux / macOS (GCC or Clang)**

```bash
g++ -std=c++17 -Wall -Wextra -Wpedantic erl_hub.cpp -o erl_hub
./erl_hub
./erl_hub --self-test
```

**Windows (MinGW-w64 / PowerShell)**

```powershell
g++ -std=c++17 -Wall -Wextra -Wpedantic erl_hub.cpp -o erl_hub.exe
.\erl_hub.exe
.\erl_hub.exe --self-test
```

## Generated data files

A normal run creates `transit_logs.bin` and `manifest_audit.bin` in the current working directory. These are generated binary records, not source files. The program validates the record header, version, record count, file size and checksum when restoring them.

## Project files

```text
erl-hub/
├── .gitignore
├── README.md
├── erl_hub.cpp
└── docs/
    └── ERL_HUB_Project_Report.pdf
```

See `docs/ERL_HUB_Project_Report.pdf` for the technical design, implementation details, requirements mapping, workflow, test coverage and project limitations.
