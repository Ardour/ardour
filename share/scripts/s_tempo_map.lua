ardour { ["type"] = "Snippet", name = "tempo map examples" }

function factory () return function ()

	-- query BPM at 00:00:10:00
	local tp = Temporal.timepos_t.from_superclock (Temporal.superclock_ticks_per_second * Session:nominal_sample_rate () * 10)
	local tm = Temporal.TempoMap.use ()
	print (tm:tempo_at (tp):quarter_notes_per_minute ())

	-- set initial tempo to 140, ramp to 120 over the first 4/4 bar, then continue at BPM 80
	Temporal.TempoMap.fetch_writable ()
	local tm = Temporal.TempoMap.use ()
	tm:set_tempo (Temporal.Tempo (140, 120, 4), Temporal.timepos_t ())
	tm:set_tempo (Temporal.Tempo (80, 80, 4), Temporal.timepos_t.from_ticks (Temporal.ticks_per_beat * 4))
	Temporal.TempoMap.update (tm)
	tm = nil

	-- Abort Edit example
	-- after every call to Temporal.TempoMap.fetch_writable ()
	-- there must be a matching call to
	-- Temporal.TempoMap.update() or Temporal.TempoMap.abort_update()
	Temporal.TempoMap.fetch_writable()
	Temporal.TempoMap.abort_update()

end end
