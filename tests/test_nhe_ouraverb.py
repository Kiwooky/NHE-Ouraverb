#!/usr/bin/env python3
"""Tests for Ouraverb (outdoor spaces). Traits: [every], [buffer], [tail], [feedback], [mix], stereo out.
One test per promise in docs/spec.md."""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
for d in (os.environ.get('PB_TOOLS', ''), os.path.join(HERE, '..', 'tools')):
    if d and os.path.exists(os.path.join(d, 'lv2test.py')):
        sys.path.insert(0, d); break
import numpy as np
from scipy.signal import find_peaks
from lv2test import Plugin, check, done, db, rms, centroid, pluck_train, tick_count, max_step

ORDER = ['place', 'expanse', 'surface', 'air', 'bounce', 'cover', 'cover_density', 'cover_height', 'mix', 'tone', 'tails', 'input_level', 'lv2_enabled']
p = Plugin(order=ORDER,
           defaults=dict(place=0, expanse=100, surface=8, air=5, bounce=4, cover=0, cover_density=3, cover_height=20,
                         mix=35, tone=5, tails=1, input_level=0, lv2_enabled=1), n_in=1, n_out=2)
sr = 48000
PLACES = ['Fjord', 'Canyon', 'Valley', 'Forest', 'Colosseum']
CLEAN = dict(mix=100, surface=10, bounce=0, air=10)   # bare geometry: mirror walls, no air, single bounce


def impulse(sr, secs, at=0.1, a=0.5):
    x = np.zeros(int(secs * sr)); x[int(at * sr)] = a
    return x


def echoes(y, sr, at=0.1, rel=0.05):
    """(time after the impulse, dB re loudest) of each distinct echo."""
    a = np.abs(y)
    pk, _ = find_peaks(a, height=a.max() * rel, distance=int(0.002 * sr))
    return [((k - at * sr) / sr, db(a[k] / a.max())) for k in pk]


def near(es, t, tol=0.002):
    return any(abs(e[0] - t) < tol for e in es)


def burst(sr, secs, at, dur=0.03, a=0.3, seed=0):
    x = np.zeros(int(secs * sr)); n = int(dur * sr)
    x[int(at * sr):int(at * sr) + n] = a * np.random.default_rng(seed).standard_normal(n) * np.hanning(n)
    return x


# --- the walls: Fjord's two rock faces, the echo ping-pongs left-right -----------------
for rate in (44100, 48000, 96000):
    y = p.run(impulse(rate, 4.0), rate, **CLEAN)
    eL, eR = echoes(y[:, 0], rate), echoes(y[:, 1], rate)
    tL = max(eL, key=lambda e: e[1])[0]; tR = max(eR, key=lambda e: e[1])[0]
    check('Fjord walls: left loudest at 2.6 s, right loudest at 3.2 s, %d Hz' % rate,
          abs(tL - 2.6) < 0.002 and abs(tR - 3.2) < 0.002, 'L loudest %.4f s, R loudest %.4f s' % (tL, tR))

# the water floor rides each wall echo, 7 ms later
y = p.run(impulse(sr, 3.0), sr, **CLEAN)[:, 0]
es = echoes(y, sr)
check('Fjord: water bounce 7 ms after the wall echo', near(es, 2.607, 0.0005), '%s' % [round(e[0], 4) for e in es if 2.5 < e[0] < 2.7])

# Expanse scales every time in the place
y = p.run(impulse(sr, 3.0), sr, expanse=50, **CLEAN)[:, 0]
es = echoes(y, sr)
check('Expanse 50 %: Fjord wall at 1.3 s, headland 0.42 -> 0.21 s', near(es, 1.3) and near(es, 0.21, 0.02),
      '%s' % [round(e[0], 3) for e in es][:6])

