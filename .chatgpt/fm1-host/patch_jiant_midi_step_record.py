from pathlib import Path
import sys

path = Path(sys.argv[1] if len(sys.argv) > 1 else "hostsrc/JIANT/firmware/src/ui_input.c")
text = path.read_text(encoding="utf-8")


def replace_once(old: str, new: str, label: str) -> None:
    global text
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected exactly one match, found {count}")
    text = text.replace(old, new, 1)


# Treat held external MIDI notes like held local keys while stopped STEP recording
# is active. This also makes the existing LEN/ARP entry controls work while a
# MIDI keyboard is held.
replace_once(
'''    uint32_t k, len = (uint32_t)t->p[P_SLEN], keys = fm1_in.notes & ~kb_layer;
    uint64_t m = 0;
    if (ui.home || ui.menu || ui.confirm || ui.layer || name_on() || !song.seq_mode || cur_page()->graph != GR_ROLL ||
        !keys)
        return 0;
''',
'''    uint32_t k, len = (uint32_t)t->p[P_SLEN], keys = fm1_in.notes & ~kb_layer;
    uint32_t midi_keys = midi_owners[song.sel % NTRK] != 0u;
    uint64_t m = 0;
    if (ui.home || ui.menu || ui.confirm || ui.layer || name_on() || !song.seq_mode || cur_page()->graph != GR_ROLL ||
        (!keys && !midi_keys))
        return 0;
''',
"lock_held MIDI support",
)

# Local key entry used to commit as soon as the FM-1 keys were up. Keep the
# entry open while external MIDI still holds notes on the selected track.
replace_once(
'''    if (ui.entry_open && !(fm1_in.notes & ~kb_layer))
        ent_commit();                                   /* (let go: LEN / the arpeggio written, the cursor on) */
}
''',
'''    if (ui.entry_open && !(fm1_in.notes & ~kb_layer) && !midi_owners[ti])
        ent_commit();                                   /* (let go: LEN / the arpeggio written, the cursor on) */
}

/* VST/web host: incoming MIDI uses midi_control.c rather than the physical
 * key matrix. While STEP recording is armed and stopped, mirror the notes held
 * on the selected track into the same entry object as the FM-1 keys. ent_note
 * de-duplicates notes, so scanning held MIDI each UI pass is safe; the shared
 * release test above commits only after both local and MIDI keys are up. */
static void seq_entry_midi(void)
{
    track_t *t = TSEL;
    step_t *st = &t->step[ui.cursor];
    uint32_t note, ti = song.sel % NTRK;
    if (!midi_owners[ti])
        return;
    for (note = 0; note < 128u; note++)
        if (midi_note_held(t, note)) {
            ent_note(t, st, note);
            last_note = (uint8_t)note;
        }
}
''',
"MIDI entry helper",
)

# Feed external MIDI into the step-entry object before the normal local-key
# edge handler. This branch is reached only when the selected track is armed,
# STEP is open, the transport is stopped, and no song chain is busy.
replace_once(
'''        else if (!chain_busy())
            seq_entry(notes);                           /* armed and stopped: step recording at the cursor */
        else if (notes)
''',
'''        else if (!chain_busy()) {
            seq_entry_midi();                           /* external MIDI: same stopped step entry */
            seq_entry(notes);                           /* FM-1 / on-screen keys */
        } else if (notes)
''',
"STEP entry call site",
)

path.write_text(text, encoding="utf-8")
print(f"Patched external MIDI STEP recording in {path}")
