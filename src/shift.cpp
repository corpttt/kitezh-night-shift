#include "kitezh/shift.hpp"

#include "kitezh/geometry.hpp"
#include "kitezh/lock.hpp"

namespace kitezh {

namespace {

bool callSignUsed(const std::vector<std::string>& names, const std::string& name) {
    for (size_t i = 0; i < names.size(); i++) {
        if (names[i] == name) {
            return true;
        }
    }
    return false;
}

// кого пускаем в ночь, кого сразу в refused
void pickWaiting(
    const std::vector<Vessel>& queue,
    std::vector<Vessel>& waiting,
    std::vector<Refusal>& refused
) {
    std::vector<std::string> used;

    for (size_t i = 0; i < queue.size(); i++) {
        Vessel cur = queue[i];

        if (callSignUsed(used, cur.call_sign)) {
            Refusal r;
            r.call_sign = cur.call_sign;
            r.reason = "duplicate";
            refused.push_back(r);
            continue;
        }
        used.push_back(cur.call_sign);

        if (validateVessel(cur)) {
            Refusal r;
            r.call_sign = cur.call_sign;
            r.reason = "invalid";
            refused.push_back(r);
            continue;
        }

        if (cur.draft_dm > kLowDepthDm) {
            Refusal r;
            r.call_sign = cur.call_sign;
            r.reason = "draft";
            refused.push_back(r);
            continue;
        }

        waiting.push_back(cur);
    }
}

// первая партия из текущего хвоста очереди
std::vector<Vessel> nextParty(std::vector<Vessel>& waiting) {
    std::vector<Vessel> party;
    if (waiting.empty()) {
        return party;
    }

    party.push_back(waiting[0]);
    waiting.erase(waiting.begin());

    // негабарит едет один
    if (party[0].kind == VesselKind::Oversized) {
        return party;
    }

    Direction d = party[0].direction;
    bool gap = false;  // кого-то уже пропустили в этой партии
    std::vector<Vessel> rest;

    for (size_t i = 0; i < waiting.size(); i++) {
        Vessel cur = waiting[i];
        bool take = false;

        if (cur.direction == d && cur.kind != VesselKind::Oversized) {
            if (fitsInChamber(party, cur)) {
                if (!gap || cur.kind == VesselKind::Ambulance) {
                    take = true;
                }
            }
        }

        if (take) {
            party.push_back(cur);
        } else {
            gap = true;
            rest.push_back(cur);
        }
    }

    waiting = rest;
    return party;
}

void moveParty(Lock& lock, const std::vector<Vessel>& party, ShiftReport& out) {
    Direction d = party[0].direction;

    Gate in_gate;
    Gate out_gate;
    Level need_level;
    if (d == Direction::Upstream) {
        in_gate = Gate::Lower;
        out_gate = Gate::Upper;
        need_level = Level::Low;
    } else {
        in_gate = Gate::Upper;
        out_gate = Gate::Lower;
        need_level = Level::High;
    }

    if (lock.level() != need_level) {
        lock.pump();
    }

    lock.open(in_gate);
    for (size_t i = 0; i < party.size(); i++) {
        lock.enter(party[i]);
    }
    lock.close(in_gate);

    lock.pump();

    lock.open(out_gate);
    for (size_t i = 0; i < party.size(); i++) {
        lock.exit(party[i].call_sign);
        out.passed.push_back(party[i].call_sign);
    }
    lock.close(out_gate);
}

}  // namespace

ShiftReport runNightShift(const std::vector<Vessel>& queue) {
    ShiftReport out;
    std::vector<Vessel> waiting;
    pickWaiting(queue, waiting, out.refused);

    Lock lock;
    while (!waiting.empty()) {
        std::vector<Vessel> party = nextParty(waiting);
        moveParty(lock, party, out);
    }

    out.pump_cycles = lock.pumpCycles();
    return out;
}

}  // namespace kitezh