# Bounce: 0 is one answer per wall; up, the echo keeps crossing the fjord
def lap_energy(bounce, t0, t1):
    y = p.run(impulse(sr, t1 + 0.5), sr, mix=100, surface=10, air=10, bounce=bounce)
    return rms(y[int((0.1 + t0) * sr):int((0.1 + t1) * sr)])
b0, b10 = lap_energy(0, 5.6, 6.0), lap_energy(10, 5.6, 6.0)   # the second crossing (2.6 + 3.2 = 5.8 s)
check('Bounce 0: no second crossing; Bounce 10: a strong one', db(b10 / max(b0, 1e-9)) > 30 and db(b0) < -100,
      'second crossing %.0f dB (Bounce 0) vs %.0f dB (Bounce 10)' % (db(b0), db(b10)))

# --- Colosseum: the seating rows, a train whose spacing grows (a falling chirp) ----------
y = p.run(impulse(sr, 0.6), sr, place=4, **CLEAN)
es = echoes(y[:, 0] + y[:, 1], sr, rel=0.02)
steps = [e[0] for e in es if 0.034 < e[0] < 0.13]
gaps = np.diff(steps)
check('Colosseum: >= 12 row reflections, spacing growing (chirp falls)',
      len(steps) >= 12 and gaps[-3:].mean() > 1.3 * gaps[:3].mean(),
      '%d rows, first gaps %.1f ms, last %.1f ms (%.0f -> %.0f Hz)'
      % (len(steps), 1e3 * gaps[:3].mean(), 1e3 * gaps[-3:].mean(), 1 / gaps[:3].mean(), 1 / gaps[-3:].mean()))
y = p.run(impulse(sr, 1.5), sr, place=4, **CLEAN)
check('Colosseum: far side answers at 1.1 s', near(echoes(y[:, 1], sr), 1.1), '')

# --- Surface: rock (10) .. snow (0) ---------------------------------------------------------------
x = burst(sr, 4.5, 0.1)
def echo_seg(y, t, w=0.08):
    return y[int((0.1 + t) * sr):int((0.1 + t + w) * sr), 0]
yr = p.run(x, sr, mix=100, surface=10, bounce=0, air=10)
yf = p.run(x, sr, mix=100, surface=3, bounce=0, air=10)
ys = p.run(x, sr, mix=100, surface=0, bounce=0, air=10)
lr, lf, ls = (rms(echo_seg(v, 2.6)) for v in (yr, yf, ys))
cr, cs = centroid(echo_seg(yr, 2.6), sr), centroid(echo_seg(ys, 2.6), sr)
check('Surface: rock > foliage > snow in level, snow much darker',
      db(lr / lf) > 3 and db(lf / ls) > 3 and cs < 0.6 * cr,
      'foliage %.1f dB, snow %.1f dB re rock; centroid %.0f -> %.0f Hz' % (db(lf / lr), db(ls / lr), cr, cs))

# foliage smears an echo into a cloud: energy spreads past the echo's own length
def spread(y, t):
    seg = y[int((0.1 + t - 0.01) * sr):int((0.1 + t + 0.12) * sr), 0] ** 2
    c = np.cumsum(seg) / seg.sum()
    return (np.searchsorted(c, 0.9) - np.searchsorted(c, 0.1)) / sr
sp_r, sp_f = spread(yr, 2.6), spread(yf, 2.6)
check('Surface 3: the wall echo is smeared (90 % energy spread > 1.5x rock)', sp_f > 1.5 * sp_r,
      '%.0f ms vs %.0f ms' % (1e3 * sp_f, 1e3 * sp_r))

# --- Air: the far headland comes back darker than the near rock --------------------------
ya = p.run(x, sr, mix=100, surface=10, bounce=0, air=0)
c_near, c_far = centroid(echo_seg(ya, 0.42, 0.06), sr), centroid(echo_seg(ya, 3.7, 0.06), sr)
c_far0 = centroid(echo_seg(yr, 3.7, 0.06), sr)
check('Air 0 (dense): 3.7 s echo much darker than 0.42 s; Air 10 (open) keeps it bright',
      c_far < 0.6 * c_near and c_far0 > 1.5 * c_far,
      'near %.0f Hz, far %.0f Hz (Air 10: %.0f Hz)' % (c_near, c_far, c_far0))

