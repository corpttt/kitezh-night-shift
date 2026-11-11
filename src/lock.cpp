#include "kitezh/lock.hpp"

#include "kitezh/geometry.hpp"

namespace kitezh {

int Lock::depthNow() const {
    if (water_ == Level::Low) {
        return kLowDepthDm;
    }
    return kHighDepthDm;
}

GateState& Lock::gateRef(Gate which) {
    if (which == Gate::Lower) {
        return lower_gate_;
    }
    return upper_gate_;
}

Lock::Lock() {
    water_ = Level::Low;
    lower_gate_ = GateState::Closed;
    upper_gate_ = GateState::Closed;
    pumps_done_ = 0;
}

Level Lock::level() const {
    return water_;
}

GateState Lock::gate(Gate which) const {
    if (which == Gate::Lower) {
        return lower_gate_;
    }
    return upper_gate_;
}

int Lock::pumpCycles() const {
    return pumps_done_;
}

std::vector<Vessel> Lock::inside() const {
    return chamber_;
}

void Lock::open(Gate which) {
    GateState& g = gateRef(which);
    if (g == GateState::Open) {
        return;
    }

    //оба сразу открыть нельзя
    if (which == Gate::Lower and upper_gate_ == GateState::Open) {
        throw LockError("cannot open");
    }
    if (which == Gate::Upper and lower_gate_ == GateState::Open) {
        throw LockError("cannot open");
    }

    //нижний — только на нижней воде верхний — на верхней
    if (which == Gate::Lower) {
        if (water_ != Level::Low) {
            throw LockError("cannot open");
        }
    } else {
        if (water_ != Level::High) {
            throw LockError("cannot open");
        }
    }

    g = GateState::Open;
}

void Lock::close(Gate which) {
    gateRef(which) = GateState::Closed;
}

void Lock::pump() {
    if (lower_gate_ == GateState::Open) {
        throw LockError("gates must be shut");
    }
    if (upper_gate_ == GateState::Open) {
        throw LockError("gates must be shut");
    }

    Level to;
    if (water_ == Level::Low) {
        to = Level::High;
    } else {
        to = Level::Low;
    }

    int new_depth;
    if (to == Level::Low) {
        new_depth = kLowDepthDm;
    } else {
        new_depth = kHighDepthDm;
    }

    for (size_t i = 0; i < chamber_.size(); i++) {
        if (chamber_[i].draft_dm > new_depth) {
            throw LockError("too deep draft after pump");
        }
    }

    water_ = to;
    ++pumps_done_;
}

void Lock::enter(const Vessel& v) {
    if (validateVessel(v)) {
        throw LockError("bad vessel");
    }

    for (size_t i = 0; i < chamber_.size(); i++) {
        if (chamber_[i].call_sign == v.call_sign) {
            throw LockError("already inside");
        }
    }

    if (v.draft_dm > depthNow()) {
        throw LockError("draft");
    }

    //входной затвор: вверх—нижний, вниз—верхний
    Gate need;
    if (v.direction == Direction::Upstream) {
        need = Gate::Lower;
    } else {
        need = Gate::Upper;
    }
    if (gate(need) != GateState::Open) {
        throw LockError("entry closed");
    }

    if (v.kind == VesselKind::Oversized) {
        if (!chamber_.empty()) {
            throw LockError("oversized alone");
        }
    }
    for (size_t i = 0; i < chamber_.size(); i++) {
        if (chamber_[i].kind == VesselKind::Oversized) {
            throw LockError("oversized alone");
        }
    }

    if (!fitsInChamber(chamber_, v)) {
        throw LockError("no space");
    }

    chamber_.push_back(v);
}

void Lock::exit(const std::string& call_sign) {
    if (chamber_.empty()) {
        throw LockError("nobody inside");
    }

    Vessel head = chamber_[0];
    if (head.call_sign != call_sign) {
        throw LockError("not first");
    }

    Gate need;
    if (head.direction == Direction::Upstream) {
        need = Gate::Upper;
    } else {
        need = Gate::Lower;
    }
    if (gate(need) != GateState::Open) {
        throw LockError("exit closed");
    }

    chamber_.erase(chamber_.begin());
}

}  //namespace kitezh
