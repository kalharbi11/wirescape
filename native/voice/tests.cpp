// SPDX-License-Identifier: MIT
#include "allocator.h"

#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <initializer_list>

#define CHECK(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "voice check failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
    std::fflush(stderr); std::abort(); \
} } while (false)

using namespace wirescape::voice;

static Note n(uint32_t source, int id, int key, int channel = 0) {
    Note note{};
    note.source_id = source;
    note.note_id = id;
    note.key = static_cast<int16_t>(key);
    note.channel = static_cast<int16_t>(channel);
    return note;
}

int main() {
    {
        Allocator a({3, Mode::Rotate});
        CHECK(a.note_on(n(1, 1, 60)).voice == 0);
        CHECK(a.note_on(n(1, 2, 62)).voice == 1);
        CHECK(a.note_off(n(1, 1, 60)) == Status::Ok);
        CHECK(a.note_on(n(1, 3, 64)).voice == 2);
        CHECK(a.note_on(n(1, 4, 65)).voice == 0);
        CHECK(a.note_on(n(1, 5, 67)).stolen); // oldest active is voice 1.
        CHECK(a.voice(1).note_id == 5);
        CHECK(a.note_off(n(1, 2, 62)) == Status::NotFound);
        CHECK(a.active_count() == 3);
        CHECK(std::fabs(a.voice(0).pitch - 5.0f / 12.0f) < 1e-7f);
        a.clear_triggers();
        CHECK(!a.voice(0).trigger && !a.voice(0).retrigger);
        a.reset();
        CHECK(a.active_count() == 0 && a.release_count() == 0 && a.held_count() == 0);
        CHECK(a.note_on(n(1, 1, 60)).voice == 0);
    }
    {
        Allocator a({3, Mode::Reuse});
        CHECK(a.note_on(n(1, 1, 60)).voice == 0);
        CHECK(a.note_on(n(1, 2, 62)).voice == 1);
        CHECK(a.note_off(n(1, 1, 60)) == Status::Ok);
        CHECK(a.note_on(n(1, 3, 60)).voice == 0); // source/key reuse.
        CHECK(a.voice(0).retrigger);
        CHECK(a.note_off(n(1, 3, 60)) == Status::Ok);
        CHECK(a.note_on(n(2, 4, 60)).voice == 0); // free slot, new source.
        CHECK(a.note_off(n(1, 4, 60)) == Status::NotFound);
        CHECK(a.voice(0).source_id == 2);
    }
    {
        Allocator a({3, Mode::Reset});
        CHECK(a.note_on(n(1, 1, 60)).voice == 0);
        CHECK(a.note_on(n(1, 2, 61)).voice == 1);
        CHECK(a.note_off(n(1, 1, 60)) == Status::Ok);
        CHECK(a.note_on(n(1, 3, 62)).voice == 0); // lowest free.
        CHECK(a.note_on(n(1, 4, 63)).voice == 2);
        CHECK(a.note_on(n(1, 5, 64)).voice == 1); // oldest active.
        CHECK(a.voice(1).note_id == 5);
    }
    {
        Allocator a({2, Mode::Rotate});
        a.note_on(n(10, 7, 60));
        a.note_on(n(20, 7, 60));
        CHECK(a.note_off(n(10, 7, 10)) == Status::Ok); // note_id controls match.
        CHECK(a.voice(1).gate && a.voice(1).source_id == 20);
        CHECK(a.sustain(20, true) == Status::Ok);
        CHECK(a.note_off(n(20, 7, 60)) == Status::Ok);
        CHECK(a.voice(1).gate);
        CHECK(a.sustain(20, false) == Status::Ok);
        CHECK(!a.voice(1).gate && a.release_count() == 2);
        a.note_on(n(10, -1, 61, 2));
        CHECK(a.note_off(n(10, -1, 61, 1)) == Status::NotFound);
        CHECK(a.note_off(n(10, -1, 61, 2)) == Status::Ok);
    }
    {
        Allocator a({2, Mode::Reset});
        a.note_on(n(1, 1, 60));
        a.note_on(n(2, 1, 64));
        CHECK(a.sustain(1, true) == Status::Ok);
        a.note_off(n(1, 1, 60));
        a.release_all_from_source(1);
        CHECK(!a.voice(0).gate && a.voice(1).gate);
        CHECK(a.note_off(n(1, 1, 60)) == Status::NotFound);
        CHECK(a.note_off(n(2, 1, 64)) == Status::Ok);
        CHECK(a.held_count() == 0);
    }
    {
        Allocator a({2, Mode::Rotate});
        CHECK(a.sustain(1, true) == Status::Ok);
        CHECK(a.note_on(n(1, 1, 60)).voice == 0);
        CHECK(a.note_on(n(2, 2, 64)).voice == 1);
        CHECK(a.note_off(n(1, 1, 60)) == Status::Ok);
        CHECK(a.voice(0).gate); // Pedal-held voice remains a steal candidate.
        const Result stolen = a.note_on(n(3, 3, 67));
        CHECK(stolen.status == Status::Ok && stolen.stolen && stolen.voice == 0);
        CHECK(a.sustain(1, false) == Status::Ok);
        CHECK(a.voice(0).gate && a.voice(0).source_id == 3);
        CHECK(a.note_off(n(1, 1, 60)) == Status::NotFound);
    }
    for (Priority priority : {Priority::Last, Priority::Low, Priority::High}) {
        Allocator a({1, Mode::Mono, priority, false});
        a.note_on(n(1, 1, 60));
        a.clear_triggers();
        a.note_on(n(1, 2, 64));
        a.note_on(n(1, 3, 57));
        const int expected = priority == Priority::Last || priority == Priority::Low ? 57 : 64;
        CHECK(a.voice(0).key == expected);
        a.note_off(n(1, 3, 57));
        CHECK(a.voice(0).key == (priority == Priority::Low ? 60 : 64));
        a.note_off(n(1, 2, 64));
        CHECK(a.voice(0).key == 60 && a.voice(0).gate && a.voice(0).trigger);
        a.note_off(n(1, 1, 60));
        CHECK(!a.voice(0).gate);
    }
    {
        Allocator a({1, Mode::Mono, Priority::Last, true});
        a.note_on(n(1, 1, 60));
        a.clear_triggers();
        a.note_on(n(1, 2, 62));
        CHECK(a.voice(0).key == 62 && !a.voice(0).trigger);
        a.note_off(n(1, 2, 62));
        CHECK(a.voice(0).key == 60 && !a.voice(0).trigger);
        a.note_off(n(1, 1, 60));
        a.note_on(n(1, 3, 64));
        CHECK(a.voice(0).trigger);
    }
    {
        Allocator a({1, Mode::Mono});
        for (int i = 0; i < kMaxHeldNotes; ++i)
            CHECK(a.note_on(n(1, i, i % 128)).status == Status::Ok);
        CHECK(a.note_on(n(1, 200, 60)).status == Status::NoteCapacity);
        CHECK(a.held_count() == kMaxHeldNotes);
        a.reset();
        for (int i = 0; i < kMaxSustainSources; ++i)
            CHECK(a.sustain(static_cast<uint32_t>(i), true) == Status::Ok);
        CHECK(a.sustain(999, true) == Status::SourceCapacity);
        CHECK(a.configure({0, Mode::Mono}) == Status::InvalidConfig);
        CHECK(a.config().max_voices == 1);
    }
    {
        Allocator a({1, Mode::Reset});
        a.note_on(n(1, 1, 69));
        CHECK(a.expression(n(1, 1, 69), .4f, .7f) == Status::Ok);
        CHECK(a.voice(0).pressure == .4f && a.voice(0).timbre == .7f);
        CHECK(a.note_on(n(1, 2, 70)).stolen);
        CHECK(a.expression(n(1, 1, 69), .5f, .5f) == Status::NotFound);
        CHECK(a.note_on(n(1, 2, 71)).status == Status::Ok); // duplicate id replaces.
        CHECK(a.held_count() == 1 && a.voice(0).key == 71);
    }
}