# --- Canopy: Cover 100 at Height 2 m rings at 343/4 = 86 Hz and its multiples -----------
xn = 0.1 * np.random.default_rng(3).standard_normal(int(4 * sr))
yc = p.run(xn, sr, place=3, mix=100, cover=100, cover_height=2, cover_density=10)[:, 0][sr:]
yo = p.run(xn, sr, place=3, mix=100, cover=0)[:, 0][sr:]
def comb_contrast(y, f0):
    Y = np.abs(np.fft.rfft(y * np.hanning(len(y)))) ** 2
    f = np.fft.rfftfreq(len(y), 1 / sr)
    pk = sum(Y[np.abs(f - k * f0) < 3].mean() for k in range(3, 12))
    tr = sum(Y[np.abs(f - (k + 0.5) * f0) < 3].mean() for k in range(3, 12))
    return 10 * np.log10(pk / tr)
f0 = 343.0 / 4
cc, co = comb_contrast(yc, f0), comb_contrast(yo, f0)
check('Canopy at 2 m: peaks at 86 Hz multiples stand out (> 6 dB over troughs)', cc > 6 and cc > co + 5,
      '%.1f dB (Cover 0: %.1f dB)' % (cc, co))
check('Canopy: Cover 100 within +-4 dB of Cover 0 (RMS)', abs(db(rms(yc) / rms(yo))) < 4, '%+.1f dB' % db(rms(yc) / rms(yo)))
# sweeping Cover height live is a flange, not a crackle
xp = pluck_train(sr, 9.0)
ev = [(int(t * sr), 'cover_height', h) for t, h in ((2.0, 3), (3.5, 40), (5.0, 1.5), (6.5, 60), (8.0, 8))]
yh = p.run(xp, sr, place=3, cover=100, events=ev)
check('Cover height swept live: no ticks', tick_count(yh[:, 0], sr) == 0 and tick_count(yh[:, 1], sr) == 0, '')

# --- [every] levels, per scene -----------------------------------------------------------
xp = pluck_train(sr, 10.0)
yb = p.run(xp, sr, lv2_enabled=0)
for s, nm in enumerate(PLACES):
    y = p.run(xp, sr, place=s)
    d = db(rms(y[sr:]) / rms(yb[sr:]))
    check('%s defaults: within +-3 dB of bypass (RMS), no ticks' % nm,
          abs(d) < 3 and tick_count(y[:, 0], sr) == 0 and tick_count(y[:, 1], sr) == 0, '%+.1f dB' % d)

# [mix] Mix 100: wet only, dry gone
y = p.run(impulse(sr, 0.5), sr, mix=100)
check('Mix 100: no dry', np.abs(y[int(0.1 * sr) - 2:int(0.1 * sr) + 2]).max() < 1e-6, '')

# stereo: the outputs are decorrelated. Discrete panned echoes are partly correlated by
# nature, and the short scenes most, so the bar is 0.2 for the big open ones, 0.5 for all
for s_, nm in enumerate(PLACES):
    y = p.run(burst(sr, 6.0, 0.1, dur=0.05), sr, place=s_, mix=100)
    cor = np.corrcoef(y[int(0.15 * sr):, 0], y[int(0.15 * sr):, 1])[0, 1]
    lim = 0.2 if s_ in (0, 1, 2) else 0.5
    check('%s: stereo outputs decorrelated (|corr| < %.1f)' % (nm, lim), abs(cor) < lim, '%.2f' % cor)

# --- place change: duck, swap, fade back in; no click ------------------------------------
xs = 0.1 * np.sin(2 * np.pi * 220 * np.arange(int(4 * sr)) / sr) * np.minimum(1, np.arange(int(4 * sr)) / (0.05 * sr))
y = p.run(xs, sr, place=3, mix=50, events=[(2 * sr, 'place', 4)])
st = max(max_step(y[int(1.9 * sr):int(2.5 * sr), c]) for c in (0, 1))
ref = max(max_step(y[int(1.0 * sr):int(1.6 * sr), c]) for c in (0, 1))
check('place change: no click', st < 1.5 * ref, 'max step %.4f vs %.4f before' % (st, ref))
yp = p.run(xp, sr, events=[(int(t * sr), 'place', s) for t, s in ((2, 1), (4, 4), (6, 2), (8, 3))])
check('place changes while playing: no ticks', tick_count(yp[:, 0], sr) == 0 and tick_count(yp[:, 1], sr) == 0, '')
# Expanse glides like tape: no ticks while it moves
yd = p.run(xp, sr, place=1, events=[(int(t * sr), 'expanse', d) for t, d in ((2, 40), (4, 180), (6, 25), (8, 100))])
check('Expanse moved while playing: no ticks', tick_count(yd[:, 0], sr) == 0 and tick_count(yd[:, 1], sr) == 0, '')

# --- [feedback] worst case: rock, Bounce 10, closed ice canopy. Stays under 0 dBFS and dies away
xw = np.concatenate([pluck_train(sr, 6.0, level=0.3), np.zeros(int(34 * sr))])
for s, nm in enumerate(PLACES):
    y = p.run(xw, sr, place=s, mix=100, air=10, bounce=10, surface=10, cover=100, cover_density=10, cover_height=2)
    a, b = db(rms(y[int(7 * sr):int(8 * sr)])), db(rms(y[int(38 * sr):int(39 * sr)]))
    check('%s worst case: peak < 1.0, decays >= 20 dB by 38 s' % nm, np.abs(y).max() < 1.0 and a - b >= 20,
          'peak %.2f, %.0f -> %.0f dB' % (np.abs(y).max(), a, b))

# --- [every] [tail] bypass ---------------------------------------------------------------
x = 0.1 * np.sin(2 * np.pi * 220 * np.arange(int(3 * sr)) / sr) * np.minimum(1, np.arange(int(3 * sr)) / (0.05 * sr))
for tails in (1, 0):
    y = p.run(x, sr, place=3, tails=tails, events=[(sr, 'lv2_enabled', 0)])
    a, b = int(1.3 * sr), int(2.9 * sr)
    step = max(max_step(y[int(0.98 * sr):int(1.05 * sr), c]) for c in (0, 1))
    if tails:
        ok = rms(y[int(1.05 * sr):int(1.2 * sr), 0] - x[int(1.05 * sr):int(1.2 * sr)]) > 1e-3   # the tail still rings
    else:
        ok = np.abs(y[a:b, 0] - x[a:b]).max() < 1e-4 and np.abs(y[a:b, 1] - x[a:b]).max() < 1e-4
    check('bypass tails=%d: %s, no click' % (tails, 'tail rings out' if tails else 'dry at unity'),
          ok and step < 1.5 * max_step(x), 'max step %.4f' % step)
# Tails off clears the lines: switching back on doesn't replay old echoes
xb = burst(sr, 8.0, 0.1)
y = p.run(xb, sr, place=0, mix=100, tails=0, events=[(int(0.5 * sr), 'lv2_enabled', 0), (int(2.0 * sr), 'lv2_enabled', 1)])
check('Tails off: old echoes cleared, nothing replays after re-enable', np.abs(y[int(2.1 * sr):]).max() < 1e-4,
      'peak %.2g' % np.abs(y[int(2.1 * sr):]).max())

# [every] first run: a non-default value from sample 0 doesn't glide in
y = p.run(impulse(sr, 2.0), sr, expanse=50, mix=100, surface=10, bounce=0, air=10)
check('first run: Expanse 50 is in place from sample 0 (wall at 1.3 s)', near(echoes(y[:, 0], sr), 1.3), '')

# [every] torture: every knob at its extremes, hot input, three rates: finite, under 0 dBFS
for srt in (44100, 48000, 96000):
    xt = 0.8 * np.random.default_rng(1).standard_normal(int(3 * srt))
    for kw in (dict(place=4, expanse=200, surface=10, air=10, bounce=10, cover=100, cover_height=1, cover_density=10, mix=100, tone=10),
               dict(place=3, expanse=2, surface=0, air=0, bounce=0, cover=0, cover_height=60, cover_density=0, mix=0, tone=0)):
        y = p.run(xt, srt, **kw)
        check('torture %d Hz %s' % (srt, 'max' if kw['bounce'] else 'min'),
              np.isfinite(y).all() and (kw['mix'] == 0 or np.abs(y).max() < 1.0), 'peak %.2f' % np.abs(y).max())

# --- factory presets (bundle/presets.ttl): each one plays at a sane level, no ticks ---------
import rdflib
PS = rdflib.Namespace('http://lv2plug.in/ns/ext/presets#'); LV = rdflib.Namespace('http://lv2plug.in/ns/lv2core#')
RS = rdflib.Namespace('http://www.w3.org/2000/01/rdf-schema#')
g = rdflib.Graph(); g.parse(os.path.join(HERE, '..', 'bundle', 'nhe-ouraverb.lv2', 'presets.ttl'), format='turtle')
xp = pluck_train(sr, 10.0)
yb = p.run(xp, sr, lv2_enabled=0)
for pr in sorted(g.subjects(rdflib.RDF.type, PS.Preset)):
    vals = {str(g.value(n, LV.symbol)): float(g.value(n, PS.value)) for n in g.objects(pr, LV['port'])}
    y = p.run(xp, sr, **vals)
    d = db(rms(y[sr:]) / rms(yb[sr:]))
    # Niels's own presets, made on his unit: tiny rooms (Expanse 2 %) stack their reflections
    # within a few ms, so they play louder. Kept as made; the limit is wider for them
    lim = 7 if str(g.value(pr, RS.label)) in ('Bottom of the Well', 'Testing Booth') else 4
    check('preset "%s": within +-%d dB of bypass, peak < 1, no ticks' % (g.value(pr, RS.label), lim),
          abs(d) < lim and np.abs(y).max() < 1.0 and tick_count(y[:, 0], sr) == 0 and tick_count(y[:, 1], sr) == 0,
          '%+.1f dB, peak %.2f' % (d, np.abs(y).max()))
    # presets leave Mix alone (many players run it 100 % wet): so each must also be safe fully wet
    y = p.run(xp, sr, **dict(vals, mix=100))
    check('preset "%s" sets no Mix, and fully wet: peak < 1, no ticks' % g.value(pr, RS.label),
          'mix' not in vals and np.abs(y).max() < 1.0 and tick_count(y[:, 0], sr) == 0 and tick_count(y[:, 1], sr) == 0,
          'peak %.2f' % np.abs(y).max())

# Inner City is a Fjord at 3 %: building fronts 13 m apart, a slap-flutter
y = p.run(impulse(sr, 0.6), sr, expanse=3, **CLEAN)
check('Fjord at Expanse 3 %: walls at 78 and 96 ms', near(echoes(y[:, 0], sr), 0.078, 0.001) and near(echoes(y[:, 1], sr), 0.096, 0.001), '')

# --- the face's orbit ripples: Input Level follows the playing (Trance Atlantic's follower) ---
xl = pluck_train(sr, 4.0)
_, lv = p.run(xl, sr, trace='input_level')
_, lv0 = p.run(np.zeros(sr), sr, trace='input_level')
_, lvb = p.run(xl, sr, lv2_enabled=0, trace='input_level')
lv, lvb = np.asarray(lv), np.asarray(lvb)
check('Input Level: a pluck reads 0.5..0.9, silence 0, still moving in bypass',
      0.5 < lv.max() < 0.9 and np.asarray(lv0).max() == 0 and lvb.max() > 0.5,
      'pluck peak %.2f, bypass peak %.2f' % (lv.max(), lvb.max()))

# --- Tone: which pitches the place throws back --------------------------------------------
import scipy.signal as _ss
def _band(v, lo, hi):
    return rms(_ss.sosfilt(_ss.butter(4, [lo, hi], 'bandpass', fs=sr, output='sos'), v))
_b = burst(sr, 12.0, 0.1, dur=0.05)
_y5 = p.run(_b, sr, place=1, mix=100, tone=5, surface=10, air=10)[:, 0]
_y10 = p.run(_b, sr, place=1, mix=100, tone=10, surface=10, air=10)[:, 0]
_y0 = p.run(_b, sr, place=1, mix=100, tone=0, surface=10, air=10)[:, 0]
_e = slice(int(0.405 * sr), int(0.47 * sr))      # Canyon's first ledge, 0.31 s after the burst
_lo = lambda v: db(_band(v[_e], 60, 300)); _hi = lambda v: db(_band(v[_e], 2000, 8000))
check('Tone 10 (bright): the first echo loses >= 8 dB of lows, keeps its highs (within 3 dB)',
      _lo(_y5) - _lo(_y10) >= 8 and abs(_hi(_y10) - _hi(_y5)) < 3,
      'lows %+.1f dB, highs %+.1f dB' % (_lo(_y10) - _lo(_y5), _hi(_y10) - _hi(_y5)))
check('Tone 0 (warm): the first echo loses >= 6 dB of highs, keeps its lows (within 3 dB)',
      _hi(_y5) - _hi(_y0) >= 6 and abs(_lo(_y0) - _lo(_y5)) < 3,
      'highs %+.1f dB, lows %+.1f dB' % (_hi(_y0) - _hi(_y5), _lo(_y0) - _lo(_y5)))
_late = slice(int(5 * sr), int(8 * sr))
_ll5, _ll10 = db(_band(_y5[_late], 30, 300)), db(_band(_y10[_late], 30, 300))
check('Tone 10: the lows roll on (5..8 s low band >= 6 dB above Tone 5)', _ll10 - _ll5 >= 6, '%+.1f dB' % (_ll10 - _ll5))
xp = pluck_train(sr, 10.0)
yb = p.run(xp, sr, lv2_enabled=0)
for s_, nm in enumerate(PLACES):
    for tn in (0, 10):
        y = p.run(xp, sr, place=s_, tone=tn)
        d = db(rms(y[sr:]) / rms(yb[sr:]))
        check('%s Tone %d: within +-4 dB of bypass, no ticks' % (nm, tn),
              abs(d) < 4 and tick_count(y[:, 0], sr) == 0 and tick_count(y[:, 1], sr) == 0, '%+.1f dB' % d)
xw = np.concatenate([pluck_train(sr, 6.0, level=0.3), np.zeros(int(40 * sr))])
y = p.run(xw, sr, place=1, mix=100, air=10, bounce=10, surface=10, cover=100, cover_density=10, cover_height=2, tone=10, expanse=200)
a, b = db(rms(y[int(7 * sr):int(8 * sr)])), db(rms(y[int(44 * sr):int(45 * sr)]))
check('Tone 10 worst case (Bounce 10, Expanse 200, closed ice): peak < 1, decays >= 20 dB by 44 s',
      np.abs(y).max() < 1.0 and a - b >= 20, 'peak %.2f, %.0f -> %.0f dB' % (np.abs(y).max(), a, b))

# [every] silence in, silence out
y = p.run(np.zeros(sr), sr)
check('silence', np.abs(y).max() < 1e-6)

done()
